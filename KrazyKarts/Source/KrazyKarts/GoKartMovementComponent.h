// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GoKartMovementComponent.generated.h"


USTRUCT()
struct FGoKartMove
{
	GENERATED_USTRUCT_BODY()

	// si no las hacemos uproperty no serán replicadas
	UPROPERTY()
	float Throttle;
	UPROPERTY()
	float SteeringThrow;

	UPROPERTY()
	float DeltaTime;
	UPROPERTY()
	float Time;

	// cheat protection
	bool IsValid() const
	{
		return FMath::Abs(Throttle) <= 1 && FMath::Abs(SteeringThrow) <= 1;
	}
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class KRAZYKARTS_API UGoKartMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGoKartMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SimulateMove(const FGoKartMove& Move);  // este parámetro porque si no especificamos una referencia nos envia una copia de eso al llamar a la función y es mejor para el performance que le pase una referencia ya que no ocupa tanto tampoco queremos cambiar el Move (referencia en vez de puntero)

	FVector GetVelocity() { return Velocity; }  // los accesor methods son tan cortos que los podemos definir en el header
	void SetVelocity(FVector Val) { Velocity = Val; };

	
	void SetThrottle(float Val) { Throttle = Val; };
	void SetSteeringThrow(float Val) { SteeringThrow = Val; };

	FGoKartMove GetLastMove() { return LastMove; };


private: 
	FGoKartMove CreateMove(float DeltaTime);
	
	FVector GetAirResistance();
	FVector GetRollingResistance();

	void UpdateLocationFromVelocity(float DeltaTime);

	void ApplyRotation(float DeltaTime, float SteeringThrow);

	// The mass of the car (kg)
	UPROPERTY(EditAnywhere)
	float Mass = 1000;

	// The force applied to the car when the throttle is fully down (N)
	UPROPERTY(EditAnywhere)
	float MaxDrivingForce = 10000;

	// Minimum radius of the car turning circle at full lock (m)
	UPROPERTY(EditAnywhere)
	float MinTurningRadius = 10;

	// Higher means more drag (air resistance = -Speed^2 * DragCoefficient)   ( DragCoefficient = AirResistance at max speed (0 aceleration = max force)(10000) / MaxSpeed^2(25^2) )
	UPROPERTY(EditAnywhere)
	float DragCoefficient = 16;  // (kg/m)

	// Higher means more rolling resistance
	UPROPERTY(EditAnywhere)
	float RollingResistanceCoefficient = 0.015; 
	
	FVector Velocity;	
	
	float Throttle;
	float SteeringThrow;

	FGoKartMove LastMove;

		
};
