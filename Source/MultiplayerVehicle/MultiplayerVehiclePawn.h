// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MultiplayerVehiclePawn.generated.h"

class USkeletalMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;

/**
 * Drivable car pawn. The car body is a skeletal mesh (a Skeletal Mesh
 * Component is required for UChaosVehicleMovementComponent to simulate at
 * all - a static mesh chassis silently never creates a physics state)
 * driven by a Chaos wheeled vehicle movement component. No AnimInstance is
 * assigned, so the mesh renders in its bind pose and the wheels don't
 * visually rotate/steer even though the physics wheels are real.
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

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Steer(const FInputActionValue& Value);
	void MoveForward(const FInputActionValue& Value);
	void CoastBrake(const FInputActionValue& Value);
	void Brake(const FInputActionValue& Value);
	void GearUp(const FInputActionValue& Value);
	void GearDown(const FInputActionValue& Value);

	/** Runs on all machines (called from the server); draws a debug sphere at the impact point. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawImpact(FVector_NetQuantize ImpactPoint);

	/** Server-only: bound to CarMesh's hit event on authority, then tells everyone to draw the impact. */
	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** The car body. Root component and sole visual/physical mesh - assign your car mesh here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> CarMesh;

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

	/** Applies the brake directly, regardless of whether the car is currently moving forward or backward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearUpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearDownAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float HarshBrakeStrength = 0.2f;

	/** Vehicle health. Set on the server only; clients get it via replication and RepNotify_UpdateHealth runs when it arrives. */
	UPROPERTY(ReplicatedUsing = RepNotify_UpdateHealth, EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Health")
	float Health = 100.f;

	/** RepNotify for Health. Only fires on clients - on the server, call it manually after changing Health. */
	UFUNCTION()
	void RepNotify_UpdateHealth();

	/** Damage dealt to another car by a perfectly head-on hit at MaxDamageSpeed or above. Scaled down by angle and speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Health")
	float MaxCollisionDamage = 50.f;

	/** Speed (cm/s) at which a hit does full damage; speed is normalised against this and clamped to 0-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Health")
	float MaxDamageSpeed = 2000.f;

	/** Server-only: lowers Health by Amount (clamped at 0) and runs the RepNotify locally, since it doesn't fire on the server. */
	void DecrementHealth(float Amount);

public:
	FORCEINLINE USkeletalMeshComponent* GetCarMesh() const { return CarMesh; }
	FORCEINLINE UChaosWheeledVehicleMovementComponent* GetVehicleMovementComponent() const { return VehicleMovementComponent; }
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
};
