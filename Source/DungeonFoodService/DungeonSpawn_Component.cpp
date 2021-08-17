// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonSpawn_Component.h"
#include "DungeonGenerator.h"

// Sets default values for this component's properties
UDungeonSpawn_Component::UDungeonSpawn_Component()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDungeonSpawn_Component::BeginPlay()
{
	Super::BeginPlay();

	TriggerSpawnThings();
}

void UDungeonSpawn_Component::TriggerSpawnThings()
{
	DungeonREF = (ADungeonGenerator*)GetOwner();

	if (RoomsOnly)
	{
		Floors = DungeonREF->FloorTiles;

	}
	else
	{
		Floors = DungeonREF->FloorTiles;
		Floors.Append(DungeonREF->CorridorTiles);

	}
	SpawnThings();
}

void UDungeonSpawn_Component::SpawnThings()
{
	//int32 ItemsSpawned = 0;
	//int32 FloorIndex;
	//TArray<FVector> TileKeys;

	//while (Quantity > ItemsSpawned)
	//{
	//	FloorIndex = DungeonREF->Stream.RandRange(0, Floors.Num());
	//	FVector SpawnLocation = 
	//		((FVector)Floors[FloorIndex] * DungeonREF->Scale) 
	//		+ DungeonREF->GetActorLocation() 
	//		+ Offset;

	//	SpawnList.GetKeys(TileKeys);

		//for each (AActor* Object in )
		//{

		//} 
	//}
}

