#include "RosBridgeSubsystem.h"
#include "RosBridgeLog.h"

#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

// =============================================================================
// Subsystem lifecycle
// =============================================================================

void URosBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // The WebSockets module may not be loaded yet at this point (depends on
    // module startup order), so force-load it before we try to create a socket.
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
        FModuleManager::Get().LoadModule("WebSockets");
    }

    UE_LOG(LogRosBridge, Log, TEXT("RosBridgeSubsystem initialized"));
}

void URosBridgeSubsystem::Deinitialize()
{
    // Disable auto-reconnect first so Disconnect doesn't trigger reconnection.
    bAutoReconnect = false;

    // Cancel any pending reconnect timer.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
        }
    }

    Disconnect();
    UE_LOG(LogRosBridge, Log, TEXT("RosBridgeSubsystem deinitialized"));

    Super::Deinitialize();
}

// =============================================================================
// Connection control
// =============================================================================

void URosBridgeSubsystem::Connect(const FString& Url)
{
    if (Socket.IsValid() && Socket->IsConnected())
    {
        UE_LOG(LogRosBridge, Warning,
            TEXT("Connect called but socket is already connected. Ignoring."));
        return;
    }

    // If we have an old socket that's not connected (e.g. from a failed attempt),
    // clean it up before creating a new one.
    if (Socket.IsValid())
    {
        Socket.Reset();
    }

    ConnectionUrl = Url;
    bAutoReconnect = true;
    CurrentReconnectDelay = BaseReconnectDelay;
    ReconnectAttemptCount = 0;

    UE_LOG(LogRosBridge, Log, TEXT("Connecting to %s ..."), *Url);

    Socket = FWebSocketsModule::Get().CreateWebSocket(Url);

    // Bind handlers. All of these fire on a non-game thread.
    Socket->OnConnected().AddUObject(this, &URosBridgeSubsystem::HandleConnected);
    Socket->OnConnectionError().AddUObject(this, &URosBridgeSubsystem::HandleConnectionError);
    Socket->OnClosed().AddUObject(this, &URosBridgeSubsystem::HandleClosed);
    Socket->OnMessage().AddUObject(this, &URosBridgeSubsystem::HandleMessage);

    Socket->Connect();
}

void URosBridgeSubsystem::Disconnect()
{
    // Explicit disconnect: disable auto-reconnect.
    bAutoReconnect = false;

    // Cancel pending reconnect timer.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
        }
    }

    if (Socket.IsValid())
    {
        if (Socket->IsConnected())
        {
            Socket->Close();
        }
        Socket.Reset();
    }
}

bool URosBridgeSubsystem::IsConnected() const
{
    return Socket.IsValid() && Socket->IsConnected();
}

// =============================================================================
// Topic operations — Subscribe (ROS → UE)
// =============================================================================

void URosBridgeSubsystem::Subscribe(const FString& Topic, const FString& Type)
{
    // Always track the subscription so it gets restored on reconnect.
    SubscribedTopics.Add(Topic, Type);

    if (!IsConnected())
    {
        UE_LOG(LogRosBridge, Log,
            TEXT("Subscribe('%s') queued — will be sent on connect."), *Topic);
        return;
    }

    // Build: {"op":"subscribe","topic":"<Topic>","type":"<Type>"}
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("op"), TEXT("subscribe"));
    Json->SetStringField(TEXT("topic"), Topic);
    Json->SetStringField(TEXT("type"), Type);
    Json->SetNumberField(TEXT("fragment_size"), 1000);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json, Writer);

    SendRaw(Payload);
    UE_LOG(LogRosBridge, Log, TEXT("Subscribed to %s (%s)"), *Topic, *Type);
}

// =============================================================================
// Topic operations — Advertise / Publish (UE → ROS)
// =============================================================================

void URosBridgeSubsystem::Advertise(const FString& Topic, const FString& Type)
{
    // Always track the advertisement so it gets restored on reconnect.
    AdvertisedTopics.Add(Topic, Type);

    if (!IsConnected())
    {
        UE_LOG(LogRosBridge, Log,
            TEXT("Advertise('%s') queued — will be sent on connect."), *Topic);
        return;
    }

    // Build: {"op":"advertise","topic":"<Topic>","type":"<Type>"}
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("op"), TEXT("advertise"));
    Json->SetStringField(TEXT("topic"), Topic);
    Json->SetStringField(TEXT("type"), Type);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json, Writer);

    SendRaw(Payload);
    UE_LOG(LogRosBridge, Log, TEXT("Advertised %s (%s)"), *Topic, *Type);
}

void URosBridgeSubsystem::Publish(const FString& Topic, const FString& MsgJson)
{
    if (!IsConnected())
    {
        UE_LOG(LogRosBridge, Warning,
            TEXT("Publish('%s') called but not connected. Dropping message."), *Topic);
        return;
    }

    if (!AdvertisedTopics.Contains(Topic))
    {
        UE_LOG(LogRosBridge, Warning,
            TEXT("Publish('%s') called but topic not advertised. Call Advertise() first. Dropping."), *Topic);
        return;
    }

    // Build: {"op":"publish","topic":"<Topic>","msg":<MsgJson>}
    //
    // We need to embed MsgJson as a JSON *object*, not as a string.
    // So we parse MsgJson first, then insert it as a sub-object.
    TSharedPtr<FJsonObject> MsgObject;
    const TSharedRef<TJsonReader<>> MsgReader = TJsonReaderFactory<>::Create(MsgJson);
    if (!FJsonSerializer::Deserialize(MsgReader, MsgObject) || !MsgObject.IsValid())
    {
        UE_LOG(LogRosBridge, Error,
            TEXT("Publish('%s'): failed to parse MsgJson as JSON object. Dropping."), *Topic);
        return;
    }

    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("op"), TEXT("publish"));
    Json->SetStringField(TEXT("topic"), Topic);
    Json->SetObjectField(TEXT("msg"), MsgObject);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json, Writer);

    SendRaw(Payload);
}

// =============================================================================
// Internal helpers
// =============================================================================

bool URosBridgeSubsystem::SendRaw(const FString& JsonPayload)
{
    if (!IsConnected())
    {
        return false;
    }

    Socket->Send(JsonPayload);
    return true;
}

void URosBridgeSubsystem::RestoreSubscriptions()
{
    for (const auto& Pair : SubscribedTopics)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("op"), TEXT("subscribe"));
        Json->SetStringField(TEXT("topic"), Pair.Key);
        Json->SetStringField(TEXT("type"), Pair.Value);
        Json->SetNumberField(TEXT("fragment_size"), 1000);

        FString Payload;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
        FJsonSerializer::Serialize(Json, Writer);

        SendRaw(Payload);
        UE_LOG(LogRosBridge, Log, TEXT("Re-subscribed to %s (%s)"), *Pair.Key, *Pair.Value);
    }
}

void URosBridgeSubsystem::RestoreAdvertisements()
{
    for (const auto& Pair : AdvertisedTopics)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("op"), TEXT("advertise"));
        Json->SetStringField(TEXT("topic"), Pair.Key);
        Json->SetStringField(TEXT("type"), Pair.Value);

        FString Payload;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
        FJsonSerializer::Serialize(Json, Writer);

        SendRaw(Payload);
        UE_LOG(LogRosBridge, Log, TEXT("Re-advertised %s (%s)"), *Pair.Key, *Pair.Value);
    }
}

// =============================================================================
// Reconnection
// =============================================================================

void URosBridgeSubsystem::ScheduleReconnect()
{
    if (!bAutoReconnect)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UWorld* World = GI->GetWorld();
    if (!World) return;

    ReconnectAttemptCount++;
    UE_LOG(LogRosBridge, Warning,
        TEXT("Scheduling reconnect attempt #%d in %.1fs ..."),
        ReconnectAttemptCount, CurrentReconnectDelay);

    World->GetTimerManager().SetTimer(
        ReconnectTimerHandle,
        this,
        &URosBridgeSubsystem::AttemptReconnect,
        CurrentReconnectDelay,
        false  // not looping
    );

    // Exponential backoff: double the delay, cap at MaxReconnectDelay.
    CurrentReconnectDelay = FMath::Min(CurrentReconnectDelay * 2.0f, MaxReconnectDelay);
}

void URosBridgeSubsystem::AttemptReconnect()
{
    if (!bAutoReconnect || ConnectionUrl.IsEmpty())
    {
        return;
    }

    UE_LOG(LogRosBridge, Log,
        TEXT("Reconnect attempt #%d to %s ..."), ReconnectAttemptCount, *ConnectionUrl);

    // Clean up old socket if it still exists.
    if (Socket.IsValid())
    {
        Socket.Reset();
    }

    Socket = FWebSocketsModule::Get().CreateWebSocket(ConnectionUrl);

    Socket->OnConnected().AddUObject(this, &URosBridgeSubsystem::HandleConnected);
    Socket->OnConnectionError().AddUObject(this, &URosBridgeSubsystem::HandleConnectionError);
    Socket->OnClosed().AddUObject(this, &URosBridgeSubsystem::HandleClosed);
    Socket->OnMessage().AddUObject(this, &URosBridgeSubsystem::HandleMessage);

    Socket->Connect();
}

// =============================================================================
// WebSocket event handlers (NON-GAME THREAD)
// =============================================================================

void URosBridgeSubsystem::HandleConnected()
{
    // Marshal to game thread — we will touch UObject state (delegates, topic maps).
    TWeakObjectPtr<URosBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
        {
            if (URosBridgeSubsystem* StrongThis = WeakThis.Get())
            {
                // Reset backoff on successful connection.
                StrongThis->CurrentReconnectDelay = BaseReconnectDelay;
                StrongThis->ReconnectAttemptCount = 0;

                UE_LOG(LogRosBridge, Log, TEXT("WebSocket connected to %s"), *StrongThis->ConnectionUrl);

                // Restore all tracked subscriptions and advertisements.
                StrongThis->RestoreSubscriptions();
                StrongThis->RestoreAdvertisements();

                // Broadcast to listeners (RobotVisualizer, etc.)
                StrongThis->OnConnected.Broadcast();
            }
        });
}

void URosBridgeSubsystem::HandleConnectionError(const FString& Error)
{
    TWeakObjectPtr<URosBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Error]()
        {
            if (URosBridgeSubsystem* StrongThis = WeakThis.Get())
            {
                UE_LOG(LogRosBridge, Error, TEXT("WebSocket connection error: %s"), *Error);
                StrongThis->OnDisconnected.Broadcast();
                StrongThis->ScheduleReconnect();
            }
        });
}

void URosBridgeSubsystem::HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    TWeakObjectPtr<URosBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, StatusCode, Reason, bWasClean]()
        {
            if (URosBridgeSubsystem* StrongThis = WeakThis.Get())
            {
                UE_LOG(LogRosBridge, Warning,
                    TEXT("WebSocket closed (code=%d, clean=%s): %s"),
                    StatusCode, bWasClean ? TEXT("true") : TEXT("false"), *Reason);
                StrongThis->OnDisconnected.Broadcast();
                StrongThis->ScheduleReconnect();
            }
        });
}

void URosBridgeSubsystem::HandleMessage(const FString& Message)
{
    // We are NOT on the game thread. Marshal to the game thread before
    // touching any UObject state (including broadcasting the delegate).
    TWeakObjectPtr<URosBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Message]()
        {
            if (URosBridgeSubsystem* StrongThis = WeakThis.Get())
            {
                StrongThis->ProcessIncomingMessage(Message);
            }
        });
}

// =============================================================================
// Game-thread message processing
// =============================================================================

void URosBridgeSubsystem::ProcessIncomingMessage(const FString& Message)
{
    // Parse the top-level JSON object.
    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogRosBridge, Error, TEXT("Failed to parse incoming JSON: %s"), *Message);
        return;
    }

    // rosbridge v2 uses an "op" field to describe the operation.
    // For a subscribed topic we care about {"op":"publish","topic":...,"msg":{...}}.
    FString Op;
    if (!Json->TryGetStringField(TEXT("op"), Op))
    {
        UE_LOG(LogRosBridge, Warning, TEXT("Incoming message has no 'op' field: %s"), *Message);
        return;
    }

    if (Op == TEXT("publish"))
    {
        FString Topic;
        if (!Json->TryGetStringField(TEXT("topic"), Topic))
        {
            UE_LOG(LogRosBridge, Warning, TEXT("publish without topic: %s"), *Message);
            return;
        }

        // Re-serialize the "msg" sub-object back into a string. This keeps the
        // subsystem message-type-agnostic: the receiver parses whatever it needs.
        const TSharedPtr<FJsonObject>* MsgObjectPtr = nullptr;
        FString MsgJson;
        if (Json->TryGetObjectField(TEXT("msg"), MsgObjectPtr) && MsgObjectPtr && MsgObjectPtr->IsValid())
        {
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgJson);
            FJsonSerializer::Serialize(MsgObjectPtr->ToSharedRef(), Writer);
        }

        OnTopicMessage.Broadcast(Topic, MsgJson);
    }
    else if (Op == TEXT("fragment"))
    {
        // rosbridge splits large messages into fragments:
        // {"op":"fragment","id":...,"num":i,"total":n,"data":"chunk"}
        FString FragId, FragData;
        int32 FragNum = 0, FragTotal = 0;
        Json->TryGetStringField(TEXT("id"), FragId);
        Json->TryGetNumberField(TEXT("num"), FragNum);
        Json->TryGetNumberField(TEXT("total"), FragTotal);
        Json->TryGetStringField(TEXT("data"), FragData);

        UE_LOG(LogRosBridge, Warning, TEXT("Fragment %d/%d id=%s"), FragNum, FragTotal, *FragId);

        if (FragTotal <= 0 || FragId.IsEmpty()) { return; }

        TArray<FString>& Parts = FragmentBuffers.FindOrAdd(FragId);
        if (Parts.Num() != FragTotal) { Parts.Init(FString(), FragTotal); }
        if (Parts.IsValidIndex(FragNum)) { Parts[FragNum] = FragData; }

        bool bComplete = true;
        for (const FString& P : Parts) { if (P.IsEmpty()) { bComplete = false; break; } }
        if (bComplete)
        {
            FString Reassembled;
            for (const FString& P : Parts) { Reassembled += P; }
            FragmentBuffers.Remove(FragId);
            ProcessIncomingMessage(Reassembled);  // re-process the full message
        }
    }
    else
    {
        UE_LOG(LogRosBridge, Verbose, TEXT("Unhandled op '%s': %s"), *Op, *Message);
    }
}
