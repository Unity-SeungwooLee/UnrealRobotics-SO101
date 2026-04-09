#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RosBridgeSubsystem.generated.h"

// Forward declarations — avoid pulling heavy headers into this header.
class IWebSocket;

/**
 * Delegate fired when a subscribed topic receives a message.
 *
 * @param Topic      The topic name (e.g. "/chatter")
 * @param MessageJson  The "msg" field of the rosbridge publish message,
 *                     already extracted as a JSON string for the caller
 *                     to parse into whatever struct they need.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnRosTopicMessage,
    const FString&, Topic,
    const FString&, MessageJson
);

/**
 * Game instance subsystem that owns the WebSocket connection to
 * rosbridge_server and dispatches incoming topic messages to subscribers.
 *
 * Typical usage from any actor or component:
 *
 *   URosBridgeSubsystem* Ros =
 *       GetGameInstance()->GetSubsystem<URosBridgeSubsystem>();
 *   Ros->Connect(TEXT("ws://localhost:9090"));
 *   Ros->Subscribe(TEXT("/chatter"), TEXT("std_msgs/String"));
 *   Ros->OnTopicMessage.AddDynamic(this, &AMyActor::HandleRosMessage);
 */
UCLASS()
class SO101_TWIN_API URosBridgeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- Subsystem lifecycle ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- Connection control ---

    /**
    * Open a WebSocket connection to the given rosbridge URL.
    *
    * Default URL uses 127.0.0.1 explicitly (not "localhost") and includes
    * a dummy "?x=1" query string to work around a libwebsockets path-parsing
    * quirk. See RosTestActor.h for details.
    */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Connect(const FString& Url = TEXT("ws://127.0.0.1:9090/?x=1"));

    /** Close the WebSocket connection if open. */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Disconnect();

    /** True if the WebSocket is currently open. */
    UFUNCTION(BlueprintPure, Category = "ROS|Bridge")
    bool IsConnected() const;

    // --- Topic operations ---

    /**
     * Subscribe to a ROS topic through rosbridge.
     * Must be called after IsConnected() returns true, OR queued
     * internally and flushed on connect. In this minimal version we
     * require the connection to be open already.
     */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Subscribe(const FString& Topic, const FString& Type);

    // --- Events ---

    /** Broadcast whenever a publish message arrives on a subscribed topic. */
    UPROPERTY(BlueprintAssignable, Category = "ROS|Bridge")
    FOnRosTopicMessage OnTopicMessage;

private:
    // --- WebSocket event handlers (all run on non-game thread!) ---
    void HandleConnected();
    void HandleConnectionError(const FString& Error);
    void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleMessage(const FString& Message);

    // --- Game-thread message processing ---
    void ProcessIncomingMessage(const FString& Message);

    // --- State ---

    /** The underlying WebSocket. Kept as a TSharedPtr because IWebSocket
     *  is a plain C++ interface, not a UObject, so UPROPERTY does not apply. */
    TSharedPtr<IWebSocket> Socket;

    /** Topics we have sent a subscribe op for, kept so we can re-subscribe on reconnect later. */
    UPROPERTY()
    TMap<FString, FString> SubscribedTopics;
};