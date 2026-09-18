// Fill out your copyright notice in the Description page of Project Settings.

#include "VehicleWheelFront.h"

UVehicleWheelFront::UVehicleWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;

	bAffectedByBrake = true;
	bAffectedByHandbrake = false;
	bAffectedByEngine = false;
}
