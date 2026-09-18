// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "VehicleWheelRear.generated.h"

/**
 * Driven, handbrake-affected rear wheel physics setup (no wheel mesh/bone - wheels are invisible, the car body is one static mesh).
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UVehicleWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UVehicleWheelRear();
};
