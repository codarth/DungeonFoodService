// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DungeonSpawn_Component.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DUNGEONFOODSERVICE_API UDungeonSpawn_Component : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDungeonSpawn_Component();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	
	UPROPERTY(VisibleAnywhere, Category = References)
		class ADungeonGenerator* DungeonREF;
	UPROPERTY(EditAnywhere, Category = SpawnSettings)
		int32 Quantity;
	UPROPERTY(EditAnywhere, Category = SpawnSettings)
		bool RoomsOnly;
	UPROPERTY(EditAnywhere, Category = SpawnSettings)
		FVector Offset = FVector(0.f, 0.f, 10.f);
	UPROPERTY(EditAnywhere, Category = SpawnSettings)
		TMap<AActor*, float> SpawnList;
	UPROPERTY(VisibleAnywhere, Category = References)
		TArray<FIntVector> Floors;

	UFUNCTION()
		void TriggerSpawnThings();

	UFUNCTION()
		void SpawnThings();

};
