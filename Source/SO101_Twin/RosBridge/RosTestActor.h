#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RosTestActor.generated.h"

/**
 * Test actor that verifies both directions of the rosbridge subsystem.
 *
 * - Subscribe: listens on a configurable topic (default /chatter) and prints to Output Log + screen
 * - Publish: advertises /unreal_test (std_msgs/String) and publishes a counter message every second
 *
 * Drop one instance into any level, then Play In Editor with rosbridge_server running.
 */
UCLASS()
class SO101_TWIN_API ARosTestActor : public AActor
{
    GENERATED_BODY()

public:
    ARosTestActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // --- Subscribe test config ---

    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString RosBridgeUrl = TEXT("ws://127.0.0.1:9090/?x=1");

    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString TopicName = TEXT("/chatter");

    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString TopicType = TEXT("std_msgs/String");

    // --- Publish test config ---

    UPROPERTY(EditAnywhere, Category = "ROS|Test|Publish")
    FString PublishTopicName = TEXT("/unreal_test");

    UPROPERTY(EditAnywhere, Category = "ROS|Test|Publish")
    FString PublishTopicType = TEXT("std_msgs/String");

    /** Interval in seconds between published messages. */
    UPROPERTY(EditAnywhere, Category = "ROS|Test|Publish")
    float PublishIntervalSeconds = 1.0f;

    /** Enable/disable the publish test. */
    UPROPERTY(EditAnywhere, Category = "ROS|Test|Publish")
    bool bEnablePublishTest = true;

private:
    UFUNCTION()
    void OnRosMessage(const FString& Topic, const FString& MessageJson);

    UFUNCTION()
    void OnRosBridgeConnected();

    /** Publish one test message. Called by repeating timer. */
    void PublishTestMessage();

    FTimerHandle PublishTimerHandle;
    int32 PublishCounter = 0;
};
