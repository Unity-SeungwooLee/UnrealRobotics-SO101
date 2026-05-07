#include "RobotVisualizer.h"
#include "RosCoordConv.h"
#include "RosBridgeSubsystem.h"
#include "RosBridgeLog.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

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
	//   base_motor_holder: xyz(-0.0063647, -0.0000994, -0.0024) rpy(90, 0, 90) deg
	Attach(BaseLink, TEXT("base_motor_holder_so101_v1"),
		-0.00636471, -0.0000994414, -0.0024,
		1.5708, 0.0, 1.5708);
	//   base_so101_v2: xyz(-0.0063647, 0, -0.0024) rpy(90, 0, 90) deg
	Attach(BaseLink, TEXT("base_so101_v2"),
		-0.00636471, 0.0, -0.0024,
		1.5708, 0.0, 1.5708);
	//   sts3215 motor: xyz(0.0263353, 0, 0.0437) rpy(0, 0, 0)
	Attach(BaseLink, TEXT("sts3215_03a_v1"),
		0.0263353, 0.0, 0.0437,
		0.0, 0.0, 0.0, true);
	//   waveshare plate: xyz(-0.0309827, -0.0001994, 0.0474) rpy(90, 0, 90)
	Attach(BaseLink, TEXT("waveshare_mounting_plate_so101_v2"),
		-0.0309827, -0.000199441, 0.0474,
		1.5708, 0.0, 1.5708);

	// === shoulder_link meshes ===
	//   sts3215 motor: xyz(-0.0303992, 0.0004222, -0.0417) rpy(90, 90, 0)
	Attach(ShoulderLink, TEXT("sts3215_03a_v1"),
		-0.0303992, 0.000422241, -0.0417,
		1.5708, 1.5708, 0.0, true);
	//   motor_holder_base: xyz(-0.0675992, -0.0001778, 0.01585) rpy(90, -90, 0)
	Attach(ShoulderLink, TEXT("motor_holder_so101_base_v1"),
		-0.0675992, -0.000177759, 0.0158499,
		1.5708, -1.5708, 0.0);
	//   rotation_pitch: xyz(0.0122008, 0.0000222, 0.0464) rpy(-90, 0, 0)
	Attach(ShoulderLink, TEXT("rotation_pitch_so101_v1"),
		0.0122008, 0.0000222413, 0.0464,
		-1.5708, 0.0, 0.0);

	// === upper_arm_link meshes ===
	//   sts3215 motor: xyz(-0.11257, -0.0155, 0.0187) rpy(-180, 0, -90)
	Attach(UpperArmLink, TEXT("sts3215_03a_v1"),
		-0.11257, -0.0155, 0.0187,
		-3.14159, 0.0, -1.5708, true);
	//   upper_arm: xyz(-0.065085, 0.012, 0.0182) rpy(180, 0, 0)
	Attach(UpperArmLink, TEXT("upper_arm_so101_v1"),
		-0.065085, 0.012, 0.0182,
		3.14159, 0.0, 0.0);

	// === lower_arm_link meshes ===
	//   under_arm: xyz(-0.0648499, -0.032, 0.0182) rpy(180, 0, 0)
	Attach(LowerArmLink, TEXT("under_arm_so101_v1"),
		-0.0648499, -0.032, 0.0182,
		3.14159, 0.0, 0.0);
	//   motor_holder_wrist: xyz(-0.0648499, -0.032, 0.018) rpy(-180, 0, 0)
	Attach(LowerArmLink, TEXT("motor_holder_so101_wrist_v1"),
		-0.0648499, -0.032, 0.018,
		-3.14159, 0.0, 0.0);
	//   sts3215 motor: xyz(-0.1224, 0.0052, 0.0187) rpy(-180, 0, -180)
	Attach(LowerArmLink, TEXT("sts3215_03a_v1"),
		-0.1224, 0.0052, 0.0187,
		-3.14159, 0.0, -3.14159, true);

	// === wrist_link meshes ===
	//   sts3215_no_horn: xyz(0, -0.0424, 0.0306) rpy(90, 90, 0)
	Attach(WristLink, TEXT("sts3215_03a_no_horn_v1"),
		0.0, -0.0424, 0.0306,
		1.5708, 1.5708, 0.0, true);
	//   wrist_roll_pitch: xyz(0, -0.028, 0.0181) rpy(-90, -90, 0)
	Attach(WristLink, TEXT("wrist_roll_pitch_so101_v2"),
		0.0, -0.028, 0.0181,
		-1.5708, -1.5708, 0.0);

	// === gripper_link meshes ===
	//   sts3215 motor: xyz(0.0077, 0.0001, -0.0234) rpy(-90, 0, 0)
	Attach(GripperLink, TEXT("sts3215_03a_v1"),
		0.0077, 0.0001, -0.0234,
		-1.5708, 0.0, 0.0, true);
	//   wrist_roll_follower: xyz(0, -0.0002182, 0.0009497) rpy(-180, 0, 0)
	Attach(GripperLink, TEXT("wrist_roll_follower_so101_v1"),
		0.0, -0.000218214, 0.000949706,
		-3.14159, 0.0, 0.0);

	// === moving_jaw_link meshes ===
	//   moving_jaw: xyz(0, 0, 0.0189) rpy(0, 0, 0)
	Attach(MovingJawLink, TEXT("moving_jaw_so101_v1"),
		0.0, 0.0, 0.0189,
		0.0, 0.0, 0.0);

	UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: %d mesh components created"), AllMeshComponents.Num());

	// --- Connect to ROS ---
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros) return;

	Ros->OnTopicMessage.AddDynamic(this, &ARobotVisualizer::OnRosMessage);

	if (!Ros->IsConnected())
	{
		Ros->Connect(RosBridgeUrl);
	}

	GetWorld()->GetTimerManager().SetTimer(
		SubscribeTimerHandle, this, &ARobotVisualizer::DoSubscribe,
		SubscribeDelaySeconds, false);
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
		}
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SubscribeTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// =============================================================================
// ROS subscription
// =============================================================================

void ARobotVisualizer::DoSubscribe()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(this);
	if (!GI) return;

	URosBridgeSubsystem* Ros = GI->GetSubsystem<URosBridgeSubsystem>();
	if (!Ros) return;

	if (!Ros->IsConnected())
	{
		UE_LOG(LogRosBridge, Warning,
			TEXT("RobotVisualizer: not connected yet, retrying in %.1fs"), SubscribeDelaySeconds);
		GetWorld()->GetTimerManager().SetTimer(
			SubscribeTimerHandle, this, &ARobotVisualizer::DoSubscribe,
			SubscribeDelaySeconds, false);
		return;
	}

	Ros->Subscribe(JointStateTopic, JointStateType);
	UE_LOG(LogRosBridge, Log, TEXT("RobotVisualizer: subscribed to %s"), *JointStateTopic);
}

// =============================================================================
// Message handling
// =============================================================================

void ARobotVisualizer::OnRosMessage(const FString& Topic, const FString& MessageJson)
{
	if (Topic == JointStateTopic)
	{
		ParseAndApplyJointStates(MessageJson);
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

		TObjectPtr<USceneComponent>* JointComp = JointComponentMap.Find(JointName);
		if (JointComp && *JointComp)
		{
			// All URDF joints in this robot have axis="0 0 1" (local Z).
			// Apply the joint angle as a Yaw rotation on top of the base
			// joint orientation that was set in the constructor.
			const float AngleDeg = RosCoordConv::RosJointAngleToUeDegrees(AngleRad);

			FRotator CurrentRot = (*JointComp)->GetRelativeRotation();
			// The base rotation was set in constructor. We need to compose
			// the joint angle on top. Store base rotation and add joint angle.
			// For simplicity, since all joints rotate about their local Z,
			// we add to the Yaw component.
			//
			// NOTE: This assumes the URDF base rotation was already applied.
			// The joint angle rotation is additive on the local Z axis,
			// which maps to Yaw after the base RPY transform.

			// We need to store base rotations separately to avoid drift.
			// For now, reconstruct from URDF data each frame.
			// TODO: Cache base rotations for cleanliness.

			// Get the base rotation that was set for this joint
			FRotator BaseRot = CurrentRot;  // Will be overwritten properly below

			// Re-derive base rotation from URDF (matches constructor values)
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

			// Compose: base rotation, then joint angle about local Z
			const FQuat BaseQuat = BaseRot.Quaternion();
			const FQuat JointQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(AngleDeg));
			const FQuat FinalQuat = BaseQuat * JointQuat;

			(*JointComp)->SetRelativeRotation(FinalQuat.Rotator());
		}
	}
}
