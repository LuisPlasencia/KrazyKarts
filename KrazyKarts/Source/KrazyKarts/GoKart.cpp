// Fill out your copyright notice in the Description page of Project Settings.


// Why would an out parameter be a pointer rather than a reference? - It makes it optional (you can set the pointer to null if you don't want the out parameter.)

// How to simulate LAG: https://www.unrealengine.com/en-US/blog/finding-network-based-exploits    (comandos NetEmulation.PktLag  PktLoss PktDup PktOrder etc...)


#include "GoKart.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"


// Sets default values
AGoKart::AGoKart()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	MovementComponent = CreateDefaultSubobject<UGoKartMovementComponent>(TEXT("MovementComponent"));
	MovementReplicator = CreateDefaultSubobject<UGoKartMovementReplicator>(TEXT("MovementReplicator"));

	// by default, components of an actor will not replicate so we have to set the actor and the component to replicate (the actor replicates by default (gokart))
	MovementReplicator->SetIsReplicated(true);

}

// Called when the game starts or when spawned
void AGoKart::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		NetUpdateFrequency = 1;   // veces por segundo en las que actualizamos dicho Actor (podemos hacer stress test con esto para que vaya mal (de manera que gracias a esto podemos ver que las réplicas deben ser notificadas por eventos cuando hay un cambio en vez de con intervalos de tiempo ya que esto produce error y el evento es en el momento en el que cambia))
	}

	SetReplicateMovement(false);  // no nos interesa replicar la posición del vehículo a otros clientes ya que vamos a añadir linear interpolation (lerp)
}


FString GetEnumText(ENetRole Role)
{
	switch (Role)
	{
	case ROLE_None:
		return "None";
	case ROLE_SimulatedProxy:
		return "SimulatedProxy"; // para un cliente - es otro cliente cuyos datos los recogemos del servidor
	case ROLE_AutonomousProxy:
		return "AutonomousProxy";  // para un cliente - cliente propio (the only one that can receive input since it is the only one that has a player controller attached)
	case ROLE_Authority:
		return "Authority";  // para un cliente - actor proveniente del server    para el server - cualquier actor o cliente
	default:
		return "ERROR";
	}
}

// Called every frame
void AGoKart::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawDebugString(GetWorld(), FVector(0, 0, 100), GetEnumText(GetLocalRole()), this, FColor::White, DeltaTime);  // en vez de GetEnumText, unreal ya tiene una función que hace eso ->  UEnum::GetValueAsString(GetLocalRole())
}


// Called to bind functionality to input
// no comprobamos el rol porque autonomous rol is the only one that can receive input since it is the only one that has a player controller attached, así que esta función solo se llama desde el cliente
void AGoKart::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AGoKart::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGoKart::MoveRight);
}

// local, se ejecuta en el cliente 
void AGoKart::MoveForward(float Value)
{
	if (MovementComponent == nullptr) return;

	MovementComponent->SetThrottle(Value);
}


// local, se ejecuta en el cliente
void AGoKart::MoveRight(float Value)
{
	if (MovementComponent == nullptr) return;

	MovementComponent->SetSteeringThrow(Value);
}




