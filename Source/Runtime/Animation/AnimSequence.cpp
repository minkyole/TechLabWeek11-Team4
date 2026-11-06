#include "EnginePCH.h"
#include "AnimSequence.h"

namespace
{
	int32 ClampKeyIndex(int32 KeyIndex, int32 NumKeys)
	{
		return FMath::Clamp(KeyIndex, 0, NumKeys - 1);
	}
}

UAnimSequenceBase::UAnimSequenceBase()
{
	DataModel = CreateDefaultSubobject<UAnimDataModel>(FName("AnimDataModel"));
}

FAnimKeyLookup UAnimSequence::FindKeyLookup(float Time) const
{
	int32 NumKeys = DataModel->GetNumberOfKeys();
	if (NumKeys <= 1)
	{
		return FAnimKeyLookup{ 0, 0, 0.f };
	}
	float ClampedTime = FMath::Clamp(Time, 0.f, GetPlayLength());
	float FrameNum = ClampedTime * DataModel->GetFrameRate().AsDecimal();
	int32 KeyIndex0 = FMath::Clamp(static_cast<int32>(std::floor(FrameNum)), 0, NumKeys - 1);
	int32 KeyIndex1 = FMath::Min(KeyIndex0 + 1, NumKeys - 1);
	float Alpha = FMath::Clamp01(FrameNum - KeyIndex0);

	return FAnimKeyLookup{KeyIndex0, KeyIndex1, Alpha};
}

void UAnimSequence::SampleTrack(const FRawAnimSequenceTrack& Track, const FAnimKeyLookup& Lookup, FAnimBoneTransform& OutTransform)
{
	// Translation
	if (!Track.PosKeys.IsEmpty())
	{
		int32 PosIndex0 = ClampKeyIndex(Lookup.KeyIndex0, Track.PosKeys.Num());
		int32 PosIndex1 = ClampKeyIndex(Lookup.KeyIndex1, Track.PosKeys.Num());

		FVector Pos0 = Track.PosKeys[PosIndex0];
		FVector Pos1 = Track.PosKeys[PosIndex1];

		OutTransform.Translation = Pos0 + (Pos1 - Pos0) * Lookup.Alpha;	
	}

	// Rotation
	if (!Track.RotKeys.IsEmpty())
	{
		int32 RotIndex0 = ClampKeyIndex(Lookup.KeyIndex0, Track.RotKeys.Num());
		int32 RotIndex1 = ClampKeyIndex(Lookup.KeyIndex1, Track.RotKeys.Num());

		FQuat Rot0 = Track.RotKeys[RotIndex0];
		FQuat Rot1 = Track.RotKeys[RotIndex1];

		OutTransform.Rotation = FMath::SlerpShortestPath(Rot0, Rot1, Lookup.Alpha);
	}

	// Scale
	if (!Track.ScaleKeys.IsEmpty())
	{
		int32 ScaleIndex0 = ClampKeyIndex(Lookup.KeyIndex0, Track.ScaleKeys.Num());
		int32 ScaleIndex1 = ClampKeyIndex(Lookup.KeyIndex1, Track.ScaleKeys.Num());

		FVector Scale0 = Track.ScaleKeys[ScaleIndex0];
		FVector Scale1 = Track.ScaleKeys[ScaleIndex1];

		OutTransform.Scale = Scale0 + (Scale1 - Scale0) * Lookup.Alpha;
	}
}
