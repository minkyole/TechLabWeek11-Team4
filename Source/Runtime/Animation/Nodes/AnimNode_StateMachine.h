#pragma once

#include "Animation/Nodes/AnimNodes.h"

using FAnimStateId = int32;
using FAnimTransitionId = int32;

inline constexpr FAnimStateId InvalidAnimStateId = -1;
inline constexpr FAnimTransitionId InvalidAnimTransitionId = -1;

enum class EAnimBlendCurve : uint8
{
	Linear,
	SmoothStep,
};

struct FAnimBlendSettings
{
	float Duration = 0.2f;
	EAnimBlendCurve Curve = EAnimBlendCurve::Linear;
};

struct FAnimStateDefinition
{
	FName Name;

	// 캐릭터별 런타임 노드 배열에서 사용하는 인덱스
	int32 RootNodeIndex = -1;
};

struct FAnimTransitionDefinition
{
	FAnimStateId From = InvalidAnimStateId;
	FAnimStateId To = InvalidAnimStateId;

	// true이면 From을 무시하고 모든 상태에서 사용할 수 있다.
	bool bFromAnyState = false;

	FAnimBlendSettings Blend;

	// 현재 상태가 To여도 재진입할 것인지.
	bool bRestartIfSameState = false;
};

struct FAnimStateMachineDefinition
{
	FAnimStateId EntryState = InvalidAnimStateId;

	TArray<FAnimStateDefinition> States;
	TArray<FAnimTransitionDefinition> Transitions;
};

enum class EAnimState : uint8
{
	Locomotion, // 안에 BlendSpace1D (Idle / Walk / Run)
	Attack,
	Hit,
};

class UMyAnimInstance;

struct FAnimNode_StateMachine : public FAnimNode_Base
{
	bool Initialize(
	    const FAnimStateMachineDefinition& InDefinition,
	    const TArray<FAnimNode_Base*>& InNodes);

	void OnEnter() override;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;

	bool TryTransition(FAnimTransitionId TransitionId);
	FAnimStateId GetCurrentState() const { return CurrentState; }

	//void ProcessState(UMyAnimInstance* AnimInstance);

	bool IsClipFinished() const;

private:
	FAnimNode_Base* GetStateNode(FAnimStateId StateId) const;

	void ChangeState(const FAnimTransitionDefinition& Transition);
	void ClearTransition();

	// 비소유 참조. 실행 중에는 정의를 수정하지 않는다.
	const FAnimStateMachineDefinition* Definition = nullptr;

	// 배열은 이 상태 머신이 소유하지만 노드 자체는 소유하지 않는다.
	TArray<FAnimNode_Base*> Nodes;

	FAnimStateId CurrentState = InvalidAnimStateId;

	// 일반 전환의 출발 노드. 비소유 참조.
	FAnimNode_Base* PreviousNode = nullptr;

	bool bTransitioning = false;
	bool bUseSnapshot = false;

	float BlendElapsed = 0.0f;
	float BlendDuration = 0.0f;
	EAnimBlendCurve BlendCurve = EAnimBlendCurve::Linear;

	// 매 Evaluate에서 저장하는 최종 출력.
	bool bHasLastOutput = false;
	FAnimationPose LastOutputPose;
	FAnimationCurve LastOutputCurve;

	// 전환이 끊긴 순간의 출력. 전환 중에는 고정한다.
	FAnimationPose TransitionSourcePose;
	FAnimationCurve TransitionSourceCurve;
};