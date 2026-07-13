#include "RecordingEntryWidget.h"
#include "Components/TextBlock.h"

void URecordingEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	URecordingEntryData* Data = Cast<URecordingEntryData>(ListItemObject);
	if (!Data) return;

	if (FilenameText)
	{
		FilenameText->SetText(FText::FromString(Data->Filename));
	}
	if (MetaText)
	{
		const FString Meta = FString::Printf(TEXT("%d frames  |  %.1fs"), Data->Frames, Data->DurationSec);
		MetaText->SetText(FText::FromString(Meta));
	}
}