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
// Topic operations
// =============================================================================

void URosBridgeSubsystem::Subscribe(const FString& Topic, const FString& Type)
{
    if (!IsConnected())
    {
        UE_LOG(LogRosBridge, Warning,
            TEXT("Subscribe('%s') called but not connected. Ignoring."), *Topic);
        return;
    }

    // Build: {"op":"subscribe","topic":"<Topic>","type":"<Type>"}
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("op"), TEXT("subscribe"));
    Json->SetStringField(TEXT("topic"), Topic);
    Json->SetStringField(TEXT("type"), Type);

    FString Payload;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
    FJsonSerializer::Serialize(Json, Writer);

    Socket->Send(Payload);
    SubscribedTopics.Add(Topic, Type);

    UE_LOG(LogRosBridge, Log, TEXT("Subscribed to %s (%s)"), *Topic, *Type);
}

// =============================================================================
// WebSocket event handlers (NON-GAME THREAD)
// =============================================================================

void URosBridgeSubsystem::HandleConnected()
{
    UE_LOG(LogRosBridge, Log, TEXT("WebSocket connected"));
}

void URosBridgeSubsystem::HandleConnectionError(const FString& Error)
{
    UE_LOG(LogRosBridge, Error, TEXT("WebSocket connection error: %s"), *Error);
}

void URosBridgeSubsystem::HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    UE_LOG(LogRosBridge, Warning,
        TEXT("WebSocket closed (code=%d, clean=%s): %s"),
        StatusCode, bWasClean ? TEXT("true") : TEXT("false"), *Reason);
}

void URosBridgeSubsystem::HandleMessage(const FString& Message)
{
    // We are NOT on the game thread. Marshal to the game thread before
    // touching any UObject state (including broadcasting the delegate).
    TWeakObjectPtr<URosBridgeSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, Message]()
        {
            if (URosBridgeSubsystem * StrongThis = WeakThis.Get())
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
    else
    {
        // Other ops (status, service_response, etc.) - just log for now.
        UE_LOG(LogRosBridge, Verbose, TEXT("Unhandled op '%s': %s"), *Op, *Message);
    }
}