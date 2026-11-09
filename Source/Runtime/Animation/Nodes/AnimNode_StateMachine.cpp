#include "EnginePCH.h"
#include "AnimNode_StateMachine.h"

#include "Animation/MyAnimInstance.h"

void FAnimNode_StateMachine::SetStateNode(
    EAnimState InState, FAnimNode_Base* Node)
{
	StateNodes[static_cast<int>(InState)] = Node;
}

void FAnimNode_StateMachine::OnEnter()
{
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
	StateNodes[static_cast<int>(State)]->Update(Context); // 현재 상태 노드에 전파
}

void FAnimNode_StateMachine::Evaluate(FPoseContext& Output)
{
	StateNodes[static_cast<int>(State)]->Evaluate(Output); // 현재 상태 노드의 포즈를 그대로 쓴다.
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
	if (!NewNode)
	{
		assert(false);
		return;
	}

	State = NewState;
	NewNode->OnEnter();
}