#include "EnginePCH.h"
#include "Class.h"

#include "Object.h"

UClass::UClass()
    : UObject(false), PropertyRegistrationState(EPropertyRegistrationState::Registered)
{
	//HTR_LOG(Info, "Create RTTI UClass");
}

UClass::UClass(const FName& InName, UClass* InSuper, uint64 InClassSize,
               ClassConstructor InConstructor, FRegisterPropertiesFunction InRegisterProperties)
    : UObject(false), Super(InSuper), ClassSize(InClassSize), Constructor(InConstructor),
	RegisterPropertiesFunction(InRegisterProperties)
{
	SetName(InName);
}

void UClass::EnsurePropertiesRegistered()
{ 
	if (PropertyRegistrationState != EPropertyRegistrationState::Unregistered)
	{
		return;
	}

	// 부모 등록 호출 이전의 재진입 차단
	PropertyRegistrationState = EPropertyRegistrationState::Registering;

	if (Super)
	{
		Super->EnsurePropertiesRegistered();
	}

	if (RegisterPropertiesFunction)
	{
		RegisterPropertiesFunction(this);
	}

	// 프로퍼티 구성 완료 후의 클래스 검색 등록
	RegisterClass(this);
	PropertyRegistrationState = EPropertyRegistrationState::Registered;
}

bool UClass::IsChildOf(const UClass* BaseClass) const
{
	if (!BaseClass) return false;
	for (const UClass* c = this; c; c = c->Super)
	{
		if (c == BaseClass) return true;
	}
	return false;
}

UObject* UClass::GetDefaultObject() 
{
	if (!DefaultObject)
	{
		DefaultObject = Constructor(UObject::operator new(ClassSize));
	}
	return DefaultObject;
}


