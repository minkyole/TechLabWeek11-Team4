#include "EnginePCH.h"
#include "Object.h"

#include "Core/EngineStatics.h"
#include "UObject/Class.h"

#include "Serialization/TypeSerializer.h"
#include "Serialization/MathSerialization.h"
#include "Serialization/Formatter/BinaryArchiveFormatter.h"
#include "Asset/AssetManager.h" //??? 의존성 제거 필요



TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	ObjectUUID = FEngineStatics::GetUUID();
	InternalIndex = GUObjectArray.Num();
	GUObjectArray.Add(this);
}

UObject::UObject(bool bRegister)
{
	bIsRegistered = bRegister;
}

UObject::~UObject()
{
	if (bIsRegistered)
	{
		if (ClassPrivate)
		{
			UnhashObject(this, ClassPrivate);
		}
		int32 LastIndex = GUObjectArray.Num() - 1;
		if (InternalIndex != LastIndex)
		{
			UObject* MovedObject = GUObjectArray[LastIndex];
			GUObjectArray[InternalIndex] = MovedObject;
			MovedObject->InternalIndex = InternalIndex; // 옮겨간 오브젝트 인덱스 갱신
		}
		GUObjectArray.RemoveLast();
	}
}

UClass* UObject::StaticClass()
{
	static UClass c;
	static bool bIsInit = false;
	if (!bIsInit)
	{
		c.Name = "Object";
		c.Super = nullptr;
		c.ClassSize = sizeof(UObject);
		c.Constructor = [](void* Memory) -> UObject*
			{
				return ::new (Memory) UObject();
			};
		bIsInit = true;
	}
	return &c;
}

bool UObject::IsA(const UClass* Class)
{
	const UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

void UObject::Serialize(FArchive& Ar)
{
	FBinaryArchiveFormatter Formatter(Ar);
	FStructuredArchive StructuredAr(Formatter);
	Serialize(StructuredAr.Open().EnterRecord());
	StructuredAr.Close();
}

void UObject::Serialize(FStructuredArchive::FRecord Record)
{
	SerializeScriptProperties(Record.EnterField("Properties"));
}

void UObject::SerializeScriptProperties(FStructuredArchive::FSlot Slot)
{
	FStructuredArchive::FRecord Properties = Slot.EnterRecord();

	const bool bIsLoading = Properties.GetUnderlyingArchive().IsLoading();

	for (UClass* c = GetClass(); c; c = c->Super)
	{
		for (const FProperty& Property : c->GetProperties())
		{
			const FString PropertyName = Property.Name.ToString();
			TOptional<FStructuredArchive::FSlot> Slot = Properties.TryEnterField(PropertyName.c_str(), true);
			if (!Slot.IsSet()) continue;

			void* Ptr = reinterpret_cast<uint8*>(this) + Property.Offset;
			FStructuredArchive::FSlot Value = Slot.GetValue();

			switch (Property.Type)
			{
			case EPropertyType::Float:     Value << *static_cast<float*>(Ptr); break;
			case EPropertyType::Int:       Value << *static_cast<int32*>(Ptr); break;
			case EPropertyType::String:    Value << *static_cast<FString*>(Ptr); break;
			case EPropertyType::Bool:      Value << *static_cast<bool*>(Ptr); break;
			case EPropertyType::Vector:    Value << *static_cast<FVector*>(Ptr); break;
			case EPropertyType::Vector4:
			case EPropertyType::LinearColor: Value << *static_cast<FVector4*>(Ptr); break;
			case EPropertyType::Color:     Value << *static_cast<FColor*>(Ptr); break;
			case EPropertyType::Rotator:   Value << *static_cast<FRotator*>(Ptr); break;
			case EPropertyType::Transform: Value << *static_cast<FTransform*>(Ptr); break;
			case EPropertyType::Object:
			{
				UObject*& Ref = *static_cast<UObject**>(Ptr);
				UObject* Previous = Ref;
				Value << Ref;

				// 불러온 객체가 프로퍼티가 기대하는 클래스가 아니면 되돌린다 (기존 Asset->IsA(Property.Class) 검사)
				if (bIsLoading && Ref && Property.Class && !Ref->IsA(Property.Class))
				{
					HTR_LOG(Warning, "Load: '{}' is not a {}, keeping default", PropertyName, Property.Class->Name);
					Ref = Previous;
				}
				break;
			}
			default:
				// 슬롯을 열었으면 반드시 값을 써야 한다. 모르는 타입은 저장 시 null로 둔다.
				HTR_LOG(Warning, "Serialize: unsupported property type for '{}'", PropertyName);
				{ UObject* Null = nullptr; Value << Null; }
				break;
			}
		}
	}
}



