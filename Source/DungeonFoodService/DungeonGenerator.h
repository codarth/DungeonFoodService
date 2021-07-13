// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonGenerator.generated.h"

UCLASS()
class DUNGEONFOODSERVICE_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADungeonGenerator();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY()
		class USceneComponent* MyRootComponent;
	UPROPERTY(EditAnywhere, Category = Meshes)
		class UInstancedStaticMeshComponent* FloorMesh;
	UPROPERTY(EditAnywhere, Category = Meshes)
		class UInstancedStaticMeshComponent* WallMesh;
	UPROPERTY(EditAnywhere, Category = Meshes)
		class UInstancedStaticMeshComponent* InnerCornerMesh;
	UPROPERTY(EditAnywhere, Category = Meshes)
		class UInstancedStaticMeshComponent* OuterCornerMesh;

	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 Seed = 100;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 RoomCount = 1;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 RoomSize_Min = 3;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 RoomSize_Max = 5;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		bool Merging = true;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 FloorCull_Min = 1;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 FloorCull_Max = 10;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		bool IsFloorCulling;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		bool Branching = false;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		int32 BranchingThreshold;
	UPROPERTY(EditAnywhere, Category = MapSettings)
		float BranchingChance = 0.5f;

	UPROPERTY(EditAnywhere, Category = EditerTools)
		bool NewSeed;
	UPROPERTY(EditAnywhere, Category = EditerTools)
		int32 MaxLoops = 15;
	UPROPERTY(EditAnywhere, Category = EditerTools)
		float Scale = 200.f;

	UPROPERTY(VisibleAnywhere, Category = "Stream")
		FRandomStream Stream;

	UPROPERTY(VisibleAnywhere, Category = TempViewing)
		FIntVector NextLocation;
	UPROPERTY(VisibleAnywhere, Category = TempViewing)
		FIntVector PrevLocation;
	UPROPERTY(VisibleAnywhere, Category = TempViewing)
		FIntVector Extents;
	UPROPERTY(VisibleAnywhere, Category = TempViewing) // Needed for garbage collection, other wise tile won't despawn
		TArray<FIntVector> FloorTiles;
	UPROPERTY(VisibleAnywhere, Category = TempViewing) // Needed for garbage collection, other wise tile won't despawn
		TArray<FIntVector> CorridorTiles;
	UPROPERTY(VisibleAnywhere, Category = TempViewing)
		TMap<FIntVector, FIntVector> Rooms; // Location, extents

	// Reset and clean variables 
	UFUNCTION()
	void ResetAndClear();

	// Create Map with given parameters
	UFUNCTION(BlueprintCallable, Category = DungeonGenerator)
		void GenerateMap();
	// Make floor tiles of room
	UFUNCTION(BlueprintCallable, Category = DungeonGenerator)
		void MakeFloorArea(const FIntVector InLocation, TArray<FIntVector>& OutFloorTiles, FIntVector& OutLocation, FIntVector& OutExtents);
	// Spawn tiles for room
	UFUNCTION(BlueprintCallable, Category = DungeonGenerator)
		void SpawnTiles();
	// Calculate next room location
	UFUNCTION(BlueprintCallable, Category = DungeonGenerator)
		void FindNextRoomLocation(bool& IsValid, FIntVector& NewLocation);

private:
	UFUNCTION()
		void TestRelativeTileLocation(const FIntVector InLocation, const TArray<FIntVector> TestArray, const int InX, const int InY, FIntVector& NewLocation, bool& IsFloorTile);
	// Build Next room and check validity
	UFUNCTION()
		void NextRoom(bool& IsValidToPlace, FIntVector& NewLocation, TArray<FIntVector>& NewFloorTiles, TArray<FIntVector>& RoomKeys, int32& LastBranch);
	// Get a room to branch to
	UFUNCTION()
		void GetBranchRoom(TArray<FIntVector>& RoomKeys, int32& LastBranch);
	// Create corridors between rooms
	UFUNCTION()
		void MapCorridors(const FIntVector RoomA, const FIntVector RoomB);
	UFUNCTION()
		void MakeYCorridor(const FIntVector From, const FIntVector To);
	UFUNCTION()
		void MakeXCorridor(const FIntVector From, const FIntVector To);
};
 
