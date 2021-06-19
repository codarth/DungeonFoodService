// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
ADungeonGenerator::ADungeonGenerator()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MyRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(MyRootComponent);

	FloorMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetMobility(EComponentMobility::Static);
	FloorMesh->SetupAttachment(RootComponent);
	//RootComponent = FloorMesh;
	WallMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetMobility(EComponentMobility::Static);
	WallMesh->SetupAttachment(RootComponent);
	
	InnerCornerMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InnerCornerMesh"));
	InnerCornerMesh->SetMobility(EComponentMobility::Static);
	InnerCornerMesh->SetupAttachment(RootComponent);
	
	OuterCornerMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("OuterCornerMesh"));
	OuterCornerMesh->SetMobility(EComponentMobility::Static);
	OuterCornerMesh->SetupAttachment(RootComponent);
}

void ADungeonGenerator::OnConstruction(const FTransform& Transform)
{
	// When NewSeed is ticked in the editor, set back to false and calculate a new seed
	if (NewSeed)
	{
		NewSeed = false;
		// Get new random seed
		Seed = FMath::RandRange(0, 999999);
	}

	// Set stream seed
	Stream.Initialize(Seed);

	FloorMesh->ClearInstances();
	WallMesh->ClearInstances();
	InnerCornerMesh->ClearInstances();
	OuterCornerMesh->ClearInstances();
	GenerateMap();
}

// Generate tile locations and spawn tiles at locations
void ADungeonGenerator::GenerateMap()
{
	NextLocation = FIntVector::ZeroValue;
	PrevLocation = FIntVector::ZeroValue;

	bool IsValidToPlace;
	FIntVector NewLocation;
	TArray<FIntVector> NewFloorTiles;

	// Loop through rooms
	for (int32 i = 0; i < RoomCount; i++)
	{
		//Check if first room
		if (i == 0)
		{
			MakeFloorArea(PrevLocation, FloorTiles, PrevLocation, Extents);
			Rooms.Add(PrevLocation, Extents);
		}
		else // Other tiles and rooms get appended and added
		{
			int32 LastBranch;
			TArray<FIntVector> RoomKeys;

			// Can branch from previous room
			if (Branching)
			{
				Rooms.GetKeys(RoomKeys);
				if ((RoomKeys.Num() >= (BranchingThreshold + LastBranch)) && UKismetMathLibrary::RandomBoolWithWeightFromStream(BranchingChance, Stream))
				{
					GetBranchRoom(RoomKeys, LastBranch);
					NextRoom(IsValidToPlace, NewLocation, NewFloorTiles, RoomKeys, LastBranch);
				}
			}
			else // Calculate next room and check validity
			{
				NextRoom(IsValidToPlace, NewLocation, NewFloorTiles, RoomKeys, LastBranch);
			}
		}
	}
	SpawnTiles();
}

void ADungeonGenerator::NextRoom(bool& IsValidToPlace, FIntVector& NewLocation, TArray<FIntVector>& NewFloorTiles, TArray<FIntVector>& RoomKeys, int32& LastBranch)
{
	FindNextRoomLocation(IsValidToPlace, NewLocation);
	NextLocation = NewLocation;
	// Valid room, build tiles
	if (IsValidToPlace)
	{
		MakeFloorArea(NextLocation, NewFloorTiles, PrevLocation, Extents);
		FloorTiles.Append(NewFloorTiles);
		Rooms.Add(NewLocation, Extents);
		PrevLocation = NextLocation;
	}
	else // Not valid branch
	{
		Rooms.GetKeys(RoomKeys);
		GetBranchRoom(RoomKeys, LastBranch);
	}
}

void ADungeonGenerator::GetBranchRoom(TArray<FIntVector>& RoomKeys, int32& LastBranch)
{
	PrevLocation = RoomKeys[Stream.RandRange(0, RoomKeys.Num())];
	LastBranch = RoomKeys.Num();
}

// Calculate the tiles in a randomly sized area
void ADungeonGenerator::MakeFloorArea(const FIntVector InLocation, TArray<FIntVector>& OutFloorTiles, FIntVector& OutLocation, FIntVector& OutExtents)
{
	// Max number of times can loop to help stop infinite loops
	int LoopCount = 0;

	// Two for less clustered numbers
	int32 OutX = Stream.RandRange(RoomSize_Min, RoomSize_Max);
	int32 OutY = Stream.RandRange(RoomSize_Min, RoomSize_Max);

	TArray<FIntVector> Tiles;
	TArray<FIntVector> ConnectedTiles;
	TArray<FIntVector> TilesCopy;

	TArray<int32> ExtentsX;
	TArray<int32> ExtentsY;

	int32 Area = OutX * OutY;

	// Get new x,y extents and add to floor tiles array (Tiles)
	for (int32 i = 0; i < Area; i++)
	{
		FIntVector TileLocation = FIntVector(
			i / OutY + InLocation.X,
			i % OutY + InLocation.Y,
			InLocation.Z);
		Tiles.Add(TileLocation);
	}

	if (IsFloorCulling)
	{
		bool Working = true;
		while (Working)
		{
			// Loop while verifying floor and loopcount < max
			if (LoopCount <= MaxLoops)
			{
				TilesCopy = Tiles;

				// Randomly remove tiles from floor
				int Length = Stream.FRandRange(FloorCull_Min, FloorCull_Max) - 1;
				Length = FMath::Clamp(Length, 0, TilesCopy.Num() / 4);
				for (int32 i = 0; i < Length; i++)
				{
					TilesCopy.RemoveAt(Stream.FRandRange(0, TilesCopy.Num()));
				}

				// Check if tiles have neighbors on all sides, if at least one neighbor exists add tile to connected tile array
				ConnectedTiles.Empty();
				ConnectedTiles.Add(TilesCopy[0]);
				bool TileFound = true;
				while (TileFound)
				{
					TileFound = false;
					//for (FIntVector Tile : ConnectedTiles)
					for (int32 t = 0; t < ConnectedTiles.Num(); t++)
					{
						for (int32 i = 0; i < 3; i++)
						{
							FIntVector Location;
							bool IsInArray = false;
							switch (i)
							{
							case 0:
								TestRelativeTileLocation(ConnectedTiles[t], TilesCopy, 1, 0, Location, IsInArray);
								break;
							case 1:
								TestRelativeTileLocation(ConnectedTiles[t], TilesCopy, 0, 1, Location, IsInArray);
								break;
							case 2:
								TestRelativeTileLocation(ConnectedTiles[t], TilesCopy, -1, 0, Location, IsInArray);
								break;
							case 3:
								TestRelativeTileLocation(ConnectedTiles[t], TilesCopy, 0, -1, Location, IsInArray);
								break;
							}
							if (IsInArray)
							{
								ConnectedTiles.Add(Location);
								TilesCopy.Remove(Location);
								TileFound = true;
							}
						}
					}
				}

				// Make sure Connected tiles is not smaller then allowed minimum room area
				if (ConnectedTiles.Num() > RoomSize_Min * RoomSize_Min)
				{
					Working = false;
				}
				else
				{
					Working = true;
					LoopCount++;
				}
			}
			else // Fail out without culling
			{
				Working = false;
				ConnectedTiles = Tiles;
			}
		}
	}
	else
	{
		ConnectedTiles = Tiles;
	}

	// Get outer extents of room
	for (FIntVector Tile : ConnectedTiles)
	{
		ExtentsX.Add(Tile.X);
		ExtentsY.Add(Tile.Y);
	}

	OutFloorTiles = ConnectedTiles;
	OutLocation = InLocation;
	OutExtents = FIntVector(ExtentsX.Max(), ExtentsY.Max(), InLocation.Z);
}

// Spawn tiles at given locations
void ADungeonGenerator::SpawnTiles()
{
	for (FIntVector Tile : FloorTiles)
	{
		// Make floor tiles
		FVector TileLocation = (FVector)Tile * Scale;
		FloorMesh->AddInstance(FTransform(FRotator::ZeroRotator, TileLocation));

		// Make walls
		for (int32 i = 0; i < 4; i++)
		{
			FIntVector notused;
			FRotator WallRotation;
			FVector WallLocation = (FVector)Tile * Scale;
			bool isFloorTile;
			switch (i)
			{
			case 0:
				TestRelativeTileLocation(Tile, FloorTiles, 1, 0, notused, isFloorTile);
				WallRotation = FRotator(0.f, 0.f, 0.f);
				break;
			case 1:
				TestRelativeTileLocation(Tile, FloorTiles, 0, 1, notused, isFloorTile);
				WallRotation = FRotator(0.f, 90.f, 0.f);
				break;
			case 2:
				TestRelativeTileLocation(Tile, FloorTiles, -1, 0, notused, isFloorTile);
				WallRotation = FRotator(0.f, 180.f, 0.f);
				break;
			case 3:
				TestRelativeTileLocation(Tile, FloorTiles, 0, -1, notused, isFloorTile);
				WallRotation = FRotator(0.f, -90.f, 0.f);
				break;
			}
			if (!isFloorTile)
			{
				WallMesh->AddInstance(FTransform(WallRotation, WallLocation));
			}
		}

		// Make Inner corners
		for (int32 i = 0; i < 4; i++)
		{
			FIntVector notused;
			FRotator PillerRotation;
			FVector PillerLocation = (FVector)Tile * Scale;
			bool isFloorTile1, isFloorTile2, isFloorTile3;
			switch (i)
			{
			case 0:
				TestRelativeTileLocation(Tile, FloorTiles, 1, -1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, -1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, 1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 0.f, 0.f);
				break;
			case 1:
				TestRelativeTileLocation(Tile, FloorTiles, 1, 1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, 1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, 1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 90.f, 0.f);
				break;
			case 2:
				TestRelativeTileLocation(Tile, FloorTiles, -1, 1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, 1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, -1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 180.f, 0.f);
				break;
			case 3:
				TestRelativeTileLocation(Tile, FloorTiles, -1, -1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, -1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, -1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, -90.f, 0.f);
				break;
			}
			if (!isFloorTile1 && (!isFloorTile2 && !isFloorTile3))
			{
				InnerCornerMesh->AddInstance(FTransform(PillerRotation, PillerLocation));
			}
		}
		// Make Outer corners
		for (int32 i = 0; i < 4; i++)
		{
			FIntVector notused;
			FRotator PillerRotation;
			FVector PillerLocation = (FVector)Tile * Scale;
			bool isFloorTile1, isFloorTile2, isFloorTile3;
			switch (i)
			{
			case 0:
				TestRelativeTileLocation(Tile, FloorTiles, 1, -1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, -1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, 1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 0.f, 0.f);
				break;
			case 1:
				TestRelativeTileLocation(Tile, FloorTiles, 1, 1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, 1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, 1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 90.f, 0.f);
				break;
			case 2:
				TestRelativeTileLocation(Tile, FloorTiles, -1, 1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, 1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, -1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, 180.f, 0.f);
				break;
			case 3:
				TestRelativeTileLocation(Tile, FloorTiles, -1, -1, notused, isFloorTile1);
				TestRelativeTileLocation(Tile, FloorTiles, 0, -1, notused, isFloorTile2);
				TestRelativeTileLocation(Tile, FloorTiles, -1, 0, notused, isFloorTile3);
				PillerRotation = FRotator(0.f, -90.f, 0.f);
				break;
			}
			if (!isFloorTile1 && (isFloorTile2 && isFloorTile3))
			{
				OuterCornerMesh->AddInstance(FTransform(PillerRotation, PillerLocation));
			}
		}

	}
}

void ADungeonGenerator::FindNextRoomLocation(bool& IsValid, FIntVector& NewLocation)
{
	IsValid = false;

	TArray<int> Directions = { 0,1,2,3,4,5,6,7 };
	bool Searching = true;
	int TestIndex;
	bool IsFloorTile = false;
	int InX;
	int InY;

	while (Searching)
	{
		if (Directions.Last() >= 0)
		{
			TestIndex = Stream.FRandRange(0, Directions.Last());
			switch (TestIndex)
			{
			case 0:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				InY = 0;
				TestRelativeTileLocation(PrevLocation, FloorTiles, InX, InY, NewLocation, IsFloorTile);
				break;
			case 1:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, InX, InY, NewLocation, IsFloorTile);
				break;
			case 2:
				InX = 0;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, InX, InY, NewLocation, IsFloorTile);
				break;
			case 3:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, -InX, InY, NewLocation, IsFloorTile);
				break;
			case 4:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				InY = 0;
				TestRelativeTileLocation(PrevLocation, FloorTiles, -InX, InY, NewLocation, IsFloorTile);
				break;
			case 5:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, -InX, -InY, NewLocation, IsFloorTile);
				break;
			case 6:
				InX = 0;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, InX, -InY, NewLocation, IsFloorTile);
				break;
			case 7:
				if (!Merging)InX = RoomSize_Max + 1; else InX = RoomSize_Max;
				if (!Merging)InY = RoomSize_Max + 1; else InY = RoomSize_Max;
				TestRelativeTileLocation(PrevLocation, FloorTiles, InX, -InY, NewLocation, IsFloorTile);
				break;
			}

			if (IsFloorTile)
			{
				Directions.Remove(TestIndex);
				Searching = true;
			}
			else
			{
				Searching = false;
				IsValid = true;
			}
		}
		else
		{
			Searching = false;
			IsValid = false;
		}
	}
}

void ADungeonGenerator::TestRelativeTileLocation(const FIntVector InLocation, const TArray<FIntVector> TestArray, const int InX, const int InY, FIntVector& NewLocation, bool& IsFloorTile)
{
	FIntVector NewVector = FIntVector(InLocation.X + InX, InLocation.Y + InY, InLocation.Z);
	NewLocation = NewVector;
	IsFloorTile = TestArray.Contains(NewVector);
}


//
//UE_LOG(LogTemp, Warning, TEXT("%d"), Tiles.Num());
