// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GoKartMovementComponent.h"  // para que reconzoca sus funciones y el struct de FGoKartMove 
#include "GoKartMovementReplicator.generated.h"


USTRUCT()
struct FGoKartState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FVector Velocity;

	UPROPERTY()
	FGoKartMove LastMove;
};

struct FHermiteCubicSpline  // nos ahorra crear métodos con muchos parámetros lo que aumenta la eficiencia del código y lo hace más readable
{
	FVector StartLocation, StartDerivative, TargetLocation, TargetDerivative;

	// esta función la marcamos como const (no vamos a modificar nada) si queremos tener en la función InterpolateSpline, el parámetro Spline constante.
	FVector InterpolateLocation(float LerpRatio) const
	{
		return FMath::CubicInterp(StartLocation, StartDerivative, TargetLocation, TargetDerivative, LerpRatio);
	}

	// gives us the slope (slope = derivative) at that point on that cubic
	FVector InterpolateDerivative(float LerpRatio) const
	{
		return FMath::CubicInterpDerivative(StartLocation, StartDerivative, TargetLocation, TargetDerivative, LerpRatio);
	}
};  // las clases y structs siempre acaban en ; (una struct es una clase que es pública por defecto)


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class KRAZYKARTS_API UGoKartMovementReplicator : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UGoKartMovementReplicator();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private: 

	void ClearAcknowledgedMoves(FGoKartMove LastMove);

	void UpdateServerState(const FGoKartMove& Move); //  no queremos cambiar el valor y queremos ahorrar espacio en memoria así que tampoco queremos recibir una copia así que recibe de parámetro una referencia

	void ClientTick(float DeltaTime);
	FHermiteCubicSpline CreateSpline();
	void InterpolateLocation(const FHermiteCubicSpline &Spline, float LerpRatio);  // no la queremos editar así que la ponemos const
	void InterpolateVelocity(const FHermiteCubicSpline &Spline, float LerpRatio);
	void InterpolateRotation(float LerpRatio);
	float VelocityToDerivative();

	// Server RPC Function (Remote Procedure Call) - Se ejecuta en el servidor aunque seamos clientes (se llama localmente y se ejecuta remotamente (en servidor)) (así le comunicamos que cambiamos de estado)
	// OJO: Por defecto, las llamadas RPC son NO RELIABLE, pero como con el movimiento debemos tener garantías de que se va a ejecutar en el servidor eso ponemos la keyword Reliable.
	// WithValidation es para indicarle al servidor qué inputs son seguros para evitar que el cliente haga trampas : anti-cheat
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendMove(FGoKartMove Move);  // poner server delante es un convention para como programador tener claro que esto se va a ejecutar en el servidor ya que es un server RPC function


	// cuando cambia es replicada a los clientes
	// Replicated Variable from the server (cada vez que cambia en el server, notifica su valor a los clientes (Replication))
	// OJO: Aqui replicamos una struct. Eso significa que se van a replicar todas las variables dentro de ese struct que sean UPROPERTY (OJITO) (simplifica el código y es eficiente)
	UPROPERTY(ReplicatedUsing = OnRep_ServerState)    // replicatedUsing en vez de replicated porque con el using le podemos indicar la función que triggerea (trigger o evento)
	FGoKartState ServerState;


	UFUNCTION()  // LOS EVENTOS HAN DE SER UFUNCTION
	void OnRep_ServerState();

	void AutonomousProxy_OnRep_ServerState();
	void SimulatedProxy_OnRep_ServerState();

	TArray<FGoKartMove> UnacknowledgedMoves;

	float ClientTimeSinceUpdate;
	float ClientTimeBetweenLastUpdates;
	FTransform ClientStartTransform;
	FVector ClientStartVelocity;

	float ClientSimulatedTime;

	UPROPERTY()
	UGoKartMovementComponent* MovementComponent;

	UPROPERTY()
	USceneComponent* MeshOffsetRoot;

	UFUNCTION(BlueprintCallable)
	void SetMeshOffsetRoot(USceneComponent* Root) { MeshOffsetRoot = Root; }

};
