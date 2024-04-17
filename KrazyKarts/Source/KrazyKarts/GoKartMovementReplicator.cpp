// Fill out your copyright notice in the Description page of Project Settings.


#include "GoKartMovementReplicator.h"
#include "Net/UnrealNetwork.h"   // para el DOREPLIFETIME
#include "GameFramework/Actor.h"

// Sets default values for this component's properties
UGoKartMovementReplicator::UGoKartMovementReplicator()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// by default, components of an actor will not replicate so we have to set the actor and the component to replicate (the actor replicates by default (gokart))
	// SetIsReplicated(true);
	
}


// Called when the game starts
void UGoKartMovementReplicator::BeginPlay()
{
	Super::BeginPlay();

	MovementComponent = GetOwner()->FindComponentByClass<UGoKartMovementComponent>();
}


// Called every frame
void UGoKartMovementReplicator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (MovementComponent == nullptr) return;

	
	FGoKartMove LastMove = MovementComponent->GetLastMove();

 	// si somos el autonomous proxy (coche que controlamos) en el cliente añade a la lista, simula y envíaselo al servidor
	if (GetOwnerRole() == ROLE_AutonomousProxy)   // get owner role en vez de get local role porque somos un component del actor que es nuestro owner
	{
		UnacknowledgedMoves.Add(LastMove);
		Server_SendMove(LastMove);
	}

	// We are the server and in control of the pawn  (IsLocallyControlled Returns true if controlled by a local (not network) Controller.)
	// Ya somos el servidor, así que no tenemos que guardar las posiciones en unacknowledgedmoves ni pasarle nada al servidor con server_sendmoves
	// alternativa del if (GetOwner()->GetRemoteRole() == ROLE_SimulatedProxy)
	if (GetOwnerRole() == ROLE_Authority && GetOwner<APawn>()->IsLocallyControlled())  // casteamos a pawn, que es padre de nuestro owner que es actor
	{
		// le comunicamos el move del coche que controla el servidor a los demás clientes
		UpdateServerState(LastMove);
	}

	// si somos un coche de otro cliente, haz linear interpolation (LERP) entre las posiciones que nos ha pasado el server para que no de saltos de un sitio a otro el coche sino que sea smooth (sobretodo cuando hay lag o retardo de paquetes que provoquen que las actualizaciones del server me tarden más en llegar)
	if (GetOwnerRole() == ROLE_SimulatedProxy)
	{
		ClientTick(DeltaTime);
	}
}

void UGoKartMovementReplicator::UpdateServerState(const FGoKartMove& Move)
{
	// updateamos la posición de los clientes para que (presumiblemente) sea la misma o muy parecida a la del servidor (avoiding lag)
	// somos el servidor, así que setea las replicated variable tras realizar la simulación (la cual se propagará a los clientes ya que ha cambiado el valor de dicha variable)
	ServerState.LastMove = Move;   
	ServerState.Transform = GetOwner()->GetActorTransform();
	ServerState.Velocity = MovementComponent->GetVelocity();
}

void UGoKartMovementReplicator::ClientTick(float DeltaTime)
{
	ClientTimeSinceUpdate += DeltaTime;

	// http://wiki.seas.harvard.edu/geos-chem/index.php/Floating_point_math_issues
	if (ClientTimeSinceUpdate < KINDA_SMALL_NUMBER) return;  // almost cero (0.0001)  - dividir por numeros muy pequeñitos nos va a dar errores altos así que mejor omitir
	if (MovementComponent == nullptr) return;

// LERP (Linear interpolation) (Location Interpolation)  (desestimado pero posible)
	// la interpolación se hace entre la posición del coche al recibir el update del servidor y la nueva posición del coche que nos ha venido del servidor. Es el lerpratio el que dicta en qué punto estamos de la recta en este caso (1 = estamos ya en B y 0 = estamos aún en A, aún así si nos pasamos de 1 la línea sigue más allá);
//	FVector NewLocation = FMath::LerpStable(StartLocation, TargetLocation, LerpRatio); 	// Math::LerpStable() es más estable que Math::Lerp() para números grandes sobretodo

	// esto se puede mejorar (que no sea tan jarring - conseguir un smoother movement teniendo en cuenta la velocidad del vehículo) usando non-linear interpolation (ex: Hermite Cubic Spline Interpolation) (sobretodo en un juego de coches, en otro tipo de juegos quizás no nos molesta tanto)
	// cubic polinomios pueden cruzar el eje de las x hasta tres veces - más complejas que las formas cuadráticas y lineares, las cuales solo cruzan el eje de las x hasta 2 veces para la cuadrática y solo una vez la linear.
	// Hermite Cubic Spline Interpolation:   https://www.desmos.com/calculator/iexoeledct?lang=es

// Cubic Interpolation (Location Interpolation)
	float LerpRatio = ClientTimeSinceUpdate / ClientTimeBetweenLastUpdates;  // tiempo que ha pasado desde la última update entre el tiempo que pasó entre la penúltima y la última
	FHermiteCubicSpline Spline = CreateSpline();
	InterpolateLocation(Spline, LerpRatio);


// Cubic Interpolation (Velocity Interpolation)
	InterpolateVelocity(Spline, LerpRatio);

// SLERP (Spherical linear interpolation) (Rotation interpolation)
	InterpolateRotation(LerpRatio);


}

FHermiteCubicSpline UGoKartMovementReplicator::CreateSpline()
{
	FHermiteCubicSpline Spline;
	Spline.TargetLocation = ServerState.Transform.GetLocation();
	Spline.StartLocation = ClientStartTransform.GetLocation();
	Spline.StartDerivative = ClientStartVelocity * VelocityToDerivative();  // el derivative es el slope (pendiente) y tiene que ver con la velocidad
	Spline.TargetDerivative = ServerState.Velocity * VelocityToDerivative(); 
	return Spline;
}

void UGoKartMovementReplicator::InterpolateLocation(const FHermiteCubicSpline &Spline, float LerpRatio)
{
	FVector NewLocation = Spline.InterpolateLocation(LerpRatio);   // esta función la marcamos como const también ya que tenemos que indicarle al copilador que no está cambiando el parámetro, solo usándolo para calcular si queremos tener const como parámetro
	
	if (MeshOffsetRoot != nullptr)
	{
		// la box collider la movemos con las actualizaciones servidor, el mesh lo movemos en la interpolation. De esta forma no hay colisiones en el cliente por trayectos por los que el coche no ha pasado realmente en el servidor.
		MeshOffsetRoot->SetWorldLocation(NewLocation);
	}

}

void UGoKartMovementReplicator::InterpolateVelocity(const FHermiteCubicSpline &Spline, float LerpRatio)
{
	FVector NewDerivative =  Spline.InterpolateDerivative(LerpRatio); 	// gives us the slope (slope = derivative) at that point on that cubic 
	FVector NewVelocity = NewDerivative / VelocityToDerivative();
//	MovementComponent->SetVelocity(NewVelocity);    // comentado por bug con el coche que controla el servidor, con otros clientes si va bien  
}

void UGoKartMovementReplicator::InterpolateRotation(float LerpRatio)
{
	FQuat TargetRotation = ServerState.Transform.GetRotation();
	FQuat StartRotation = ClientStartTransform.GetRotation();

	// para rotaciones siempre mejor Slerp (Spherical linear interpolation - usa quaternions) que Lerp ya que el Lerp no sabe interpretar ángulos (no sabe ir por el camino más corto con ángulos)
	FQuat NewRotation = FQuat::Slerp( StartRotation, TargetRotation, LerpRatio );

	if (MeshOffsetRoot != nullptr)
	{
		// la box collider la movemos con las actualizaciones servidor, el mesh lo movemos en la interpolation. De esta forma no hay colisiones en el cliente por trayectos por los que el coche no ha pasado realmente en el servidor.
		MeshOffsetRoot->SetWorldRotation(NewRotation);
	}
}



// la velocity está en m/s pero la location está en cm así que tenemos que pasar de m a cm
float UGoKartMovementReplicator::VelocityToDerivative()
{
	return ClientTimeBetweenLastUpdates * 100;
}

// se llama automaticamente al hacer bReplicates = true  a la frequencia de NetUpdateFrequency  -- https://docs.unrealengine.com/5.0/en-US/property-replication-in-unreal-engine/
// Las replicated variables son importantes para que los clientes vean a los otros clientes con sus posiciones reales (ya que es el servidor el que nos las comunica) y que sus propias posiciones sean actualizadas también con los cálculos del servidor cada vez que haya una replication notification (así no hay diferencia entre cliente y servidor)
void UGoKartMovementReplicator::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGoKartMovementReplicator, ServerState);   // este macro (DOREPLIFETIME) registra dicha variable para replication (servidor manda esta variable a los clientes).

}


// Replication notification que se triggerea en el cliente según el NetUpdateFrequency - Simulate between updates (triggering code on replication)
void UGoKartMovementReplicator::OnRep_ServerState()
{
	switch (GetOwnerRole())
	{
		case ROLE_AutonomousProxy: 
			AutonomousProxy_OnRep_ServerState();
			break;
		case ROLE_SimulatedProxy:
			SimulatedProxy_OnRep_ServerState();
			break;
		default:
			break;
	}
}

void UGoKartMovementReplicator::AutonomousProxy_OnRep_ServerState()
{
	if (MovementComponent == nullptr) return;

	GetOwner()->SetActorTransform(ServerState.Transform);  // overrideamos la transform con la replicated variable que nos viene del server
	MovementComponent->SetVelocity(ServerState.Velocity);   		   // overrideamos la velocity con la replicated variable que nos viene del server

	ClearAcknowledgedMoves(ServerState.LastMove);

	// volvemos a simular aquellos moves que tengamos por delante del server 
	// (minimiza así la sensación de lag cuando el move que recibimos del servidor es anterior al que hemos calculado localmente desde el cliente) 
	// (aunq esto nos haga estar un pelín por delante del servidor, nos asegura que acabaremos en el mismo sitio que en el servidor porque está recalculando en base a este)
	// habrá glitch si por ejemplo, al haber lag y el cliente ir por delante del servidor, en el servidor se cruce un coche que nos haga parar y el servidor se lo comunica al cliente y el cliente, que no sabía que por ahí pasaría un coche volverá a simular y dará un salto de posición hasta el lugar de la colisión.
	for(const FGoKartMove& Move : UnacknowledgedMoves)
	{
		MovementComponent->SimulateMove(Move);
	}
}

// recibimos nueva posición de un coche que no controlamos
// con esto calculamos en el tick el siguiente lerp (linear interpolation en los coches que no son el que controlamos) ya que nos ha llegado una nueva posición del server
void UGoKartMovementReplicator::SimulatedProxy_OnRep_ServerState()
{
	if (MovementComponent == nullptr) return;

	ClientTimeBetweenLastUpdates = ClientTimeSinceUpdate;
	ClientTimeSinceUpdate = 0;

	if (MeshOffsetRoot != nullptr)
	{
		ClientStartTransform.SetLocation(MeshOffsetRoot->GetComponentLocation());
		ClientStartTransform.SetRotation(MeshOffsetRoot->GetComponentQuat());
	}
	ClientStartVelocity = MovementComponent->GetVelocity();

	// la box collider la movemos con las actualizaciones servidor, el mesh lo movemos en la interpolation. De esta forma no hay colisiones en el cliente por trayectos por los que el coche no ha pasado realmente en el servidor.
	GetOwner()->SetActorTransform(ServerState.Transform);
}


// Nos cargamos los Moves que tengan un time menor al que me llego del server
void UGoKartMovementReplicator::ClearAcknowledgedMoves(FGoKartMove LastMove)
{
	TArray<FGoKartMove> NewMoves;

	for(const FGoKartMove& Move : UnacknowledgedMoves)
	{
		if (Move.Time > LastMove.Time)
		{
			NewMoves.Add(Move);
		}
	}
	UnacknowledgedMoves = NewMoves;
}


// la implementation se ejecuta en el server
void UGoKartMovementReplicator::Server_SendMove_Implementation(FGoKartMove Move)
{
	if (MovementComponent == nullptr) return;
	ClientSimulatedTime += Move.DeltaTime;

	// simulamos el movimiento (por si fuera distinto al del cliente)
	// estamos actualizando variables con el input system del cliente. Si no lo hacemos a través de un RPC (que se ejecute en el servidor), el server no se entera de que las variables cambian.
	MovementComponent->SimulateMove(Move);
	
	UpdateServerState(Move);

}


// Tenemos que indicarle al servidor a partir de qué inputs de esta función consideramos que el cliente puede hacer cheat. Esto es un anti-cheat.
// si la validación retorna negativo, el server nos expulsa (perdemos la conexión con el host)
bool UGoKartMovementReplicator::Server_SendMove_Validate(FGoKartMove Move)
{
	// si devolvemos false consideramos que hay trampa y el server saca a dicho cliente del juego
	float ProposedTime = ClientSimulatedTime + Move.DeltaTime;
	bool ClientNotRunningAhead = ProposedTime < GetWorld()->TimeSeconds;
	if(!ClientNotRunningAhead)  // si el cliente ha llamado más de la cuenta a Server_SendMove(LastMove) o tiene un deltaTime más alto del debido está haciendo trampas
	{
		UE_LOG(LogTemp, Error, TEXT("Client is running too fast"));
		return false;
	}
	if(!Move.IsValid())  // si el cliente tiene más amplitud en el throttle o en el steeringThrow del máximo está haciendo trampas
	{
		UE_LOG(LogTemp, Error, TEXT("Received invalid move"));
		return false;
	}

	return true;
}