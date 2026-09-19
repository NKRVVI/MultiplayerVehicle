// Fill out your copyright notice in the Description page of Project Settings.

#include "MultiplayerVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "VehicleWheelFront.h"
#include "VehicleWheelRear.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Curves/CurveFloat.h"

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

	// VehicleMappingContext, SteerAction, MoveForwardAction, GearUpAction and GearDownAction are
	// assigned in the editor (see the header) - nothing to construct here.
}

void AMultiplayerVehiclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const int32 CurrentGear = VehicleMovementComponent->GetCurrentGear();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow, FString::Printf(TEXT("Gear: %d"), CurrentGear));
		GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Cyan, FString::Printf(TEXT("HasValidPhysicsState: %s"), VehicleMovementComponent->HasValidPhysicsState() ? TEXT("true") : TEXT("false")));
	}

	UE_LOG(LogTemp, Log, TEXT("Current gear: %d"), CurrentGear);
}

void AMultiplayerVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Activate the mapping context (key-to-action mapping lives on the asset itself, assigned in the editor).
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (VehicleMappingContext)
				{
					Subsystem->AddMappingContext(VehicleMappingContext, 0);
				}
			}
		}
	}

	// Bind each action to its handler function.
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::Steer);
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::None, this, &AMultiplayerVehiclePawn::Steer);

		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::MoveForward);
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::None, this, &AMultiplayerVehiclePawn::Stop);

		// Manual gear shifting disabled for now - automatic transmission handles gears.
		// EnhancedInputComponent->BindAction(GearUpAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearUp);
		// EnhancedInputComponent->BindAction(GearDownAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearDown);
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

	if (AxisValue > 0.f)
	{
		VehicleMovementComponent->SetThrottleInput(AxisValue);
		VehicleMovementComponent->SetBrakeInput(0.f);
	}
	else if (AxisValue < 0.f)
	{
		VehicleMovementComponent->SetThrottleInput(0.f);
		VehicleMovementComponent->SetBrakeInput(-AxisValue);
	}
}

void AMultiplayerVehiclePawn::Stop(const FInputActionValue& Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, TEXT("Brake"));
	if (VehicleMovementComponent->GetCurrentGear() < 0)
	{
		// Keys released while in reverse gear - the automatic gearbox's auto-reverse
		// reads Brake input as the reverse throttle while stopped/reversing, so applying
		// our usual release brake here would just keep the car creeping backward instead
		// of stopping it. Apply a light forward throttle instead - while rolling backward
		// this acts as the counter-force (brake) against the reverse motion.
		VehicleMovementComponent->SetThrottleInput(CoastBrakeStrength);
		VehicleMovementComponent->SetBrakeInput(0.f);
	}
	else
	{
		// Keys released - light auto-brake so the car visibly slows down
		// instead of just coasting on engine braking/rolling resistance.
		VehicleMovementComponent->SetThrottleInput(0.f);
		VehicleMovementComponent->SetBrakeInput(CoastBrakeStrength);
	}
}

void AMultiplayerVehiclePawn::GearUp(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetUseAutomaticGears(false);
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() + 1, false);
}

void AMultiplayerVehiclePawn::GearDown(const FInputActionValue& Value)
{
	VehicleMovementComponent->SetUseAutomaticGears(false);
	VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() - 1, false);
}
