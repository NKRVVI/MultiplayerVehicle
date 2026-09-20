// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "CarPlayerController.generated.h"

class AMultiplayerVehiclePawn;
class UInputAction;
class UInputMappingContext;

UCLASS()
class MULTIPLAYERVEHICLE_API ACarPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACarPlayerController();
	void SetHealthHUDVisbility(bool bVisible);
	
	void RefreshHUD();

	virtual void OnRep_Pawn() override;

protected:
	virtual void SetupInputComponent() override;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Server-only: bound to the possessed vehicle's onvehicledead; throws the driver out of dead car. */
	void HandleVehicleDead();

	/** Bound to the possessed vehicle's onhealthupdate on the local controller, updates hud widget */
	void HandleHealthUpdate(float HealthPercent);

	/** Bound to the possessed vehicle's ongearchange on the local controller; updates gear widget*/
	void UpdateGear(int32 Gear);

	/** Binds the OnHealthUpdate and OnGearChange to NewPawn, unbinds the previous vehicle, and syncs the HUD. Local controllers only. */
	void BindHealthHUD(APawn* NewPawn);

	/** The vehicle whose OnHealthUpdate we are currently bound to. */
	TWeakObjectPtr<AMultiplayerVehiclePawn> BoundVehicle;

	/** asks the server to take us out of the car.*/
	void EnterExit(const FInputActionValue& Value);

	/** leaves the current session. */
	void Quit(const FInputActionValue& Value);

	/** Traces from the center of the screen to the world and returns the car closest to the hit location, or null if nothing was hit / no car exists. */
	AMultiplayerVehiclePawn* FindCarNearScreenCenter() const;

	/** Runs on the server: if the vehicle isn't possessed yet, possesses it and destroys the defaultpawn we were possessing */
	UFUNCTION(Server, Reliable)
	void ServerEnterVehicle(AMultiplayerVehiclePawn* Vehicle);

	/** Runs on the server: exits the vehicle and possesses a defaultpawn */
	UFUNCTION(Server, Reliable)
	void ServerExitVehicle();

	//imc for the controller inputs
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> ExitMappingContext;

	//imc for the vehicle inputs
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> EnterExitInputAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> QuitInputAction;

	/** Pawn spawned and possessed when leaving the car. Defaults to ADefaultPawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	TSubclassOf<APawn> ExitPawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float ExitSpawnOffset = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle")
	float GroundTraceDistance = 100000.f;
};
