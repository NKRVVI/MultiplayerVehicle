// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CarHealthHUDWidget.generated.h"

/**
 * Base class for the on-screen health widget of the car this machine controls. 
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UCarHealthHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Health")
	void UpdateCarHealth(float Percentage);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Gear")
	void UpdateGear(int32 Gear);
};
