#pragma once

#include "Animation/AnimationRuntime.h"
#include "Animation/AnimationContext.h"
#include "Animation/Nodes/PoseLink.h"

struct FAnimationUpdateContext;
struct FPoseContext;
class UAnimSequence;

// 모든 노드의 뿌리. 프레임마다 Update -> Evaluate 순서로 불린다.
struct FAnimNode_Base
{
	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) {}
};

// 클립 하나를 재생한다.
struct FAnimNode_SequencePlayer : public FAnimNode_Base
{
	UAnimSequence* Sequence;
	float CurrentTime = 0.0f;
	bool bLoop = true;
};

// 두 포즈를 Alpha 로 섞는다.
struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A; // 입력 포즈 (다른 노드에 연결)
	FPoseLink B;
	float Alpha = 0.0f; // 0 = A, 1 = B

	virtual void Update(const FAnimationUpdateContext& Context) override
	{
		A.Update(Context); // 각 입력이 자기 재생 시간을 흘린다
		B.Update(Context);
	}

	virtual void Evaluate(FPoseContext& Output) override
	{
		FPoseContext PoseA(Output);
		FPoseContext PoseB(Output);
		A.Evaluate(PoseA);
		B.Evaluate(PoseB);

		// 본마다 위치 / 스케일은 Lerp, 회전은 Quaternion Slerp
		FAnimationRuntime::BlendTwoPosesTogether(
		    PoseA.Pose, PoseB.Pose, PoseA.Curve, PoseB.Curve,
		    1.0f - Alpha, // 첫 번째 포즈(A) 의 가중치
		    Output.Pose, Output.Curve);
	}
};