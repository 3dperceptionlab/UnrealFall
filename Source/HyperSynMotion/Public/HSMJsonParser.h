// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
// Import "Json.h" in UE5
#include "Json.h"
#include "HSMJsonParser.generated.h"

USTRUCT()
struct FHSMActorState
{
	GENERATED_USTRUCT_BODY()

	FVector Position;
	FRotator Rotation;

	FHSMActorState()
	{}
};

USTRUCT()
struct FHSMActorStateExtended
{
	GENERATED_USTRUCT_BODY()

	FVector Position;
	FRotator Rotation;
	FVector BoundingBox_Min;
	FVector BoundingBox_Max;

	FHSMActorStateExtended()
	{}
};

USTRUCT()
struct FHSMSkeletonState
{
	GENERATED_USTRUCT_BODY()

	FVector Position;
	FRotator Rotation;
	TMap<FString, FHSMActorState> Bones;

	FHSMSkeletonState()
	{}
};

USTRUCT()
struct FHSMCameraState
{
	GENERATED_USTRUCT_BODY()

	int stereo;
	float fov;
	FVector Position;
	FRotator Rotation;
	TArray<FString> FilePaths;
	//Create a TArray of transforms for the camera (4x4 float matrix )
	TArray<FMatrix> Transforms;



	FHSMCameraState()
	{}
};

USTRUCT()
struct FHSMFrame
{
	GENERATED_USTRUCT_BODY()

		int n_frame;
	float time_stamp;
	TMap<FString, FHSMActorState> Cameras;
	TMap<FString, FHSMActorStateExtended> Objects;
	TMap<FString, FHSMSkeletonState> Skeletons;

	FHSMFrame()
	{}
};


/**
 * 
 */
class HYPERSYNMOTION_API HSMJsonParser
{
public:
	HSMJsonParser();
	~HSMJsonParser();

	bool LoadFile(FString filename);
	bool LoadSceneFile(FString filename,UObject* hsmtracker);
	FHSMFrame GetFrameData(uint64 nFrame);
	static FString IntToStringDigits(int i, int nDigits);
	static TArray<TSharedPtr<FJsonValue>> MatrixToArray(FMatrix matrix);
	static void SceneTxtToJson(FString path, FString txt_filename, FString json_filename);

	FORCEINLINE uint64 GetNumFrames() const
	{
		return NumFrames;
	}

	FORCEINLINE FString GetSequenceName() const
	{
		return SequenceName;
	}

	FORCEINLINE FString GetSceneFileName() const
	{
		return scene_file_name;
	}

	FORCEINLINE float GetTotalTime() const
	{
		return TotalTime;
	}

	FORCEINLINE float GetMeanFramerate() const
	{
		return MeanFramerate;
	}

	FORCEINLINE TArray<FString> GetCameraNames() const
	{
		return CameraNames;
	}

	FORCEINLINE TArray<FString> GetPawnNames() const
	{
		return PawnNames;
	}

	FORCEINLINE TArray<FString> GetAnimationNames() const
	{
		return AnimationNames;
	}

	FHSMSkeletonState GetSkeletonState(FString name) const;
	FHSMCameraState GetCameraState(FString name) const;
	FString GetAnimationName(FString name) const;
	


protected:
	uint64 NumFrames;
	FString SequenceName;
	FString scene_file_name;
	float TotalTime;
	float MeanFramerate;

	TArray<FString> PawnNames;
	TArray<FString> CameraNames;
	TArray<FString> AnimationNames;

	TArray<std::pair<FString, FString>> AnimationsArray;
	TArray<TSharedPtr<FJsonValue>> CamerasJsonArray;
	TArray<TSharedPtr<FJsonValue>> FramesJsonArray;
	TArray<std::pair<TSharedPtr<FJsonValue>, FString>> PawnsJsonArray;

};
