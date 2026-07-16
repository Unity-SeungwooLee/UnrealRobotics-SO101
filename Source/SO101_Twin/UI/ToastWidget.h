#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToastWidget.generated.h"

/** Toast severity — drives the accent colour. */
UENUM(BlueprintType)
enum class EToastLevel : uint8
{
	Info     UMETA(DisplayName = "Info"),
	Success  UMETA(DisplayName = "Success"),
	Warning  UMETA(DisplayName = "Warning"),
	Error    UMETA(DisplayName = "Error"),
};

/**
 * Slide-down toast for event notifications (Phase 10 Stage 4a).
 *
 * Replaces the AddOnScreenDebugMessage spam: recording saved, USB error,
 * state changes, etc. Messages queue and show one at a time. The slide uses
 * the same cosine ease-in/out as the replay approach, for a consistent feel.
 *
 * Position is driven entirely in NativeTick by offsetting the root panel's
 * render transform — the WBP only supplies the visual (a bordered box with
 * ToastText inside), anchored top-centre.
 */
UCLASS()
class SO101_TWIN_API UToastWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Queue a message. Safe to call every frame — duplicates of the currently
	 *  showing message are ignored so polled state changes don't spam. */
	void Push(const FString& Message, EToastLevel Level = EToastLevel::Info, float HoldSeconds = 3.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ToastText;

	/** Optional coloured strip / border. Bind if present in the WBP. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UBorder> ToastBorder;

	/** How far above the anchored rest position the toast hides (pixels). */
	UPROPERTY(EditAnywhere, Category = "Toast")
	float HiddenOffsetY = -120.0f;

	/** Slide duration for both in and out (seconds). */
	UPROPERTY(EditAnywhere, Category = "Toast")
	float SlideSeconds = 0.35f;

private:
	enum class EPhase : uint8 { Idle, SlideIn, Hold, SlideOut };

	struct FToastMsg
	{
		FString Text;
		EToastLevel Level = EToastLevel::Info;
		float Hold = 3.0f;
	};

	void BeginNext();
	void ApplyOffset(float OffsetY);
	static FLinearColor LevelColor(EToastLevel Level);

	TArray<FToastMsg> Queue;
	FToastMsg Current;
	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.0f;

	/** Guards against re-queuing the same message that's currently visible. */
	FString ActiveText;
};