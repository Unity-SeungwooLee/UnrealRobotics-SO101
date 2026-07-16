#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JointGraphWidget.generated.h"

class ARobotVisualizer;

/**
 * Rolling history plot of all six joints (Phase 10 Stage 3c).
 *
 * Y is normalised to each joint's own physical range, so the top and bottom
 * borders of the plot ARE the joint limits regardless of the joint's scale.
 * A trace touching a border means that joint hit its limit. X is the last
 * ~10 seconds, newest sample pinned to the right edge.
 *
 * Drawing is done in NativePaint — no child widgets, no tick.
 */
UCLASS()
class SO101_TWIN_API UJointGraphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Data source. Pushed in by URobotControlWidget once the actor resolves. */
	void SetRobot(ARobotVisualizer* InRobot);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TWeakObjectPtr<ARobotVisualizer> Robot;
};