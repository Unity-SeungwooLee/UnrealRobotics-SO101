#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RobotVisualizer.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class URosBridgeSubsystem;

/**
 * Visualizes the SO-ARM-101 follower arm in Unreal Engine.
 *
 * The component hierarchy mirrors the URDF link/joint structure:
 *   BaseLink -> ShoulderPanJoint -> ShoulderLink -> ShoulderLiftJoint -> ...
 *
 * Each "Joint" SceneComponent is where the ROS joint angle gets applied
 * as a local Z-axis rotation. All child links/meshes rotate with it.
 *
 * Mesh offsets and joint origins are hardcoded from the URDF
 * (so101_follower.urdf), converted to UE coordinates (cm, left-handed).
 *
 * Usage:
 *   1. Place this actor in the level
 *   2. It auto-connects to rosbridge and subscribes to /joint_states
 *   3. Moving the real robot updates the 3D model in real time
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

	// --- Configuration ---

	UPROPERTY(EditAnywhere, Category = "ROS|Bridge")
	FString RosBridgeUrl = TEXT("ws://127.0.0.1:9090/?x=1");

	UPROPERTY(EditAnywhere, Category = "ROS|Topics")
	FString JointStateTopic = TEXT("/joint_states");

	UPROPERTY(EditAnywhere, Category = "ROS|Topics")
	FString JointStateType = TEXT("sensor_msgs/JointState");

	UPROPERTY(EditAnywhere, Category = "ROS|Bridge")
	float SubscribeDelaySeconds = 1.0f;

private:
	// --- Component hierarchy ---

	// Root
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	TObjectPtr<USceneComponent> RobotRoot;

	// Link SceneComponents (parent for mesh offsets)
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

	// Joint SceneComponents (rotation applied here)
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

	// --- Joint name → component mapping ---
	UPROPERTY()
	TMap<FName, TObjectPtr<USceneComponent>> JointComponentMap;

	// --- Mesh components (for material assignment etc.) ---
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AllMeshComponents;

	// --- ROS connection ---
	FTimerHandle SubscribeTimerHandle;

	void DoSubscribe();

	UFUNCTION()
	void OnRosMessage(const FString& Topic, const FString& MessageJson);

	void ParseAndApplyJointStates(const FString& MessageJson);

	// --- Helpers ---

	/** Create a SceneComponent with a relative transform (already in UE coordinates). */
	USceneComponent* CreateJointComponent(const FName& Name, USceneComponent* Parent,
		const FVector& Location, const FRotator& Rotation);

	/** Create a SceneComponent as a link container. */
	USceneComponent* CreateLinkComponent(const FName& Name, USceneComponent* Parent);

	/** Attach a StaticMeshComponent to a link with the given mesh offset (UE coordinates). */
	UStaticMeshComponent* AttachMesh(USceneComponent* Parent, UStaticMesh* Mesh,
		const FName& Name, const FVector& Location, const FRotator& Rotation,
		bool bIsMotor);
};
