// Fill out your copyright notice in the Description page of Project Settings.


#include "HSMExporter.h"
#include "EngineUtils.h"


// Sets default values
AHSMExporter::AHSMExporter()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	file_name = "example";
	Output_path = FPaths::ProjectUserDir();
}

// Called when the game starts or when spawned
void AHSMExporter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("BEGIN PLAY"));
	//get current world
	UWorld* world = GetWorld();

	if (world)
	{
		UE_LOG(LogTemp, Warning, TEXT("HAY WORLD"));
		//scene
		if (bScene)
		{
			TArray<AActor*> arrayscenes = GetActorByType<AActor>(world);
			if (arrayscenes.IsEmpty()) {
				UE_LOG(LogTemp, Warning, TEXT("ESTA VACÍO"));
			}else{
				UE_LOG(LogTemp, Warning, TEXT("ESTA LLENO"));
			}
			
			GenerateSceneJson(world, arrayscenes);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Scene not choose to be generated"));
		}

		//cameras and metahumans
		if (bCameras && bMetahumans) {
			GenerateCamerasAndMetaHumanJson(world, GetActorByType<ACameraActor>(world), GetActorByType<ACharacter>(world));
		}
		else {
			if (bCameras) { // only cameras
				GenerateCamerasJson(world, GetActorByType<ACameraActor>(world));
			}
			else { 
				if(bMetahumans) // only metahumans
					GenerateMetaHumanJson(world, GetActorByType<ACharacter>(world));
			}
		}
	}
}

// Called every frame
void AHSMExporter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AHSMExporter::Savejson(TSharedPtr<FJsonObject> RootObject, FString file_path)
{
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
	FFileHelper::SaveStringToFile(OutputString, *file_path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_None);
}

void AHSMExporter::GenerateSceneJson(const UWorld* world, TArray<AActor*> scenes)
{
	UE_LOG(LogTemp, Warning, TEXT("generate scene json"));

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

	for (AActor* scene : scenes) {
		UE_LOG(LogTemp, Warning, TEXT("Actor location: %s"), *scene->GetName());
		// Crear el objeto JSON para la escena
		TSharedPtr<FJsonObject> SceneObject = MakeShareable(new FJsonObject());

		// Extraer el nombre del blueprint
		FString BlueprintPath;
		if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(scene->GetClass()))
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
			{
				BlueprintPath = Blueprint->GetPathName();
			}
		}
		SceneObject->SetStringField(TEXT("name"), "//Script/Engine.Blueprint'" + BlueprintPath + "'" );
		UE_LOG(LogTemp, Warning, TEXT("Actor location: %s"), *BlueprintPath);

		// Posición
		UE_LOG(LogTemp, Warning, TEXT("Actor location: %s"), *scene->GetActorLocation().ToString());

		FVector ActorLocation = scene->GetActorLocation();
		TSharedPtr<FJsonObject> PositionObject = MakeShareable(new FJsonObject());
		PositionObject->SetNumberField(TEXT("x"), ActorLocation.X);
		PositionObject->SetNumberField(TEXT("y"), ActorLocation.Y);
		PositionObject->SetNumberField(TEXT("z"), ActorLocation.Z);
		SceneObject->SetObjectField(TEXT("position"), PositionObject);



		// Rotación
		UE_LOG(LogTemp, Warning, TEXT("Actor rotation: %s"), *scene->GetActorRotation().ToString());

		FRotator ActorRotation = scene->GetActorRotation();
		TSharedPtr<FJsonObject> RotationObject = MakeShareable(new FJsonObject());
		RotationObject->SetNumberField(TEXT("x"), ActorRotation.Pitch);
		RotationObject->SetNumberField(TEXT("y"), ActorRotation.Yaw);
		RotationObject->SetNumberField(TEXT("z"), ActorRotation.Roll);
		SceneObject->SetObjectField(TEXT("rotation"), RotationObject);


		// Escala
		UE_LOG(LogTemp, Warning, TEXT("Actor scale: %s"), *scene->GetActorScale3D().ToString());

		FVector ActorScale = scene->GetActorScale3D();
		TSharedPtr<FJsonObject> ScaleObject = MakeShareable(new FJsonObject());
		ScaleObject->SetNumberField(TEXT("x"), ActorScale.X);
		ScaleObject->SetNumberField(TEXT("y"), ActorScale.Y);
		ScaleObject->SetNumberField(TEXT("z"), ActorScale.Z);
		SceneObject->SetObjectField(TEXT("scale"), ScaleObject);

		// Función auxiliar para procesar propiedades de tipo FVector3f
		auto ProcessVectorProperty = [&](const FString& PropertyName, const FString& FieldName) {
			FProperty* Property = scene->GetClass()->FindPropertyByName(*PropertyName);
			if (Property) {
				void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(scene);
				FVector3f PropertyValue = *reinterpret_cast<FVector3f*>(PropertyAddress);
				TSharedPtr<FJsonObject> PropertyObject = MakeShareable(new FJsonObject());
				PropertyObject->SetNumberField(TEXT("x"), PropertyValue.X);
				PropertyObject->SetNumberField(TEXT("y"), PropertyValue.Y);
				PropertyObject->SetNumberField(TEXT("z"), PropertyValue.Z);
				SceneObject->SetObjectField(*FieldName, PropertyObject);
			}
		};
		// CropMin
		/*FProperty* CropMin = scene->GetClass()->FindPropertyByName("CropMin");
		void* CropMinAddress = CropMin->ContainerPtrToValuePtr<void>(scene);
		FVector3f CropMinValue = *reinterpret_cast<FVector3f*>(CropMinAddress);
		FString CropMinString = FString::Printf(TEXT("(%f, %f, %f)"), CropMinValue.X, CropMinValue.Y, CropMinValue.Z);
		UE_LOG(LogTemp, Warning, TEXT("Actor cropmin: %s"),*CropMinString);*/

		ProcessVectorProperty(TEXT("CropMin"), TEXT("crop_bounding_box_min"));

		// CropMax
		/*FProperty* CropMax = scene->GetClass()->FindPropertyByName("CropMax");
		void* CropMaxAddress = CropMax->ContainerPtrToValuePtr<void>(scene);
		FVector3f CropMaxValue = *reinterpret_cast<FVector3f*>(CropMaxAddress);
		FString CropMaxString = FString::Printf(TEXT("(%f, %f, %f)"), CropMaxValue.X, CropMaxValue.Y, CropMaxValue.Z);
		UE_LOG(LogTemp, Warning, TEXT("Actor cropmax: %s"), *CropMaxString);*/

		ProcessVectorProperty(TEXT("CropMax"), TEXT("crop_bounding_box_max"));

		// CropCenter
		/*FProperty* CropCenter = scene->GetClass()->FindPropertyByName("CropCenter");
		void* CropCenterAddress = CropCenter->ContainerPtrToValuePtr<void>(scene);
		FVector3f CropCenterValue = *reinterpret_cast<FVector3f*>(CropCenterAddress);
		FString CropCenterString = FString::Printf(TEXT("(%f, %f, %f)"), CropCenterValue.X, CropCenterValue.Y, CropCenterValue.Z);
		UE_LOG(LogTemp, Warning, TEXT("Actor cropcenter: %s"), *CropCenterString);*/

		ProcessVectorProperty(TEXT("CropCenter"), TEXT("crop_bounding_center"));

		// CropRotation
		/*FProperty* CropRotation = scene->GetClass()->FindPropertyByName("CropRotation");
		void* CropRotationAddress = CropRotation->ContainerPtrToValuePtr<void>(scene);
		FVector3f CropRotationValue = *reinterpret_cast<FVector3f*>(CropRotationAddress);
		FString CropRotationString = FString::Printf(TEXT("(%f, %f, %f)"), CropRotationValue.X, CropRotationValue.Y, CropRotationValue.Z);
		UE_LOG(LogTemp, Warning, TEXT("Actor croprotation: %s"), *CropRotationString);*/

		ProcessVectorProperty(TEXT("CropRotation"), TEXT("crop_rotation"));

		// Agregar la escena al objeto raíz
		RootObject->SetObjectField(TEXT("scene"), SceneObject);
	}

	// Convertir el objeto JSON a una cadena and save
	FString json_file_path = Output_path + "/" + file_name + "_scene.json";
	Savejson(RootObject, json_file_path);
}

TArray<TSharedPtr<FJsonValue>> AHSMExporter::GetArrayCameras(TArray<ACameraActor*> cameras)
{
	// Crear un array JSON para las cámaras
	TArray<TSharedPtr<FJsonValue>> CamerasArray;

	for (AActor* camera : cameras) {
		// Crear el objeto JSON para cada cámara
		TSharedPtr<FJsonObject> CameraObject = MakeShareable(new FJsonObject());

		// Nombre de la cámara
		CameraObject->SetStringField(TEXT("name"), camera->GetActorLabel());

		// Propiedad 'stereo' (aquí se pone un valor fijo, pero puedes modificarlo según sea necesario)
		CameraObject->SetNumberField(TEXT("stereo"), 0);

		ACameraActor* cinecamera = Cast<ACameraActor>(camera);
		if (cinecamera) {
			//// FOV (Field of View)
			CameraObject->SetNumberField(TEXT("fov"), cinecamera->GetCameraComponent()->FieldOfView);
		}

		// Posición
		FVector CameraLocation = camera->GetActorLocation();
		TSharedPtr<FJsonObject> PositionObject = MakeShareable(new FJsonObject());
		PositionObject->SetNumberField(TEXT("x"), CameraLocation.X);
		PositionObject->SetNumberField(TEXT("y"), CameraLocation.Y);
		PositionObject->SetNumberField(TEXT("z"), CameraLocation.Z);
		CameraObject->SetObjectField(TEXT("position"), PositionObject);

		// Rotación
		FRotator CameraRotation = camera->GetActorRotation();
		TSharedPtr<FJsonObject> RotationObject = MakeShareable(new FJsonObject());
		RotationObject->SetNumberField(TEXT("r"), CameraRotation.Roll);
		RotationObject->SetNumberField(TEXT("p"), CameraRotation.Pitch);
		RotationObject->SetNumberField(TEXT("y"), CameraRotation.Yaw);
		CameraObject->SetObjectField(TEXT("rotation"), RotationObject);

		// Agregar la cámara al array de cámaras
		CamerasArray.Add(MakeShareable(new FJsonValueObject(CameraObject)));
	}

	return CamerasArray;
}
void AHSMExporter::GenerateCamerasJson(const UWorld* world, TArray<ACameraActor*> cameras)
{
	UE_LOG(LogTemp, Warning, TEXT("generate json cameras"));
	UE_LOG(LogTemp, Warning, TEXT("generate json cameras: %i"),cameras.Num());
	// Crear el objeto raíz del JSON
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
	
	// Obtenemos el Array de Cameras
	TArray<TSharedPtr<FJsonValue>> CamerasArray = GetArrayCameras(cameras);

	// Agregar el array de cámaras al objeto raíz
	RootObject->SetArrayField(TEXT("cameras"), CamerasArray);

	// Convertir el objeto JSON a una cadena
	FString json_file_path = Output_path + "/" + file_name + "_cameras.json";
	Savejson(RootObject, json_file_path);
}


TArray<TSharedPtr<FJsonValue>> AHSMExporter::GetArrayMetahumans(TArray<ACharacter*> characters)
{
	// Crear un array JSON para los avatares
	TArray<TSharedPtr<FJsonValue>> AvatarsArray;

	for (ACharacter* character : characters) {
		// Crear el objeto JSON para cada avatar
		TSharedPtr<FJsonObject> AvatarObject = MakeShareable(new FJsonObject());

		// Nombre del avatar
		AvatarObject->SetStringField(TEXT("name"), character->GetActorLabel());

		// Obtener el componente de malla del personaje
		USkeletalMeshComponent* MeshComponent = character->GetMesh();
		if (MeshComponent) {
			// Obtener el asset de animación actual, si se usa un Animation Asset
			UAnimationAsset* AnimationAsset = MeshComponent->AnimationData.AnimToPlay;
			if (AnimationAsset) {
				// Obtener el path de la animación
				FString AnimationPath = TEXT("/Script/Engine.AnimSequence'/Game/Motion/" + AnimationAsset->GetName() + "." + AnimationAsset->GetName() + "'");
				AvatarObject->SetStringField(TEXT("animation"), AnimationPath);
			}
			else {
				AvatarObject->SetStringField(TEXT("animation"), TEXT("None"));
			}
		}

		// Crear el objeto JSON para el esqueleto
		TSharedPtr<FJsonObject> SkeletonObject = MakeShareable(new FJsonObject());
		SkeletonObject->SetNumberField(TEXT("num_bones"), 24); // Número de huesos (puedes ajustarlo según tu necesidad)

		// Posición
		FVector CharacterLocation = character->GetActorLocation();
		TSharedPtr<FJsonObject> PositionObject = MakeShareable(new FJsonObject());
		PositionObject->SetNumberField(TEXT("x"), CharacterLocation.X);
		PositionObject->SetNumberField(TEXT("y"), CharacterLocation.Y);
		PositionObject->SetNumberField(TEXT("z"), CharacterLocation.Z);
		SkeletonObject->SetObjectField(TEXT("position"), PositionObject);

		// Rotación
		FRotator CharacterRotation = character->GetActorRotation();
		TSharedPtr<FJsonObject> RotationObject = MakeShareable(new FJsonObject());
		RotationObject->SetNumberField(TEXT("r"), CharacterRotation.Roll);
		RotationObject->SetNumberField(TEXT("p"), CharacterRotation.Pitch);
		RotationObject->SetNumberField(TEXT("y"), CharacterRotation.Yaw);
		SkeletonObject->SetObjectField(TEXT("rotation"), RotationObject);

		// Agregar el objeto Skeleton al objeto Avatar
		AvatarObject->SetObjectField(TEXT("skeleton"), SkeletonObject);

		// Agregar el avatar al array de avatares
		AvatarsArray.Add(MakeShareable(new FJsonValueObject(AvatarObject)));
	}

	return AvatarsArray;
}
void AHSMExporter::GenerateMetaHumanJson(const UWorld* world, TArray<ACharacter*> characters)
{
	UE_LOG(LogTemp, Warning, TEXT("generate json metahumans"));
	UE_LOG(LogTemp, Warning, TEXT("generate json character: %i"), characters.Num());
	// Crear el objeto raíz del JSON
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

	// Get metahumans array
	TArray<TSharedPtr<FJsonValue>> AvatarsArray = GetArrayMetahumans(characters);

	// Agregar el array de avatares al objeto raíz
	RootObject->SetArrayField(TEXT("avatars"), AvatarsArray);

	// Convertir el objeto JSON a una cadena
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	// Guardar el archivo JSON
	FString json_file_path = Output_path + "/" + file_name + "_metahumans.json";
	FFileHelper::SaveStringToFile(OutputString, *json_file_path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_None);
}

void AHSMExporter::GenerateCamerasAndMetaHumanJson(const UWorld* world, TArray<ACameraActor*> cameras, TArray<ACharacter*> characters)
{
	UE_LOG(LogTemp, Warning, TEXT("generate json cameas and metahumans"));

	// Crear el objeto raíz del JSON
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

	// Añadir el nombre y el número total de frames al objeto raíz
	RootObject->SetStringField(TEXT("name"), TEXT("Anim Man 1 and Woman 2"));
	RootObject->SetNumberField(TEXT("total_frames"), 120);

	// Agregar el array de cameras al objeto raíz
	RootObject->SetArrayField(TEXT("cameras"), GetArrayCameras(cameras));

	// Agregar el array de avatares al objeto raíz
	RootObject->SetArrayField(TEXT("avatars"), GetArrayMetahumans(characters));

	// Convertir el objeto JSON a una cadena
	FString json_file_path = Output_path + "/" + file_name + "_sequence.json";
	Savejson(RootObject, json_file_path);
}
