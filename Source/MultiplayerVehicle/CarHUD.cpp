// Fill out your copyright notice in the Description page of Project Settings.

#include "CarHUD.h"
#include "CarHealthHUDWidget.h"
#include "CarPlayerController.h"
#include "GameFramework/PlayerController.h"

void ACarHUD::BeginPlay()
{
	Super::BeginPlay();

	if (HealthWidgetClass)
	{
		HealthWidget = CreateWidget<UCarHealthHUDWidget>(GetOwningPlayerController(), HealthWidgetClass);
		if (HealthWidget)
		{
			HealthWidget->AddToViewport();

			// A listen-server host possesses its first pawn before this HUD exists, so the hud is not updated, so updating when hud is spawned
			if (ACarPlayerController* CarController = Cast<ACarPlayerController>(GetOwningPlayerController()))
			{
				CarController->RefreshHUD();
			}
		}
	}
}
