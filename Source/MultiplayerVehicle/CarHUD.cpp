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

			// A listen-server host possesses its first pawn before this HUD exists, so the pawn's health and gear
			// were pushed to a null widget. Catch the widget up now.
			if (ACarPlayerController* CarController = Cast<ACarPlayerController>(GetOwningPlayerController()))
			{
				CarController->RefreshHUD();
			}
		}
	}
}
