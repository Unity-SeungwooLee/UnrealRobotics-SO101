#include "RosTestActor.h"
#include "RosBridgeSubsystem.h"
#include "RosBridgeLog.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ARosTestActor::ARosTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ARosTestActor::BeginPlay()
{
    Super::BeginPlay();

    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    if (!GI) return;

    URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
    if (!Ros) return;

    // Bind delegates.
    Ros->OnTopicMessage.AddDynamic(this, &ARosTestActor::OnRosMessage);
    Ros->OnConnected.AddDynamic(this, &ARosTestActor::OnRosBridgeConnected);

    // Queue subscribe — sent automatically when connected.
    Ros->Subscribe(TopicName, TopicType);

    // Queue advertise for publish test.
    if (bEnablePublishTest)
    {
        Ros->Advertise(PublishTopicName, PublishTopicType);
    }

    // Initiate connection if not already connected.
    if (!Ros->IsConnected())
    {
        Ros->Connect(RosBridgeUrl);
    }
}

void ARosTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Stop publish timer.
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PublishTimerHandle);
    }

    // Unbind delegates.
    if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
    {
        if (URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>())
        {
            Ros->OnTopicMessage.RemoveDynamic(this, &ARosTestActor::OnRosMessage);
            Ros->OnConnected.RemoveDynamic(this, &ARosTestActor::OnRosBridgeConnected);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ARosTestActor::OnRosBridgeConnected()
{
    UE_LOG(LogRosBridge, Log, TEXT("RosTestActor: connected — starting publish timer"));

    // Start repeating publish timer.
    if (bEnablePublishTest && GetWorld())
    {
        PublishCounter = 0;
        GetWorld()->GetTimerManager().SetTimer(
            PublishTimerHandle,
            this,
            &ARosTestActor::PublishTestMessage,
            PublishIntervalSeconds,
            true  // looping
        );
    }
}

void ARosTestActor::PublishTestMessage()
{
    UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
    if (!GI) return;

    URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
    if (!Ros) return;

    // Build std_msgs/String JSON: {"data": "Hello from Unreal #42"}
    const FString Msg = FString::Printf(
        TEXT("{\"data\": \"Hello from Unreal #%d\"}"), PublishCounter);

    Ros->Publish(PublishTopicName, Msg);

    UE_LOG(LogRosBridge, Log, TEXT("Published to %s: %s"), *PublishTopicName, *Msg);
    PublishCounter++;
}

void ARosTestActor::OnRosMessage(const FString& Topic, const FString& MessageJson)
{
    UE_LOG(LogRosBridge, Log, TEXT("Received on %s: %s"), *Topic, *MessageJson);

    if (GEngine)
    {
        const FString ScreenMsg = FString::Printf(TEXT("%s: %s"), *Topic, *MessageJson);
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, ScreenMsg);
    }
}
