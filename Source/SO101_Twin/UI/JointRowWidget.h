#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobotVisualizer.h"
#include "JointRowWidget.generated.h"

/**
 * One row of the joint monitor panel (Phase 10 Stage 3).
 *
 * Rows are spawned in C++ by URobotControlWidget — one per joint, in the
 * canonical order from ARobotVisualizer::GetJointNames(). The WBP only
 * supplies the visual layout; all values are pushed in via Refresh().
 */
UCLASS()
class SO101_TWIN_API UJointRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Assign the joint this row represents. Called once, at spawn. */
	void InitJoint(FName InJointName);

	/** Pull the joint's current angle / range / warn level from the robot. */
	void Refresh(const ARobotVisualizer* Robot);

	FName GetJointName() const { return JointName; }

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> JointNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> JointAngleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UProgressBar> JointRangeBar;

private:
	FName JointName;
};