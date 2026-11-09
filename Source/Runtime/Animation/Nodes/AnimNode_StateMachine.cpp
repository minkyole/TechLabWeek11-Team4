#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"

#include "Animation/MyAnimInstance.h"

#include <algorithm>
#include <cmath>

namespace
{
	float EvaluateBlendAlpha(
		EAnimBlendCurve Curve,
		float Progress)
	{
		const float T = std::clamp(Progress, 0.0f, 1.0f);

		switch (Curve)
		{
		case EAnimBlendCurve::SmoothStep:
			return T * T * (3.0f - 2.0f * T);
		case EAnimBlendCurve::Linear:
		default:
			return T;
		}
	}
}

bool FAnimNode_StateMachine::Initialize(
    const FAnimStateMachineDefinition& InDefinition, 
	const TArray<FAnimNode_Base*>& InNodes)
{
	ClearTransition();

	Definition = nullptr;
	Nodes = {};
	CurrentState = InvalidAnimStateId;

	bHasLastOutput = false;
	LastOutputPose = FAnimationPose();
	LastOutputCurve.Reset();

	if (!InDefinition.States.IsValidIndex(InDefinition.EntryState))
		return false;

	for (const FAnimStateDefinition& State : InDefinition.States)
	{
		if (!InNodes.IsValidIndex(State.RootNodeIndex))
			return false;

		if (!InNodes[State.RootNodeIndex])
			return false;
	}

	for (const FAnimTransitionDefinition& Transition
		: InDefinition.Transitions)
	{
		if (!InDefinition.States.IsValidIndex(Transition.To))
			return false;

		if (!Transition.bFromAnyState &&
			!InDefinition.States.IsValidIndex(Transition.From))
		{
			return false;
		}

		if (!std::isfinite(Transition.Blend.Duration) ||
			Transition.Blend.Duration < 0.0f)
		{
			return false;
		}

		if (Transition.Blend.Curve != EAnimBlendCurve::Linear &&
			Transition.Blend.Curve != EAnimBlendCurve::SmoothStep)
		{
			return false;
		}
	}

	Definition = &InDefinition;
	Nodes = InNodes;

	return true;
}

FAnimNode_Base* FAnimNode_StateMachine::GetStateNode(
	FAnimStateId StateId) const
{
	if (!Definition || !Definition->States.IsValidIndex(StateId))
		return nullptr;

	const int32 NodeIndex =
	    Definition->States[StateId].RootNodeIndex;

	if (!Nodes.IsValidIndex(NodeIndex))
		return nullptr;

	return Nodes[NodeIndex];
}

void FAnimNode_StateMachine::OnEnter()
{
	ClearTransition();

	bHasLastOutput = false;
	LastOutputPose = FAnimationPose();
	LastOutputCurve.Reset();

	CurrentState = InvalidAnimStateId;

	FAnimNode_Base* EntryNode =
	    GetStateNode(Definition->EntryState);

	if (!EntryNode)
	{
		assert(false);
		return;
	}

	CurrentState = Definition->EntryState;
	EntryNode->OnEnter(); // 초기 상태 노드에 진입
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	FAnimNode_Base* CurrentNode = GetStateNode(CurrentState);

	if (!CurrentNode)
	{
		assert(false);
		return;
	}

	// 일반 크로스페이드: 이전 애니메이션도 계속 진행한다.
	// 스냅샷 전환: 이전 포즈가 고정되어 있으므로 업데이트 하지 않는다.
	if (bTransitioning && !bUseSnapshot && PreviousNode)
	{
		PreviousNode->Update(Context);
	}

	CurrentNode->Update(Context);

	if (bTransitioning)
	{
		const float DeltaSeconds =
		    std::isfinite(Context.DeltaSeconds)
		        ? std::max(Context.DeltaSeconds, 0.0f)
		        : 0.0f;

		BlendElapsed += DeltaSeconds;

		if (BlendElapsed >= BlendDuration)
		{
			ClearTransition();
		}
	}
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	FAnimNode_Base* CurrentNode = GetStateNode(CurrentState);

	if (!CurrentNode)
	{
		assert(false);
		Output.ResetToRefPose();
		bHasLastOutput = false;
		return;
	}

	// 복사 생성자는 동일한 평가 환경의 기준 포즈 버퍼를 만든다.
	FPoseContext CurrentPose(Output);
	CurrentNode->Evaluate(CurrentPose);

	if (bTransitioning)
	{
		FPoseContext SourcePose(Output);
		bool bSourceValid = false;

		if (bUseSnapshot)
		{
			// 메시 교체 등으로 다른 스켈레톤의 포즈가 섞이지 않게 검사.
			bSourceValid =
			    TransitionSourcePose.TargetMesh ==
			        CurrentPose.Pose.TargetMesh &&
			    TransitionSourcePose.LocalTransforms.Num() ==
			        CurrentPose.Pose.LocalTransforms.Num();

			if (bSourceValid)
			{
				// Context 복사 생성 대신 실제 포즈와 커브를 대입한다.
				SourcePose.Pose = TransitionSourcePose;
				SourcePose.Curve = TransitionSourceCurve;
			}
		}
		else if (PreviousNode)
		{
			PreviousNode->Evaluate(SourcePose);
			bSourceValid = true;
		}

		if (bSourceValid)
		{
			const float Alpha = EvaluateBlendAlpha(
			    BlendCurve,
			    BlendElapsed / BlendDuration);

			// 이 프로젝트의 함수는 첫 번째 포즈의 가중치를 받는다.
			FAnimationRuntime::BlendTwoPosesTogether(
				SourcePose.Pose, CurrentPose.Pose,
				SourcePose.Curve, CurrentPose.Curve,
			    1.0f - Alpha, Output.Pose, Output.Curve);
		}
		else
		{
			Output.Pose = CurrentPose.Pose;
			Output.Curve = CurrentPose.Curve;
		}
	}
	else // 전환 중이 아니면 현재 상태의 포즈를 그대로 출력한다.
	{
		Output.Pose = CurrentPose.Pose;
		Output.Curve = CurrentPose.Curve;
	}

	// 반드시 혼합이 끝난 최종 출력을 저장한다.
	LastOutputPose = Output.Pose;
	LastOutputCurve = Output.Curve;
	bHasLastOutput = Output.Pose.TargetMesh != nullptr;
}

bool FAnimNode_StateMachine::TryTransition(
	FAnimTransitionId TransitionId)
{
	if (!Definition ||
		!Definition->Transitions.IsValidIndex(TransitionId))
	{
		return false;
	}

	const FAnimTransitionDefinition& Transition =
	    Definition->Transitions[TransitionId];

	if (!Transition.bFromAnyState &&
		Transition.From != CurrentState)
	{
		return false;
	}

	if (Transition.To == CurrentState &&
		!Transition.bRestartIfSameState)
	{
		return false;
	}

	if (!GetStateNode(CurrentState) ||
		!GetStateNode(Transition.To))
	{
		return false;
	}

	ChangeState(Transition);
	return true;
}

bool FAnimNode_StateMachine::IsClipFinished() const
{
	const FAnimNode_Base* CurrentNode = GetStateNode(CurrentState);
	return CurrentNode && CurrentNode->IsFinished();
}

void FAnimNode_StateMachine::ChangeState(
    const FAnimTransitionDefinition& Transition)
{
	FAnimNode_Base* NewNode = GetStateNode(Transition.To);
	FAnimNode_Base* OldNode = GetStateNode(CurrentState);

	const float RequestedDuration = Transition.Blend.Duration;

	// 진행 중인 혼합을 끊거나 같은 플레이어를 재시작할 때는
	// 노드 두 개로 이전 출력을 재현할 수 없으므로 스냅샷 사용.
	const bool bNeedsSnapshot =
	    bTransitioning || OldNode == NewNode;

	const bool bCanBlend =
	    RequestedDuration > 0.0f &&
	    (!bNeedsSnapshot || bHasLastOutput);

	ClearTransition();

	if (bCanBlend)
	{
		bTransitioning = true;
		BlendDuration = RequestedDuration;
		BlendCurve = Transition.Blend.Curve;
		bUseSnapshot = bNeedsSnapshot;

		if (bUseSnapshot)
		{
			TransitionSourcePose = LastOutputPose;
			TransitionSourceCurve = LastOutputCurve;
		}
		else
		{
			PreviousNode = OldNode;
		}
	}

	CurrentState = Transition.To;
	NewNode->OnEnter();
}

void FAnimNode_StateMachine::ClearTransition()
{
	PreviousNode = nullptr;
	bTransitioning = false;
	bUseSnapshot = false;

	BlendElapsed = 0.0f;
	BlendDuration = 0.0f;
	BlendCurve = EAnimBlendCurve::Linear;

	TransitionSourcePose = FAnimationPose();
	TransitionSourceCurve.Reset();
}