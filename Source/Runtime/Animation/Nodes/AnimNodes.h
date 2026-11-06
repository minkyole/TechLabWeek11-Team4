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
	UAnimSequence* Sequence = nullptr;

	float CurrentTime = 0.0f;
	float PlayRate = 1.0f;

	bool bLoop = true;
	bool bPlaying = true;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};

// 두 포즈를 Alpha 로 섞는다.
struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A; // 입력 포즈 (다른 노드에 연결)
	FPoseLink B;
	float Alpha = 0.0f; // 0 = A, 1 = B

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};

// 단일 포즈를 그대로 출력한다. 디버깅용.
struct FAnimNode_Constant : public FAnimNode_Base
{
	FPoseContext ConstantPose;
	
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};