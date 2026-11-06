#pragma once
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Core/NameTypes.h"
#include "Containers/Array.h"

struct FFrameRate
{
	int32 Numerator = 30; // 분자
	int32 Denominator = 1; // 분모
	

	FFrameRate() = default;
	FFrameRate(int32 InNumerator, int32 InDenominator)
	    : Numerator(InNumerator), Denominator(InDenominator) {}

	// Frame per Second
	float AsDecimal() const
	{
		return static_cast<float>(Numerator) / static_cast<float>(Denominator);
	}

	// Second per Frame
	float AsInterval() const
	{
		return static_cast<float>(Denominator) / static_cast<float>(Numerator);
	}
};

struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys; // 위치
	TArray<FQuat> RotKeys; // 회전
	TArray<FVector> ScaleKeys; // 스케일

	bool IsValid() const
	{
		return !PosKeys.IsEmpty() && !RotKeys.IsEmpty() && !ScaleKeys.IsEmpty();
	}
};

struct FBoneAnimationTrack
{
	FName Name; // Bone 이름 
	FRawAnimSequenceTrack InternalTrack; // 애니메이션 데이터
};

// 시간 하나를 두 키 사이의 위치(alpha)로 바꾼 결과
struct FAnimKeyLookup
{
	int32 KeyIndex0 = 0;
	int32 KeyIndex1 = 0;
	float Alpha = 0.f;
};
