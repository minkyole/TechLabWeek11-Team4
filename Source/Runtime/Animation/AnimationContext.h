#pragma once 

#include "Containers/Map.h"
#include "Core/NameTypes.h"
#include "AnimationPose.h"

class UAnimInstance;
struct FAnimationPose;

struct FAnimationUpdateContext
{
	UAnimInstance* AnimInstance = nullptr;
	float DeltaSeconds = 0.0f;
};

struct FAnimationCurve
{
	TMap<FName, float, FNameHash> Values;
	
	void Reset();
};

struct FPoseContext
{
	UAnimInstance* AnimInstance = nullptr;
	FAnimationPose Pose;
	FAnimationCurve Curve;

	explicit FPoseContext(UAnimInstance& InInstance);

	// 같은 평가 환경과 본 구성을 갖는 독립된 출력 버퍼 생성.
	// Pose 데이터는 RefPose, Curve는 빈 값으로 초기화한다.
	FPoseContext(const FPoseContext& Other);
	FPoseContext& operator=(const FPoseContext& Other) = delete;

	void ResetToRefPose();
};