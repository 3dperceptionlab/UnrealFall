// Fill out your copyright notice in the Description page of Project Settings.


#include "AHSMTracker.h"
// in UE4 #include "DateTime.h" in UE5 
#include "Misc/DateTime.h"
#include "TimerManager.h"
#include "Engine/PostProcessVolume.h"
//#include "ROXObjectPainter.h"
//#include "ROXTypes.h"
// in UE4 #include "CommandLine.h"
#include "Misc/CommandLine.h"

// TO include the FJsonSerializer class in UE5 
#include "Serialization/JsonSerializer.h"

// Sets default values
AHSMTracker::AHSMTracker() :
bIsRecording(false),
bRecordMode(false),
bStandaloneMode(false),
Persistence_Level_Filter_Str("UEDPIE_0"),
bDebugMode(false),
initial_delay(2.0f),
place_cameras_delay(.1f),
first_viewmode_of_frame_delay(.1f),
change_viewmode_delay(.2f),
take_screenshot_delay(.1f),
scene_folder("SceneText"),
screenshots_folder("Screenshots"),
scene_file_name_prefix("scene"),
input_scene_TXT_file_name("scene"),
output_scene_json_file_name("scene"),
generate_rgb(true),
format_rgb(EHSMRGBImageFormats::RIF_JPG95),
fps_anim(30.0f),
generate_depth(true),
generate_object_mask(true),
generate_normal(true),
generate_depth_txt_cm(false),
screenshot_width(1920),
screenshot_height(1080),
frame_status_output_period(100),
fileHeaderWritten(false),
numFrame(0),
CurrentViewmode(EHSMViewMode_First),
CurrentViewTarget(0),
CurrentCamRebuildMode(0),
CurrentJsonFile(0),
JsonReadStartTime(0),
LastFrameTime(0)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = bRecordMode;
	PrimaryActorTick.bStartWithTickEnabled = true;

	DepthMat = nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterial> matDepth(TEXT("/Game/Common/ViewModeMats/SceneDepth.SceneDepth"));
	if (matDepth.Succeeded())
	{
		DepthMat = (UMaterial*)matDepth.Object;
	}

	DepthWUMat = nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterial> matDepthWU(TEXT("/Game/Common/ViewModeMats/SceneDepthWorldUnits.SceneDepthWorldUnits"));
	if (matDepthWU.Succeeded())
	{
		DepthWUMat = (UMaterial*)matDepthWU.Object;
	}

	DepthCmMat = nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterial> matDepthCm(TEXT("/Game/Common/ViewModeMats/SceneDepth_CmToGray.SceneDepth_CmToGray"));
	if (matDepthCm.Succeeded())
	{
		DepthCmMat = (UMaterial*)matDepthCm.Object;
	}

	NormalMat = nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterial> matNormal(TEXT("/Game/Common/ViewModeMats/WorldNormal.WorldNormal"));
	if (matNormal.Succeeded())
	{
		NormalMat = (UMaterial*)matNormal.Object;
	}

	SegmentMat = nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterial> matSegment(TEXT("/Game/Common/ViewModeMats/Segmentation.Segmentation"));
	if (matNormal.Succeeded())
	{
		SegmentMat = (UMaterial*)matSegment.Object;
	}

	DepthTextureRenderer = nullptr;
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> sceneCapture(TEXT("/Game/Common/ViewModeMats/RT_SceneDepth.RT_SceneDepth"));
	if (sceneCapture.Succeeded())
	{
		DepthTextureRenderer = (UTextureRenderTarget2D*)sceneCapture.Object;
	}

	json_file_names.Add("scene");
	start_frames.Add(0);
	scene_save_directory = FPaths::ProjectUserDir();
	scene_charge_directory = FPaths::ProjectUserDir();
	screenshots_save_directory = FPaths::ProjectUserDir();
	absolute_file_path = scene_save_directory + scene_folder + "/" + scene_file_name_prefix + ".txt";
}

// Called when the game starts or when spawned
void AHSMTracker::BeginPlay()
{
	Super::BeginPlay();

	// PPX init
	GameShowFlags = new FEngineShowFlags(GetWorld()->GetGameViewport()->EngineShowFlags);
	//FROXObjectPainter::Get().Reset(GetLevel()); UE4
	FEngineShowFlags ShowFlags = GetWorld()->GetGameViewport()->EngineShowFlags;

	//screenshot resolution
	GScreenshotResolutionX = screenshot_width; // 1920  1280
	GScreenshotResolutionY = screenshot_height;  // 1080  720

	for (APawn* pawn : Pawns)
	{
		//UE_LOG(LogTemp, Warning, TEXT("%s"),*pawn->GetFName().ToString());
		// pawn->InitFromTracker(bRecordMode, bDebugMode, this); UE4
		ViewTargets.Add(pawn);

		if (pawn->GetController())
		{
			ControllerPawn = pawn;
			//ControllerPawn->isRecordMode(); UE4
		}
	} 

	for (ACameraActor* Cam : CameraActors)
	{
		ViewTargets.Add(Cam);
	}

	EHSMViewModeList.Empty();
	if (generate_rgb) EHSMViewModeList.Add(EHSMViewMode::RVM_Lit);
	if (generate_depth || generate_depth_txt_cm) EHSMViewModeList.Add(EHSMViewMode::RVM_Depth);
	if (generate_object_mask) EHSMViewModeList.Add(EHSMViewMode::RVM_ObjectMask);
	if (generate_normal) EHSMViewModeList.Add(EHSMViewMode::RVM_Normal);

	if (EHSMViewModeList.Num() == 0) EHSMViewModeList.Add(EHSMViewMode::RVM_Lit);
	EHSMViewMode_First = EHSMViewModeList[0];
	EHSMViewMode_Last = EHSMViewModeList.Last();

	if (bDebugMode) {
		initial_delay = 1.0;
		place_cameras_delay = 0.1;
		first_viewmode_of_frame_delay = 0.05;
		change_viewmode_delay = 0.3;
		take_screenshot_delay = 0.1;
	}


	if (!bRecordMode)
	{
		// Spawn SceneCapture2D for capture Depth data
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActorSpawnParams.bDeferConstruction = true;
		ActorSpawnParams.Name = FName("SceneCaptureDepth");
		SceneCapture_depth = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), ActorSpawnParams);
		SceneCapture_depth->GetCaptureComponent2D()->TextureTarget = DepthTextureRenderer;
		SceneCapture_depth->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;


		while (json_file_names.Num() > start_frames.Num())
		{
			start_frames.Add(0);
		}

		RebuildModeBegin();
	}
	else
	{
		PrintInstanceClassJson();
	}
}

void AHSMTracker::PrintInstanceClassJson()
{
	FString instance_class_json;
	FString instance_class_json_path = scene_save_directory + scene_folder + "/instance_class.json";
	bool file_found = FFileHelper::LoadFileToString(instance_class_json, *instance_class_json_path);
	if (!file_found)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

		for (TObjectIterator<AStaticMeshActor> Itr; Itr; ++Itr)
		{
			FString fullName = Itr->GetFullName();
			if (bStandaloneMode || fullName.Contains(Persistence_Level_Filter_Str))
			{
				JsonObject->SetStringField(Itr->GetName(), "none");
			}
		}

		// Write JSON file
		instance_class_json = "";
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&instance_class_json);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(instance_class_json, *instance_class_json_path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);

		FString success_message("JSON file instance_class.json has been created successfully.");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *success_message);
	}
	else
	{
		FString error_message("JSON file instance_class.json already exists. Please, check and remove or rename that file in order to create a new one.");
		UE_LOG(LogTemp, Warning, TEXT("%s"), *error_message);
	}
}


// Called every frame
void AHSMTracker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GEngine)
	{
		if (bRecordMode && bIsRecording)
		{
			WriteScene();
		}
	}
}

void AHSMTracker::WriteHeader()
{
	//Camera Info
	FString begin_string_ = "Cameras " + FString::FromInt(CameraActors.Num()) + "\r\n";
	for (ACameraActor* CameraActor : CameraActors)
	{
		/* TODO: stereo dist and fov will be part of the functionality added in a near future */
		float stereo_dist(0.0f);
		float field_of_view(CameraActor->GetCameraComponent()->FieldOfView);
		/* TODO: stereo dist and fov will be part of the functionality added in a near future */
		begin_string_ += CameraActor->GetName() + " " + FString::SanitizeFloat(stereo_dist) + " " + FString::SanitizeFloat(field_of_view) + "\r\n";
	}

	// Movable StaticMeshActor dump
	CacheStaticMeshActors();
	begin_string_ += "Objects " + FString::FromInt(CachedSM.Num()) + "\r\n";

	// Pawns dump
	FString skeletaldump("");
	for (APawn* rbp : Pawns)
	{
		//skeletaldump += rbp->GetActorLabel() + " " + FString::FromInt(rbp->GetMeshComponent()->GetAllSocketNames().Num()) + "\r\n"; UE4
	}
	begin_string_ += "Skeletons " + FString::FromInt(Pawns.Num()) + "\r\n" + skeletaldump;

	// Non-movable StaticMeshActor dump
	FString nonmovabledump("");
	int n_nonmovable = 0;
	for (TObjectIterator<AStaticMeshActor> Itr; Itr; ++Itr)
	{
		FString fullName = Itr->GetFullName();
		if (!Itr->ActorHasTag(FName("trojan")) && (fullName.Contains(Persistence_Level_Filter_Str) || bStandaloneMode || bDebugMode) && Itr->GetStaticMeshComponent()->Mobility != EComponentMobility::Movable)
		{
			n_nonmovable++;
			FString actor_name_ = Itr->GetName();
			FString actor_full_name_ = Itr->GetFullName();
			FVector actor_location_ = Itr->GetActorLocation();
			FRotator actor_rotation_ = Itr->GetActorRotation();

			nonmovabledump += actor_name_ + " " + actor_location_.ToString() + " " + actor_rotation_.ToString()
				+ " MIN:" + Itr->GetComponentsBoundingBox(true).Min.ToString() + " MAX:" + Itr->GetComponentsBoundingBox(true).Max.ToString() +
				((bDebugMode) ? (" " + actor_full_name_ + "\r\n") : "\r\n");
		}
	}
	begin_string_ += "NonMovableObjects " + FString::FromInt(n_nonmovable) + "\r\n" + nonmovabledump;


	(new FAutoDeleteAsyncTask<FWriteStringTask>(begin_string_, absolute_file_path))->StartBackgroundTask();
}

void AHSMTracker::WriteScene()
{
	//GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, "Recording..");
	FString tick_string_ = "frame\r\n";
	float time_ = UGameplayStatics::GetRealTimeSeconds(GetWorld());
	FString time_string_ = FString::SanitizeFloat(time_ * 1000.0f);
	tick_string_ += FString::FromInt(numFrame) + " " + time_string_ + "\r\n";
	numFrame++;

	// Camera dump
	FString tick_string_cam_aux_ = "";
	int n_cameras = 0;
	for (ACameraActor* CameraActor : CameraActors)
	{
		if (IsValid(CameraActor))
		{
			FString camera_name_ = CameraActor->GetName();
			FVector camera_location_ = CameraActor->GetActorLocation();
			FRotator camera_rotation_ = CameraActor->GetActorRotation();

			FString camera_string_ = camera_name_ + " " + camera_location_.ToString() + " " + camera_rotation_.ToString();
			//FString camera_string_ = "Camera " + camera_location_.ToString() + " " + camera_rotation_.ToString();
			if (bDebugMode)
			{
				camera_string_ += " " + CameraActor->GetFullName();
			}
			tick_string_cam_aux_ += camera_string_ + "\r\n";
		}
	}
	//tick_string_ += "Cameras " + FString::FromInt(n_cameras) + "\r\n" + tick_string_cam_aux_;
	tick_string_ += tick_string_cam_aux_;



	FString ObjectsString("objects\r\n");
	FString SkeletonsString("skeletons\r\n");

	// We cannot assume all the pawns will have a skeleton, only those inheriting our type will
	/*for (auto rbp : Pawns)
	{
		// Same as with the Object Iterator, access the subclass instance with the * or -> operators.
		TArray<FName> sckt_name = rbp->GetMeshComponent()->GetAllSocketNames();

		SkeletonsString += rbp->GetActorLabel() + " " + rbp->GetActorLocation().ToString() + " " +
			rbp->GetActorRotation().ToString() + "\r\n";

		for (FName scktnm : sckt_name)
		{
			FTransform sckttrans(rbp->GetMeshComponent()->GetSocketTransform(scktnm));
			SkeletonsString += scktnm.ToString() + " " + sckttrans.GetLocation().ToString() + " " + sckttrans.Rotator().ToString() +
				+" MIN:" + FVector::ZeroVector.ToString() + " MAX:" + FVector::ZeroVector.ToString() + "\r\n";
		}
	} UE4 */

	// StaticMeshActor dump
	for (auto Itr : CachedSM)
	{
		FString actor_name_ = Itr->GetName();
		FString actor_full_name_ = Itr->GetFullName();
		FVector actor_location_ = Itr->GetActorLocation();
		FRotator actor_rotation_ = Itr->GetActorRotation();

		ObjectsString += actor_name_ + " " + actor_location_.ToString() + " " + actor_rotation_.ToString()
			+ " MIN:" + Itr->GetComponentsBoundingBox(true).Min.ToString() + " MAX:" + Itr->GetComponentsBoundingBox(true).Max.ToString() +
			((bDebugMode) ? (" " + actor_full_name_ + "\r\n") : "\r\n");
	}
	tick_string_ += ObjectsString + SkeletonsString;
	(new FAutoDeleteAsyncTask<FWriteStringTask>(tick_string_, absolute_file_path))->StartBackgroundTask();
}

void AHSMTracker::GenerateSequenceJson()
{
	FString path = scene_save_directory + scene_folder;
	HSMJsonParser::SceneTxtToJson(path, input_scene_TXT_file_name, output_scene_json_file_name);
}

void AHSMTracker::ExportMatrixArrayToJSON(FString file_path, TArray<FMatrix> matrixArray){

	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	//Create a Array JSON object
	auto matrixArrayObject = TArray<TSharedPtr<FJsonValue>>();

	for ( int i = 0; i < matrixArray.Num(); i++){
		//Create a JSON value 
		TSharedPtr<FJsonObject> matrixObject = MakeShareable(new FJsonObject());

		matrixObject->SetStringField("file_path", "images/" + HSMJsonParser::IntToStringDigits(i, 6) + ".jpg");
		matrixObject->SetArrayField("transform_matrix", HSMJsonParser::MatrixToArray(matrixArray[i]));
		matrixArrayObject.Add(MakeShareable(new FJsonValueObject(matrixObject)));
	}
	JsonObject->SetArrayField("trajectory",matrixArrayObject);

	// Write JSON file
	FString text = "";
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&text);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(text, *file_path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);

	FString success_message("JSON file "+ file_path + " has been created successfully.");
	UE_LOG(LogTemp, Warning, TEXT("%s"), *success_message);
}

void AHSMTracker::ToggleRecording()
{
	bIsRecording = !bIsRecording;
	numFrame = 0;

	if (bRecordMode && bIsRecording && !fileHeaderWritten)
	{
		fileHeaderWritten = true;
		absolute_file_path = scene_save_directory + scene_folder + "/" + scene_file_name_prefix + "_" + GetDateTimeString() + ".txt";
		WriteHeader();
	}
	else if (!bIsRecording)
	{
		fileHeaderWritten = false;
	}
}

FString AHSMTracker::GetDateTimeString()
{
	return 	FDateTime::Now().ToString(TEXT("%Y%m%d%-%H%M%S"));
}

/*************************************************************************************/
/*************************************************************************************/
/*************************************************************************************/

APostProcessVolume* AHSMTracker::GetPostProcessVolume()
{
	UWorld* World = GetWorld();
	static APostProcessVolume* PostProcessVolume = nullptr;
	static UWorld* CurrentWorld = nullptr; // Check whether the world has been restarted.
	if (PostProcessVolume == nullptr || CurrentWorld != World)
	{
		PostProcessVolume = World->SpawnActor<APostProcessVolume>();
		PostProcessVolume->bUnbound = true;
		CurrentWorld = World;
	}
	return PostProcessVolume;
}

void AHSMTracker::SetVisibility(FEngineShowFlags& Target, FEngineShowFlags& Source)
{
	Target.SetStaticMeshes(Source.StaticMeshes);
	Target.SetLandscape(Source.Landscape);
	Target.SetInstancedFoliage(Source.InstancedFoliage);
	Target.SetInstancedGrass(Source.InstancedGrass);
	Target.SetInstancedStaticMeshes(Source.InstancedStaticMeshes);
	Target.SetSkeletalMeshes(Source.SkeletalMeshes);
}

void AHSMTracker::VertexColor(FEngineShowFlags& ShowFlags)
{
	FEngineShowFlags PreviousShowFlags(ShowFlags); // Store previous ShowFlags
	ApplyViewMode(VMI_Lit, true, ShowFlags);

	// From MeshPaintEdMode.cpp:2942
	ShowFlags.SetMaterials(false);
	ShowFlags.SetLighting(false);
	ShowFlags.SetBSPTriangles(true);
	ShowFlags.SetVertexColors(true);
	ShowFlags.SetPostProcessing(false);
	ShowFlags.SetHMDDistortion(false);
	ShowFlags.SetTonemapper(false); // This won't take effect here

	GVertexColorViewMode = EVertexColorViewMode::Color;
	SetVisibility(ShowFlags, PreviousShowFlags); // Store the visibility of the scene, such as folliage and landscape.
}

void AHSMTracker::PostProcess(FEngineShowFlags& ShowFlags)
{
	FEngineShowFlags PreviousShowFlags(ShowFlags); // Store previous ShowFlags

	// Basic Settings
	ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_All0);
	ShowFlags.SetRendering(true);
	ShowFlags.SetStaticMeshes(true);

	ShowFlags.SetMaterials(true);
	// These are minimal setting
	ShowFlags.SetPostProcessing(true);
	ShowFlags.SetPostProcessMaterial(true);
	// ShowFlags.SetVertexColors(true); // This option will change object material to vertex color material, which don't produce surface normal

	GVertexColorViewMode = EVertexColorViewMode::Color;
	SetVisibility(ShowFlags, PreviousShowFlags); // Store the visibility of the scene, such as folliage and landscape.
}

void AHSMTracker::Lit()
{
	UWorld* World = GetWorld();
	GetPostProcessVolume()->BlendWeight = 0;

	if (GameShowFlags == nullptr)
	{
		return;
	}
	World->GetGameViewport()->EngineShowFlags = *GameShowFlags;

	CurrentViewmode = EHSMViewMode::RVM_Lit;
}

void AHSMTracker::Object()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World->GetGameViewport();

	PostProcess(Viewport->EngineShowFlags);


	APostProcessVolume* PostProcessVolume = GetPostProcessVolume();
	PostProcessVolume->Settings.WeightedBlendables.Array.Empty();
	PostProcessVolume->Settings.AddBlendable(SegmentMat, 1);
	PostProcessVolume->BlendWeight = 1;

	CurrentViewmode = EHSMViewMode::RVM_ObjectMask;
}

void AHSMTracker::Depth()
{
	UWorld* World = GetWorld();
	UGameViewportClient* GameViewportClient = World->GetGameViewport();
	FSceneViewport* SceneViewport = GameViewportClient->GetGameViewport();

	PostProcess(GameViewportClient->EngineShowFlags);

	APostProcessVolume* PostProcessVolume = GetPostProcessVolume();
	PostProcessVolume->Settings.WeightedBlendables.Array.Empty();
	PostProcessVolume->Settings.AddBlendable(DepthMat, 1);
	PostProcessVolume->BlendWeight = 1;

	CurrentViewmode = EHSMViewMode::RVM_Depth;
}

void AHSMTracker::Normal()
{
	UWorld* World = GetWorld();
	UGameViewportClient* GameViewportClient = World->GetGameViewport();
	FSceneViewport* SceneViewport = GameViewportClient->GetGameViewport();

	PostProcess(GameViewportClient->EngineShowFlags);

	APostProcessVolume* PostProcessVolume = GetPostProcessVolume();
	PostProcessVolume->Settings.WeightedBlendables.Array.Empty();
	PostProcessVolume->Settings.AddBlendable(NormalMat, 1);
	PostProcessVolume->BlendWeight = 1;

	CurrentViewmode = EHSMViewMode::RVM_Normal;
}

/*******************************************************/

AActor* AHSMTracker::CameraNext()
{
	CurrentViewTarget++;
	if (CurrentViewTarget > ViewTargets.Num() - 1)
	{
		CurrentViewTarget = 0;
	}
	return (ViewTargets[CurrentViewTarget]);
}

AActor* AHSMTracker::CameraPrev()
{
	CurrentViewTarget--;
	if (CurrentViewTarget < 0)
	{
		CurrentViewTarget = ViewTargets.Num() - 1;
	}
	return (ViewTargets[CurrentViewTarget]);
}

void AHSMTracker::TakeScreenshot(EHSMViewMode vm)
{
	FString screenshot_filename = screenshots_save_directory + screenshots_folder + "/" + FDateTime::Now().ToString(TEXT("%Y%m%d%-%H%M%S%s"));
	if (vm != EHSMViewMode::RVM_Depth)
	{
		HighResSshot(GetWorld()->GetGameViewport(), screenshot_filename, vm);
	}
	else
	{
		TakeDepthScreenshotFolder(screenshot_filename);
	}
}

void AHSMTracker::TakeScreenshotFolder(EHSMViewMode vm, FString CameraName)
{
	FString screenshot_filename = screenshots_save_directory + screenshots_folder + "/" + json_file_names[CurrentJsonFile] + "/" + ViewmodeString(vm) + "/" + CameraName + "/" +HSMJsonParser::IntToStringDigits(numFrame, 6);
	if (true || vm != EHSMViewMode::RVM_Depth)
	{
		HighResSshot(GetWorld()->GetGameViewport(), screenshot_filename, vm);
	}
	else
	{
		TakeDepthScreenshotFolder(screenshot_filename);
	}
}

void AHSMTracker::HighResSshot(UGameViewportClient* ViewportClient, const FString& FullFilename, const EHSMViewMode viewmode)
{
	EHSMRGBImageFormats rgb_image_format = format_rgb;
	int jpg_quality = 0;
	switch (rgb_image_format)
	{
	case EHSMRGBImageFormats::RIF_PNG: jpg_quality = 0;
		break;
	case EHSMRGBImageFormats::RIF_JPG95: jpg_quality = 95;
		break;
	case EHSMRGBImageFormats::RVM_JPG80: jpg_quality = 80;
		break;
	default: jpg_quality = 0;
		break;
	}

	ViewportClient->Viewport->TakeHighResScreenShot();
	ViewportClient->OnScreenshotCaptured().Clear();
	ViewportClient->OnScreenshotCaptured().AddLambda(
		[FullFilename, viewmode, rgb_image_format, jpg_quality](int32 SizeX, int32 SizeY, const TArray<FColor>& Bitmap)
		{
			TArray<FColor>& RefBitmap = const_cast<TArray<FColor>&>(Bitmap);
			TArray<uint8> RGBData8Bit;
			for (auto& Color : RefBitmap)
			{
				Color.A = 255; // Make sure that all alpha values are opaque.
				RGBData8Bit.Add(Color.R);
				RGBData8Bit.Add(Color.G);
				RGBData8Bit.Add(Color.B);
				RGBData8Bit.Add(Color.A);
			}

			TArray<uint8> ImgData;
			FString FullFilenameExtension;
			if (viewmode == EHSMViewMode::RVM_Lit && (rgb_image_format == EHSMRGBImageFormats::RVM_JPG80 || rgb_image_format == EHSMRGBImageFormats::RIF_JPG95))
			{
				static IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
				static TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
				ImageWrapper->SetRaw(RGBData8Bit.GetData(), RGBData8Bit.GetAllocatedSize(), SizeX, SizeY, ERGBFormat::RGBA, 8);
				ImgData = ImageWrapper->GetCompressed(jpg_quality);
				FullFilenameExtension = FullFilename + ".jpg";
			}
			else
			{
				TArray<uint8> CompressedBitmap;
				FImageUtils::CompressImageArray(SizeX, SizeY, RefBitmap, ImgData);
				FullFilenameExtension = FullFilename + ".png";
			}

			(new FAutoDeleteAsyncTask<FScreenshotTask>(ImgData, FullFilenameExtension))->StartBackgroundTask();
		});
}

void AHSMTracker::TakeDepthScreenshotFolder(const FString& FullFilename)
{
	//SceneCapture_depth;
	int32 Width = DepthTextureRenderer->SizeX, Height = DepthTextureRenderer->SizeY;
	TArray<FFloat16Color> ImageData;
	FTextureRenderTargetResource* RenderTargetResource;
	ImageData.AddUninitialized(Width * Height);
	RenderTargetResource = DepthTextureRenderer->GameThread_GetRenderTargetResource();
	RenderTargetResource->ReadFloat16Pixels(ImageData);

	if (ImageData.Num() != 0 && ImageData.Num() == Width * Height)
	{
		if (generate_depth)
		{
			TArray<uint16> Grayscaleuint16Data;

			for (auto px : ImageData)
			{
				// Max value float16: 65504.0 -> It is cm, so it can represent up to 655.04m
				// Max value uint16: 65535 (65536 different values) -> It is going to be mm, so it can represent up to 65.535m - 6553.5cm
				float pixelCm = px.R.GetFloat();
				if (pixelCm > 6553.4f || pixelCm < 0.3f)
				{
					Grayscaleuint16Data.Add(0);
				}
				else
				{
					float pixelMm = pixelCm * 10.0f;
					Grayscaleuint16Data.Add((uint16)floorf(pixelMm + 0.5f));
				}
			}

			//UE print allocated size and size
			UE_LOG(LogTemp, Warning, TEXT("Grayscaleuint16Data allocated size: %d"), Grayscaleuint16Data.GetAllocatedSize());
			UE_LOG(LogTemp, Warning, TEXT("Grayscaleuint16Data size: %d"), Grayscaleuint16Data.Num());
			
			// Save Png Monochannel 16bits
			static IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			static TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

			int32 bytesPerRow = Width * 2;
		
			ImageWrapper->SetRaw(Grayscaleuint16Data.GetData(), Grayscaleuint16Data.Num()*2, Width, Height, ERGBFormat::Gray, 16, bytesPerRow);
			//Check if the image wrapper is valid
			if (!ImageWrapper.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("ImageWrapper is not valid"));
				return;
			}

			const TArray64<uint8>& ImgGrayData = ImageWrapper->GetCompressed(100);

			//Convert TArray64<uint8> to TArray<uint8>
			const TArray<uint8>& ImgGrayData2 = (const TArray<uint8>&)ImgGrayData;
			
			//The above line is not working, 'inicializando': no se puede realizar la conversi�n de 'const TArray<uint8,FDefaultAllocator64>' a 'const TArray<uint8,FDefaultAllocator> &'
			//So, we have to do this: apply conversion from 'const TArray<uint8,FDefaultAllocator64>' to 'const TArray<uint8,FDefaultAllocator> &
			//TArray<uint8> ImgGrayData;

			//for (uint8 i = 0; i < ImageWrapper->GetCompressed().Num(); i++)
			//{
			//	ImgGrayData.Add(ImageWrapper->GetCompressed()[i]);
			//}



			(new FAutoDeleteAsyncTask<FScreenshotTask>(ImgGrayData2, FullFilename + ".png"))->StartBackgroundTask();
		}

		if (generate_depth_txt_cm)
		{
			FString DepthCm("");
			for (auto px : ImageData)
			{
				DepthCm += FString::SanitizeFloat(px.R.GetFloat()) + "\n";
			}
			(new FAutoDeleteAsyncTask<FWriteStringTask>(DepthCm, FullFilename + ".txt"))->StartBackgroundTask();
		}
	}
}

/**********************************************************/
void AHSMTracker::ChangeViewmode(EHSMViewMode vm)
{
	switch (vm)
	{
	case EHSMViewMode::RVM_Lit: Lit();
		break;
	case EHSMViewMode::RVM_Depth: Depth();
		break;
	case EHSMViewMode::RVM_ObjectMask: Object();
		break;
	case EHSMViewMode::RVM_Normal: Normal();
		break;
	}
}

FString AHSMTracker::ViewmodeString(EHSMViewMode vm)
{
	FString res("rgb");
	switch (vm)
	{
	case EHSMViewMode::RVM_Lit: res = "rgb";
		break;
	case EHSMViewMode::RVM_Depth: res = "depth";
		break;
	case EHSMViewMode::RVM_ObjectMask: res = "mask";
		break;
	case EHSMViewMode::RVM_Normal: res = "normal";
		break;
	}
	return res;
}

EHSMViewMode AHSMTracker::NextViewmode(EHSMViewMode vm)
{
	EHSMViewMode res(EHSMViewMode_First);

	if (vm == EHSMViewMode_Last)
	{
		res = EHSMViewMode_First;
	}
	else
	{
		bool found = false;
		for (int i = 0; i < (EHSMViewModeList.Num() - 1) && !found; i++)
		{
			if (EHSMViewModeList[i] == vm)
			{
				res = EHSMViewModeList[i + 1];
				found = true;
			}
		}
	}
	return res;
}

void AHSMTracker::CacheStaticMeshActors()
{
	// StaticMeshActor dump
	CachedSM.Empty();
	for (TObjectIterator<AStaticMeshActor> Itr; Itr; ++Itr)
	{
		FString fullName = Itr->GetFullName();
		if ((fullName.Contains(Persistence_Level_Filter_Str) || bStandaloneMode || bDebugMode) && Itr->GetStaticMeshComponent()->Mobility == EComponentMobility::Movable)
		{
			CachedSM.Add(*Itr);
		}
	}
}

void AHSMTracker::CacheSceneActors(const TArray<FString>& PawnNames, const TArray<FString>& CameraNames)
{
	// StaticMeshActor dump
	CacheStaticMeshActors();

	// Cameras dump
	CameraActors.Empty();
	for (TObjectIterator<ACameraActor> Itr; Itr; ++Itr)
	{
		FString FullName = Itr->GetFullName();
		if (FullName.Contains(Persistence_Level_Filter_Str) || bStandaloneMode)
		{
			for (FString camName : CameraNames)
			{
				if (camName == Itr->GetName())
				{
					CameraActors.Add(*Itr);
				}
			}
		}
	}

	// Pawn dump
	Pawns.Empty();
	for (TObjectIterator<APawn> Itr; Itr; ++Itr)
	{
		FString FullName = Itr->GetFullName();
		if (FullName.Contains(Persistence_Level_Filter_Str) || bStandaloneMode)
		{
			for (FString pawnName : PawnNames)
			{
				if (pawnName == Itr->GetActorLabel())
				{
					Pawns.Add(*Itr);
				}
			}
		}
	}
}

void AHSMTracker::DisableGravity()
{
	CachedSM_Gravity.Empty();
	CachedSM_Physics.Empty();

	for (AStaticMeshActor* sm : CachedSM)
	{
		CachedSM_Gravity.Add(sm->GetStaticMeshComponent()->IsGravityEnabled());
		CachedSM_Physics.Add(sm->GetStaticMeshComponent()->IsSimulatingPhysics());

		sm->GetStaticMeshComponent()->SetEnableGravity(false);
		sm->GetStaticMeshComponent()->SetSimulatePhysics(false);
	}
}

void AHSMTracker::RestoreGravity()
{
	int i = 0;
	for (AStaticMeshActor* sm : CachedSM)
	{
		if (i < CachedSM_Gravity.Num() && i < CachedSM_Physics.Num())
		{
			sm->GetStaticMeshComponent()->SetEnableGravity(CachedSM_Gravity[i]);
			sm->GetStaticMeshComponent()->SetSimulatePhysics(CachedSM_Physics[i]);
		}
		i++;
	}
}

void AHSMTracker::ChangeViewmodeDelegate(EHSMViewMode vm)
{
	if (vm == EHSMViewMode_First)
	{
	 //Get player controller and set view target to the first camera from ControllerPawn and CameraActors[CurrentCamRebuildMode]
	 auto playerController = GetWorld()->GetFirstPlayerController();
	 //check that player controller is valid
	 if (playerController && !bDebugMode) {
		 playerController->SetViewTarget(CameraActors[CurrentCamRebuildMode]);
	 }

	   //SceneCapture_depth->SetActorLocationAndRotation(CameraActors[CurrentCamRebuildMode]->GetActorLocation(), CameraActors[CurrentCamRebuildMode]->GetActorRotation());UE4 */
	}
	ChangeViewmode(vm);

	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::TakeScreenshotDelegate, vm), take_screenshot_delay, false);
}

void AHSMTracker::TakeScreenshotDelegate(EHSMViewMode vm)
{
	FTimerHandle TimerHandle;

	if (true || !bDebugMode) {
		TakeScreenshotFolder(vm, CameraActors[CurrentCamRebuildMode]->GetActorLabel());
	}

	if (vm == EHSMViewMode_Last)
	{
		++CurrentCamRebuildMode;
		if (CurrentCamRebuildMode < CameraActors.Num())
		{
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::ChangeViewmodeDelegate, EHSMViewMode_First), change_viewmode_delay, false);
		}
		else
		{
			CurrentCamRebuildMode = 0;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::RebuildModeMain), change_viewmode_delay, false);
		}
	}
	else
	{
		EHSMViewMode NextVM = NextViewmode(vm);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::ChangeViewmodeDelegate, NextVM), change_viewmode_delay, false);
	}
}

void AHSMTracker::RebuildModeBegin()
{
	if (CurrentJsonFile < start_frames.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("MENOR QUE START FRAMESSSSSSSS"));
		numFrame = start_frames[CurrentJsonFile];
		UE_LOG(LogTemp, Warning, TEXT("NUMFRAME XD: %i"), numFrame);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("IGUAL O MAYOR QUE START FRAMESSSSS"));
		numFrame = 0;
	}

	JsonReadStartTime = FDateTime::Now().ToUnixTimestamp();
	LastFrameTime = JsonReadStartTime;

	// Print sceneObject JSON 
	FString sceneObject_json_filename = screenshots_save_directory + screenshots_folder + "/" + json_file_names[CurrentJsonFile] + "/sceneObject.json";
	//FROXObjectPainter::Get().PrintToJson(sceneObject_json_filename); UE4

	JsonParser = new HSMJsonParser(); 
	JsonParser->LoadFile(scene_save_directory + scene_folder + "/" + json_file_names[CurrentJsonFile] + ".json");
	//CacheSceneActors(JsonParser->GetPawnNames(), JsonParser->GetCameraNames());
	DisableGravity();
	
	// Initialize scene (gaussian splat)
	if (!scene_charged)
	{
		if JsonParser->GetSceneFileName() != ""{
			scene_file_name = JsonParser->GetSceneFileName();
		}
		UE_LOG(LogTemp, Warning, TEXT("SPAWNEANDO LA ESCENA"));
		scene_charged = JsonParser->LoadSceneFile(scene_charge_directory + scene_folder + "/" + scene_file_name + ".json",this);
		//UE_LOG(LogTemp, Warning, TEXT("Scene chargeg value is: %s"), scene_charged ? TEXT("true") : TEXT("false"));
	}

	// Initialize Pawns in scene
	for (APawn* sk : Pawns){
		FString skName = sk->GetActorLabel();
		UE_LOG(LogTemp, Warning, TEXT("Name of Pawn: %s"), *skName);
		FHSMSkeletonState SkState = JsonParser->GetSkeletonState(skName);
		// check if SkState has valid values of position and rotation variables
		if (SkState.Position != FVector::ZeroVector && SkState.Rotation != FRotator::ZeroRotator)
		{
			sk->SetActorLocationAndRotation(SkState.Position, SkState.Rotation);
			//Set visibility of the pawn to true
			sk->SetActorHiddenInGame(false);

			//Get animation from the jsonparser
			FString animationName = JsonParser->GetAnimationName(skName);
			UE_LOG(LogTemp, Warning, TEXT("Name of Animation: %s"), *animationName);
			//Get the animationn object from the game with the name animationName
			UAnimSequence* anim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), NULL, *(animationName)));
			//Check if the animation is valid
			if (anim != nullptr) {
				//Set the skeletal mesh component (CharacterMesh0) of the pawn to play the animation
				UActorComponent* animMesh = sk->GetComponentByClass(USkeletalMeshComponent::StaticClass());
				//Check if the animation mesh is valid
				if (animMesh != nullptr) {
					/*pawnMesh = Cast<USkeletalMeshComponent>(animMesh);*/
					USkeletalMeshComponent* pawnMesh = Cast<USkeletalMeshComponent>(animMesh);
					if (pawnMesh != nullptr) {
						UE_LOG(LogTemp, Warning, TEXT("%s with Animation: %s es v�lida"), *skName,*animationName);
						//add to array
						pawnMeshArray.Add(pawnMesh);
						//pawnMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						pawnMesh->SetAnimation(anim);
						//Get size of anim
						animLength = anim->GetPlayLength();
						//pawnMesh->PlayAnimation(anim, false);
						pawnMesh->Play(false);
						//pawnMesh->PlayAnimation(anim, false);
						pawnMesh->bPauseAnims = true;
						//Set frame 0 of the animation
						pawnMesh->GlobalAnimRateScale = 0.0f;
						pawnMesh->SetPlayRate(0.0f);
						//pawnMesh->SetPosition(1.0f, false);
						//print pawnMesh->GetPosition()
						//UE_LOG(LogTemp, Warning, TEXT("Position of pawnMesh: %f"), pawnMesh->GetPosition());
					}

				}
			}

			

		}else{
			sk->SetActorHiddenInGame(true);
		}


	}

	// Initialize Cameras in scene
	for (ACameraActor* cam : CameraActors){
		FString camName = cam->GetActorLabel();
		UE_LOG(LogTemp, Warning, TEXT("Name of Camera: %s"), *camName);
		FHSMCameraState CamState = JsonParser->GetCameraState(camName);
		if (CameraActors.Num() == 1) 
			currentCamState = CamState;
		
		// check if CamState has valid values of position and rotation variables
		if (CamState.Position != FVector::ZeroVector && CamState.Rotation != FRotator::ZeroRotator)
		{
			cam->SetActorLocationAndRotation(CamState.Position, CamState.Rotation);
			//Set fov of the camera
			cam->GetCameraComponent()->FieldOfView = CamState.fov;
		}
	
	}

	if (JsonParser->GetNumFrames() > 0)
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::RebuildModeMain), initial_delay, false);
	} 
}

void AHSMTracker::RebuildModeMain()
{
	//se ha eliminado la condici�n de las transfornaciones ya que hay 5 c�maras y currentcamstate solo tiene valor si hay una c�mra ( hay 5 en la escena )
	//numFrame < currentCamState.Transforms.Num()
	if (numFrame < JsonParser->GetNumFrames() && (JsonParser->GetAnimationNames().Num() == 0 || animLength > numFrame / fps_anim)) { //Check if the animation is finished or if the animation is not valid
		int64 currentTime = FDateTime::Now().ToUnixTimestamp();
		PrintStatusToLog(start_frames[CurrentJsonFile], JsonReadStartTime, LastFrameTime, numFrame, currentTime, JsonParser->GetNumFrames());
		LastFrameTime = currentTime;

		//currentFrame = JsonParser->GetFrameData(numFrame);
		
		//update all meshes positions
		for (USkeletalMeshComponent* pawnMesh : pawnMeshArray) {
			if (pawnMesh != nullptr) {
				float framePos = (float)numFrame / fps_anim;
				pawnMesh->SetPosition(framePos, false);
			}
		}

		/*if (pawnMesh != nullptr) {*/
			//Check size of the animation of pawnMesh
			//Set initial position of frame numFrame of the animation taking into account the fps_anim and refresh the mesh
			//pawnMesh->SetUpdateAnimationInEditor(true);
			/*float framePos = (float)numFrame / fps_anim;
			pawnMesh->SetPosition(framePos, false);*/
			//pawnMesh->RefreshBoneTransforms();
			// print pawnMesh->GetPosition()
			//UE_LOG(LogTemp, Warning, TEXT("Position of pawnMesh: %f"), pawnMesh->GetPosition());
			//To solve problems about no updates of the animation in the editor
			//pawnMesh->TickAnimation(0.0f, false);
			//pawnMesh->TickPose(0.0f, false);
		/*}*/


		// Rebuild Pawns animation
		TMap<FName, FTransform> NameTransformMap;
		for (APawn* sk : Pawns)
		{
			FString skName = sk->GetActorLabel();

		}

		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::RebuildModeMain_Camera), place_cameras_delay, false);
	}
	else
	{
		CurrentJsonFile++;
		if (CurrentJsonFile < json_file_names.Num())
		{
			RebuildModeBegin();
		}
		else
		{
			RestoreGravity();
		}
	}
	
	int a = 0;
}

void AHSMTracker::RebuildModeMain_Camera()
{
	// Rebuild Cameras
	/*for (ACameraActor* cam : CameraActors)
	{
		FHSMActorState* CamState = currentFrame.Cameras.Find(cam->GetName());
		if (CamState != nullptr)
		{
			cam->SetActorLocationAndRotation(CamState->Position, CamState->Rotation);
		}
	}*/

	if(CameraActors.Num() == 1){
	{
		//Check if the currentCamState struct is not defined
		if(currentCamState.Position == FVector::ZeroVector && currentCamState.Rotation == FRotator::ZeroRotator){
			currentCamState = JsonParser->GetCameraState(CameraActors[0]->GetActorLabel());
		}
		
		FMatrix matrix = currentCamState.Transforms[numFrame];
		//Set the transform of the camera
		//Multiply m14, m23 and m32 by -1 to get the correct position of the camera
		//matrix.M[0][3] = -matrix.M[0][3];
		//matrix.M[1][2] = -matrix.M[1][2];
		//matrix.M[2][1] = -matrix.M[2][1];


		//FMatrix transMat = matrix;
		matrix =  matrix.GetTransposed();
		//Set row 1 to 0, row 2 to 1 and row 0 to 2
		//transMat.SetColumn(1, matrix.GetColumn(0));
		//transMat.SetColumn(2, matrix.GetColumn(1));
		//transMat.SetColumn(0, matrix.GetColumn(2));
		//transMat.SetColumn(3, matrix.GetColumn(3));


		//Transpose the matrix
		//matrix = matrix.GetTransposed();
		FTransform transform = FTransform(matrix);
		//change matrix positions (in m) to cm
		//transform.SetLocation(transform.GetLocation() * 100);
		CameraActors[0]->GetCameraComponent()->SetWorldTransform(transform);
		}
	}

	++numFrame;
	CurrentCamRebuildMode = 0;
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &AHSMTracker::ChangeViewmodeDelegate, EHSMViewMode_First), first_viewmode_of_frame_delay, false);
}

FString SecondsToString(int timeSec)
{
	int seconds = timeSec % 60;
	int timeMin = timeSec / 60;
	int minutes = timeMin % 60;
	int timeHrs = timeMin / 60;

	return FString(FString::FromInt(timeHrs) + "h " + FString::FromInt(minutes) + "min " + FString::FromInt(seconds) + "sec");
}

void AHSMTracker::PrintStatusToLog(int startFrame, int64 startTimeSec, int64 lastFrameTimeSec, int currentFrameInt, int64 currentTimeSec, int totalFrames)
{
	int nDoneFrames = currentFrameInt - startFrame;

	if (frame_status_output_period > 0 && nDoneFrames != 0 && (nDoneFrames % frame_status_output_period == 0.0f || nDoneFrames == 2 || nDoneFrames == frame_status_output_period / 10 || nDoneFrames == frame_status_output_period / 2))
	{
		int totalFramesForRebuild = totalFrames - startFrame;
		int elapsedTimeSec = (int)(currentTimeSec - startTimeSec);
		int remainingTimeSec = (elapsedTimeSec / nDoneFrames) * (totalFrames - currentFrameInt);
		int lastFrameElapsedTimeSec = (int)(currentTimeSec - lastFrameTimeSec);

		FString status_msg("Frame " + FString::FromInt(nDoneFrames) + " / " + FString::FromInt(totalFramesForRebuild) + " (" + FString::FromInt(currentFrameInt) + "/" + FString::FromInt(totalFrames) + ")");
		status_msg += " - Estimated Remaining Time: " + SecondsToString(remainingTimeSec) + " - Last Frame Time: " + FString::FromInt(lastFrameElapsedTimeSec) + "sec - Total Elapsed Time: " + SecondsToString(elapsedTimeSec);

		UE_LOG(LogTemp, Warning, TEXT("%s"), *status_msg);
	}
}


