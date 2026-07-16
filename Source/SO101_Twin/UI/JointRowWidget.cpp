#include "JointRowWidget.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

namespace
{
	/** Colour scheme shared by the angle text and the range bar. */
	FLinearColor WarnColor(EJointWarn Warn)
	{
		switch (Warn)
		{
		case EJointWarn::Danger:  return FLinearColor(1.0f, 0.15f, 0.15f);  // red
		case EJointWarn::Caution: return FLinearColor(1.0f, 0.8f, 0.0f);    // amber
		default:                  return FLinearColor(0.35f, 0.85f, 0.45f); // green
		}
	}
}

void UJointRowWidget::InitJoint(FName InJointName)
{
	JointName = InJointName;
	if (JointNameText)
	{
		JointNameText->SetText(FText::FromName(JointName));
	}
}

void UJointRowWidget::Refresh(const ARobotVisualizer* Robot)
{
	if (!Robot) { return; }

	// Until the worker has sent its limits there is nothing meaningful to show.
	if (!Robot->HasJointLimits())
	{
		if (JointAngleText)
		{
			JointAngleText->SetText(FText::FromString(TEXT("--")));
			JointAngleText->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
		}
		if (JointRangeBar)
		{
			JointRangeBar->SetPercent(0.0f);
			JointRangeBar->SetFillColorAndOpacity(FLinearColor::Gray);
		}
		return;
	}

	const float Angle = Robot->GetJointAngleDeg(JointName);
	const float Alpha = Robot->GetJointRangeAlpha(JointName);
	const EJointWarn Warn = Robot->GetJointWarnLevel(JointName);
	const FLinearColor Color = WarnColor(Warn);

	float Min = 0.0f, Max = 0.0f;
	Robot->GetJointLimit(JointName, Min, Max);

	if (JointAngleText)
	{
		JointAngleText->SetText(FText::FromString(
			FString::Printf(TEXT("%7.1f\u00B0   [%.0f ~ %.0f]"), Angle, Min, Max)));
		JointAngleText->SetColorAndOpacity(FSlateColor(Color));
	}
	if (JointRangeBar)
	{
		JointRangeBar->SetPercent(Alpha);
		JointRangeBar->SetFillColorAndOpacity(Color);
	}
}