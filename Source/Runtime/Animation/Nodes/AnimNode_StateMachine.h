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
	// State는 전환 중에도 목적지 상태를 의미한다.
	EAnimState State = EAnimState::Locomotion;
	FAnimNode_Base* StateNodes[3] = {}; // 상태마다 포즈를 내는 노드

	float DefaultBlendDuration = 0.2f;
	float HitBlendDuration = 0.1f;

	void SetStateNode(EAnimState InState, FAnimNode_Base* Node);

	void OnEnter() override;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	void ProcessState(UMyAnimInstance* AnimInstance);

	bool IsClipFinished() const;

private:
	void ChangeState(EAnimState NewState, bool bRestartIfSameState);
	void ClearTransition();

	// 일반 전환의 출발 노드. 비소유 참조.
	FAnimNode_Base* PreviousNode = nullptr;

	bool bTransitioning = false;
	bool bUseSnapshot = false;

	float BlendElapsed = 0.0f;
	float BlendDuration = 0.0f;

	// 매 Evaluate에서 저장하는 최종 출력.
	bool bHasLastOutput = false;
	FAnimationPose LastOutputPose;
	FAnimationCurve LastOutputCurve;

	// 전환이 끊긴 순간의 출력. 전환 중에는 고정한다.
	FAnimationPose TransitionSourcePose;
	FAnimationCurve TransitionSourceCurve;
};