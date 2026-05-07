#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Log category for the rosbridge WebSocket client and all related
 * ROS message handling code in this module.
 *
 * Usage:
 *   UE_LOG(LogRosBridge, Log, TEXT("Connected to %s"), *Url);
 *   UE_LOG(LogRosBridge, Warning, TEXT("Reconnect attempt %d"), Attempt);
 *   UE_LOG(LogRosBridge, Error, TEXT("JSON parse failed: %s"), *RawPayload);
 */
DECLARE_LOG_CATEGORY_EXTERN(LogRosBridge, Log, All);