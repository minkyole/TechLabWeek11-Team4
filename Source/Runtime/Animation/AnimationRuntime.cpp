#include "EnginePCH.h"
#include "AnimationRuntime.h"

#include "Animation/AnimationPose.h"
#include "Animation/AnimationContext.h"

void FAnimationRuntime::BlendTwoPosesTogether(
    const FAnimationPose& PoseA, const FAnimationPose& PoseB,
    const FAnimationCurve& CurveA, const FAnimationCurve& CurveB,
    float WeightA, FAnimationPose& OutPose, FAnimationCurve& OutCurve)
{
	// 입력과 출력을 같은 객체로 사용하는 것은 지원하지 않는다.
	// 특히 Curve를 지우면 입력까지 지워질 수 있다.
	const bool bSepartedOutputs =
	    &OutPose != &PoseA &&
	    &OutPose != &PoseB &&
	    &OutCurve != &CurveA &&
	    &OutCurve != &CurveB;

	if (!bSepartedOutputs)
	{
		assert(false && "Blend outputs must not alias inputs");
		return;
	}

	if (!PoseA.IsCompatibleWith(PoseB) ||
	    !std::isfinite(WeightA))
	{
		assert(false && "Invalid animation blend inputs");

		OutPose = FAnimationPose{};
		OutCurve.Reset();
		return;
	}

	const float WA = FMath::Clamp01(WeightA);
	const float WB = 1.0f - WA;

	// 끝점에서는 해당 입력을 그대로 사용한다.
	if (WA == 1.0f)
	{
		OutPose = PoseA;
		OutCurve = CurveA;
		return;
	}
	else if (WB == 1.0f)
	{
		OutPose = PoseB;
		OutCurve = CurveB;
		return;
	}

	OutPose.TargetMesh = PoseA.TargetMesh;

	const int32 BoneCount = PoseA.LocalTransforms.Num();
	OutPose.LocalTransforms.SetNum(BoneCount, false);

	for (int32 Index = 0; Index < BoneCount; ++Index)
	{
		const FAnimBoneTransform& BoneA =
		    PoseA.LocalTransforms[Index];
		
		const FAnimBoneTransform& BoneB =
		    PoseB.LocalTransforms[Index];

		FAnimBoneTransform& Result =
		    OutPose.LocalTransforms[Index];

		Result.Translation =
		    BoneA.Translation * WA + BoneB.Translation * WB;
		Result.Scale = 
			BoneA.Scale * WA + BoneB.Scale * WB;

		// Slerp의 T는 B쪽으로 이동하는 비율이다.
		Result.Rotation = FMath::SlerpShortestPath(
		    BoneA.Rotation, BoneB.Rotation, WB);
	}

	// Curve는 없는 값을 0으로 보고 가중합한다.
	OutCurve.Reset();

	for (const auto& [Name, ValueA] : CurveA.Values)
	{
		const float* ValueB = CurveB.Values.Find(Name);
		const float Result =
			ValueA * WA +
		    (ValueB ? *ValueB : 0.0f) * WB;

		OutCurve.Values.Add(Name, Result);
	}

	// B에만 있는 항목도 추가한다.
	for (const auto& [Name, ValueB] : CurveB.Values)
	{
		if (!CurveA.Values.Contains(Name))
		{
			OutCurve.Values.Add(Name, ValueB * WB);
		}
	}
}