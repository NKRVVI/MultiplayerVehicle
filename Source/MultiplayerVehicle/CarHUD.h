// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CarHUD.generated.h"

class UCarHealthHUDWidget;

UCLASS()
class MULTIPLAYERVEHICLE_API ACarHUD : public AHUD
{
	GENERATED_BODY()

public:
	FORCEINLINE UCarHealthHUDWidget* GetHealthWidget() const { return HealthWidget; }

protected:
	virtual void BeginPlay() override;

	/** Widget Blueprint (reparented to UCarHealthHUDWidget) shown on screen. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCarHealthHUDWidget> HealthWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UCarHealthHUDWidget> HealthWidget;
};
