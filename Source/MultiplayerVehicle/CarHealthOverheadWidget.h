// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CarHealthOverheadWidget.generated.h"

/**
 * Base class for the health widget shown above other players' cars. Make a Widget Blueprint
 * that reparents to this and override CarHealth to update the visuals (e.g. a progress bar).
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UCarHealthOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called with the car's current health as a 0-1 fraction of max. The C++ default does nothing; override in the Widget Blueprint. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Health")
	void UpdateCarHealth(float Percentage);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void UpdateCarName(const FName Name);
};
