// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "MultiplayerVehiclePawn.generated.h"

class USkeletalMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UWidgetComponent;

//Broadcast on the server when a vehicle's health reaches 0
DECLARE_MULTICAST_DELEGATE(FOnVehicleDead);

//Broadcast on the machine controlling a vehicle whenever its health changes
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealthUpdate, float);

//Broadcast on the machine controlling a vehicle whenever its gear may have changed
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGearChange, int32);

/**
 * Drivable car pawn.
 *
 * Steering / throttle / brake / reverse / gear-shift input is bound via Enhanced Input.
 */
UCLASS()
class MULTIPLAYERVEHICLE_API AMultiplayerVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	AMultiplayerVehiclePawn();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Steer(const FInputActionValue& Value);
	void Accelerate(const FInputActionValue& Value);
	void ReleaseAccelerate(const FInputActionValue& Value);
	void Brake(const FInputActionValue& Value);
	void ReleaseBrake(const FInputActionValue& Value);
	void GearUp(const FInputActionValue& Value);
	void GearDown(const FInputActionValue& Value);

	/** Runs on all machines (called from the server); draws a debug sphere at the impact point. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawImpact(FVector_NetQuantize ImpactPoint);

	/** Server-only: bound to CarMesh's hit event on authority, then tells everyone to draw the impact. */
	UFUNCTION()
	void OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> CarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	//overhead health widget component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> OverheadWidget;

	//if locally controlled update hud widget, otherwise overhead widget
	void UpdateHealthWidget();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AccelerateAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearUpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> GearDownAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
	float HarshBrakeStrength = 0.2f;

	UPROPERTY(ReplicatedUsing = RepNotify_UpdateHealth, EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Health")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Health")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnDead, BlueprintReadOnly, Category = "Vehicle|Health")
	bool bDead = false;

	//RepNotify for bDead
	UFUNCTION()
	void OnDead();

	//RepNotify for Health
	UFUNCTION()
	void RepNotify_UpdateHealth();

	//maximum possible damage in a single hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Health")
	float MaxCollisionDamage = 50.f;

	//speed at which damage is maximised, not accounting for angle of hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Health")
	float MaxDamageSpeed = 2000.f;

	//Seconds after this car takes collision damage during which further collision damage to it is ignored
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Health", meta = (ClampMin = "0.0"))
	float CollisionDamageCooldown = 0.75f;

	// server only : last dime a collision damage was done
	float LastCollisionDamageTime = -BIG_NUMBER;

	//Server-only: lowers Health by Amount 
	void DecrementHealth(float Amount);

public:
	//Server-only: fires once when Health drops to 0.
	FOnVehicleDead OnVehicleDead;

	//Broadcast from UpdateHealthWidget when this pawn is locally controlled.
	FOnHealthUpdate OnHealthUpdate;

	//Broadcast on gear up/down
	FOnGearChange OnGearChange;

	//The gear the vehicle is shifting to
	int32 GetTargetGear() const;

	FORCEINLINE float GetHealthPercent() const { return Health / MaxHealth; }
	FORCEINLINE bool IsDead() const { return bDead; }
	FORCEINLINE USkeletalMeshComponent* GetCarMesh() const { return CarMesh; }
	FORCEINLINE UChaosWheeledVehicleMovementComponent* GetVehicleMovementComponent() const { return VehicleMovementComponent; }
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
};
