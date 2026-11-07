#pragma once

#include "Property.h"
#include "Object.h"
#include "Serialization/MathSerialization.h"

using ClassConstructor = UObject * (*)(void* Memory);
using FRegisterPropertiesFunction = void (*)(UClass* Class);

class UClass : public UObject
{
public:
	UClass();

	// 클래스 기본 정보와 등록 함수의 보관
	UClass(const FName& InName, UClass* InSuper, uint64 InClassSize,
	       ClassConstructor InConstructor, FRegisterPropertiesFunction InRegisterProperties);
	
	void EnsurePropertiesRegistered();

	bool ArePropertiesRegistered() const { return PropertyRegistrationState == EPropertyRegistrationState::Registered; }

	UClass* Super = nullptr;
	uint64 ClassSize = 0;
	ClassConstructor Constructor = nullptr;

	TArray<FProperty> Properties;
	UObject* DefaultObject = nullptr;

	bool IsChildOf(const UClass* BaseClass) const;
	UObject* GetDefaultObject();

	const TArray<FProperty>& GetProperties() const
	{
		const_cast<UClass*>(this)->EnsurePropertiesRegistered();
		return Properties;
	}

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, uint32 InFlags = CPF_Default)
	{
		AddProperty<T>(InName, InOffset, GetPropertyType<T>(), InFlags);
	}

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType, uint32 InFlags = CPF_Default)
	{
		UClass* ObjectClass = nullptr;
		if constexpr (std::is_pointer_v<T> && std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			ObjectClass = std::remove_pointer_t<T>::GetPrivateStaticClass();
		}

		// 메타데이터 생성 후 실제 타입의 값 연산 연결
		FProperty Property{ InName, InType, InOffset, sizeof(T), ObjectClass, InFlags };
		Property.InitializeValueOperations<T>();

		    // 실제 선언 타입에 맞는 슬롯 직렬화 연결
		Property.SerializeFunction = [](FStructuredArchive::FSlot Slot, void* ValueAddress)
		{
			T& Value = *static_cast<T*>(ValueAddress);

			if constexpr (std::is_pointer_v<T> && std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
			{
				// 공통 객체 참조 직렬화 경로로 전달
				UObject* Reference = Value;
				Slot << Reference;

				// 선언된 타입과 호환되는 참조만 반영
				if (Slot.GetUnderlyingArchive().IsLoading() && (!Reference || Reference->IsA<std::remove_pointer_t<T>>()))
				{
					Value = static_cast<T>(Reference);
				}
			}
			else
			{
				Slot << Value;
			}
		};

		Properties.Add(Property);
	}

private:
	enum class EPropertyRegistrationState : uint8
	{
		Unregistered,
		Registering,
		Registered
	};
	FRegisterPropertiesFunction RegisterPropertiesFunction = nullptr;
	EPropertyRegistrationState PropertyRegistrationState = EPropertyRegistrationState::Unregistered;

	mutable int32 ClassUnique = 0;
	friend FName MakeUniqueObjectName(UObject*, const UClass*, FName);
};

// 클래스 계층에 등록된 프로퍼티 값 복사
inline void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	for (UClass* Class = FromClass; Class; Class = Class->Super)
	{
		for (const FProperty& Property : Class->GetProperties())
		{
			void* Destination = Property.ContainerPtrToValuePtr(Dst);
			const void* Source = Property.ContainerPtrToValuePtr(Src);
			Property.CopyCompleteValue(Destination, Source);
		}
	}
}