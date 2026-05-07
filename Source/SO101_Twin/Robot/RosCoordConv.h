#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"

/**
 * Static helper functions for converting between ROS and Unreal Engine
 * coordinate systems.
 *
 * ROS:    meters, right-handed, Z-up, +X forward, +Y left
 * Unreal: centimeters, left-handed, Z-up, +X forward, +Y right
 *
 * The Y axis flips between the two systems.
 */
namespace RosCoordConv
{
	/** Convert ROS position (meters, right-handed) to UE position (cm, left-handed). */
	FORCEINLINE FVector RosToUePosition(double X_m, double Y_m, double Z_m)
	{
		return FVector(
			static_cast<float>(X_m * 100.0),
			static_cast<float>(-Y_m * 100.0),   // flip Y
			static_cast<float>(Z_m * 100.0)
		);
	}

	/** Convert ROS RPY (radians) to UE FRotator (degrees), accounting for handedness flip. */
	FORCEINLINE FRotator RosRpyToUeRotator(double Roll_rad, double Pitch_rad, double Yaw_rad)
	{
		// ROS RPY: rotation about X(roll), Y(pitch), Z(yaw)
		// UE FRotator: (Pitch, Yaw, Roll) in degrees
		// Y axis flip means: negate pitch and yaw
		return FRotator(
			static_cast<float>(FMath::RadiansToDegrees(-Pitch_rad)),  // UE Pitch = -ROS Pitch
			static_cast<float>(FMath::RadiansToDegrees(-Yaw_rad)),    // UE Yaw   = -ROS Yaw
			static_cast<float>(FMath::RadiansToDegrees(Roll_rad))     // UE Roll  =  ROS Roll
		);
	}

	/** Convert ROS quaternion to UE FQuat, accounting for handedness flip. */
	FORCEINLINE FQuat RosQuatToUe(double Qx, double Qy, double Qz, double Qw)
	{
		return FQuat(
			static_cast<float>(Qx),
			static_cast<float>(-Qy),   // flip Y
			static_cast<float>(Qz),
			static_cast<float>(-Qw)    // flip W
		).GetNormalized();
	}

	/**
	 * Convert a single joint angle from ROS (radians) to UE rotation
	 * about the local Z axis (degrees), accounting for Y axis flip.
	 *
	 * URDF joints in this robot all have axis="0 0 1" (Z axis).
	 * In the ROS coordinate frame, positive rotation about Z follows
	 * right-hand rule. In UE's left-hand frame, we negate to match.
	 */
	FORCEINLINE float RosJointAngleToUeDegrees(double AngleRadians)
	{
		return static_cast<float>(FMath::RadiansToDegrees(-AngleRadians));
	}

	/** Convert UE position (cm, left-handed) to ROS position (meters, right-handed). */
	FORCEINLINE void UeToRosPosition(const FVector& UePos, double& OutX, double& OutY, double& OutZ)
	{
		OutX =  UePos.X / 100.0;
		OutY = -UePos.Y / 100.0;   // flip Y
		OutZ =  UePos.Z / 100.0;
	}
}
