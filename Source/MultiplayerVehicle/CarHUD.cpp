// Fill out your copyright notice in the Description page of Project Settings.

#include "CarHUD.h"
#include "CarHealthHUDWidget.h"
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
		}
	}
}
