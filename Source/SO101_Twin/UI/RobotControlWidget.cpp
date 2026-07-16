#include "RobotControlWidget.h"
#include "RobotVisualizer.h"
#include "RosBridgeSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Components/EditableTextBox.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"

#include "Components/ListView.h"
#include "Components/ComboBoxString.h"
#include "Components/ProgressBar.h"
#include "RecordingEntryWidget.h"

#include "TimerManager.h"
#include "Engine/World.h"

#include "Components/VerticalBox.h"
#include "JointRowWidget.h"

#include "JointGraphWidget.h"

void URobotControlWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveRobot();

	if (EStopButton)
	{
		EStopButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleEStopClicked);
	}
	if (SyncOnButton)      SyncOnButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleSyncOnClicked);
	if (SyncOffButton)     SyncOffButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleSyncOffClicked);
	if (StartRecordButton) StartRecordButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleStartRecordClicked);
	if (StopRecordButton)  StopRecordButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleStopRecordClicked);
	if (StartReplayButton) StartReplayButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleStartReplayClicked);
	if (StopReplayButton)  StopReplayButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleStopReplayClicked);

	if (ApproachSpeedSpinBox)
	{
		ApproachSpeedSpinBox->SetMinValue(5.0f);
		ApproachSpeedSpinBox->SetMaxValue(300.0f);
		ApproachSpeedSpinBox->SetMinSliderValue(5.0f);
		ApproachSpeedSpinBox->SetMaxSliderValue(300.0f);
		ApproachSpeedSpinBox->SetValue(45.0f);
	}
	RefreshControlUI();
	RefreshSafetyUI();

	if (RefreshRecordingsButton)
	{
		RefreshRecordingsButton->OnClicked.AddDynamic(this, &URobotControlWidget::HandleRefreshRecordingsClicked);
	}
	if (RecordingComboBox)
	{
		RecordingComboBox->OnSelectionChanged.AddDynamic(this, &URobotControlWidget::HandleRecordingSelected);
	}

	BuildJointRows();
}

ARobotVisualizer* URobotControlWidget::ResolveRobot()
{
	if (!Robot.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			Robot = Cast<ARobotVisualizer>(
				UGameplayStatics::GetActorOfClass(World, ARobotVisualizer::StaticClass()));
		}
	}
	if (!Ros.IsValid())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			Ros = GI->GetSubsystem<URosBridgeSubsystem>();
		}
	}
	return Robot.Get();
}

// --- Commands ---

void URobotControlWidget::CmdSyncOn() { if (ARobotVisualizer* R = ResolveRobot()) { R->SyncOn(); } }
void URobotControlWidget::CmdSyncOff() { if (ARobotVisualizer* R = ResolveRobot()) { R->SyncOff(); } }
void URobotControlWidget::CmdStartRecord() { if (ARobotVisualizer* R = ResolveRobot()) { R->StartRecord(); } }
void URobotControlWidget::CmdStopRecord() { if (ARobotVisualizer* R = ResolveRobot()) { R->StopRecord(); } }
void URobotControlWidget::CmdStopReplay() { if (ARobotVisualizer* R = ResolveRobot()) { R->StopReplay(); } }
void URobotControlWidget::CmdEStop() { if (ARobotVisualizer* R = ResolveRobot()) { R->EStop(); } }

void URobotControlWidget::CmdStartReplay(const FString& Filename, bool bLoop, float ApproachSpeed)
{
	if (ARobotVisualizer* R = ResolveRobot())
	{
		R->ReplayFilename = Filename;
		R->bReplayLoop = bLoop;
		R->ApproachSpeed = ApproachSpeed;
		R->StartReplay();
	}
}

void URobotControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Poll the robot state ~5x/sec (cheap, no delegates needed).
	RefreshAccum += InDeltaTime;
	if (RefreshAccum >= 0.2f)
	{
		RefreshAccum = 0.0f;
		RefreshSafetyUI();
		RefreshControlUI();
		RefreshAutoRequests();
		RefreshRecordingsList();
		RefreshReplayProgress();
		RefreshJointMonitor();
		RefreshToasts();
	}
}

void URobotControlWidget::HandleEStopClicked()
{
	CmdEStop();
}

void URobotControlWidget::RefreshSafetyUI()
{
	// --- Connection status text + color (priority order) ---
	if (ConnectionStatusText)
	{
		FString StatusStr;
		FLinearColor Color;

		if (!IsRosBridgeConnected())
		{
			StatusStr = TEXT("DISCONNECTED");
			Color = FLinearColor::Red;
		}
		else if (!IsBridgeNodeAlive())
		{
			StatusStr = TEXT("Bridge node: DOWN");
			Color = FLinearColor::Red;
		}
		else if (!IsWorkerAlive())
		{
			StatusStr = TEXT("Worker: DOWN");
			Color = FLinearColor(1.0f, 0.5f, 0.0f); // orange
		}
		else
		{
			const FString WS = GetWorkerState();
			// WorkerState may still hold a stale connection-layer string
			// ("disconnected", "bridge lost", "worker lost", "unknown") until
			// the worker sends its first real /robot_status. Filter those out.
			const bool bRealState =
				(WS == TEXT("idle") || WS == TEXT("syncing") ||
					WS == TEXT("recording") || WS == TEXT("replaying"));

			StatusStr = bRealState
				? FString::Printf(TEXT("Connected  |  %s"), *WS)
				: TEXT("Connected  |  (awaiting status)");
			Color = FLinearColor::Green;
		}

		ConnectionStatusText->SetText(FText::FromString(StatusStr));
		ConnectionStatusText->SetColorAndOpacity(FSlateColor(Color));
	}

	// --- Device error panel (hidden when no errors) ---
	if (DeviceErrorText)
	{
		TArray<FString> Errors;
		if (HasFollowerError()) { Errors.Add(TEXT("Follower: USB/Serial ERROR")); }
		if (HasLeaderError()) { Errors.Add(TEXT("Leader: USB/Serial ERROR")); }

		if (Errors.Num() > 0)
		{
			DeviceErrorText->SetText(FText::FromString(FString::Join(Errors, TEXT("\n"))));
			DeviceErrorText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
			DeviceErrorText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			DeviceErrorText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URobotControlWidget::HandleSyncOnClicked() { CmdSyncOn(); }
void URobotControlWidget::HandleSyncOffClicked() { CmdSyncOff(); }
void URobotControlWidget::HandleStartRecordClicked() { CmdStartRecord(); }
void URobotControlWidget::HandleStopRecordClicked() { CmdStopRecord(); }
void URobotControlWidget::HandleStopReplayClicked() { CmdStopReplay(); }

void URobotControlWidget::HandleStartReplayClicked()
{
	FString Filename;
	if (RecordingComboBox && !RecordingComboBox->GetSelectedOption().IsEmpty())
		Filename = RecordingComboBox->GetSelectedOption();
	else if (ReplayFilenameTextBox)
		Filename = ReplayFilenameTextBox->GetText().ToString();
	const bool bLoop = LoopCheckBox ? LoopCheckBox->IsChecked() : false;
	const float Speed = ApproachSpeedSpinBox ? ApproachSpeedSpinBox->GetValue() : 45.0f;
	CmdStartReplay(Filename, bLoop, Speed);
}

void URobotControlWidget::RefreshControlUI()
{
	const FString WS = GetWorkerState();
	const bool bIdle = (WS == TEXT("idle"));
	const bool bSyncing = (WS == TEXT("syncing"));
	const bool bRecording = (WS == TEXT("recording"));
	const bool bReplaying = (WS == TEXT("replaying"));

	if (WorkerStateText)
	{
		FString Label;
		FLinearColor Color;
		if (bRecording) { Label = TEXT("RECORDING"); Color = FLinearColor::Red; }
		else if (bReplaying) { Label = TEXT("REPLAYING"); Color = FLinearColor(0.0f, 1.0f, 1.0f); }
		else if (bSyncing) { Label = TEXT("SYNCING");   Color = FLinearColor::Green; }
		else if (bIdle) { Label = TEXT("IDLE");      Color = FLinearColor::Gray; }
		else { Label = TEXT("--");        Color = FLinearColor::Gray; }
		WorkerStateText->SetText(FText::FromString(Label));
		WorkerStateText->SetColorAndOpacity(FSlateColor(Color));
	}

	// Workflow guards — only enable actions that make sense in the current state.
	const bool bAlive = IsWorkerAlive();
	if (SyncOnButton)      SyncOnButton->SetIsEnabled(bAlive && !bRecording && !bReplaying);
	if (SyncOffButton)     SyncOffButton->SetIsEnabled(bAlive && IsSyncActive() && !bRecording);
	if (StartRecordButton) StartRecordButton->SetIsEnabled(bAlive && IsSyncActive() && !bRecording && !bReplaying);
	if (StopRecordButton)  StopRecordButton->SetIsEnabled(bAlive && bRecording);
	if (StartReplayButton) StartReplayButton->SetIsEnabled(bAlive && !bRecording && !bReplaying);
	if (StopReplayButton)  StopReplayButton->SetIsEnabled(bAlive && bReplaying);
}

void URobotControlWidget::HandleRefreshRecordingsClicked()
{
	if (ARobotVisualizer* R = ResolveRobot())
	{
		R->PublishRobotCommand(TEXT("{\"cmd\":\"list_recordings\"}"));
	}
}

void URobotControlWidget::HandleRecordingSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (ReplayFilenameTextBox && !SelectedItem.IsEmpty())
	{
		ReplayFilenameTextBox->SetText(FText::FromString(SelectedItem));
	}
}

void URobotControlWidget::RefreshAutoRequests()
{
	ARobotVisualizer* R = ResolveRobot();
	if (!R || !IsWorkerAlive()) { return; }

	// Commands fired the instant the worker goes alive are silently lost —
	// the bridge's /robot_command subscription isn't wired yet (Appendix A #47).
	// Retry every ~2s; each request stops on its own once its data lands.
	AutoRequestAccum += 0.2f;   // NativeTick calls this at 5 Hz
	if (AutoRequestAccum < 2.0f) { return; }
	AutoRequestAccum = 0.0f;

	if (R->GetRecordingsVersion() == 0)
	{
		R->PublishRobotCommand(TEXT("{\"cmd\":\"list_recordings\"}"));
	}
	if (!R->HasJointLimits())
	{
		R->PublishRobotCommand(TEXT("{\"cmd\":\"get_joint_limits\"}"));
	}
}

void URobotControlWidget::RefreshRecordingsList()
{
	if (!RecordingComboBox) return;
	ARobotVisualizer* R = ResolveRobot();
	if (!R) return;

	// Rebuild the combo only when the actor's recordings actually changed.
	if (R->GetRecordingsVersion() == CachedRecordingsVersion) return;
	CachedRecordingsVersion = R->GetRecordingsVersion();

	const FString Prev = RecordingComboBox->GetSelectedOption();
	RecordingComboBox->ClearOptions();
	for (const FRecordingInfo& Info : R->GetRecordings())
	{
		RecordingComboBox->AddOption(Info.Filename);
	}
	// Default to the most recent (top of the list, index 0).
	if (!Prev.IsEmpty() && RecordingComboBox->FindOptionIndex(Prev) != INDEX_NONE)
	{
		RecordingComboBox->SetSelectedOption(Prev);
	}
	else if (RecordingComboBox->GetOptionCount() > 0)
	{
		RecordingComboBox->SetSelectedIndex(0);
	}
}

void URobotControlWidget::RefreshReplayProgress()
{
	const bool bReplaying = (GetWorkerState() == TEXT("replaying"));
	ARobotVisualizer* R = Robot.Get();

	if (ReplayProgressBar)
	{
		float Pct = 0.0f;
		if (bReplaying && R && R->GetReplayTotal() > 0)
		{
			Pct = static_cast<float>(R->GetReplayIndex()) / static_cast<float>(R->GetReplayTotal());
		}
		ReplayProgressBar->SetPercent(Pct);
	}
	if (ReplayProgressText)
	{
		if (bReplaying && R)
		{
			const FString T = R->IsReplayApproaching()
				? TEXT("approaching...")
				: FString::Printf(TEXT("%d / %d"), R->GetReplayIndex(), R->GetReplayTotal());
			ReplayProgressText->SetText(FText::FromString(T));
		}
		else
		{
			ReplayProgressText->SetText(FText::GetEmpty());
		}
	}
}

// --- State getters ---

FString URobotControlWidget::GetWorkerState() const
{
	return Robot.IsValid() ? Robot->GetWorkerState() : TEXT("no robot");
}

bool URobotControlWidget::IsSyncActive() const { return Robot.IsValid() && Robot->IsSyncActive(); }
bool URobotControlWidget::IsRosBridgeConnected() const { return Robot.IsValid() && Robot->IsRosBridgeConnected(); }
bool URobotControlWidget::IsBridgeNodeAlive() const { return Robot.IsValid() && Robot->IsBridgeNodeAlive(); }
bool URobotControlWidget::IsWorkerAlive() const { return Robot.IsValid() && Robot->IsWorkerAlive(); }
bool URobotControlWidget::HasFollowerError() const { return Robot.IsValid() && Robot->HasFollowerError(); }
bool URobotControlWidget::HasLeaderError() const { return Robot.IsValid() && Robot->HasLeaderError(); }
bool URobotControlWidget::HasRobot() const { return Robot.IsValid(); }

// --- Joint monitoring passthroughs (Stage 3) ---

bool URobotControlWidget::HasJointLimits() const
{
	return Robot.IsValid() && Robot->HasJointLimits();
}

float URobotControlWidget::GetJointAngleDeg(FName JointName) const
{
	return Robot.IsValid() ? Robot->GetJointAngleDeg(JointName) : 0.0f;
}

float URobotControlWidget::GetJointRangeAlpha(FName JointName) const
{
	return Robot.IsValid() ? Robot->GetJointRangeAlpha(JointName) : 0.0f;
}

EJointWarn URobotControlWidget::GetJointWarnLevel(FName JointName) const
{
	return Robot.IsValid() ? Robot->GetJointWarnLevel(JointName) : EJointWarn::Normal;
}

TArray<float> URobotControlWidget::GetJointHistory(FName JointName) const
{
	return Robot.IsValid() ? Robot->GetJointHistory(JointName) : TArray<float>();
}

// --- Joint monitor panel (Stage 3b) ---

void URobotControlWidget::BuildJointRows()
{
	if (!JointListBox || !JointRowClass) { return; }

	JointListBox->ClearChildren();
	JointRows.Reset();

	// One row per joint, in the canonical worker/bridge order.
	for (const FName& JointName : ARobotVisualizer::GetJointNames())
	{
		UJointRowWidget* Row = CreateWidget<UJointRowWidget>(this, JointRowClass);
		if (!Row) { continue; }

		Row->InitJoint(JointName);
		JointListBox->AddChildToVerticalBox(Row);
		JointRows.Add(Row);
	}
}

void URobotControlWidget::RefreshJointMonitor()
{
	ARobotVisualizer* R = Robot.Get();
	if (!R) { return; }

	if (JointGraph) { JointGraph->SetRobot(R); }

	for (UJointRowWidget* Row : JointRows)
	{
		if (Row) { Row->Refresh(R); }
	}
}

// --- Event toasts (Stage 4a) ---

void URobotControlWidget::RefreshToasts()
{
	ARobotVisualizer* R = Robot.Get();
	if (!R || !Toast) { return; }

	// Worker state transitions (idle/syncing/recording/replaying).
	const FString WS = R->GetWorkerState();
	if (WS != LastToastWorkerState)
	{
		if (WS == TEXT("recording"))
			Toast->Push(TEXT("Recording started"), EToastLevel::Warning);
		else if (WS == TEXT("replaying"))
			Toast->Push(TEXT("Replay started"), EToastLevel::Info);
		else if (WS == TEXT("syncing"))
			Toast->Push(TEXT("Teleop sync ON"), EToastLevel::Info);
		else if (WS == TEXT("idle") && !LastToastWorkerState.IsEmpty())
			Toast->Push(TEXT("Idle"), EToastLevel::Info);
		LastToastWorkerState = WS;
	}

	// Follower device error edges.
	const bool bFollowerErr = R->HasFollowerError();
	if (bFollowerErr != bLastFollowerErr)
	{
		Toast->Push(bFollowerErr
			? TEXT("Follower: USB/Serial ERROR")
			: TEXT("Follower: USB restored"),
			bFollowerErr ? EToastLevel::Error : EToastLevel::Success);
		bLastFollowerErr = bFollowerErr;
	}

	// Leader device error edges.
	const bool bLeaderErr = R->HasLeaderError();
	if (bLeaderErr != bLastLeaderErr)
	{
		Toast->Push(bLeaderErr
			? TEXT("Leader: USB/Serial ERROR")
			: TEXT("Leader: USB restored"),
			bLeaderErr ? EToastLevel::Error : EToastLevel::Success);
		bLastLeaderErr = bLeaderErr;
	}

	// Recording-saved edge — RecordingsVersion bumps when a new list lands,
	// but the "saved" event is better keyed off the actor's last saved file.
	const FString Saved = R->GetLastSavedRecording();
	if (!Saved.IsEmpty() && Saved != LastToastSaved)
	{
		Toast->Push(FString::Printf(TEXT("Recording saved: %s"), *Saved),
			EToastLevel::Success);
		LastToastSaved = Saved;
	}
}