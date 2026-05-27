#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobotVisualizer.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class URosBridgeSubsystem;

/**
 * Visualizes the SO-ARM-101 follower arm in Unreal Engine and provides
 * MoveIt command interface via rosbridge.
 *
 * The component hierarchy mirrors the URDF link/joint structure:
 *   BaseLink -> ShoulderPanJoint -> ShoulderLink -> ShoulderLiftJoint -> ...
 *
 * Each "Joint" SceneComponent is where the ROS joint angle gets applied
 * as a local Z-axis rotation. All child links/meshes rotate with it.
 *
 * Phase 8 additions:
 *   - SendNamedTarget(): publish to /moveit_goal_named (std_msgs/String)
 *   - SendJointGoal(): publish to /moveit_goal_joints (sensor_msgs/JointState)
 *   - SendPoseGoal(): publish to /moveit_goal_pose (geometry_msgs/PoseStamped)
 *   - Blueprint-callable + editor-testable via UPROPERTY buttons
 *
 * Phase 9 additions (Record/Replay/E-Stop):
 *   - StartRecord(): begin teleop recording on worker
 *   - StopRecord(): stop recording, save trajectory
 *   - StartReplay(): replay most recent (or named) recording
 *   - StopReplay(): stop replay
 *   - EStop(): emergency stop all motion
 *   - All commands publish JSON to /robot_command topic
 *   - Worker state feedback via /robot_status subscription
 */
UCLASS()
class SO101_TWIN_API ARobotVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ARobotVisualizer();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// =================================================================
	// Configuration
	// =================================================================

	UPROPERTY(EditAnywhere, Category = "ROS|Bridge")
	FString RosBridgeUrl = TEXT("ws://127.0.0.1:9090/?x=1");

	UPROPERTY(EditAnywhere, Category = "ROS|Topics")
	FString JointStateTopic = TEXT("/joint_states");

	UPROPERTY(EditAnywhere, Category = "ROS|Topics")
	FString JointStateType = TEXT("sensor_msgs/JointState");

	// =================================================================
	// MoveIt Command Interface (Phase 8)
	// =================================================================

	// --- Named target ---

	/** Named target to send (e.g. "home", "ready"). Set in Details panel, then call SendNamedTarget(). */
	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt")
	FString MoveItNamedTarget = TEXT("home");

	/** Send the named target to MoveIt via /moveit_goal_named topic. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|MoveIt")
	void SendNamedTarget();

	// --- Joint goal ---

	/** Joint goal values in radians. Set in Details panel, then call SendJointGoal(). */
	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Joints")
	float GoalShoulderPan = 0.0f;

	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Joints")
	float GoalShoulderLift = 0.0f;

	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Joints")
	float GoalElbowFlex = 0.0f;

	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Joints")
	float GoalWristFlex = 0.0f;

	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Joints")
	float GoalWristRoll = 0.0f;

	/** Send joint goal to MoveIt via /moveit_goal_joints topic. Values are in radians. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|MoveIt|Joints")
	void SendJointGoal();

	// --- Pose goal (Cartesian, position-only for 5DOF) ---

	/** Target position in UE coordinates (cm). Converted to ROS (meters) on send. */
	UPROPERTY(EditAnywhere, Category = "ROS|MoveIt|Pose")
	FVector GoalPositionUE = FVector(10.0f, 0.0f, 15.0f);

	/** Send position-only goal to MoveIt via /moveit_goal_pose topic.
	 *  GoalPositionUE is in Unreal cm, auto-converted to ROS meters with Y flip. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|MoveIt|Pose")
	void SendPoseGoal();

	// =================================================================
	// Teleop Sync (Phase 9)
	// =================================================================

	/** Activate leader→follower sync (teleop). Must be ON before recording. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Sync")
	void SyncOn();

	/** Deactivate leader→follower sync. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Sync")
	void SyncOff();

	/** Whether teleop (sync) is currently active. Updated from /robot_status. */
	UPROPERTY(VisibleAnywhere, Category = "ROS|Status")
	bool bSyncActive = false;

	// =================================================================
	// Record / Replay / E-Stop (Phase 9)
	// =================================================================

	/** Start recording: activates teleop on worker, buffers joint trajectory. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Record")
	void StartRecord();

	/** Stop recording: saves trajectory to file on worker. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Record")
	void StopRecord();

	/** Replay filename (empty = most recent recording). */
	UPROPERTY(EditAnywhere, Category = "ROS|Replay")
	FString ReplayFilename;

	/** Whether to loop the replay continuously. */
	UPROPERTY(EditAnywhere, Category = "ROS|Replay")
	bool bReplayLoop = false;

	/** Approach speed in degrees/sec. Controls how fast the robot moves
	 *  to the start position before replay begins. Lower = smoother. */
	UPROPERTY(EditAnywhere, Category = "ROS|Replay", meta = (ClampMin = "5.0", ClampMax = "300.0"))
	float ApproachSpeed = 45.0f;

	/** Start replaying a recorded trajectory on the follower arm. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Replay")
	void StartReplay();

	/** Stop replay immediately. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Replay")
	void StopReplay();

	/** Emergency stop: abort ALL motion immediately (recording, replay, teleop). */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "ROS|Safety")
	void EStop();

	/** Current worker state (idle/recording/replaying). Updated from /robot_status. */
	UPROPERTY(VisibleAnywhere, Category = "ROS|Status")
	FString WorkerState = TEXT("unknown");

private:
	// =================================================================
	// Component hierarchy
	// =================================================================

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	TObjectPtr<USceneComponent> RobotRoot;

	// Link SceneComponents
	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> BaseLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> ShoulderLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> UpperArmLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> LowerArmLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> WristLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> GripperLink;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Links")
	TObjectPtr<USceneComponent> MovingJawLink;

	// Joint SceneComponents
	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> ShoulderPanJoint;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> ShoulderLiftJoint;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> ElbowFlexJoint;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> WristFlexJoint;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> WristRollJoint;

	UPROPERTY(VisibleAnywhere, Category = "Robot|Joints")
	TObjectPtr<USceneComponent> GripperJoint;

	// Joint name -> component mapping
	UPROPERTY()
	TMap<FName, TObjectPtr<USceneComponent>> JointComponentMap;

	// Mesh components
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AllMeshComponents;

	// =================================================================
	// ROS connection
	// =================================================================

	UFUNCTION()
	void OnRosBridgeConnected();

	UFUNCTION()
	void OnRosBridgeDisconnected();

	UFUNCTION()
	void OnRosMessage(const FString& Topic, const FString& MessageJson);

	/** Tracks rosbridge connection state for viewport warnings. */
	bool bRosBridgeConnected = false;

	void ParseAndApplyJointStates(const FString& MessageJson);

	// =================================================================
	// MoveIt publish helpers
	// =================================================================

	/** Advertise MoveIt command topics. Called once on connect. */
	void AdvertiseMoveItTopics();

	/** Whether MoveIt topics have been advertised in this connection session. */
	bool bMoveItTopicsAdvertised = false;

	// =================================================================
	// Record / Replay / E-Stop helpers
	// =================================================================

	/** Advertise /robot_command and subscribe /robot_status. Called once on connect. */
	void SetupRecordReplayTopics();

	/** Whether record/replay topics have been set up. */
	bool bRecordReplayTopicsSetup = false;

	/** Send a JSON command to /robot_command topic. */
	void PublishRobotCommand(const FString& JsonCmd);

	/** Handle /robot_status messages from the bridge node. */
	UFUNCTION()
	void OnRobotStatus(const FString& Topic, const FString& MessageJson);

	// =================================================================
	// Connection health monitoring (Unreal-side)
	// =================================================================

	/** Called periodically to check bridge and worker heartbeats. */
	void CheckConnectionHealth();

	/** Timer handle for health check. */
	FTimerHandle ConnectionHealthTimerHandle;

	/** Last time we received /bridge_heartbeat. */
	double LastBridgeHeartbeatTime = 0.0;

	/** Last time we received /joint_states (worker data via bridge). */
	double LastJointStatesTime = 0.0;

	/** Timeout in seconds for bridge heartbeat (bridge publishes every 1s). */
	float BridgeHeartbeatTimeoutSec = 4.0f;

	/** Timeout in seconds for worker data (/joint_states at 30Hz). */
	float WorkerDataTimeoutSec = 3.0f;

	/** Whether bridge heartbeat has been lost. */
	bool bBridgeHeartbeatLost = false;

	/** Whether worker data has been lost. */
	bool bWorkerDataLost = false;

	/** Tracks device-level USB/serial error state for recovery messages. */
	bool bFollowerDeviceError = false;
	bool bLeaderDeviceError = false;

	// =================================================================
	// Helpers (declared in original header, kept for compatibility)
	// =================================================================

	USceneComponent* CreateJointComponent(const FName& Name, USceneComponent* Parent,
		const FVector& Location, const FRotator& Rotation);

	USceneComponent* CreateLinkComponent(const FName& Name, USceneComponent* Parent);

	UStaticMeshComponent* AttachMesh(USceneComponent* Parent, UStaticMesh* Mesh,
		const FName& Name, const FVector& Location, const FRotator& Rotation,
		bool bIsMotor);
};
