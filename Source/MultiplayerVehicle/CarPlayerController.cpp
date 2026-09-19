// Fill out your copyright notice in the Description page of Project Settings.


#include "CarPlayerController.h"
#include "MultiplayerVehiclePawn.h"
#include "CarHUD.h"
#include "CarHealthHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"

ACarPlayerController::ACarPlayerController()
{
	ExitPawnClass = ADefaultPawn::StaticClass();
}

void ACarPlayerController::SetHealthHUDVisbility(bool bVisible)
{
	if (IsLocalController())
	{
		if (const ACarHUD* CarHUD = GetHUD<ACarHUD>())
		{
			if (UCarHealthHUDWidget* HUDWidget = CarHUD->GetHealthWidget())
			{
				HUDWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
	}
}

void ACarPlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	// On foot there is no car to show health for, so take the HUD widget off the screen.
	if (Cast<ADefaultPawn>(GetPawn()))
	{
		SetHealthHUDVisbility(false);
	}
	else
	{
		SetHealthHUDVisbility(true);
	}
}

void ACarPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (AMultiplayerVehiclePawn* Vehicle = Cast<AMultiplayerVehiclePawn>(InPawn))
	{
		Vehicle->OnVehicleDead.AddUObject(this, &ACarPlayerController::HandleVehicleDead);
	}
}

void ACarPlayerController::OnUnPossess()
{
	// Grab the pawn first: Super clears it.
	if (AMultiplayerVehiclePawn* Vehicle = Cast<AMultiplayerVehiclePawn>(GetPawn()))
	{
		Vehicle->OnVehicleDead.RemoveAll(this);
	}

	Super::OnUnPossess();
}

void ACarPlayerController::HandleVehicleDead()
{
	// Already on the server, so this runs the implementation directly.
	ServerExitVehicle();
}

void ACarPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Only called for local controllers, so this never runs for other players' controllers on the server.
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			// Both contexts stay active for the controller's lifetime. The pawns only bind actions from VehicleMappingContext;
			// the exit context has the higher priority so it wins any key conflict.
			if (VehicleMappingContext)
			{
				Subsystem->AddMappingContext(VehicleMappingContext, 0);
			}
			if (ExitMappingContext)
			{
				Subsystem->AddMappingContext(ExitMappingContext, 2);
			}
		}
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (EnterExitInputAction)
		{
			EnhancedInputComponent->BindAction(EnterExitInputAction, ETriggerEvent::Started, this, &ACarPlayerController::EnterExit);
		}
	}
}

void ACarPlayerController::EnterExit(const FInputActionValue& Value)
{
	if (Cast<ADefaultPawn>(GetPawn()))
	{
		// On foot: look for the car nearest to where the player is aiming on the ground.
		if (AMultiplayerVehiclePawn* NearestCar = FindCarNearScreenCenter())
		{
			ServerEnterVehicle(NearestCar);
		}
		return;
	}

	ServerExitVehicle();
}

AMultiplayerVehiclePawn* ACarPlayerController::FindCarNearScreenCenter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	FVector TraceStart;
	FVector TraceDirection;
	if (!DeprojectScreenPositionToWorld(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f, TraceStart, TraceDirection))
	{
		return nullptr;
	}

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceStart + TraceDirection * GroundTraceDistance, ECC_Visibility))
	{
		return nullptr;
	}

	AMultiplayerVehiclePawn* NearestCar = nullptr;
	float NearestDistSquared = FLT_MAX;
	for (TActorIterator<AMultiplayerVehiclePawn> It(World); It; ++It)
	{
		const float DistSquared = FVector::DistSquared(It->GetActorLocation(), Hit.ImpactPoint);
		if (DistSquared < NearestDistSquared)
		{
			NearestDistSquared = DistSquared;
			NearestCar = *It;
		}
	}
	return NearestCar;
}

void ACarPlayerController::ServerEnterVehicle_Implementation(AMultiplayerVehiclePawn* Vehicle)
{
	// Only take a car nobody is driving, and only from on foot, so the pawn we destroy below is always the exit pawn.
	ADefaultPawn* DefaultPawn = Cast<ADefaultPawn>(GetPawn());
	if (!Vehicle || !DefaultPawn || Vehicle->GetController())
	{
		return;
	}

	// Possess unpossesses the default pawn first.
	Possess(Vehicle);
	if (GetPawn() == Vehicle)
	{
		DefaultPawn->Destroy();
		SetHealthHUDVisbility(true);
	}
}

void ACarPlayerController::ServerExitVehicle_Implementation()
{
	// Only leave from a vehicle, so pressing the key while on foot doesn't spawn extra pawns.
	AMultiplayerVehiclePawn* Vehicle = Cast<AMultiplayerVehiclePawn>(GetPawn());
	UWorld* World = GetWorld();
	if (!Vehicle || !World || !ExitPawnClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FVector PawnSpawnLocation = Vehicle->GetActorLocation() + Vehicle->GetActorRightVector() * ExitSpawnOffset;
	const FRotator SpawnRotation(0.f, Vehicle->GetActorRotation().Yaw, 0.f);

	if (APawn* NewPawn = World->SpawnActor<APawn>(ExitPawnClass, PawnSpawnLocation, SpawnRotation, Params))
	{
		// Possess unpossesses the vehicle first; the vehicle stays in the world.
		Possess(NewPawn);
		SetHealthHUDVisbility(false);
	}
}
