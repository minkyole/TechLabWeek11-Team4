#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"

#include "Animation/MyAnimInstance.h"

#include <algorithm>
#include <cmath>

void FAnimNode_StateMachine::SetStateNode(
    EAnimState InState, FAnimNode_Base* Node)
{
	StateNodes[static_cast<int>(InState)] = Node;
}

void FAnimNode_StateMachine::OnEnter()
{
	ClearTransition();

	bHasLastOutput = false;
	LastOutputPose = FAnimationPose();
	LastOutputCurve.Reset();

	State = EAnimState::Locomotion; // 초기 상태

	FAnimNode_Base* EntryNode = StateNodes[static_cast<int>(State)];
	if (!EntryNode)
	{
		assert(false);
		return;
	}

	EntryNode->OnEnter(); // 초기 상태 노드에 진입
}

void FAnimNode_StateMachine::Update(const FAnimationUpdateContext& Context)
{
	UMyAnimInstance* AnimInstance =
	    Cast<UMyAnimInstance>(Context.AnimInstance); // 단순화 (UE: AnimInstanceProxy)
	if (!AnimInstance)
	{
		assert(false);
		return;
	}

	ProcessState(AnimInstance);

	FAnimNode_Base* CurrentNode =
	    StateNodes[static_cast<int>(State)];

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
	FAnimNode_Base* CurrentNode =
	    StateNodes[static_cast<int>(State)];

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
			const float Alpha = std::clamp(
			    BlendElapsed / BlendDuration, 0.0f, 1.0f);

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

void FAnimNode_StateMachine::ProcessState(UMyAnimInstance* AnimInstance)
{
	if (AnimInstance->bIsHit) // 우선 순위가 가장 높다. 공격 중에도 피격이 들어오면 Hit로 전환한다.
	{
		ChangeState(EAnimState::Hit, true);
		AnimInstance->bIsHit = false;
	}
	else if (State == EAnimState::Locomotion && AnimInstance->bIsAttacking)
	{
		ChangeState(EAnimState::Attack, false);
		AnimInstance->bIsAttacking = false;
	}
	else if (State != EAnimState::Locomotion && IsClipFinished()) // UE: Automatic Rule / Time Remaining
	{
		ChangeState(EAnimState::Locomotion, false);
	}
}

bool FAnimNode_StateMachine::IsClipFinished() const
{
	const FAnimNode_Base* CurrentNode =
	    StateNodes[static_cast<int>(State)];
	if (!CurrentNode)
	{
		assert(false);
		return true;
	}
	return CurrentNode->IsFinished();
}

void FAnimNode_StateMachine::ChangeState(
    EAnimState NewState, bool bRestartIfSameState)
{
	if (State == NewState && !bRestartIfSameState)
	{
		return;
	}

	FAnimNode_Base* NewNode = StateNodes[static_cast<int>(NewState)];
	FAnimNode_Base* OldNode = StateNodes[static_cast<int>(State)];

	if (!NewNode || !OldNode)
	{
		assert(false);
		return;
	}

	const float RequestedDuration =
	    NewState == EAnimState::Hit
	        ? HitBlendDuration
	        : DefaultBlendDuration;

	// 진행 중인 혼합을 끊거나 같은 플레이어를 재시작할 때는
	// 노드 두 개로 이전 출력을 재현할 수 없으므로 스냅샷 사용.
	const bool bNeedsSnapshot =
	    bTransitioning || OldNode == NewNode;

	const bool bCanBlend =
	    std::isfinite(RequestedDuration) &&
	    RequestedDuration > 0.0f &&
	    (!bNeedsSnapshot || bHasLastOutput);

	ClearTransition();

	if (bCanBlend)
	{
		bTransitioning = true;
		BlendDuration = RequestedDuration;
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

	State = NewState;
	NewNode->OnEnter();
}

void FAnimNode_StateMachine::ClearTransition()
{
	PreviousNode = nullptr;
	bTransitioning = false;
	bUseSnapshot = false;

	BlendElapsed = 0.0f;
	BlendDuration = 0.0f;

	TransitionSourcePose = FAnimationPose();
	TransitionSourceCurve.Reset();
}