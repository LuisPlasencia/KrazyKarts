// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GoKartMovementComponent.h"  // para que reconzoca sus funciones y el struct de FGoKartMove 
#include "GoKartMovementReplicator.h"
#include "GoKart.generated.h"



UCLASS()
class KRAZYKARTS_API AGoKart : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AGoKart();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)  // no queremos editar el contenido del pointer en el editor simplemente ver su contenido
	UGoKartMovementComponent* MovementComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)  // no queremos editar el contenido del pointer en el editor simplemente ver su contenido
	UGoKartMovementReplicator* MovementReplicator;

private: 

	void MoveForward(float Value);
	void MoveRight(float Value);


	// // cuando cambia es replicada a los clientes
	// UPROPERTY(Replicated)
	// FVector Velocity;

	// // Replicated Variable from the server (cada vez que cambia en el server, notifica su valor a los clientes (Replication))
	// UPROPERTY(ReplicatedUsing = OnRep_ReplicatedTransform)   // replicatedUsing en vez de replicated porque con el using le podemos indicar la función que triggerea (trigger o evento)
	// FTransform ReplicatedTransform;

	// UFUNCTION() // LOS EVENTOS HAN DE SER UFUNCTION
	// void OnRep_ReplicatedTransform();


};
