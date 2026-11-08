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
	if (const FObjectInitializer* Initializer = FObjectInitializer::GetForObject(this))
	{
		const FStaticConstructObjectParameters& Parameters = Initializer->GetParameters();
		ClassPrivate = const_cast<UClass*>(Parameters.Class);
		Outer = Parameters.Outer;
		Name = Parameters.Name;
		Flags = Parameters.SetFlags;
		ObjectLabel = Name.ToString();
	}
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

bool UObject::IsTemplate() const
{
	// 기본 객체에 속한 전체 소유 경로의 판별
	for (const UObject* Object = this; Object; Object = Object->GetOuter())
	{
		if (Object->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
		{
			return true;
		}
	}
	return false;
}

UClass* UObject::GetPrivateStaticClass()
{
	static UClass c(
	    FName("Object"), nullptr, sizeof(UObject),
	    [](void* Memory) -> UObject*
	    {
		    return ::new (Memory) UObject();
	    },
	    nullptr);
	return &c;
}

UClass* UObject::StaticClass()
{
	UClass* c = GetPrivateStaticClass();
	c->EnsurePropertiesRegistered();
	return c;
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
	// 인스턴스 라벨의 필수 필드 직렬화
	Record << SA_VALUE("ObjectLabel", ObjectLabel);

	// 등록된 프로퍼티의 직렬화
	SerializeScriptProperties(Record.EnterField("Properties"));
}

void UObject::SerializeScriptProperties(FStructuredArchive::FSlot Slot)
{
	// 등록된 프로퍼티의 저장 레코드
	FStructuredArchive::FRecord Properties = Slot.EnterRecord();

	for (UClass* Class = GetClass(); Class; Class = Class->Super)
	{
		for (const FProperty& Property : Class->GetProperties())
		{
			// 프로퍼티 이름에 대응하는 슬롯 준비
			const FString PropertyName = Property.Name.ToString();
			TOptional<FStructuredArchive::FSlot> ValueSlot = Properties.TryEnterField(PropertyName.c_str(), true);

			if (!ValueSlot.IsSet())	{ continue; }

			// 실제 멤버 주소를 등록된 타입 연산에 전달
			void* ValueAddress = Property.ContainerPtrToValuePtr(this);
			Property.SerializeItem(ValueSlot.GetValue(), ValueAddress);
		}
	}
}



