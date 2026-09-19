// Fill out your copyright notice in the Description page of Project Settings.

#include "MultiplayerVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "VehicleWheelFront.h"
#include "VehicleWheelRear.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "CarHealthOverheadWidget.h"
#include "CarHealthHUDWidget.h"
#include "CarHUD.h"
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

namespace
{
	// Placeholder chassis mesh - SportsCar sample mesh bundled with the ChaosModularVehicleExamples
	// plugin (content-only, enabled in the .uproject). Swap for your own car mesh once you have one.
	// No AnimInstance is assigned, so it renders in its bind pose - wheels don't visually rotate/steer.
	const TCHAR* PlaceholderMeshPath = TEXT("/ChaosModularVehicleExamples/Models/SportsCar/SKM_SportsCar.SKM_SportsCar");

	// Bone names on the placeholder mesh's skeleton - used to position the (invisible-motion) physics wheels.
	const FName WheelBoneFrontLeft(TEXT("Phys_Wheel_FL"));
	const FName WheelBoneFrontRight(TEXT("Phys_Wheel_FR"));
	const FName WheelBoneRearLeft(TEXT("Phys_Wheel_BL"));
	const FName WheelBoneRearRight(TEXT("Phys_Wheel_BR"));
}

AMultiplayerVehiclePawn::AMultiplayerVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true; // required for the NetMulticast impact RPC

	CarMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CarMesh"));
	SetRootComponent(CarMesh);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> PlaceholderMeshFinder(PlaceholderMeshPath);
	if (PlaceholderMeshFinder.Succeeded())
	{
		CarMesh->SetSkeletalMeshAsset(PlaceholderMeshFinder.Object);
	}

	CarMesh->SetCollisionProfileName(TEXT("Vehicle"));
	CarMesh->SetSimulatePhysics(true);
	CarMesh->SetGenerateOverlapEvents(true);
	CarMesh->SetNotifyRigidBodyCollision(true); // "Simulation Generates Hit Events" - required for OnComponentHit
	CarMesh->BodyInstance.bOverrideMass = true;
	CarMesh->BodyInstance.SetMassOverride(1500.f);

	VehicleMovementComponent = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovementComponent"));
	VehicleMovementComponent->SetUpdatedComponent(CarMesh);

	VehicleMovementComponent->WheelSetups.SetNum(4);

	VehicleMovementComponent->WheelSetups[0].WheelClass = UVehicleWheelFront::StaticClass();
	VehicleMovementComponent->WheelSetups[0].BoneName = WheelBoneFrontLeft;

	VehicleMovementComponent->WheelSetups[1].WheelClass = UVehicleWheelFront::StaticClass();
	VehicleMovementComponent->WheelSetups[1].BoneName = WheelBoneFrontRight;

	VehicleMovementComponent->WheelSetups[2].WheelClass = UVehicleWheelRear::StaticClass();
	VehicleMovementComponent->WheelSetups[2].BoneName = WheelBoneRearLeft;

	VehicleMovementComponent->WheelSetups[3].WheelClass = UVehicleWheelRear::StaticClass();
	VehicleMovementComponent->WheelSetups[3].BoneName = WheelBoneRearRight;

	// --- Chase camera, positioned behind the car ---

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
	// Hidden by default; UpdateOverheadWidgetVisibility() shows it once we know the controller.

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(CarMesh);
	OverheadWidget->SetVisibility(false);

	// SteerAction, MoveForwardAction, BrakeAction, GearUpAction and GearDownAction are
	// assigned in the editor (see the header) - nothing to construct here.
}

void AMultiplayerVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Black, TEXT("Current Health is ") + FString::FromInt(Health));
	}
}

void AMultiplayerVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	// Collisions are only detected on the server; clients are told about them via MulticastDrawImpact.
	if (HasAuthority())
	{
		// A skeletal mesh's hit-event flag lives on each physics-asset body, not on the component's BodyInstance,
		// and those bodies only exist once physics state is created - so this can't be done in the constructor.
		CarMesh->SetAllBodiesNotifyRigidBodyCollision(true);
		CarMesh->OnComponentHit.AddDynamic(this, &AMultiplayerVehiclePawn::OnCarHit);
	}

	VehicleMovementComponent->SetUseAutomaticGears(false);
	VehicleMovementComponent->SetTargetGear(0.f, true);

	UpdateOverheadWidgetVisibility();
}

void AMultiplayerVehiclePawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	UpdateOverheadWidgetVisibility();
	if (IsLocallyControlled())
	{
		OverheadWidget->SetVisibility(false);
	}
}

void AMultiplayerVehiclePawn::UnPossessed()
{
	Super::UnPossessed();

	// The controller is cleared by now, so this car is no longer locally controlled and shows its overhead widget again.
	UpdateOverheadWidgetVisibility();
}

void AMultiplayerVehiclePawn::OnRep_Controller()
{
	Super::OnRep_Controller();

	// On the owning client the controller usually arrives after BeginPlay, so IsLocallyControlled() was still false then.
	UpdateOverheadWidgetVisibility();
	
	if (IsLocallyControlled())
	{
		OverheadWidget->SetVisibility(false);
	}
}

void AMultiplayerVehiclePawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// On clients the pawn's PlayerState arrives after BeginPlay (and other clients never get this car's controller),
	// so the name has to be pushed to the overhead widget when it shows up.
	UpdateOverheadWidgetVisibility();
	
	if (IsLocallyControlled())
	{
		OverheadWidget->SetVisibility(false);
	}
}

void AMultiplayerVehiclePawn::UpdateOverheadWidgetVisibility()
{
	// A wreck never shows its overhead widget again, even when the driver is kicked out and the car becomes uncontrolled.
	if (bDead)
	{
		OverheadWidget->SetVisibility(false);
		return;
	}

	if (IsLocallyControlled())
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (const ACarHUD* CarHUD = PlayerController->GetHUD<ACarHUD>())
			{
				if (UCarHealthHUDWidget* HUDWidget = CarHUD->GetHealthWidget())
				{
					HUDWidget->UpdateCarHealth(Health / MaxHealth);
				}
			}
		}
	}
	else
	{
		OverheadWidget->SetVisibility(true);
		if (UCarHealthOverheadWidget* HealthWidget = Cast<UCarHealthOverheadWidget>(OverheadWidget->GetUserWidgetObject()))
		{
			HealthWidget->UpdateCarHealth(Health / MaxHealth);
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
}

void AMultiplayerVehiclePawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMultiplayerVehiclePawn, Health);
	DOREPLIFETIME(AMultiplayerVehiclePawn, bDead);
}

void AMultiplayerVehiclePawn::RepNotify_UpdateHealth()
{
	// Every client runs this for every pawn it can see: the pawn this machine controls updates the HUD
	// widget, every other pawn pushes its health to the overhead widget.
	if (IsLocallyControlled())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange, FString::Printf(TEXT("Health: %.0f"), Health));

		if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			if (const ACarHUD* CarHUD = PlayerController->GetHUD<ACarHUD>())
			{
				if (UCarHealthHUDWidget* HUDWidget = CarHUD->GetHealthWidget())
				{
					HUDWidget->UpdateCarHealth(Health / MaxHealth);
				}
			}
		}
	}
	else if (UCarHealthOverheadWidget* HealthWidget = Cast<UCarHealthOverheadWidget>(OverheadWidget->GetUserWidgetObject()))
	{
		HealthWidget->UpdateCarHealth(Health / MaxHealth);
	}
}

void AMultiplayerVehiclePawn::OnDead()
{
	OverheadWidget->SetVisibility(false);
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

	// Only car-vs-car hits do damage. This event describes the damage *this* car deals; the other car's own
	// hit event covers the reverse direction, so each car is only ever damaged as the recipient.
	AMultiplayerVehiclePawn* Recipient = Cast<AMultiplayerVehiclePawn>(OtherActor);
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Emerald, OtherActor->GetName());
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

	// How directly this car is heading at the hit point: 1 = straight at it, 0 = sideways, <0 = moving away.
	const FVector ToHitPoint = (Hit.ImpactPoint - CarMesh->GetCenterOfMass()).GetSafeNormal();
	const float Alignment = FMath::Abs(FVector::DotProduct(Velocity / Speed, ToHitPoint));
	if (Alignment <= 0.f)
	{
		return;
	}

	// Contact events fire every physics frame while cars scrape or bounce, so only the first hit in the
	// cooldown window damages the recipient.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - Recipient->LastCollisionDamageTime < Recipient->CollisionDamageCooldown)
	{
		return;
	}
	Recipient->LastCollisionDamageTime = Now;

	GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Purple, FString::Printf(TEXT("Hit: %s / bone %s  |  Mine: %s / bone %s"),
		*GetNameSafe(OtherComp), *Hit.BoneName.ToString(), *GetNameSafe(HitComp), *Hit.MyBoneName.ToString()));
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

	// The key-to-action mapping context is added by the player controller (ACarPlayerController);
	// this pawn only binds the actions to its handler functions.
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::Steer);
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::None, this, &AMultiplayerVehiclePawn::Steer);

		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::MoveForward);
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::None, this, &ThisClass::CoastBrake);

		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::Brake);

		// Manual gear shifting disabled for now - automatic transmission handles gears.
		EnhancedInputComponent->BindAction(GearUpAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearUp);
		EnhancedInputComponent->BindAction(GearDownAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearDown);
	}
}

void AMultiplayerVehiclePawn::Steer(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetSteeringInput(Value.Get<float>());
}

void AMultiplayerVehiclePawn::MoveForward(const FInputActionValue& Value)
{
	// Gear functionality disabled for now - no manual gear/reverse forcing here,
	// just throttle forward / brake back. Relies on the movement component's own
	// automatic transmission + auto-reverse for anything gear-related.
	const float AxisValue = Value.Get<float>();

	VehicleMovementComponent->SetThrottleInput(AxisValue);
	VehicleMovementComponent->SetBrakeInput(0.f);
}

void AMultiplayerVehiclePawn::CoastBrake(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetThrottleInput(0.f);
	VehicleMovementComponent->SetBrakeInput(0.f);
}

void AMultiplayerVehiclePawn::Brake(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Cyan, TEXT("Brake"));
	VehicleMovementComponent->SetBrakeInput(HarshBrakeStrength);
}

void AMultiplayerVehiclePawn::GearUp(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() + 1, true);
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::Printf(TEXT("CurrentGear: %d"), VehicleMovementComponent->GetCurrentGear()));
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("TargetGear: %d"), VehicleMovementComponent->GetTargetGear()));
}

void AMultiplayerVehiclePawn::GearDown(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() - 1, true);
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, FString::Printf(TEXT("CurrentGear: %d"), VehicleMovementComponent->GetCurrentGear()));
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("TargetGear: %d"), VehicleMovementComponent->GetTargetGear()));
}
