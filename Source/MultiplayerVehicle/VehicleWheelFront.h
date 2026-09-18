// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "VehicleWheelFront.generated.h"

/**
 * Steered, non-driven front wheel physics setup (no wheel mesh/bone - wheels are invisible, the car body is one static mesh).
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UVehicleWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UVehicleWheelFront();
};
