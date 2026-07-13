#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "RecordingEntryWidget.generated.h"

/** Data backing one recording row in the ListView. */
UCLASS(BlueprintType)
class SO101_TWIN_API URecordingEntryData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "ROS|UI") FString Filename;
	UPROPERTY(BlueprintReadOnly, Category = "ROS|UI") int32 Frames = 0;
	UPROPERTY(BlueprintReadOnly, Category = "ROS|UI") float DurationSec = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "ROS|UI") FString RecordedAt;
};

/** Visual row for the recordings ListView. WBP_RecordingEntry reparents to this. */
UCLASS()
class SO101_TWIN_API URecordingEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> FilenameText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<class UTextBlock> MetaText;
};