// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Camera/CameraActor.h"
#include "HSMExporter.generated.h"

UCLASS()
class HYPERSYNMOTION_API AHSMExporter : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHSMExporter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	//generation data
	UPROPERTY(EditAnywhere, Category = "Gegerated scene json file")
	FString file_name;

	UPROPERTY(EditAnywhere, Category = "Gegerated scene json file")
	FString Output_path;

	//Export data
	UPROPERTY(EditAnywhere, Category = "Export data");
	bool bScene = false;

	UPROPERTY(EditAnywhere, Category = "Export data")
	bool bMetahumans = false;

	UPROPERTY(EditAnywhere, Category = "Export data")
	bool bCameras = false;

	//template function to get actors by type
	template<typename ActorType>
	TArray<ActorType*> GetActorByType(const UWorld* currentworld)
	{
		TArray<ActorType*> ActorsOfType;

		// Iterar los actores del mundo filtrando por ActorType
		for (TActorIterator<ActorType> It(currentworld); It; ++It)
		{
			ActorType* Actor = *It; // Aqu� declaramos la variable Actor correctamente
			//obtener los gaussian splat
			if (Actor->IsA<AActor>())
			{
				if (Actor->GetClass()->GetName() == "MeetingRoom_GaussianSplatLuma_C") {
					UE_LOG(LogTemp, Warning, TEXT("Found Actor: %s and parent: %s"), *Actor->GetName(), *Actor->GetClass()->GetName());
					ActorsOfType.Add(Actor);
				}
			}
			else {
				ActorsOfType.Add(Actor);
			}
			
		}

		return ActorsOfType;
	}

	// Creation json files functions
	void GenerateSceneJson(const UWorld* world, TArray<AActor*> scenes);

	void GenerateCamerasJson(const UWorld* world, TArray<ACameraActor*> scenes);

	void GenerateMetaHumanJson(const UWorld* world, TArray<ACharacter*> scenes);

	void GenerateCamerasAndMetaHumanJson(const UWorld* world);
};
