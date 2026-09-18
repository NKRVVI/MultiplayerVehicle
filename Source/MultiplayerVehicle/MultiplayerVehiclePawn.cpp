// Fill out your copyright notice in the Description page of Project Settings.

#include "MultiplayerVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "VehicleWheelFront.h"
#include "VehicleWheelRear.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

namespace
{
	// Placeholder body mesh - swap CarMesh's Static Mesh for the real car asset once you have one.
	const TCHAR* PlaceholderMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	// No wheel meshes/bones: wheels are simulated invisibly at these offsets from the body origin (cm).
	constexpr float WheelOffsetForwardX = 140.f;
	constexpr float WheelOffsetSideY = 90.f;
	constexpr float WheelOffsetDownZ = -50.f;
}

AMultiplayerVehiclePawn::AMultiplayerVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	CarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarMesh"));
	SetRootComponent(CarMesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMeshFinder(PlaceholderMeshPath);
	if (PlaceholderMeshFinder.Succeeded())
	{
		CarMesh->SetStaticMesh(PlaceholderMeshFinder.Object);
		CarMesh->SetRelativeScale3D(FVector(4.5f, 2.0f, 1.0f));
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
	VehicleMovementComponent->WheelSetups[0].AdditionalOffset = FVector(WheelOffsetForwardX, -WheelOffsetSideY, WheelOffsetDownZ);

	VehicleMovementComponent->WheelSetups[1].WheelClass = UVehicleWheelFront::StaticClass();
	VehicleMovementComponent->WheelSetups[1].AdditionalOffset = FVector(WheelOffsetForwardX, WheelOffsetSideY, WheelOffsetDownZ);

	VehicleMovementComponent->WheelSetups[2].WheelClass = UVehicleWheelRear::StaticClass();
	VehicleMovementComponent->WheelSetups[2].AdditionalOffset = FVector(-WheelOffsetForwardX, -WheelOffsetSideY, WheelOffsetDownZ);

	VehicleMovementComponent->WheelSetups[3].WheelClass = UVehicleWheelRear::StaticClass();
	VehicleMovementComponent->WheelSetups[3].AdditionalOffset = FVector(-WheelOffsetForwardX, WheelOffsetSideY, WheelOffsetDownZ);

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
		EnhancedInputComponent->BindAction(SteerAction, ETriggerEvent::Completed, this, &AMultiplayerVehiclePawn::Steer);
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AMultiplayerVehiclePawn::MoveForward);
		EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Completed, this, &AMultiplayerVehiclePawn::MoveForward);
		EnhancedInputComponent->BindAction(GearUpAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearUp);
		EnhancedInputComponent->BindAction(GearDownAction, ETriggerEvent::Started, this, &AMultiplayerVehiclePawn::GearDown);
	}
}

void AMultiplayerVehiclePawn::Steer(const FInputActionValue& Value)
{
	if (VehicleMovementComponent)
	{
		VehicleMovementComponent->SetSteeringInput(Value.Get<float>());
	}
}

void AMultiplayerVehiclePawn::MoveForward(const FInputActionValue& Value)
{
	if (!VehicleMovementComponent)
	{
		return;
	}

	const float AxisValue = Value.Get<float>();

	if (AxisValue >= 0.f)
	{
		if (VehicleMovementComponent->GetCurrentGear() < 0)
		{
			// Coming out of reverse - hand gear selection back to the automatic gearbox.
			VehicleMovementComponent->SetUseAutomaticGears(true);
		}

		VehicleMovementComponent->SetThrottleInput(AxisValue);
		VehicleMovementComponent->SetBrakeInput(0.f);
	}
	else if (VehicleMovementComponent->GetForwardSpeed() > ReverseSpeedThreshold)
	{
		// Still moving forward - brake rather than instantly reversing.
		VehicleMovementComponent->SetThrottleInput(0.f);
		VehicleMovementComponent->SetBrakeInput(-AxisValue);
	}
	else
	{
		// Stopped (or already reversing) - engage reverse gear.
		VehicleMovementComponent->SetUseAutomaticGears(false);
		VehicleMovementComponent->SetTargetGear(-1, true);
		VehicleMovementComponent->SetThrottleInput(-AxisValue);
		VehicleMovementComponent->SetBrakeInput(0.f);
	}
}

void AMultiplayerVehiclePawn::GearUp(const FInputActionValue& Value)
{
	if (VehicleMovementComponent)
	{
		VehicleMovementComponent->SetUseAutomaticGears(false);
		VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() + 1, false);
	}
}

void AMultiplayerVehiclePawn::GearDown(const FInputActionValue& Value)
{
	if (VehicleMovementComponent)
	{
		VehicleMovementComponent->SetUseAutomaticGears(false);
		VehicleMovementComponent->SetTargetGear(VehicleMovementComponent->GetCurrentGear() - 1, false);
	}
}
