// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MultiplayerVehiclePawn.generated.h"

class UStaticMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;

/**
 * Drivable car pawn. The car body is a single static mesh (no separate wheel
 * meshes/bones) driven by a Chaos wheeled vehicle movement component, which
 * simulates the wheels invisibly at fixed offsets from the body.
 *
 * Steering / throttle / brake / reverse / gear-shift input is bound via
 * Enhanced Input. VehicleMappingContext and the IA_* actions below are
 * assigned in the editor (e.g. on a Blueprint subclass) - create the Input
 * Mapping Context and Input Action assets there, with the key mappings set
 * on the mapping context asset; this class only binds actions to functions.
 */
UCLASS()
class MULTIPLAYERVEHICLE_API AMultiplayerVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMultiplayerVehiclePawn();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Steer(const FInputActionValue& Value);
	void MoveForward(const FInputActionValue& Value);
	void GearUp(const FInputActionValue& Value);
	void GearDown(const FInputActionValue& Value);

	/** The car body. Root component and sole visual/physical mesh - assign your car mesh here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CarMesh;

	/** Drives CarMesh: steering, throttle, brake, gears. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovementComponent;

	/** Booms the chase camera out behind the car. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	/** Chase camera, positioned behind the car via SpringArm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	/** Input Mapping Context asset - assign in the editor. Holds the key-to-action mappings for the actions below. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearUpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearDownAction;

	/** Below this forward speed (cm/s), holding the brake/reverse input shifts into reverse instead of just braking. */
	UPROPERTY(EditAnywhere, Category = "Vehicle")
	float ReverseSpeedThreshold = 15.f;

public:
	FORCEINLINE UStaticMeshComponent* GetCarMesh() const { return CarMesh; }
	FORCEINLINE UChaosWheeledVehicleMovementComponent* GetVehicleMovementComponent() const { return VehicleMovementComponent; }
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
};
