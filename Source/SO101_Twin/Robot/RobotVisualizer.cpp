#include "RobotVisualizer.h"
#include "RosCoordConv.h"
#include "RosBridgeSubsystem.h"
#include "RosBridgeLog.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "RobotControlWidget.h"

// =============================================================================
// Mesh asset path helper
// =============================================================================

static UStaticMesh* LoadMeshAsset(const TCHAR* AssetName)
{
	// All meshes live under /Game/Robot/Meshes/
	FString Path = FString::Printf(TEXT("/Game/Robot/Meshes/%s.%s"), AssetName, AssetName);
	UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
	if (!Mesh)
	{
		UE_LOG(LogRosBridge, Warning, TEXT("Failed to load mesh: %s"), *Path);
	}
	return Mesh;
}

// =============================================================================
// Constructor — build the entire component hierarchy
// =============================================================================

ARobotVisualizer::ARobotVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	// --- Root ---
	RobotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RobotRoot"));
	RootComponent = RobotRoot;

	// =========================================================================
	// URDF data converted to UE coordinates:
	//   Position: meters * 100 = cm, Y flipped
	//   Rotation: RPY radians -> FRotator degrees, pitch & yaw negated
	//
	// All values below are pre-computed from so101_follower.urdf.
	// =========================================================================

	// --- base_link (attached directly to root, no joint) ---
	BaseLink = CreateDefaultSubobject<USceneComponent>(TEXT("BaseLink"));
	BaseLink->SetupAttachment(RobotRoot);

	// --- shoulder_pan joint ---
	// URDF origin: xyz(0.0388353, 0, 0.0624) rpy(3.14159, 0, -3.14159)
	ShoulderPanJoint = CreateDefaultSubobject<USceneComponent>(TEXT("ShoulderPanJoint"));
	ShoulderPanJoint->SetupAttachment(BaseLink);
	ShoulderPanJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(0.0388353, 0.0, 0.0624));
	ShoulderPanJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(3.14159, 0.0, -3.14159));

	ShoulderLink = CreateDefaultSubobject<USceneComponent>(TEXT("ShoulderLink"));
	ShoulderLink->SetupAttachment(ShoulderPanJoint);

	// --- shoulder_lift joint ---
	// URDF origin: xyz(-0.0303992, -0.0182778, -0.0542) rpy(-1.5708, -1.5708, 0)
	ShoulderLiftJoint = CreateDefaultSubobject<USceneComponent>(TEXT("ShoulderLiftJoint"));
	ShoulderLiftJoint->SetupAttachment(ShoulderLink);
	ShoulderLiftJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(-0.0303992, -0.0182778, -0.0542));
	ShoulderLiftJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(-1.5708, -1.5708, 0.0));

	UpperArmLink = CreateDefaultSubobject<USceneComponent>(TEXT("UpperArmLink"));
	UpperArmLink->SetupAttachment(ShoulderLiftJoint);

	// --- elbow_flex joint ---
	// URDF origin: xyz(-0.11257, -0.028, 0) rpy(0, 0, 1.5708)
	ElbowFlexJoint = CreateDefaultSubobject<USceneComponent>(TEXT("ElbowFlexJoint"));
	ElbowFlexJoint->SetupAttachment(UpperArmLink);
	ElbowFlexJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(-0.11257, -0.028, 0.0));
	ElbowFlexJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(0.0, 0.0, 1.5708));

	LowerArmLink = CreateDefaultSubobject<USceneComponent>(TEXT("LowerArmLink"));
	LowerArmLink->SetupAttachment(ElbowFlexJoint);

	// --- wrist_flex joint ---
	// URDF origin: xyz(-0.1349, 0.0052, 0) rpy(0, 0, -1.5708)
	WristFlexJoint = CreateDefaultSubobject<USceneComponent>(TEXT("WristFlexJoint"));
	WristFlexJoint->SetupAttachment(LowerArmLink);
	WristFlexJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(-0.1349, 0.0052, 0.0));
	WristFlexJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(0.0, 0.0, -1.5708));

	WristLink = CreateDefaultSubobject<USceneComponent>(TEXT("WristLink"));
	WristLink->SetupAttachment(WristFlexJoint);

	// --- wrist_roll joint ---
	// URDF origin: xyz(0, -0.0611, 0.0181) rpy(1.5708, 0.0486795, 3.14159)
	WristRollJoint = CreateDefaultSubobject<USceneComponent>(TEXT("WristRollJoint"));
	WristRollJoint->SetupAttachment(WristLink);
	WristRollJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(0.0, -0.0611, 0.0181));
	WristRollJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(1.5708, 0.0486795, 3.14159));

	GripperLink = CreateDefaultSubobject<USceneComponent>(TEXT("GripperLink"));
	GripperLink->SetupAttachment(WristRollJoint);

	// --- gripper joint ---
	// URDF origin: xyz(0.0202, 0.0188, -0.0234) rpy(1.5708, 0, 0)
	GripperJoint = CreateDefaultSubobject<USceneComponent>(TEXT("GripperJoint"));
	GripperJoint->SetupAttachment(GripperLink);
	GripperJoint->SetRelativeLocation(RosCoordConv::RosToUePosition(0.0202, 0.0188, -0.0234));
	GripperJoint->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(1.5708, 0.0, 0.0));

	MovingJawLink = CreateDefaultSubobject<USceneComponent>(TEXT("MovingJawLink"));
	MovingJawLink->SetupAttachment(GripperJoint);

	// --- Joint name mapping (matches ROS /joint_states names) ---
	JointComponentMap.Add(FName("shoulder_pan"),  ShoulderPanJoint);
	JointComponentMap.Add(FName("shoulder_lift"), ShoulderLiftJoint);
	JointComponentMap.Add(FName("elbow_flex"),    ElbowFlexJoint);
	JointComponentMap.Add(FName("wrist_flex"),    WristFlexJoint);
	JointComponentMap.Add(FName("wrist_roll"),    WristRollJoint);
	JointComponentMap.Add(FName("gripper"),       GripperJoint);
}

// =============================================================================
// BeginPlay — load meshes and attach, connect to ROS
// =============================================================================

void ARobotVisualizer::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = bShowOnScreenDebug;
	}

	// --- Load meshes and attach to links ---
	// Meshes are loaded at runtime (not in constructor) because
	// StaticLoadObject is safer to call here and allows hot-reload.

	// Helper lambda to reduce repetition
	auto Attach = [this](USceneComponent* Parent, const TCHAR* MeshName,
		double RosX, double RosY, double RosZ,
		double RosRoll, double RosPitch, double RosYaw,
		bool bIsMotor = false)
	{
		UStaticMesh* Mesh = LoadMeshAsset(MeshName);
		if (!Mesh) return;

		UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(this);
		SMC->SetStaticMesh(Mesh);
		SMC->SetRelativeLocation(RosCoordConv::RosToUePosition(RosX, RosY, RosZ));
		SMC->SetRelativeRotation(RosCoordConv::RosRpyToUeRotator(RosRoll, RosPitch, RosYaw));
		SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SMC->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		SMC->RegisterComponent();
		AllMeshComponents.Add(SMC);
	};

	// === base_link meshes ===
	Attach(BaseLink, TEXT("base_motor_holder_so101_v1"),
		-0.00636471, -0.0000994414, -0.0024,
		1.5708, 0.0, 1.5708);
	Attach(BaseLink, TEXT("base_so101_v2"),
		-0.00636471, 0.0, -0.0024,
		1.5708, 0.0, 1.5708);
	Attach(BaseLink, TEXT("sts3215_03a_v1"),
		0.0263353, 0.0, 0.0437,
		0.0, 0.0, 0.0, true);
	Attach(BaseLink, TEXT("waveshare_mounting_plate_so101_v2"),
		-0.0309827, -0.000199441, 0.0474,
		1.5708, 0.0, 1.5708);

	// === shoulder_link meshes ===
	Attach(ShoulderLink, TEXT("sts3215_03a_v1"),
		-0.0303992, 0.000422241, -0.0417,
		1.5708, 1.5708, 0.0, true);
	Attach(ShoulderLink, TEXT("motor_holder_so101_base_v1"),
		-0.0675992, -0.000177759, 0.0158499,
		1.5708, -1.5708, 0.0);
	Attach(ShoulderLink, TEXT("rotation_pitch_so101_v1"),
		0.0122008, 0.0000222413, 0.0464,
		-1.5708, 0.0, 0.0);

	// === upper_arm_link meshes ===
	Attach(UpperArmLink, TEXT("sts3215_03a_v1"),
		-0.11257, -0.0155, 0.0187,
		-3.14159, 0.0, -1.5708, true);
	Attach(UpperArmLink, TEXT("upper_arm_so101_v1"),
		-0.065085, 0.012, 0.0182,
		3.14159, 0.0, 0.0);

	// === lower_arm_link meshes ===
	Attach(LowerArmLink, TEXT("under_arm_so101_v1"),
		-0.0648499, -0.032, 0.0182,
		3.14159, 0.0, 0.0);
	Attach(LowerArmLink, TEXT("motor_holder_so101_wrist_v1"),
		-0.0648499, -0.032, 0.018,
		-3.14159, 0.0, 0.0);
	Attach(LowerArmLink, TEXT("sts3215_03a_v1"),
		-0.1224, 0.0052, 0.0187,
		-3.14159, 0.0, -3.14159, true);

	// === wrist_link meshes ===
	Attach(WristLink, TEXT("sts3215_03a_no_horn_v1"),
		0.0, -0.0424, 0.0306,
		1.5708, 1.5708, 0.0, true);
	Attach(WristLink, TEXT("wrist_roll_pitch_so101_v2"),
		0.0, -0.028, 0.0181,
		-1.5708, -1.5708, 0.0);

	// === gripper_link meshes ===
	Attach(GripperLink, TEXT("sts3215_03a_v1"),
		0.0077, 0.0001, -0.0234,
		-1.5708, 0.0, 0.0, true);
	Attach(GripperLink, TEXT("wrist_roll_follower_so101_v1"),
		0.0, -0.000218214, 0.000949706,
		-3.14159, 0.0, 0.0);

	// === moving_jaw_link meshes ===
	Attach(MovingJawLink, TEXT("moving_jaw_so101_v1"),
		0.0, 0.0, 0.0189,
		0.0, 0.0, 0.0);

	UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: %d mesh components created"), AllMeshComponents.Num());

	// --- Connect to ROS via Subsystem ---
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros) return;

	// Bind delegates.
	Ros->OnTopicMessage.AddDynamic(this, &ARobotVisualizer::OnRosMessage);
	Ros->OnConnected.AddDynamic(this, &ARobotVisualizer::OnRosBridgeConnected);
	Ros->OnDisconnected.AddDynamic(this, &ARobotVisualizer::OnRosBridgeDisconnected);

	// Subscribe is now queued even before connection — the subsystem will
	// send it automatically when connected (including on reconnect).
	Ros->Subscribe(JointStateTopic, JointStateType);

	// Queue MoveIt topic advertisements (sent on connect).
	AdvertiseMoveItTopics();

	// Queue record/replay/estop topics.
	SetupRecordReplayTopics();

	// Subscribe to bridge heartbeat for connection health monitoring.
	Ros->Subscribe(TEXT("/bridge_heartbeat"), TEXT("std_msgs/String"));

	// Start connection health monitor (checks every 2 seconds).
	const double Now = FPlatformTime::Seconds();
	LastBridgeHeartbeatTime = Now;
	LastJointStatesTime = Now;
	GetWorldTimerManager().SetTimer(
		ConnectionHealthTimerHandle, this,
		&ARobotVisualizer::CheckConnectionHealth, 2.0f, true);

	// Initiate connection if not already connected.
	if (!Ros->IsConnected())
	{
		Ros->Connect(RosBridgeUrl);
	}
	else
	{
		// Already connected (e.g. another actor already called Connect).
		UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: already connected, subscription sent."));
	}

	// --- Spawn the in-viewport control UI (Phase 10) ---
	if (ControlWidgetClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			ControlWidget = CreateWidget<URobotControlWidget>(PC, ControlWidgetClass);
			if (ControlWidget)
			{
				ControlWidget->AddToViewport();
				UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: control widget added to viewport."));
			}
		}
	}
}

// =============================================================================
// EndPlay
// =============================================================================

void ARobotVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>())
		{
			Ros->OnTopicMessage.RemoveDynamic(this, &ARobotVisualizer::OnRosMessage);
			Ros->OnConnected.RemoveDynamic(this, &ARobotVisualizer::OnRosBridgeConnected);
			Ros->OnDisconnected.RemoveDynamic(this, &ARobotVisualizer::OnRosBridgeDisconnected);
		}
	}

	bMoveItTopicsAdvertised = false;
	bRecordReplayTopicsSetup = false;

	GetWorldTimerManager().ClearTimer(ConnectionHealthTimerHandle);

	if (ControlWidget)
	{
		ControlWidget->RemoveFromParent();
		ControlWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// ROS connection callback
// =============================================================================

void ARobotVisualizer::OnRosBridgeConnected()
{
	bRosBridgeConnected = true;
	UE_LOG(LogRosBridge, Log,
		TEXT("RobotVisualizer: rosbridge connected — subscriptions restored by subsystem."));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
			TEXT("ROS Bridge: Connected"));
	}
}

// =============================================================================
// ROS disconnection callback
// =============================================================================

void ARobotVisualizer::OnRosBridgeDisconnected()
{
	bRosBridgeConnected = false;
	WorkerState = TEXT("disconnected");

	UE_LOG(LogRosBridge, Warning,
		TEXT("RobotVisualizer: rosbridge DISCONNECTED — cannot send commands. Auto-reconnect active."));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
			TEXT("*** ROS Bridge DISCONNECTED *** Cannot send commands. Reconnecting..."));
	}
}

// =============================================================================
// Connection health monitoring
// =============================================================================

void ARobotVisualizer::CheckConnectionHealth()
{
	// Only check when WebSocket is connected — if WebSocket is down,
	// OnRosBridgeDisconnected already shows a warning for that.
	if (!bRosBridgeConnected)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	// Check bridge heartbeat (published every 1s by bridge_node)
	const double BridgeElapsed = Now - LastBridgeHeartbeatTime;
	if (BridgeElapsed > BridgeHeartbeatTimeoutSec && !bBridgeHeartbeatLost)
	{
		bBridgeHeartbeatLost = true;
		bWorkerDataLost = true;  // if bridge is down, worker data can't reach us either
		WorkerState = TEXT("bridge lost");

		UE_LOG(LogRosBridge, Warning,
			TEXT("Bridge heartbeat lost (%.1fs). bridge_node may be down."), BridgeElapsed);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
				TEXT("*** Bridge Node: DOWN *** Check bridge_node terminal (terminal 2)."));
		}
		return;  // no need to check worker separately
	}

	// Check worker data (/joint_states at ~30Hz, flows through bridge)
	// Only meaningful if bridge is alive.
	if (!bBridgeHeartbeatLost)
	{
		const double WorkerElapsed = Now - LastJointStatesTime;
		if (WorkerElapsed > WorkerDataTimeoutSec && !bWorkerDataLost)
		{
			bWorkerDataLost = true;
			WorkerState = TEXT("worker lost");

			UE_LOG(LogRosBridge, Warning,
				TEXT("Worker data lost (%.1fs). lerobot_worker may be down."), WorkerElapsed);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red,
					TEXT("*** Worker: DOWN *** Check lerobot_worker terminal (terminal 1)."));
			}
		}
	}
}

// =============================================================================
// MoveIt topic advertisements
// =============================================================================

void ARobotVisualizer::AdvertiseMoveItTopics()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros) return;

	// These are queued and sent automatically when connected.
	Ros->Advertise(TEXT("/moveit_goal_named"), TEXT("std_msgs/String"));
	Ros->Advertise(TEXT("/moveit_goal_joints"), TEXT("sensor_msgs/JointState"));
	Ros->Advertise(TEXT("/moveit_goal_pose"), TEXT("geometry_msgs/PoseStamped"));

	bMoveItTopicsAdvertised = true;
	UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: MoveIt command topics advertised."));
}

// =============================================================================
// MoveIt commands — SendNamedTarget
// =============================================================================

void ARobotVisualizer::SendNamedTarget()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros || !Ros->IsConnected())
	{
		UE_LOG(LogRosBridge, Warning, TEXT("SendNamedTarget: not connected to rosbridge."));
		return;
	}

	// std_msgs/String: {"data": "home"}
	FString MsgJson = FString::Printf(TEXT("{\"data\":\"%s\"}"), *MoveItNamedTarget);
	Ros->Publish(TEXT("/moveit_goal_named"), MsgJson);

	UE_LOG(LogRosBridge, Log, TEXT("SendNamedTarget: published '%s' to /moveit_goal_named"), *MoveItNamedTarget);
}

// =============================================================================
// MoveIt commands — SendJointGoal
// =============================================================================

void ARobotVisualizer::SendJointGoal()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros || !Ros->IsConnected())
	{
		UE_LOG(LogRosBridge, Warning, TEXT("SendJointGoal: not connected to rosbridge."));
		return;
	}

	// sensor_msgs/JointState:
	// {
	//   "header": {"stamp": {"sec": 0, "nanosec": 0}, "frame_id": ""},
	//   "name": ["shoulder_pan", "shoulder_lift", "elbow_flex", "wrist_flex", "wrist_roll"],
	//   "position": [0.0, -0.5, 0.5, 0.0, 0.0],
	//   "velocity": [],
	//   "effort": []
	// }

	FString MsgJson = FString::Printf(
		TEXT("{\"header\":{\"stamp\":{\"sec\":0,\"nanosec\":0},\"frame_id\":\"\"},")
		TEXT("\"name\":[\"shoulder_pan\",\"shoulder_lift\",\"elbow_flex\",\"wrist_flex\",\"wrist_roll\"],")
		TEXT("\"position\":[%f,%f,%f,%f,%f],")
		TEXT("\"velocity\":[],\"effort\":[]}"),
		GoalShoulderPan, GoalShoulderLift, GoalElbowFlex, GoalWristFlex, GoalWristRoll
	);

	Ros->Publish(TEXT("/moveit_goal_joints"), MsgJson);

	UE_LOG(LogRosBridge, Log,
		TEXT("SendJointGoal: [%.3f, %.3f, %.3f, %.3f, %.3f] to /moveit_goal_joints"),
		GoalShoulderPan, GoalShoulderLift, GoalElbowFlex, GoalWristFlex, GoalWristRoll);
}

// =============================================================================
// MoveIt commands — SendPoseGoal
// =============================================================================

void ARobotVisualizer::SendPoseGoal()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros || !Ros->IsConnected())
	{
		UE_LOG(LogRosBridge, Warning, TEXT("SendPoseGoal: not connected to rosbridge."));
		return;
	}

	// Convert UE position (cm, left-handed) to ROS (meters, right-handed)
	double RosX, RosY, RosZ;
	RosCoordConv::UeToRosPosition(GoalPositionUE, RosX, RosY, RosZ);

	// geometry_msgs/PoseStamped (orientation defaults to identity — position-only goal)
	FString MsgJson = FString::Printf(
		TEXT("{\"header\":{\"stamp\":{\"sec\":0,\"nanosec\":0},\"frame_id\":\"base_link\"},")
		TEXT("\"pose\":{\"position\":{\"x\":%f,\"y\":%f,\"z\":%f},")
		TEXT("\"orientation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0,\"w\":1.0}}}"),
		RosX, RosY, RosZ
	);

	Ros->Publish(TEXT("/moveit_goal_pose"), MsgJson);

	UE_LOG(LogRosBridge, Log,
		TEXT("SendPoseGoal: UE(%.1f, %.1f, %.1f)cm -> ROS(%.4f, %.4f, %.4f)m to /moveit_goal_pose"),
		GoalPositionUE.X, GoalPositionUE.Y, GoalPositionUE.Z,
		RosX, RosY, RosZ);
}

// =============================================================================
// Message handling
// =============================================================================

void ARobotVisualizer::OnRosMessage(const FString& Topic, const FString& MessageJson)
{
	if (Topic == JointStateTopic)
	{
		// /joint_states arrives at ~30Hz — proves both bridge AND worker are alive.
		LastJointStatesTime = FPlatformTime::Seconds();
		LastBridgeHeartbeatTime = LastJointStatesTime; // joint_states flows through bridge
		if (bWorkerDataLost)
		{
			bWorkerDataLost = false;
			UE_LOG(LogRosBridge, Log, TEXT("Worker data restored."));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
					TEXT("Worker: Connection restored"));
			}
		}
		if (bBridgeHeartbeatLost)
		{
			bBridgeHeartbeatLost = false;
			UE_LOG(LogRosBridge, Log, TEXT("Bridge heartbeat restored (via joint_states)."));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
					TEXT("Bridge: Connection restored"));
			}
		}

		ParseAndApplyJointStates(MessageJson);
	}
	else if (Topic == TEXT("/bridge_heartbeat"))
	{
		// Bridge heartbeat arrives every 1s — proves bridge is alive
		// (but worker may still be dead if /joint_states is not arriving).
		LastBridgeHeartbeatTime = FPlatformTime::Seconds();
		if (bBridgeHeartbeatLost)
		{
			bBridgeHeartbeatLost = false;
			UE_LOG(LogRosBridge, Log, TEXT("Bridge heartbeat restored."));
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
					TEXT("Bridge: Connection restored"));
			}
		}
	}
	else if (Topic == TEXT("/robot_status"))
	{
		OnRobotStatus(Topic, MessageJson);
	}
}

void ARobotVisualizer::ParseAndApplyJointStates(const FString& MessageJson)
{
	// Parse sensor_msgs/JointState JSON:
	// { "name": ["shoulder_pan", ...], "position": [0.1, ...], ... }

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MessageJson);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* NameArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* PosArray = nullptr;

	if (!Json->TryGetArrayField(TEXT("name"), NameArray) ||
		!Json->TryGetArrayField(TEXT("position"), PosArray))
	{
		return;
	}

	const int32 Count = FMath::Min(NameArray->Num(), PosArray->Num());
	for (int32 i = 0; i < Count; ++i)
	{
		const FName JointName(*(*NameArray)[i]->AsString());
		const double AngleRad = (*PosArray)[i]->AsNumber();

		// Cache the RAW LeRobot degrees (not the UE-converted value below).
		// The bridge publishes deg2rad(LeRobot degrees) with no URDF remap,
		// so this is directly comparable to the worker's JOINT_LIMITS_DEG.
		CurrentJointDeg.Add(JointName, static_cast<float>(FMath::RadiansToDegrees(AngleRad)));

		TObjectPtr<USceneComponent>* JointComp = JointComponentMap.Find(JointName);
		if (JointComp && *JointComp)
		{
			const float AngleDeg = RosCoordConv::RosJointAngleToUeDegrees(AngleRad);

			FRotator BaseRot;
			if (JointName == FName("shoulder_pan"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(3.14159, 0.0, -3.14159);
			else if (JointName == FName("shoulder_lift"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(-1.5708, -1.5708, 0.0);
			else if (JointName == FName("elbow_flex"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(0.0, 0.0, 1.5708);
			else if (JointName == FName("wrist_flex"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(0.0, 0.0, -1.5708);
			else if (JointName == FName("wrist_roll"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(1.5708, 0.0486795, 3.14159);
			else if (JointName == FName("gripper"))
				BaseRot = RosCoordConv::RosRpyToUeRotator(1.5708, 0.0, 0.0);

			const FQuat BaseQuat = BaseRot.Quaternion();
			const FQuat JointQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(AngleDeg));
			const FQuat FinalQuat = BaseQuat * JointQuat;

			(*JointComp)->SetRelativeRotation(FinalQuat.Rotator());
		}
	}
	RecordJointHistory();
}

// =============================================================================
// Record / Replay / E-Stop — Topic setup
// =============================================================================

void ARobotVisualizer::SetupRecordReplayTopics()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros) return;

	// Advertise the command topic
	Ros->Advertise(TEXT("/robot_command"), TEXT("std_msgs/String"));

	// Subscribe to status feedback
	Ros->Subscribe(TEXT("/robot_status"), TEXT("std_msgs/String"));

	bRecordReplayTopicsSetup = true;
	UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: Record/Replay topics set up."));
}

// =============================================================================
// Record / Replay / E-Stop — Command publisher helper
// =============================================================================

void ARobotVisualizer::PublishRobotCommand(const FString& JsonCmd)
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros || !Ros->IsConnected())
	{
		UE_LOG(LogRosBridge, Warning, TEXT("PublishRobotCommand: not connected to rosbridge."));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
				TEXT("Robot: Not connected to rosbridge!"));
		}
		return;
	}

	// std_msgs/String: {"data": "<json_cmd>"}
	// The JSON command is nested inside the "data" field, with quotes escaped.
	FString EscapedCmd = JsonCmd.Replace(TEXT("\""), TEXT("\\\""));
	FString MsgJson = FString::Printf(TEXT("{\"data\":\"%s\"}"), *EscapedCmd);
	Ros->Publish(TEXT("/robot_command"), MsgJson);

	UE_LOG(LogRosBridge, Log, TEXT("PublishRobotCommand: %s"), *JsonCmd);
}

// =============================================================================
// Record / Replay / E-Stop — Button handlers
// =============================================================================

void ARobotVisualizer::StartRecord()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"start_record\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			TEXT("Robot: Recording started (teleop active)"));
	}
}

void ARobotVisualizer::StopRecord()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"stop_record\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			TEXT("Robot: Recording stopped, saving..."));
	}
}

void ARobotVisualizer::StartReplay()
{
	FString ArgsJson;
	if (ReplayFilename.IsEmpty())
	{
		ArgsJson = FString::Printf(
			TEXT("{\"cmd\":\"start_replay\",\"args\":{\"loop\":%s,\"approach_speed\":%f}}"),
			bReplayLoop ? TEXT("true") : TEXT("false"),
			ApproachSpeed);
	}
	else
	{
		ArgsJson = FString::Printf(
			TEXT("{\"cmd\":\"start_replay\",\"args\":{\"filename\":\"%s\",\"loop\":%s,\"approach_speed\":%f}}"),
			*ReplayFilename,
			bReplayLoop ? TEXT("true") : TEXT("false"),
			ApproachSpeed);
	}
	PublishRobotCommand(ArgsJson);

	if (GEngine)
	{
		FString DisplayName = ReplayFilename.IsEmpty() ? TEXT("(most recent)") : ReplayFilename;
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
			FString::Printf(TEXT("Robot: Replaying %s (loop=%s, speed=%.0f°/s)"),
				*DisplayName, bReplayLoop ? TEXT("yes") : TEXT("no"), ApproachSpeed));
	}
}

void ARobotVisualizer::StopReplay()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"stop_replay\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			TEXT("Robot: Replay stopped"));
	}
}

void ARobotVisualizer::EStop()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"estop\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
			TEXT("*** E-STOP *** All motion halted"));
	}
}

// =============================================================================
// Teleop Sync — SyncOn / SyncOff
// =============================================================================

void ARobotVisualizer::SyncOn()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"start_teleop\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			TEXT("Sync ON: leader -> follower active"));
	}
}

void ARobotVisualizer::SyncOff()
{
	PublishRobotCommand(TEXT("{\"cmd\":\"stop_teleop\"}"));

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow,
			TEXT("Sync OFF: leader -> follower deactivated"));
	}
}

// =============================================================================
// Record / Replay — Status feedback handler
// =============================================================================

void ARobotVisualizer::OnRobotStatus(const FString& Topic, const FString& MessageJson)
{
	// /robot_status flows through bridge from worker — both are alive.
	const double Now = FPlatformTime::Seconds();
	LastBridgeHeartbeatTime = Now;
	LastJointStatesTime = Now;

	if (bBridgeHeartbeatLost)
	{
		bBridgeHeartbeatLost = false;
	}
	if (bWorkerDataLost)
	{
		bWorkerDataLost = false;
	}

	// rosbridge wraps std_msgs/String as: {"data": "..."}
	TSharedPtr<FJsonObject> OuterJson;
	TSharedRef<TJsonReader<>> OuterReader = TJsonReaderFactory<>::Create(MessageJson);
	if (!FJsonSerializer::Deserialize(OuterReader, OuterJson) || !OuterJson.IsValid())
	{
		return;
	}

	FString DataStr;
	if (!OuterJson->TryGetStringField(TEXT("data"), DataStr))
	{
		return;
	}

	// Parse the inner JSON status
	TSharedPtr<FJsonObject> StatusJson;
	TSharedRef<TJsonReader<>> StatusReader = TJsonReaderFactory<>::Create(DataStr);
	if (!FJsonSerializer::Deserialize(StatusReader, StatusJson) || !StatusJson.IsValid())
	{
		return;
	}

	FString State;
	if (StatusJson->TryGetStringField(TEXT("state"), State))
	{
		if (State != WorkerState)
		{
			WorkerState = State;
			UE_LOG(LogRosBridge, Log, TEXT("Worker state: %s"), *WorkerState);
		}
	}

	// Update sync (teleop) status
	bool bTeleop = false;
	if (StatusJson->TryGetBoolField(TEXT("teleop"), bTeleop))
	{
		if (bTeleop != bSyncActive)
		{
			bSyncActive = bTeleop;
			UE_LOG(LogRosBridge, Log, TEXT("Sync (teleop): %s"),
				bSyncActive ? TEXT("ON") : TEXT("OFF"));
		}
	}

	// Log errors from commands
	FString Status;
	if (StatusJson->TryGetStringField(TEXT("status"), Status) && Status == TEXT("error"))
	{
		FString Reason;
		StatusJson->TryGetStringField(TEXT("reason"), Reason);
		UE_LOG(LogRosBridge, Warning, TEXT("Robot command error: %s"), *Reason);
	}

	// Recording saved: worker sends `filename` on BOTH stop_record (state=idle)
	// AND start_replay (state=replaying). Only the former is a real save, so
	// gate on state=idle + a real frame count to avoid a false "saved" toast
	// when the user starts a replay.
	FString Filename;
	if (StatusJson->TryGetStringField(TEXT("filename"), Filename) && !Filename.IsEmpty())
	{
		int32 Frames = 0;
		StatusJson->TryGetNumberField(TEXT("frames"), Frames);
		double Duration = 0.0;
		StatusJson->TryGetNumberField(TEXT("duration_sec"), Duration);

		if (State == TEXT("idle") && Frames > 0)
		{
			UE_LOG(LogRosBridge, Log, TEXT("Recording saved: %s (%d frames, %.1fs)"),
				*Filename, Frames, Duration);
			LastSavedRecording = Filename;
		}
	}

	// --- Recordings list (reply to list_recordings) ---
	const TArray<TSharedPtr<FJsonValue>>* RecArray = nullptr;
	if (StatusJson->TryGetArrayField(TEXT("recordings"), RecArray) && RecArray)
	{
		Recordings.Reset();
		for (const TSharedPtr<FJsonValue>& V : *RecArray)
		{
			const TSharedPtr<FJsonObject> RecObj = V->AsObject();
			if (RecObj.IsValid())
			{
				FRecordingInfo Info;
				RecObj->TryGetStringField(TEXT("filename"), Info.Filename);
				RecObj->TryGetNumberField(TEXT("frames"), Info.Frames);
				double Dur = 0.0;
				RecObj->TryGetNumberField(TEXT("duration_sec"), Dur);
				Info.DurationSec = static_cast<float>(Dur);
				RecObj->TryGetStringField(TEXT("recorded_at"), Info.RecordedAt);
				Recordings.Add(Info);
			}
		}
		++RecordingsVersion;
		UE_LOG(LogRosBridge, Log, TEXT("Recordings list updated: %d files"), Recordings.Num());
	}

	// --- Recordings list, per-item (split to avoid large-frame drops) ---
	const TSharedPtr<FJsonObject>* RecItemObj = nullptr;
	if (StatusJson->TryGetObjectField(TEXT("recording_item"), RecItemObj) && RecItemObj && RecItemObj->IsValid())
	{
		int32 RecIndex = 0, RecTotal = 0;
		StatusJson->TryGetNumberField(TEXT("index"), RecIndex);
		StatusJson->TryGetNumberField(TEXT("total"), RecTotal);

		if (RecIndex == 0) { PendingRecordings.Reset(); }

		FRecordingInfo Info;
		(*RecItemObj)->TryGetStringField(TEXT("filename"), Info.Filename);
		(*RecItemObj)->TryGetNumberField(TEXT("frames"), Info.Frames);
		double Dur = 0.0;
		(*RecItemObj)->TryGetNumberField(TEXT("duration_sec"), Dur);
		Info.DurationSec = static_cast<float>(Dur);
		(*RecItemObj)->TryGetStringField(TEXT("recorded_at"), Info.RecordedAt);
		PendingRecordings.Add(Info);

		if (RecTotal > 0 && PendingRecordings.Num() >= RecTotal)
		{
			Recordings = PendingRecordings;
			Recordings.Sort([](const FRecordingInfo& A, const FRecordingInfo& B)
				{
					return A.Filename > B.Filename;   // newest first (top)
				});
			PendingRecordings.Reset();
			++RecordingsVersion;
			UE_LOG(LogRosBridge, Log, TEXT("Recordings list updated: %d files"), Recordings.Num());
		}
	}

	// --- Recordings list cleared (empty result) ---
	bool bRecClear = false;
	if (StatusJson->TryGetBoolField(TEXT("recordings_clear"), bRecClear) && bRecClear)
	{
		Recordings.Reset();
		PendingRecordings.Reset();
		++RecordingsVersion;
		UE_LOG(LogRosBridge, Log, TEXT("Recordings list cleared"));
	}

	// --- Joint limits (reply to get_joint_limits) ---
	// Single source of truth: the worker owns JOINT_LIMITS_DEG and we mirror it.
	// The payload is ~200 bytes, so no chunking is needed (unlike list_recordings).
	const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
	if (StatusJson->TryGetObjectField(TEXT("joint_limits"), LimitsObj) && LimitsObj && LimitsObj->IsValid())
	{
		JointLimitsDeg.Reset();
		for (const auto& Pair : (*LimitsObj)->Values)
		{
			const TSharedPtr<FJsonObject> LObj = Pair.Value->AsObject();
			if (!LObj.IsValid()) { continue; }

			double MinV = -180.0, MaxV = 180.0;
			LObj->TryGetNumberField(TEXT("min"), MinV);
			LObj->TryGetNumberField(TEXT("max"), MaxV);

			FJointLimit L;
			L.Min = static_cast<float>(MinV);
			L.Max = static_cast<float>(MaxV);
			JointLimitsDeg.Add(FName(*Pair.Key), L);
		}
		UE_LOG(LogRosBridge, Log, TEXT("Joint limits received: %d joints"), JointLimitsDeg.Num());
	}

	// --- Replay progress ---
	const TSharedPtr<FJsonObject>* ProgObj = nullptr;
	if (StatusJson->TryGetObjectField(TEXT("replay_progress"), ProgObj) && ProgObj)
	{
		(*ProgObj)->TryGetNumberField(TEXT("index"), ReplayIndex);
		(*ProgObj)->TryGetNumberField(TEXT("total"), ReplayTotal);
		(*ProgObj)->TryGetStringField(TEXT("filename"), ReplayProgFilename);
		(*ProgObj)->TryGetBoolField(TEXT("approaching"), bReplayApproaching);
	}

	// Display device-level errors (USB disconnection, serial errors)
	const TSharedPtr<FJsonObject>* DeviceErrors = nullptr;
	bool bHasDeviceErrors = StatusJson->TryGetObjectField(TEXT("device_errors"), DeviceErrors) && DeviceErrors;

	// Check follower error
	FString FollowerErr;
	if (bHasDeviceErrors)
	{
		(*DeviceErrors)->TryGetStringField(TEXT("follower"), FollowerErr);
	}
	if (!FollowerErr.IsEmpty() && !bFollowerDeviceError)
	{
		bFollowerDeviceError = true;
		UE_LOG(LogRosBridge, Error, TEXT("Follower USB error: %s"), *FollowerErr);
	}
	else if (FollowerErr.IsEmpty() && bFollowerDeviceError)
	{
		bFollowerDeviceError = false;
		UE_LOG(LogRosBridge, Log, TEXT("Follower USB restored."));
	}

	// Check leader error
	FString LeaderErr;
	if (bHasDeviceErrors)
	{
		(*DeviceErrors)->TryGetStringField(TEXT("leader"), LeaderErr);
	}
	if (!LeaderErr.IsEmpty() && !bLeaderDeviceError)
	{
		bLeaderDeviceError = true;
		UE_LOG(LogRosBridge, Error, TEXT("Leader USB error: %s"), *LeaderErr);
	}
	else if (LeaderErr.IsEmpty() && bLeaderDeviceError)
	{
		bLeaderDeviceError = false;
		UE_LOG(LogRosBridge, Log, TEXT("Leader USB restored."));
	}
}

// =============================================================================
// Phase 10 Stage 3 — Joint monitoring
// =============================================================================

TArray<FName> ARobotVisualizer::GetJointNames()
{
	static const TArray<FName> Names = {
		FName("shoulder_pan"), FName("shoulder_lift"), FName("elbow_flex"),
		FName("wrist_flex"),   FName("wrist_roll"),    FName("gripper")
	};
	return Names;
}

float ARobotVisualizer::GetJointAngleDeg(FName JointName) const
{
	const float* V = CurrentJointDeg.Find(JointName);
	return V ? *V : 0.0f;
}

bool ARobotVisualizer::GetJointLimit(FName JointName, float& OutMin, float& OutMax) const
{
	if (const FJointLimit* L = JointLimitsDeg.Find(JointName))
	{
		OutMin = L->Min;
		OutMax = L->Max;
		return true;
	}
	OutMin = -180.0f;
	OutMax = 180.0f;
	return false;
}

float ARobotVisualizer::GetJointRangeAlpha(FName JointName) const
{
	float Min = 0.0f, Max = 0.0f;
	if (!GetJointLimit(JointName, Min, Max) || FMath::IsNearlyEqual(Min, Max))
	{
		return 0.0f;
	}
	return FMath::Clamp((GetJointAngleDeg(JointName) - Min) / (Max - Min), 0.0f, 1.0f);
}

EJointWarn ARobotVisualizer::GetJointWarnLevel(FName JointName) const
{
	float Min = 0.0f, Max = 0.0f;
	if (!GetJointLimit(JointName, Min, Max))
	{
		return EJointWarn::Normal;   // limits not received yet — don't cry wolf
	}

	const float A = GetJointAngleDeg(JointName);

	// Headroom = absolute degrees to the nearest limit. Absolute rather than
	// percentage: the gripper's range is only ~99 deg, so a percentage threshold
	// would fire far too late on it relative to the shoulder joints.
	const float Headroom = FMath::Min(A - Min, Max - A);

	if (Headroom <= JointDangerMarginDeg) { return EJointWarn::Danger; }
	if (Headroom <= JointCautionMarginDeg) { return EJointWarn::Caution; }
	return EJointWarn::Normal;
}

void ARobotVisualizer::RecordJointHistory()
{
	for (const FName& N : GetJointNames())
	{
		TArray<float>& Buf = JointHistory.FindOrAdd(N);
		if (Buf.Num() != JointHistoryCapacity)
		{
			Buf.Init(0.0f, JointHistoryCapacity);
		}
		const float* Cur = CurrentJointDeg.Find(N);
		Buf[JointHistoryHead] = Cur ? *Cur : 0.0f;
	}

	JointHistoryHead = (JointHistoryHead + 1) % JointHistoryCapacity;
	JointHistoryCount = FMath::Min(JointHistoryCount + 1, JointHistoryCapacity);
}

TArray<float> ARobotVisualizer::GetJointHistory(FName JointName) const
{
	TArray<float> Out;
	const TArray<float>* Buf = JointHistory.Find(JointName);
	if (!Buf || Buf->Num() != JointHistoryCapacity || JointHistoryCount == 0)
	{
		return Out;
	}

	// Unroll the ring buffer into chronological order (oldest -> newest).
	const int32 Start = (JointHistoryHead - JointHistoryCount + JointHistoryCapacity) % JointHistoryCapacity;
	Out.Reserve(JointHistoryCount);
	for (int32 i = 0; i < JointHistoryCount; ++i)
	{
		Out.Add((*Buf)[(Start + i) % JointHistoryCapacity]);
	}
	return Out;
}