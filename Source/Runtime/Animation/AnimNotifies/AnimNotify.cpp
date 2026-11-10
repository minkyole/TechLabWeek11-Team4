#include "EnginePCH.h"
#include "AnimNotify.h"

FString UAnimNotify::GetNotifyName() const
{
	return GetDisplayNameForClass(GetClass());
}

FString UAnimNotify::GetDisplayNameForClass(const UClass* Class)
{
	static const FString Prefix = "UAnimNotify_";

	FString Name = Class ? Class->Name : FString("UAnimNotify");
	if (Name.starts_with(Prefix))
	{
		Name = Name.substr(Prefix.size());
	}
	return Name;
}
