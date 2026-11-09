#pragma once

#include "Animation/Nodes/AnimNodes.h"

enum class EAnimState : uint8
{
	Locomotion, // 안에 BlendSpace1D (Idle / Walk / Run)
	Attack,
	Hit,
};

class UMyAnimInstance;

struct FAnimNode_StateMachine : public FAnimNode_Base
{
	EAnimState State = EAnimState::Locomotion;
	FAnimNode_Base* StateNodes[3] = {}; // 상태마다 포즈를 내는 노드

	void SetStateNode(EAnimState InState, FAnimNode_Base* Node);

	void OnEnter() override;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	void ProcessState(UMyAnimInstance* AnimInstance);

	bool IsClipFinished() const;

private:
	void ChangeState(EAnimState NewState, bool bRestartIfSameState);
};