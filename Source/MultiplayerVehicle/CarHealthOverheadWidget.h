// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CarHealthOverheadWidget.generated.h"

/**
 * Base class for the health widget shown above other players' cars.
 */
UCLASS()
class MULTIPLAYERVEHICLE_API UCarHealthOverheadWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle|Health")
	void UpdateCarHealth(float Percentage);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void UpdateCarName(const FName Name);
};
