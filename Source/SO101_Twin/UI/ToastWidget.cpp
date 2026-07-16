#include "ToastWidget.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"

void UToastWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Start fully hidden above the top edge.
	ApplyOffset(HiddenOffsetY);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FLinearColor UToastWidget::LevelColor(EToastLevel Level)
{
	switch (Level)
	{
	case EToastLevel::Success: return FLinearColor(0.30f, 0.85f, 0.40f);
	case EToastLevel::Warning: return FLinearColor(1.00f, 0.75f, 0.15f);
	case EToastLevel::Error:   return FLinearColor(1.00f, 0.25f, 0.25f);
	default:                   return FLinearColor(0.55f, 0.75f, 1.00f);  // Info
	}
}

void UToastWidget::Push(const FString& Message, EToastLevel Level, float HoldSeconds)
{
	if (Message.IsEmpty()) { return; }

	// Ignore repeats of whatever is on screen right now (polled callers may
	// call Push every tick while a state persists).
	if (Message == ActiveText) { return; }
	for (const FToastMsg& M : Queue)
	{
		if (M.Text == Message) { return; }
	}

	FToastMsg Msg;
	Msg.Text = Message;
	Msg.Level = Level;
	Msg.Hold = HoldSeconds;
	Queue.Add(Msg);

	if (Phase == EPhase::Idle)
	{
		BeginNext();
	}
}

void UToastWidget::BeginNext()
{
	if (Queue.Num() == 0)
	{
		Phase = EPhase::Idle;
		ActiveText.Empty();
		return;
	}

	Current = Queue[0];
	Queue.RemoveAt(0);
	ActiveText = Current.Text;

	if (ToastText)
	{
		ToastText->SetText(FText::FromString(Current.Text));
		ToastText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	if (ToastBorder)
	{
		ToastBorder->SetBrushColor(LevelColor(Current.Level));
	}

	Phase = EPhase::SlideIn;
	PhaseTime = 0.0f;
}

void UToastWidget::ApplyOffset(float OffsetY)
{
	SetRenderTranslation(FVector2D(0.0f, OffsetY));
}

void UToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Phase == EPhase::Idle) { return; }

	PhaseTime += InDeltaTime;

	switch (Phase)
	{
	case EPhase::SlideIn:
	{
		const float T = FMath::Clamp(PhaseTime / SlideSeconds, 0.0f, 1.0f);
		// Cosine ease-in/out — same curve family as the replay approach.
		const float Eased = 0.5f - 0.5f * FMath::Cos(T * PI);
		ApplyOffset(FMath::Lerp(HiddenOffsetY, 0.0f, Eased));
		if (T >= 1.0f) { Phase = EPhase::Hold; PhaseTime = 0.0f; }
		break;
	}
	case EPhase::Hold:
	{
		if (PhaseTime >= Current.Hold) { Phase = EPhase::SlideOut; PhaseTime = 0.0f; }
		break;
	}
	case EPhase::SlideOut:
	{
		const float T = FMath::Clamp(PhaseTime / SlideSeconds, 0.0f, 1.0f);
		const float Eased = 0.5f - 0.5f * FMath::Cos(T * PI);
		ApplyOffset(FMath::Lerp(0.0f, HiddenOffsetY, Eased));
		if (T >= 1.0f) { BeginNext(); }   // chain to the next queued message
		break;
	}
	default: break;
	}
}