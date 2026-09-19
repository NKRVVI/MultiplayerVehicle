// Fill out your copyright notice in the Description page of Project Settings.


#include "CarPlayerController.h"
#include "MultiplayerVehiclePawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"

ACarPlayerController::ACarPlayerController()
{
	ExitPawnClass = ADefaultPawn::StaticClass();
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
		if (ExitInputAction)
		{
			EnhancedInputComponent->BindAction(ExitInputAction, ETriggerEvent::Started, this, &ACarPlayerController::OnExitInput);
		}
	}
}

void ACarPlayerController::OnExitInput(const FInputActionValue& Value)
{
	ServerExitVehicle();
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
	}
}
