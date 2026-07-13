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

/** Delegate fired when the WebSocket connection to rosbridge is established. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRosBridgeConnected);

/** Delegate fired when the WebSocket connection to rosbridge is lost. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRosBridgeDisconnected);

/**
 * Game instance subsystem that owns the WebSocket connection to
 * rosbridge_server and dispatches incoming topic messages to subscribers.
 *
 * Phase 6 additions:
 *   - Advertise/Publish API for UE → ROS messaging
 *   - FOnRosBridgeConnected / FOnRosBridgeDisconnected delegates
 *   - Automatic reconnection with exponential backoff
 *   - Re-subscribe + re-advertise on reconnect
 *
 * Typical usage from any actor or component:
 *
 *   URosBridgeSubsystem* Ros =
 *       GetGameInstance()->GetSubsystem<URosBridgeSubsystem>();
 *   Ros->OnConnected.AddDynamic(this, &AMyActor::OnRosConnected);
 *   Ros->Connect(TEXT("ws://127.0.0.1:9090/?x=1"));
 *
 *   // In OnRosConnected():
 *   Ros->Subscribe(TEXT("/joint_states"), TEXT("sensor_msgs/JointState"));
 *   Ros->Advertise(TEXT("/cmd_vel"), TEXT("geometry_msgs/Twist"));
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
    * quirk. See SKILL.md sections 7.1 and 7.2 for details.
    *
    * After a successful connection, OnConnected is broadcast and
    * auto-reconnection is enabled for this URL.
    */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Connect(const FString& Url = TEXT("ws://127.0.0.1:9090/?x=1"));

    /** Close the WebSocket connection if open. Disables auto-reconnection. */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Disconnect();

    /** True if the WebSocket is currently open. */
    UFUNCTION(BlueprintPure, Category = "ROS|Bridge")
    bool IsConnected() const;

    // --- Topic operations (subscribe: ROS → UE) ---

    /**
     * Subscribe to a ROS topic through rosbridge.
     * If not yet connected, the subscription is queued and sent
     * automatically when the connection is established.
     */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Subscribe(const FString& Topic, const FString& Type);

    // --- Topic operations (publish: UE → ROS) ---

    /**
     * Advertise a ROS topic through rosbridge.
     * Must be called before Publish(). If not yet connected, the
     * advertisement is queued and sent when the connection is established.
     *
     * rosbridge silently drops publishes on un-advertised topics.
     */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Advertise(const FString& Topic, const FString& Type);

    /**
     * Publish a message to a previously advertised ROS topic.
     *
     * @param Topic     The topic name (must have been Advertise'd first)
     * @param MsgJson   The "msg" payload as a JSON string. The caller is
     *                  responsible for building the correct message structure.
     *                  Example for geometry_msgs/Twist:
     *                  {"linear":{"x":0.5,"y":0,"z":0},"angular":{"x":0,"y":0,"z":0.3}}
     */
    UFUNCTION(BlueprintCallable, Category = "ROS|Bridge")
    void Publish(const FString& Topic, const FString& MsgJson);

    // --- Events ---

    /** Broadcast whenever a publish message arrives on a subscribed topic. */
    UPROPERTY(BlueprintAssignable, Category = "ROS|Bridge")
    FOnRosTopicMessage OnTopicMessage;

    /** Broadcast when the WebSocket connection to rosbridge is established (including reconnects). */
    UPROPERTY(BlueprintAssignable, Category = "ROS|Bridge")
    FOnRosBridgeConnected OnConnected;

    /** Broadcast when the WebSocket connection to rosbridge is lost. */
    UPROPERTY(BlueprintAssignable, Category = "ROS|Bridge")
    FOnRosBridgeDisconnected OnDisconnected;

private:
    // --- WebSocket event handlers (all run on non-game thread!) ---
    void HandleConnected();
    void HandleConnectionError(const FString& Error);
    void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleMessage(const FString& Message);

    // --- Game-thread message processing ---
    void ProcessIncomingMessage(const FString& Message);

    // --- Internal helpers ---

    /** Send raw JSON string over the WebSocket. Returns false if not connected. */
    bool SendRaw(const FString& JsonPayload);

    /** Send all tracked subscriptions to rosbridge. Called on (re)connect. */
    void RestoreSubscriptions();

    /** Send all tracked advertisements to rosbridge. Called on (re)connect. */
    void RestoreAdvertisements();

    // --- Reconnection ---

    /** Schedule a reconnection attempt with exponential backoff. */
    void ScheduleReconnect();

    /** Called by the reconnect timer. */
    void AttemptReconnect();

    /** The URL passed to Connect(), saved for reconnection. */
    FString ConnectionUrl;

    /** Whether auto-reconnection is active. Set true on Connect(), false on explicit Disconnect(). */
    bool bAutoReconnect = false;

    /** Current backoff delay in seconds (doubles each failure, caps at MaxReconnectDelay). */
    float CurrentReconnectDelay = 1.0f;

    /** Base reconnect delay in seconds. */
    static constexpr float BaseReconnectDelay = 1.0f;

    /** Maximum reconnect delay in seconds. */
    static constexpr float MaxReconnectDelay = 30.0f;

    /** Timer handle for the reconnect timer. */
    FTimerHandle ReconnectTimerHandle;

    /** Number of consecutive reconnect attempts (for logging). */
    int32 ReconnectAttemptCount = 0;

    // --- State ---

    /** The underlying WebSocket. Kept as a TSharedPtr because IWebSocket
     *  is a plain C++ interface, not a UObject, so UPROPERTY does not apply. */
    TSharedPtr<IWebSocket> Socket;

    /** Topics we have subscribed to, kept so we can re-subscribe on reconnect. */
    UPROPERTY()
    TMap<FString, FString> SubscribedTopics;

    /** Topics we have advertised, kept so we can re-advertise on reconnect. */
    UPROPERTY()
    TMap<FString, FString> AdvertisedTopics;

    /** Reassembly buffers for rosbridge "fragment" messages, keyed by fragment id. */
    TMap<FString, TArray<FString>> FragmentBuffers;
};
