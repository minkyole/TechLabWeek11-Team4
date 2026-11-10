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

// 시간을 한 번 진행한 결과. 상태 전환과 Notify 판단이 이 값을 본다.
enum class ETypeAdvanceAnim : uint8
{
	Default,  // 경계에 닿지 않았다
	Looped,   // 루프 경계를 넘어 되감겼다
	Finished, // 루프가 아니고 진행 방향의 끝(정방향 1, 역방향 0)에 닿았다
};

// 이번 프레임에 재생 헤드가 지나간 구간. 정규화 시간(0~1) 단위.
struct FDeltaTimeRecord
{
	float Previous = 0.0f; // 움직이기 전 시간
	float Delta = 0.0f;    // 되감기 전 값이라 부호로 방향, 크기로 바퀴 수를 알 수 있다

	void Set(float InPrevious, float InDelta)
	{
		Previous = InPrevious;
		Delta = InDelta;
	}

	bool IsMoved() const { return Delta != 0.0f; }
};
