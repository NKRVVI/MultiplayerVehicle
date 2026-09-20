// Fill out your copyright notice in the Description page of Project Settings.

#include "MultiplayerVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "CarHealthOverheadWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

AMultiplayerVehiclePawn::AMultiplayerVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	//for cars that are preplaced on the server level
	AutoPossessAI = EAutoPossessAI::Disabled;
	
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	CarMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarMesh"));
	SetRootComponent(CarMesh);

	CarMesh->SetCollisionProfileName(TEXT("Vehicle"));
	CarMesh->SetSimulatePhysics(true);
	CarMesh->SetGenerateOverlapEvents(true);
	CarMesh->SetNotifyRigidBodyCollision(true);
	CarMesh->BodyInstance.bOverrideMass = true;
	CarMesh->BodyInstance.SetMassOverride(1500.f);

	VehicleMovementComponent = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovementComponent"));
	VehicleMovementComponent->SetUpdatedComponent(CarMesh);
	
	//camera, positioned behind the car

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CarMesh);
	SpringArm->TargetArmLength = 600.f;
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	SpringArm->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = true;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 3.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 5.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// --- Overhead widget, only visible to machines that don't control this car ---
	// Hidden by default; UpdateHealthWidget() shows it once we know the controller.

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(CarMesh);
	OverheadWidget->SetVisibility(false);
}

void AMultiplayerVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	// Collisions are only detected on the server; clients are told about them via MulticastDrawImpact.
	if (HasAuthority())
	{
		CarMesh->SetAllBodiesNotifyRigidBodyCollision(true);
		CarMesh->OnComponentHit.AddDynamic(this, &AMultiplayerVehiclePawn::OnCarHit);
	}

	VehicleMovementComponent->SetUseAutomaticGears(false);
	VehicleMovementComponent->SetTargetGear(0.f, true);

	UpdateHealthWidget();
}

void AMultiplayerVehiclePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UpdateHealthWidget();
	OnGearChange.Broadcast(GetTargetGear());
}

void AMultiplayerVehiclePawn::UnPossessed()
{
	Super::UnPossessed();

	// The controller is cleared by now, so this car is no longer locally controlled and shows its overhead widget again.
	UpdateHealthWidget();
}

void AMultiplayerVehiclePawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	
	// driver exits and this car is no longer locally controlled, so it goes back to the overhead widget.
	UpdateHealthWidget();
	if (GetController())
	{
		OnGearChange.Broadcast(GetTargetGear());
	}
}

void AMultiplayerVehiclePawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// On clients the pawn's PlayerState arrives after BeginPlay so the name has to be updated to the overhead widget when it shows up.
	UpdateHealthWidget();
}

void AMultiplayerVehiclePawn::UpdateHealthWidget()
{
	if (IsLocallyControlled())
	{
		// The HUD belongs to the local controller, which listens to OnHealthUpdate.
		OverheadWidget->SetVisibility(false);
		OnHealthUpdate.Broadcast(GetHealthPercent());
		return;
	}

	//dead cars don't have a health bar
	if (bDead)
	{
		OverheadWidget->SetVisibility(false);
		return;
	}

	OverheadWidget->SetVisibility(true);
	if (UCarHealthOverheadWidget* HealthWidget = Cast<UCarHealthOverheadWidget>(OverheadWidget->GetUserWidgetObject()))
	{
		HealthWidget->UpdateCarHealth(GetHealthPercent());
		if (const APlayerState* CarPlayerState = GetPlayerState())
		{
			HealthWidget->UpdateCarName(FName(*CarPlayerState->GetPlayerName()));
		}
		else
		{
			HealthWidget->UpdateCarName(" ");
		}
	}
}

void AMultiplayerVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMultiplayerVehiclePawn, Health);
	DOREPLIFETIME(AMultiplayerVehiclePawn, bDead);
}

void AMultiplayerVehiclePawn::RepNotify_UpdateHealth()
{
	//updates the widget of every non-authoritative pawn
	UpdateHealthWidget();
}

void AMultiplayerVehiclePawn::OnDead()
{
	OverheadWidget->SetVisibility(false);
	
	VehicleMovementComponent->SetSteeringInput(0.f);
	VehicleMovementComponent->SetThrottleInput(0.f);
	VehicleMovementComponent->SetBrakeInput(0.f);
}

void AMultiplayerVehiclePawn::DecrementHealth(float Amount)
{
	if (!HasAuthority())
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	RepNotify_UpdateHealth();

	if (!bDead && Health <= 0.f)
	{
		bDead = true;
		OnDead();
		OnVehicleDead.Broadcast();
	}
}

void AMultiplayerVehiclePawn::OnCarHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{

	// Only car-vs-car hits do damage. this function calculates the damage this car does on otheractor
	AMultiplayerVehiclePawn* Recipient = Cast<AMultiplayerVehiclePawn>(OtherActor);
	if (!Recipient)
	{
		return;
	}

	const FVector Velocity = CarMesh->GetPhysicsLinearVelocity();
	const float Speed = Velocity.Size();
	if (Speed < KINDA_SMALL_NUMBER)
	{
		return;
	}

	// How directly this car is heading at the hit point
	const FVector ToHitPoint = (Hit.ImpactPoint - CarMesh->GetCenterOfMass()).GetSafeNormal();
	const float Alignment = FMath::Abs(FVector::DotProduct(Velocity / Speed, ToHitPoint));
	if (Alignment <= 0.f)
	{
		return;
	}

	// Contact events fire every physics frame while cars scrape or bounce, so only the first hit in the cooldown window damages the recipient.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - Recipient->LastCollisionDamageTime < Recipient->CollisionDamageCooldown)
	{
		return;
	}
	Recipient->LastCollisionDamageTime = Now;
	
	const float NormalisedSpeed = FMath::Clamp(Speed / MaxDamageSpeed, 0.f, 1.f);
	Recipient->DecrementHealth(Alignment * MaxCollisionDamage * NormalisedSpeed);
	MulticastDrawImpact(Hit.ImpactPoint);
}

void AMultiplayerVehiclePawn::MulticastDrawImpact_Implementation(FVector_NetQuantize ImpactPoint)
{
	DrawDebugSphere(GetWorld(), ImpactPoint, 25.f, 12, FColor::Red, false, 2.f);
}

void AMultiplayerVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// The key-to-action mapping context is added by the player controller (ACarPlayerController). 
	// this pawn only binds the actions to its handler functions.
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::Triggered, this, &ThisClass::Steer);
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::None, this, &ThisClass::Steer);

		EnhancedInputComponent->BindAction(AccelerateAction, ETriggerEvent::Triggered, this, &ThisClass::Accelerate);
		EnhancedInputComponent->BindAction(AccelerateAction, ETriggerEvent::Completed, this, &ThisClass::ReleaseAccelerate);

		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ThisClass::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ThisClass::ReleaseBrake);

		EnhancedInputComponent->BindAction(GearUpAction, ETriggerEvent::Started, this, &ThisClass::GearUp);
		EnhancedInputComponent->BindAction(GearDownAction, ETriggerEvent::Started, this, &ThisClass::GearDown);
	}
}

void AMultiplayerVehiclePawn::Steer(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetSteeringInput(Value.Get<float>());
}

void AMultiplayerVehiclePawn::Accelerate(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();

	VehicleMovementComponent->SetThrottleInput(AxisValue);
}

void AMultiplayerVehiclePawn::ReleaseAccelerate(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetThrottleInput(0.f);
}

void AMultiplayerVehiclePawn::Brake(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetBrakeInput(HarshBrakeStrength);
}

void AMultiplayerVehiclePawn::ReleaseBrake(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetBrakeInput(0.f);
}

void AMultiplayerVehiclePawn::GearUp(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() + 1, true);
	OnGearChange.Broadcast(GetTargetGear());
}

void AMultiplayerVehiclePawn::GearDown(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() - 1, true);
	OnGearChange.Broadcast(GetTargetGear());
}

int32 AMultiplayerVehiclePawn::GetTargetGear() const
{
	return VehicleMovementComponent->GetTargetGear();
}
