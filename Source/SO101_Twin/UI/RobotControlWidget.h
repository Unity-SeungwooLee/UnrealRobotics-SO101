#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RobotControlWidget.generated.h"

class ARobotVisualizer;
class URosBridgeSubsystem;

/**
 * Base class for the in-viewport robot control UI (Phase 10).
 *
 * This C++ base owns the logic: it finds the ARobotVisualizer in the level,
 * exposes the robot's commands as BlueprintCallable functions (bind WBP
 * buttons to these), and exposes its state as BlueprintPure getters (bind
 * WBP text / visibility to these).
 *
 * Create a Widget Blueprint (WBP_RobotControl) reparented to this class,
 * lay out the visuals in the UMG designer, and wire buttons/text to the
 * functions below. No Blueprint scripting required — just bindings.
 */
UCLASS()
class SO101_TWIN_API URobotControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Commands (bind WBP buttons' OnClicked to these) ---

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdSyncOn();

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdSyncOff();

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdStartRecord();

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdStopRecord();

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdStartReplay(const FString& Filename, bool bLoop, float ApproachSpeed);

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdStopReplay();

	UFUNCTION(BlueprintCallable, Category = "ROS|UI")
	void CmdEStop();

	// --- State getters (bind WBP text / visibility to these) ---

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	FString GetWorkerState() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool IsSyncActive() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool IsRosBridgeConnected() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool IsBridgeNodeAlive() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool IsWorkerAlive() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool HasFollowerError() const;

	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool HasLeaderError() const;

	/** True once the robot actor has been located in the level. */
	UFUNCTION(BlueprintPure, Category = "ROS|UI")
	bool HasRobot() const;

protected:
	virtual void NativeConstruct() override;

	/** Find and cache the ARobotVisualizer + subsystem. Safe to call repeatedly. */
	ARobotVisualizer* ResolveRobot();

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleEStopClicked();

	/** Recompute the safety panel from the robot's current state. */
	void RefreshSafetyUI();

	// --- Bound widgets: names MUST match the WBP widget names exactly ---
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UButton> EStopButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> ConnectionStatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<class UTextBlock> DeviceErrorText;

	float RefreshAccum = 0.0f;

	TWeakObjectPtr<ARobotVisualizer> Robot;
	TWeakObjectPtr<URosBridgeSubsystem> Ros;

	UFUNCTION() void HandleSyncOnClicked();
	UFUNCTION() void HandleSyncOffClicked();
	UFUNCTION() void HandleStartRecordClicked();
	UFUNCTION() void HandleStopRecordClicked();
	UFUNCTION() void HandleStartReplayClicked();
	UFUNCTION() void HandleStopReplayClicked();

	/** Recompute the control panel (worker state label + button enable states). */
	void RefreshControlUI();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> SyncOnButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> SyncOffButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> StartRecordButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> StopRecordButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> StartReplayButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> StopReplayButton;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UEditableTextBox> ReplayFilenameTextBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UCheckBox> LoopCheckBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class USpinBox> ApproachSpeedSpinBox;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> WorkerStateText;

	UFUNCTION() void HandleRefreshRecordingsClicked();
	UFUNCTION() void HandleRecordingSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	void RefreshRecordingsList();
	void RefreshReplayProgress();

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UButton> RefreshRecordingsButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UComboBoxString> RecordingComboBox;
	bool bInitialListRequested = false;
	float AutoRequestAccum = 0.0f;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UProgressBar> ReplayProgressBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> ReplayProgressText;

	int32 CachedRecordingsVersion = -1;
};