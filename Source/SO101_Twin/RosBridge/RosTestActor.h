#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RosTestActor.generated.h"

/**
 * Minimal test actor that verifies the rosbridge subsystem end-to-end.
 *
 * Drop one instance of this actor into any level, then Play In Editor.
 * With rosbridge_server running in WSL2 and demo_nodes_cpp talker publishing
 * on /chatter, the Output Log should print a "Received on /chatter: ..."
 * line every second and the same message will appear on-screen.
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

    /**
    *URL of the rosbridge WebSocket server.
    *
    * NOTE: The trailing "/?x=1" is a workaround for a libwebsockets quirk in
    * Unreal's IWebSocket implementation. When the path is empty or just "/",
    * the underlying library sends "GET //" which rosbridge rejects with 404.
    * Any non - trivial query string(or path) avoids the double - slash bug.
    */
    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString RosBridgeUrl = TEXT("ws://127.0.0.1:9090/?x=1");

    /** Topic we want to subscribe to. */
    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString TopicName = TEXT("/chatter");

    /** ROS message type string that rosbridge expects for the subscribe op. */
    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    FString TopicType = TEXT("std_msgs/String");

    /** Delay (seconds) between Connect and Subscribe, to give the handshake time to complete. */
    UPROPERTY(EditAnywhere, Category = "ROS|Test")
    float SubscribeDelaySeconds = 1.0f;

private:
    /** Handler bound to URosBridgeSubsystem::OnTopicMessage. */
    UFUNCTION()
    void OnRosMessage(const FString& Topic, const FString& MessageJson);

    /** Timer handle for the delayed Subscribe call. */
    FTimerHandle SubscribeTimerHandle;

    /** Actually send the subscribe op (called by the timer). */
    void DoSubscribe();
};