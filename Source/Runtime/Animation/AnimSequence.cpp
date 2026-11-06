#include "EnginePCH.h"
#include "AnimSequence.h"

UAnimSequenceBase::UAnimSequenceBase()
{
	DataModel = CreateDefaultSubobject<UAnimDataModel>(FName("AnimDataModel"));
}