#include "JointGraphWidget.h"
#include "RobotVisualizer.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Stable per-joint identity colour — matches the legend order. */
	FLinearColor JointColor(int32 Index)
	{
		static const FLinearColor Colors[] = {
			FLinearColor(1.00f, 0.35f, 0.35f),  // shoulder_pan
			FLinearColor(1.00f, 0.65f, 0.20f),  // shoulder_lift
			FLinearColor(0.95f, 0.90f, 0.30f),  // elbow_flex
			FLinearColor(0.40f, 0.90f, 0.45f),  // wrist_flex
			FLinearColor(0.35f, 0.75f, 1.00f),  // wrist_roll
			FLinearColor(0.85f, 0.50f, 1.00f),  // gripper
		};
		return Colors[FMath::Clamp(Index, 0, 5)];
	}
}

void UJointGraphWidget::SetRobot(ARobotVisualizer* InRobot)
{
	Robot = InRobot;
}

int32 UJointGraphWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const ARobotVisualizer* R = Robot.Get();
	if (!R || !R->HasJointLimits())
	{
		return Base;   // nothing meaningful to plot until limits arrive
	}

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float LegendW = 90.0f;                 // left column for the legend
	const float PadL = LegendW + 6.0f, PadR = 6.0f, PadT = 6.0f, PadB = 6.0f;
	const float W = static_cast<float>(Size.X) - PadL - PadR;
	const float H = static_cast<float>(Size.Y) - PadT - PadB;
	if (W <= 8.0f || H <= 8.0f)
	{
		return Base;
	}

	const FPaintGeometry PaintGeo = AllottedGeometry.ToPaintGeometry();
	int32 Layer = Base + 1;

	// --- Frame + midline. The top/bottom edges are the joint limits. ---
	{
		TArray<FVector2D> Frame = {
			FVector2D(PadL,     PadT),
			FVector2D(PadL + W, PadT),
			FVector2D(PadL + W, PadT + H),
			FVector2D(PadL,     PadT + H),
			FVector2D(PadL,     PadT)
		};
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeo, Frame,
			ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.30f), false, 1.0f);

		TArray<FVector2D> Mid = {
			FVector2D(PadL,     PadT + H * 0.5f),
			FVector2D(PadL + W, PadT + H * 0.5f)
		};
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeo, Mid,
			ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.10f), false, 1.0f);
	}
	++Layer;

	// --- Traces ---
	const int32 Cap = ARobotVisualizer::GetJointHistoryCapacity();
	const float DX = (Cap > 1) ? (W / static_cast<float>(Cap - 1)) : W;
	const TArray<FName> Names = ARobotVisualizer::GetJointNames();

	for (int32 j = 0; j < Names.Num(); ++j)
	{
		const TArray<float> Hist = R->GetJointHistory(Names[j]);
		if (Hist.Num() < 2) { continue; }

		float Min = 0.0f, Max = 0.0f;
		if (!R->GetJointLimit(Names[j], Min, Max) || FMath::IsNearlyEqual(Min, Max)) { continue; }

		const int32 N = Hist.Num();
		TArray<FVector2D> Pts;
		Pts.Reserve(N);

		for (int32 i = 0; i < N; ++i)
		{
			// Newest sample pinned to the right edge; older samples scroll left.
			const float X = PadL + W - static_cast<float>(N - 1 - i) * DX;
			// Normalised to THIS joint's range, so 1.0 = upper limit, 0.0 = lower.
			const float A = FMath::Clamp((Hist[i] - Min) / (Max - Min), 0.0f, 1.0f);
			const float Y = PadT + H * (1.0f - A);
			Pts.Add(FVector2D(X, Y));
		}

		FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeo, Pts,
			ESlateDrawEffect::None, JointColor(j), true, 1.5f);
	}
	++Layer;

	// --- Legend: vertical column on the left, one row per joint ---
	{
		const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11);
		const float RowH = H / static_cast<float>(Names.Num());
		const float SwatchW = 14.0f, SwatchH = 3.0f, Gap = 4.0f;

		for (int32 j = 0; j < Names.Num(); ++j)
		{
			const float RowY = PadT + RowH * j + RowH * 0.5f;

			// Colour swatch (short line segment matching the trace colour).
			TArray<FVector2D> Swatch = {
				FVector2D(2.0f, RowY),
				FVector2D(2.0f + SwatchW, RowY)
			};
			FSlateDrawElement::MakeLines(OutDrawElements, Layer, PaintGeo, Swatch,
				ESlateDrawEffect::None, JointColor(j), true, 3.0f);

			// Joint name to the right of the swatch.
			FSlateDrawElement::MakeText(OutDrawElements, Layer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(LegendW, 14.0f),
					FSlateLayoutTransform(FVector2f(2.0f + SwatchW + Gap, RowY - 7.0f))),
				Names[j].ToString(), Font, ESlateDrawEffect::None, JointColor(j));
		}
	}
	++Layer;

	return Layer;
}