// Fill out your copyright notice in the Description page of Project Settings.

#include "VehicleWheelRear.h"

UVehicleWheelRear::UVehicleWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	MaxSteerAngle = 0.f;

	bAffectedByBrake = true;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}
