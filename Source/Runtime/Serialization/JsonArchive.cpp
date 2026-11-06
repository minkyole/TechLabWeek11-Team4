#include "EnginePCH.h"
#include "JsonArchive.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectHash.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "TypeSerializer.h"
#include "Asset/AssetManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Core/Misc/FileHelper.h"

#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Serialization/Formatter/JsonArchiveOutputFormatter.h"
#include "Serialization/Formatter/JsonArchiveInputFormatter.h"

namespace
{
	constexpr int32 SceneVersion = 5;   // 필수 라벨과 RGBA 색상 데이터
}

bool FJsonArchive::SaveWorldToBytes(UWorld* World, TArray<uint8>& OutBytes)
{
	if (!World)
		return false;

	FMemoryWriter Writer(OutBytes);
	{
		// 선언 순서 = 소멸 역순: Ar(Close) → Formatter(dump → Writer)
		FJsonArchiveOutputFormatter Formatter(Writer);
		FStructuredArchive Ar(Formatter);

		FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();
		int32 Version = SceneVersion;
		Root << SA_VALUE("Version", Version);
		World->Serialize(Root.EnterRecord("World"));

		Ar.Close();
	} 

	return !Writer.IsError();
}

bool FJsonArchive::LoadWorldFromBytes(UWorld* World, const TArray<uint8>& Bytes)
{
	if (!World)
		return false;

	FMemoryReader Reader(Bytes);
	FJsonArchiveInputFormatter Formatter(Reader);   
	if (Reader.IsError())
	{
		HTR_LOG(Error, "Load: invalid scene data");
		return false;                               
	}

	FStructuredArchive Ar(Formatter);
	FStructuredArchive::FRecord Root = Ar.Open().EnterRecord();

	int32 Version = 0;
	Root << SA_VALUE("Version", Version);
	if (Version != SceneVersion)
	{
		HTR_LOG(Error, "Load: scene version {} is not supported (expected {})", Version, SceneVersion);
		return false;                               
	}

	World->ClearWorld();
	World->Serialize(Root.EnterRecord("World"));
	Ar.Close();

	World->GetScene().BuildBVH();
	return !Reader.IsError();
}

bool FJsonArchive::SaveWorld(UWorld* World, const FString& Path)
{
	TArray<uint8> Bytes;
	if (!SaveWorldToBytes(World, Bytes))
		return false;

    return FFileHelper::SaveArrayToFile(Bytes, Path);
}

bool FJsonArchive::LoadWorld(UWorld* World, const FString& Path)
{
	TArray<uint8> Bytes{};
	if (!FFileHelper::LoadFileToArray(Bytes, Path))
	{
		return false;
	}

	return LoadWorldFromBytes(World, Bytes);
}