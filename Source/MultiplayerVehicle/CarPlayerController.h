// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CarPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Player controller for the project. Set as the game mode's PlayerControllerClass
 * (or use a Blueprint subclass of this) to have it used.
 */
UCLASS()
class MULTIPLAYERVEHICLE_API ACarPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACarPlayerController();

protected:
	virtual void SetupInputComponent() override;

	/** Input handler: asks the server to take us out of the car. */
	void OnExitInput(const FInputActionValue& Value);

	/** Runs on the server: spawns ExitPawnClass next to the current vehicle and possesses it. */
	UFUNCTION(Server, Reliable)
	void ServerExitVehicle();

	/** Input Mapping Context asset - assign in the editor. Holds the key mapping for ExitInputAction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> ExitMappingContext;

	/** Input Mapping Context asset - assign in the editor. Holds the key mappings for the vehicle pawn's actions. Added at a lower priority than ExitMappingContext. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	/** Input Action asset - assign in the editor. Triggers exiting the car. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> ExitInputAction;

	/** Pawn spawned and possessed when leaving the car. Defaults to ADefaultPawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TSubclassOf<APawn> ExitPawnClass;

	/** How far to the side of the car (cm) the exit pawn is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float ExitSpawnOffset = 250.f;
};
