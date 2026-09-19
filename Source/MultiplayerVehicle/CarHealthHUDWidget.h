// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CarHealthHUDWidget.generated.h"

/**
 * Base class for the on-screen health widget of the car this machine controls. Make a Widget
 * Blueprint that reparents to this and override UpdateCarHealth to update the visuals (e.g. a progress bar).
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UCarHealthHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called with the car's current health as a 0-1 fraction of max. The C++ default does nothing; override in the Widget Blueprint. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Health")
	void UpdateCarHealth(float Percentage);

	/** Called with the car's current gear. The C++ default does nothing; override in the Widget Blueprint. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Gear")
	void UpdateGear(int32 Gear);
};
