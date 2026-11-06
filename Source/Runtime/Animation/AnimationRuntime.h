#pragma once

struct FAnimationPose;
struct FAnimationCurve;

struct FAnimationRuntime
{
	// 두 포즈를 Alpha 로 섞는다.
	static void BlendTwoPosesTogether(
	    const FAnimationPose& PoseA, const FAnimationPose& PoseB,
	    const FAnimationCurve& CurveA, const FAnimationCurve& CurveB,
	    float WeightA, FAnimationPose& OutPose, FAnimationCurve& OutCurve);

};