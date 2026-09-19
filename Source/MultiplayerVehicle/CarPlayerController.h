// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CarPlayerController.generated.h"

class AMultiplayerVehiclePawn;
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
	void SetHealthHUDVisbility(bool bVisible);

	/** Local-only: pushes the current pawn's state (visibility, health, gear) to the HUD widget. Call once the widget exists. */
	void RefreshHUD();

	virtual void OnRep_Pawn() override;

protected:
	virtual void SetupInputComponent() override;

	// Server-only: bind to / unbind from a vehicle's OnVehicleDead while we possess it.
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Server-only: bound to the possessed vehicle's OnVehicleDead; throws the driver out of the wrecked car. */
	void HandleVehicleDead();

	/** Bound to the possessed vehicle's OnHealthUpdate on the local controller; pushes the health to the HUD widget. */
	void HandleHealthUpdate(float HealthPercent);

	/** Bound to the possessed vehicle's OnGearChange on the local controller; pushes the gear to the HUD widget. */
	void UpdateGear(int32 Gear);

	/** Moves the OnHealthUpdate and OnGearChange bindings to NewPawn (unbinding the previous vehicle) and syncs the HUD. Local controllers only. */
	void BindHealthHUD(APawn* NewPawn);

	/** The vehicle whose OnHealthUpdate we are currently bound to. */
	TWeakObjectPtr<AMultiplayerVehiclePawn> BoundVehicle;

	/** Input handler: asks the server to take us out of the car. */
	void EnterExit(const FInputActionValue& Value);

	/** Traces from the center of the screen to the world and returns the car closest to the hit location, or null if nothing was hit / no car exists. */
	AMultiplayerVehiclePawn* FindCarNearScreenCenter() const;

	/** Runs on the server: if the vehicle isn't possessed yet, possesses it and destroys the pawn we were on foot with. */
	UFUNCTION(Server, Reliable)
	void ServerEnterVehicle(AMultiplayerVehiclePawn* Vehicle);

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
	TObjectPtr<UInputAction> EnterExitInputAction;

	/** Pawn spawned and possessed when leaving the car. Defaults to ADefaultPawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TSubclassOf<APawn> ExitPawnClass;

	/** How far to the side of the car (cm) the exit pawn is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float ExitSpawnOffset = 250.f;

	/** How far (cm) the screen-center trace reaches when looking for the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float GroundTraceDistance = 100000.f;
};
