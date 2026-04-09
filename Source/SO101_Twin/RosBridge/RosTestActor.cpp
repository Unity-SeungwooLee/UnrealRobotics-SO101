#include "RosTestActor.h"
#include "RosBridgeSubsystem.h"
#include "RosBridgeLog.h"

#include "Engine/Engine.h"       // GEngine
#include "Engine/World.h"        // GetWorld
#include "TimerManager.h"        // FTimerManager
#include "Kismet/GameplayStatics.h"

ARosTestActor::ARosTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARosTestActor::BeginPlay()
{
    Super::BeginPlay();

    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    if (!GI)
    {
        UE_LOG(LogRosBridge, Error, TEXT("RosTestActor: no GameInstance"));
        return;
    }

    URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
    if (!Ros)
    {
        UE_LOG(LogRosBridge, Error, TEXT("RosTestActor: RosBridgeSubsystem not found"));
        return;
    }

    // Bind the delegate BEFORE connecting so we do not miss early messages.
    Ros->OnTopicMessage.AddDynamic(this, &ARosTestActor::OnRosMessage);

    if (!Ros->IsConnected())
    {
        Ros->Connect(RosBridgeUrl);
    }

    // Connect is asynchronous. Schedule the subscribe call slightly later so
    // the WebSocket handshake has a chance to finish. This is a minimal
    // approach — a more robust version would wait for an "on connected" event.
    GetWorld()->GetTimerManager().SetTimer(
        SubscribeTimerHandle,
        this,
        &ARosTestActor::DoSubscribe,
        SubscribeDelaySeconds,
        false // not looping
    );
}

void ARosTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
    {
        if (URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>())
        {
            Ros->OnTopicMessage.RemoveDynamic(this, &ARosTestActor::OnRosMessage);
        }
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(SubscribeTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void ARosTestActor::DoSubscribe()
{
    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    if (!GI) return;

    URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
    if (!Ros) return;

    if (!Ros->IsConnected())
    {
        UE_LOG(LogRosBridge, Warning,
            TEXT("RosTestActor: not connected yet, retrying subscribe in %.1fs"),
            SubscribeDelaySeconds);

        // Retry once more.
        GetWorld()->GetTimerManager().SetTimer(
            SubscribeTimerHandle,
            this,
            &ARosTestActor::DoSubscribe,
            SubscribeDelaySeconds,
            false);
        return;
    }

    Ros->Subscribe(TopicName, TopicType);
}

void ARosTestActor::OnRosMessage(const FString& Topic, const FString& MessageJson)
{
    UE_LOG(LogRosBridge, Log, TEXT("Received on %s: %s"), *Topic, *MessageJson);

    if (GEngine)
    {
        const FString ScreenMsg = FString::Printf(TEXT("%s: %s"), *Topic, *MessageJson);
        GEngine->AddOnScreenDebugMessage(
            -1,        // key (-1 means always add a new line)
            2.0f,      // display time in seconds
            FColor::Green,
            ScreenMsg
        );
    }
}