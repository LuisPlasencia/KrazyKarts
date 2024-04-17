// Fill out your copyright notice in the Description page of Project Settings.


#include "GoKartMovementComponent.h"
#include "GameFramework/GameStateBase.h"

// Sets default values for this component's properties
UGoKartMovementComponent::UGoKartMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UGoKartMovementComponent::BeginPlay()
{
	Super::BeginPlay();

}


// Called every frame
void UGoKartMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// si somos un cliente en control del pawn  o   si somos el servidor en control del pawn (getremoterole nos dice el rol de los otros integrantes sobre el pawn, ya que )
	if (GetOwnerRole() == ROLE_AutonomousProxy || (GetOwnerRole() == ROLE_Authority && GetOwner<APawn>()->IsLocallyControlled()))
	{
		LastMove = CreateMove(DeltaTime);
		SimulateMove(LastMove);
	} 
}


void UGoKartMovementComponent::SimulateMove(const FGoKartMove& Move)  // este parámetro porque si no especificamos una referencia nos envia una copia de eso al llamar a la función y es mejor para el performance que le pase una referencia ya que no ocupa tanto y tampoco queremos cambiar el Move (referencia en vez de puntero)
{
	FVector Force = GetOwner()->GetActorForwardVector() * MaxDrivingForce * Move.Throttle;

	Force += GetAirResistance();  
	Force += GetRollingResistance();  // sin esto el coche tarda mucho en parar del todo

	FVector Acceleration = Force / Mass;

	Velocity = Velocity + Acceleration * Move.DeltaTime;

	ApplyRotation(Move.DeltaTime, Move.SteeringThrow);

	UpdateLocationFromVelocity(Move.DeltaTime);
}

FGoKartMove UGoKartMovementComponent::CreateMove(float DeltaTime)
{
	FGoKartMove Move;
	Move.DeltaTime = DeltaTime;   // delta time lo pasamos también para reducir los numerical integration errors
	Move.SteeringThrow = SteeringThrow;
	Move.Throttle = Throttle;
	Move.Time = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();  // GetWorld()->GetTimeSeconds()  no es fiable entre servidor y cliente porque ambos pueden crearse a tiempos distintos

	return Move;
}

// Air resistance - nos limita la velocidad máxima y nos desacelera cuando dejamos de acelerar
FVector UGoKartMovementComponent::GetAirResistance()   
{
	return -Velocity.GetSafeNormal() * Velocity.SizeSquared() * DragCoefficient;  // (air resistance = -Speed^2 * DragCoefficient)  - hacer sizesquared nos devuelve ya la magnitud al cuadrado, es más eficiente que hacer FMath::Square(Velocity.Size()) porque esto hace 2 operaciones
}

// Rolling Resistance (Fuerza de rozamiento) = RRCoefficient * NormalForce  (constant force)  (NormalForce = m*G ,  RRCoefficient = Coeficiente de rozamiento)
FVector UGoKartMovementComponent::GetRollingResistance()
{
	float AccelerationDueToGravity= -GetWorld()->GetGravityZ() / 100;  // GetGravityZ = -980 por default pero se peude cambiar dependiendo del mapa (fuerza de gravedad de la tierra en cm)-
	float NormalForce = Mass * AccelerationDueToGravity;
	return -Velocity.GetSafeNormal()  * RollingResistanceCoefficient * NormalForce;
}

// dx = dangle(radians) * radius
void UGoKartMovementComponent::ApplyRotation(float DeltaTime, float CurrentSteeringThrow)
{
	// UE_LOG(LogTemp, Warning, TEXT("Forward Vector: %s"), *GetActorForwardVector().ToString());
	// UE_LOG(LogTemp, Warning, TEXT("Velocity %s"), *Velocity.ToString());
	float DeltaLocation = FVector::DotProduct(GetOwner()->GetActorForwardVector(), Velocity) * DeltaTime; // dx = dv * dt  (desplazamiento) -- dot product es el producto escalar y devuelve un número positivo o negativo (con la magnitud de la velocidad (Velocity.size)) dependiendo de si está yendo hacia adelante o marcha atrás (forward vector positivo o negativo). (para que el coche rote de forma realista, si giramos el volante hacia la izquierda y estamos en reversa deberá girar hacia la derecha y no hacia la izquierda)
	float RotationAngle =  DeltaLocation / MinTurningRadius * CurrentSteeringThrow;  // dx = dangle * r  ---- (dx / radius * SteeringThrow) = dangle (radianes)  -> a mitad de volante nos sale el doble de radio y esto tiene sentido porque cuanto mas radio tenga el círculo mas nos costará girar
	FQuat RotationDelta(GetOwner()->GetActorUpVector(), RotationAngle); // quaternion - te permite hacer angle axis rotation cosa que no podemos hacer con rotators porque solo tienen x y z - ojo el angulo debe estar en radianes. Util para pasar de grados a radianes: (FMath::DegreesToRadians(RotationAngle)
	Velocity = RotationDelta.RotateVector(Velocity);  // rotamos el vector velocidad junto con el coche. Si no estaría patinando el coche al girar
	
	GetOwner()->AddActorWorldRotation(RotationDelta);
}

void UGoKartMovementComponent::UpdateLocationFromVelocity(float DeltaTime)
{
	FVector Translation = Velocity * 100 * DeltaTime;  // 1.  (m/s * s)  = m  -> 2.  m * 100 = cm

	FHitResult Hit;  // hit es un out parameter
	GetOwner()->AddActorWorldOffset(Translation, true, &Hit);  // Sweep (segundo parametro) a true triggerea overlaps, es decir, chekea colisiones 
	if (Hit.IsValidBlockingHit())  // devuelve true si entramos en contacto con un collision block pero si ya estabamos dentro de la colisión o en cualquier otro caso dentro false 
	{
		Velocity = FVector::ZeroVector;  // Reseteamos la velocidad al chocar con algo
	}
}