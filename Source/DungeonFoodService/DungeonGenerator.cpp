// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "DrawDebugHelpers.h"

// Sets default values
ADungeonGenerator::ADungeonGenerator()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	MyRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	MyRootComponent->SetMobility(EComponentMobility::Static);
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

	DoorMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetMobility(EComponentMobility::Static);
	DoorMesh->SetupAttachment(RootComponent);
}

void ADungeonGenerator::OnConstruction(const FTransform& Transform)
{
	//auto StartTime = FPlatformTime::Seconds();
	auto StartTime = FDateTime::UtcNow();

	FlushPersistentDebugLines(GetWorld());
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
	DoorMesh->ClearInstances();

	PrevLocation = FIntVector::ZeroValue;
	NextLocation = FIntVector::ZeroValue;
	Extents = FIntVector::ZeroValue;

	FloorTiles.Empty();
	CorridorTiles.Empty();
	Rooms.Empty();

	GenerateMap();

	//auto EndTime = FPlatformTime::Seconds();
	auto EndTime = FDateTime::UtcNow();
	UE_LOG(LogTemp, Warning, TEXT("Time for map generation: (start) %s, (end) %s,  %s"), *StartTime.ToString(), *EndTime.ToString(), *(EndTime - StartTime).ToString());
}

// Reset and clear data
void ADungeonGenerator::ResetAndClear()
{

}

// Generate tile locations and spawn tiles at locations
void ADungeonGenerator::GenerateMap()
{
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
				int32 Keys = Rooms.GetKeys(RoomKeys);

				if ((Keys >= (BranchingThreshold + LastBranch)) && UKismetMathLibrary::RandomBoolWithWeightFromStream(BranchingChance, Stream))
				{
					GetBranchRoom(RoomKeys, LastBranch);
					NextRoom(IsValidToPlace, NewLocation, NewFloorTiles, RoomKeys, LastBranch);
				}
				else
				{
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
	//UE_LOG(LogTemp, Warning, TEXT("BEFORE :: NewLoc: %s, Prev: %s, Next: %s"), *NewLocation.ToString(), *PrevLocation.ToString(), *NextLocation.ToString());

	FindNextRoomLocation(IsValidToPlace, NewLocation);
	NextLocation = NewLocation;
	// Valid room, build tiles
	if (IsValidToPlace)
	{
		MakeFloorArea(NextLocation, NewFloorTiles, PrevLocation, Extents);
		FloorTiles.Append(NewFloorTiles);
		Rooms.Add(NewLocation, Extents);

		//UE_LOG(LogTemp, Warning, TEXT("AFTER  :: NewLoc: %s"), *NewLocation.ToString());
		//UE_LOG(LogTemp, Warning, TEXT("AFTER  :: Prev: %s"), *PrevLocation.ToString());
		//UE_LOG(LogTemp, Warning, TEXT("AFTER  :: Next: %s"), *NextLocation.ToString());

		MapCorridors(PrevLocation, NextLocation);

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
	//OutLocation = InLocation; // TODO: remove out location
	OutExtents = FIntVector(FMath::Max<int32>(ExtentsX), FMath::Max<int32>(ExtentsY), InLocation.Z);
}

// Spawn tiles at given locations
void ADungeonGenerator::SpawnTiles()
{
	// Remove unnessesary tiles
	for (FIntVector Tile : FloorTiles)
	{
		auto Valid = CorridorTiles.Find(Tile) != INDEX_NONE ? true : false;
		if (Valid)
		{
			CorridorTiles.RemoveAt(CorridorTiles.Find(Tile));
		}
	}

	// Spawn Doors
	for (FIntVector Tile : CorridorTiles)
	{
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
			if (isFloorTile)
			{
				DoorMesh->AddInstance(FTransform(WallRotation, WallLocation)); 
			}
		}
	}

	FloorTiles.Append(CorridorTiles);

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
		if (Directions.Last() >= 0) // TODO:: just = ?
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

void ADungeonGenerator::MapCorridors(const FIntVector RoomA, const FIntVector RoomB)
{
	FIntVector* RoomAExtent = Rooms.Find(RoomA);
	FIntVector* RoomBExtent = Rooms.Find(RoomB);
	FIntVector PointRoomA, PointRoomB, PointCorner;

	int LoopCount = 0;

	//DrawDebugBox(GetWorld(), (FVector)RoomA * Scale, FVector(50, 50, 50), FColor::Turquoise, true, -1.0f, 0U, 5);
	//DrawDebugBox(GetWorld(), FVector(RoomAExtent->X, RoomAExtent->Y, RoomAExtent->Z) * Scale, FVector(50, 50, 50), FColor::Blue, true, -1.0f, 0U, 5);
	//DrawDebugBox(GetWorld(), (FVector)RoomB * Scale, FVector(50, 50, 50), FColor::Yellow, true, -1.0f, 0U, 5);
	//DrawDebugBox(GetWorld(), FVector(RoomBExtent->X, RoomBExtent->Y, RoomBExtent->Z) * Scale, FVector(50, 50, 50), FColor::Green, true, -1.0f, 0U, 5);

	// Room parrallel on X with overlapping
	if ((FMath::Max(RoomA.X, RoomB.X)) <= (FMath::Min(RoomAExtent->X, RoomBExtent->X)))
	{
		// Room B is to the right? Work in positive direction
		if (RoomB.Y > RoomA.Y)
		{
			// Check that rooms are not merged
			if (RoomB.Y - RoomAExtent->Y > 1)
			{
				while (LoopCount <= MaxLoops)
				{
					// Corridor from A to B on Y axis
					int OutX = Stream.RandRange(FMath::Max(RoomA.X, RoomB.X), FMath::Min(RoomAExtent->X, RoomBExtent->X));
					PointRoomA = FIntVector(OutX, RoomAExtent->Y, RoomA.Z);
					PointRoomB = FIntVector(OutX, RoomB.Y, RoomB.Z);
					DebugBoxes(PointRoomA, PointRoomB);
					if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
					{
						MakeYCorridor(PointRoomA, PointRoomB);
						break;
					}
					else
					{
						LoopCount++;
					}
				}
			}
		}
		else // B to left
		{
			// Check that rooms are not merged
			if (RoomA.Y - RoomBExtent->Y > 1)
			{
				while (LoopCount <= MaxLoops)
				{
					// Corridor from B to A on Y axis
					int OutX = Stream.RandRange(FMath::Max(RoomA.X, RoomB.X), FMath::Min(RoomAExtent->X, RoomBExtent->X));
					PointRoomA = FIntVector(OutX, RoomA.Y, RoomA.Z);
					PointRoomB = FIntVector(OutX, RoomBExtent->Y, RoomB.Z);
					DebugBoxes(PointRoomA, PointRoomB);
					if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
					{
						MakeYCorridor(PointRoomB, PointRoomA);
						break;
					}
					else
					{
						LoopCount++;
					}
				}
			}
		}
	}
	// Room parrallel on Y with overlapping
	else if ((FMath::Max(RoomA.Y, RoomB.Y)) <= (FMath::Min(RoomAExtent->Y, RoomBExtent->Y)))
	{
		// Room B is to the forward? Work in positive direction
		if (RoomB.X > RoomA.X)
		{
			// Check that rooms are not merged
			if (RoomB.X - RoomAExtent->X > 1)
			{
				while (LoopCount <= MaxLoops)
				{
					// Corridor from A to B on X axis
					int OutY = Stream.RandRange(FMath::Max(RoomA.Y, RoomB.Y), FMath::Min(RoomAExtent->Y, RoomBExtent->Y));
					PointRoomA = FIntVector(RoomAExtent->X, OutY, RoomA.Z);
					PointRoomB = FIntVector(RoomB.X, OutY, RoomB.Z);
					DebugBoxes(PointRoomA, PointRoomB);
					if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
					{
						MakeXCorridor(PointRoomA, PointRoomB);
						break;
					}
					else
					{
						LoopCount++;
					}
				}
			}
		}
		else // B behind
		{
			// Check that rooms are not merged
			if (RoomA.X - RoomBExtent->X > 1)
			{
				while (LoopCount <= MaxLoops)
				{
					// Corridor from B to A on X axis
					int OutY = Stream.RandRange(FMath::Max(RoomA.Y, RoomB.Y), FMath::Min(RoomAExtent->Y, RoomBExtent->Y));
					PointRoomA = FIntVector(RoomA.X, OutY, RoomA.Z);
					PointRoomB = FIntVector(RoomBExtent->X, OutY, RoomB.Z);
					DebugBoxes(PointRoomA, PointRoomB);
					if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
					{
						MakeXCorridor(PointRoomB, PointRoomA);
						break;
					}
					else
					{
						LoopCount++;
					}
				}
			}
		}
	}
	// Corner Corridors
	else
	{
		// Room B is to the forward? Work in positive direction
		if (RoomB.X > RoomA.X)
		{
			// Room B is to the right? Work in positive direction
			if (RoomB.Y > RoomA.Y)
			{
				// Random choose hook direction
				if (UKismetMathLibrary::RandomBoolFromStream(Stream))
				{
					// Hook up the right
					UpRight(true, LoopCount, RoomB, RoomBExtent, RoomA, RoomAExtent, PointRoomA, PointRoomB, PointCorner);
				}
				else
				{
					// Hook right then up
					RightUp(true, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
				}
			}
			else
			{
				// Random choose hook direction
				if (UKismetMathLibrary::RandomBoolFromStream(Stream))
				{
					// Up then left
					UpLeft(true, LoopCount, RoomB, RoomBExtent, RoomA, RoomAExtent, PointRoomA, PointRoomB, PointCorner);
				}
				else
				{
					// Left then Up
					LeftUp(true, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
				}
			}
		}
		// RoomB back (X)
		else
		{
			// Room B is to the right? Work in positive direction
			if (RoomB.Y > RoomA.Y)
			{
				// Random choose hook direction
				if (UKismetMathLibrary::RandomBoolFromStream(Stream))
				{
					// Hook right then down
					RightDown(true, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
				}
				else
				{
					// Hook down then right
					DownRight(true, LoopCount, RoomB, RoomBExtent, RoomA, RoomAExtent, PointRoomA, PointRoomB, PointCorner);
				}
			}
			// Back Left
			else
			{
				// Random choose hook direction
				if (UKismetMathLibrary::RandomBoolFromStream(Stream))
				{
					// Left then down
					LeftDown(true, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
				}
				else
				{
					// Down then left
					DownLeft(true, LoopCount, RoomB, RoomBExtent, RoomA, RoomAExtent, PointRoomA, PointRoomB, PointCorner);
				}
			}

		}
	}
}

void ADungeonGenerator::UpRight(bool FirstAttempt, int& LoopCount, const FIntVector& RoomB, FIntVector* RoomBExtent, const FIntVector& RoomA, FIntVector* RoomAExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from A to Corner (X), Corner to B (Y)
		int OutX = Stream.RandRange(RoomB.X, RoomBExtent->X);
		int OutY = Stream.RandRange(RoomA.Y, RoomAExtent->Y);
		PointRoomA = FIntVector(RoomAExtent->X, OutY, RoomA.Z);
		PointRoomB = FIntVector(OutX, RoomB.Y, RoomB.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointRoomA, PointCorner);
			MakeYCorridor(PointCorner, PointRoomB);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}
	if (!complete)
	{
		if (FirstAttempt)
		{
			LoopCount = 0;
			RightUp(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			return;
		}
	}
}

void ADungeonGenerator::RightUp(bool FirstAttempt, int& LoopCount, const FIntVector& RoomA, FIntVector* RoomAExtent, const FIntVector& RoomB, FIntVector* RoomBExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from A to Corner (Y), Corner to B (X)
		int OutX = Stream.RandRange(RoomA.X, RoomAExtent->X);
		int OutY = Stream.RandRange(RoomB.Y, RoomBExtent->Y);
		PointRoomA = FIntVector(OutX, RoomAExtent->Y, RoomB.Z);
		PointRoomB = FIntVector(RoomB.X, OutY, RoomA.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointCorner, PointRoomB);
			MakeYCorridor(PointRoomA, PointCorner);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}
	if (!complete)
	{
		if (FirstAttempt)
		{
			LoopCount = 0;
			UpRight(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			return;
		}
	}
}

void ADungeonGenerator::UpLeft(bool FirstAttempt, int& LoopCount, const FIntVector& RoomB, FIntVector* RoomBExtent, const FIntVector& RoomA, FIntVector* RoomAExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from A to Corner (X), B to Corner (Y)
		int OutX = Stream.RandRange(RoomB.X, RoomBExtent->X);
		int OutY = Stream.RandRange(RoomA.Y, RoomAExtent->Y);
		PointRoomA = FIntVector(RoomAExtent->X, OutY, RoomA.Z);
		PointRoomB = FIntVector(OutX, RoomBExtent->Y, RoomB.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointRoomA, PointCorner);
			MakeYCorridor(PointRoomB, PointCorner);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}
	
	if (!complete)
	{
		if (FirstAttempt)
		{
			LoopCount = 0;
			LeftUp(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			return;
		}
	}
}

void ADungeonGenerator::LeftUp(bool FirstAttempt, int& LoopCount, const FIntVector& RoomA, FIntVector* RoomAExtent, const FIntVector& RoomB, FIntVector* RoomBExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from Corner to A (Y), Corner to B (X)
		int OutX = Stream.RandRange(RoomA.X, RoomAExtent->X);
		int OutY = Stream.RandRange(RoomB.Y, RoomBExtent->Y);
		PointRoomA = FIntVector(OutX, RoomA.Y, RoomB.Z);
		PointRoomB = FIntVector(RoomB.X, OutY, RoomA.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointCorner, PointRoomB);
			MakeYCorridor(PointCorner, PointRoomA);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}

	if (!complete)
	{
		if (FirstAttempt)
		{
			LoopCount = 0;
			UpLeft(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			return;
		}
	}
}

// TODO: Functionize the up directions like the down were done

void ADungeonGenerator::DownLeft(bool FirstAttempt, int& LoopCount, const FIntVector& RoomB, FIntVector* RoomBExtent, const FIntVector& RoomA, FIntVector* RoomAExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from Corner to A (X), B to Corner (Y)
		int OutX = Stream.RandRange(RoomB.X, RoomBExtent->X);
		int OutY = Stream.RandRange(RoomA.Y, RoomAExtent->Y);
		PointRoomA = FIntVector(RoomA.X, OutY, RoomA.Z);
		PointRoomB = FIntVector(OutX, RoomBExtent->Y, RoomB.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointCorner, PointRoomA);
			MakeYCorridor(PointRoomB, PointCorner);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}	
	
	if (!complete)
	{
		if (FirstAttempt)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Down Left"));
			//UE_LOG(LogTemp, Warning, TEXT("Attempting Left Down"));
			LoopCount = 0;
			LeftDown(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Down Left"));
			return;
		}
	}
}

void ADungeonGenerator::LeftDown(bool FirstAttempt, int& LoopCount, const FIntVector& RoomA, FIntVector* RoomAExtent, const FIntVector& RoomB, FIntVector* RoomBExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from Corner to A (Y), B to Corner (X)
		int OutX = Stream.RandRange(RoomA.X, RoomAExtent->X);
		int OutY = Stream.RandRange(RoomB.Y, RoomBExtent->Y);
		PointRoomA = FIntVector(OutX, RoomA.Y, RoomB.Z);
		PointRoomB = FIntVector(RoomBExtent->X, OutY, RoomA.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointRoomB, PointCorner);
			MakeYCorridor(PointCorner, PointRoomA);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
		}
	}
	if (!complete)
	{
		if (FirstAttempt)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Left Down"));
			//UE_LOG(LogTemp, Warning, TEXT("Attempting Down Left"));
			LoopCount = 0;
			DownLeft(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Left Down"));
			return;
		}
	}
}

#pragma region CornerCorridors
// TODO: Refactor other directions
// TODO: Refactor to single function? input direction and drawing of halls 

void ADungeonGenerator::RightDown(bool FirstAttempt, int& LoopCount, const FIntVector& RoomA, FIntVector* RoomAExtent, const FIntVector& RoomB, FIntVector* RoomBExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from A to Corner (Y), B to Corner (X)
		int OutX = Stream.RandRange(RoomA.X, RoomAExtent->X);
		int OutY = Stream.RandRange(RoomB.Y, RoomBExtent->Y);
		PointRoomA = FIntVector(OutX, RoomAExtent->Y, RoomA.Z);
		PointRoomB = FIntVector(RoomBExtent->X, OutY, RoomB.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointRoomB, PointCorner);
			MakeYCorridor(PointRoomA, PointCorner);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
			//UE_LOG(LogTemp, Warning, TEXT("Failure %d"), LoopCount++);
		}
	}
	if (!complete)
	{
		if (FirstAttempt)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Right Down"));
			//UE_LOG(LogTemp, Warning, TEXT("Attempting Down Right"));
			LoopCount = 0;
			DownRight(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Right Down"));
			return;
		}
	}
}

void ADungeonGenerator::DownRight(bool FirstAttempt, int& LoopCount, const FIntVector& RoomB, FIntVector* RoomBExtent, const FIntVector& RoomA, FIntVector* RoomAExtent, FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	bool complete = false;
	while (LoopCount <= MaxLoops)
	{
		// Corridor from Corner to A (X), Corner to B (Y)
		int OutX = Stream.RandRange(RoomB.X, RoomBExtent->X);
		int OutY = Stream.RandRange(RoomA.Y, RoomAExtent->Y);
		PointRoomA = FIntVector(RoomA.X, OutY, RoomB.Z);
		PointRoomB = FIntVector(OutX, RoomB.Y, RoomA.Z);
		PointCorner = FIntVector(OutX, OutY, RoomB.Z);
		DebugBoxesWCorners(PointRoomA, PointRoomB, PointCorner);
		if (FloorTiles.Contains(PointRoomA) && FloorTiles.Contains(PointRoomB))
		{
			CorridorTiles.Add(PointCorner);
			MakeXCorridor(PointCorner, PointRoomA);
			MakeYCorridor(PointCorner, PointRoomB);
			complete = true;
			break;
		}
		else
		{
			LoopCount++;
			//UE_LOG(LogTemp, Warning, TEXT("Failure %d"), LoopCount++);
		}
	}
	if (!complete)
	{
		if (FirstAttempt)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Down Right"));
			//UE_LOG(LogTemp, Warning, TEXT("Attempting Right Down"));
			LoopCount = 0;
			RightDown(false, LoopCount, RoomA, RoomAExtent, RoomB, RoomBExtent, PointRoomA, PointRoomB, PointCorner);
		}
		else
		{
			//UE_LOG(LogTemp, Warning, TEXT("Failure to build Corridor Down Right"));
			return;
		}
	}
}


void ADungeonGenerator::MakeYCorridor(const FIntVector From, const FIntVector To)
{
	for (int32 i = 1; i < FMath::Abs(From.Y - To.Y); i++)
	{
		//DrawDebugBox(GetWorld(), FVector(From.X, From.Y + i, From.Z) * Scale, FVector(50, 50, 50), FColor::Red, true, -1.0f, 0U, 10);
		//UE_LOG(LogTemp, Warning, TEXT("%d"), From.Y + i);
		FIntVector NewTile = FIntVector(From.X, From.Y + i, From.Z);
		if (!FloorTiles.Contains(NewTile))
			CorridorTiles.Add(NewTile);
	}
}

void ADungeonGenerator::MakeXCorridor(const FIntVector From, const FIntVector To)
{
	for (int32 i = 1; i < FMath::Abs(From.X - To.X); i++)
	{
		//DrawDebugBox(GetWorld(), FVector(From.X + i, From.Y, From.Z) * Scale, FVector(50, 50, 50), FColor::Orange, true, -1.0f, 0U, 10);
		//UE_LOG(LogTemp, Warning, TEXT("%d"), From.X + i);
		FIntVector NewTile = FIntVector(From.X + i, From.Y, From.Z);
		if(!FloorTiles.Contains(NewTile))
		CorridorTiles.Add(NewTile);
	}
}

#pragma endregion CornerCorridors



//
//UE_LOG(LogTemp, Warning, TEXT("%d"), Tiles.Num());






void ADungeonGenerator::DebugBoxes(FIntVector& PointRoomA, FIntVector& PointRoomB)
{
	//DrawDebugBox(GetWorld(), FVector(PointRoomA.X, PointRoomA.Y, PointRoomA.Z) * Scale, FVector(50, 50, 50), FColor::Blue, true, -1.0f, 0U, 15);
	//DrawDebugBox(GetWorld(), FVector(PointRoomB.X, PointRoomB.Y, PointRoomB.Z) * Scale, FVector(50, 50, 50), FColor::Green, true, -1.0f, 0U, 15);
}

void ADungeonGenerator::DebugBoxesWCorners(FIntVector& PointRoomA, FIntVector& PointRoomB, FIntVector& PointCorner)
{
	//DrawDebugBox(GetWorld(), FVector(PointRoomA.X, PointRoomA.Y, PointRoomA.Z) * Scale, FVector(50, 50, 50), FColor::Blue, true, -1.0f, 0U, 15);
	//DrawDebugBox(GetWorld(), FVector(PointRoomB.X, PointRoomB.Y, PointRoomB.Z) * Scale, FVector(50, 50, 50), FColor::Green, true, -1.0f, 0U, 15);
	//DrawDebugBox(GetWorld(), FVector(PointCorner.X, PointCorner.Y, PointCorner.Z) * Scale, FVector(50, 50, 50), FColor::Purple, true, -1.0f, 0U, 15);
}


