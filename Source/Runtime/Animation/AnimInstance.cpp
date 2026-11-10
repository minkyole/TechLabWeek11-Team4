#include "EnginePCH.h"
#include "AnimInstance.h"

#include "Animation/Nodes/AnimNodes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"

void UAnimInstance::InitializeAnimation(
    USkeletalMeshComponent* InComponent)
{
	// 기존 연결과 파생 클래스의 초기화 상태부터 정리한다.
	UnInitializeAnimation();

	if (!InComponent || !InComponent->GetSkeletalMesh())
	{
		assert(false);
		return;
	}

	// 파생 클래스에서 RootNode 연결 등의 작업을 수행한다.
	OwningComponent = InComponent;

	bIsInitialized = true;

	NativeInitializeAnimation();
}

void UAnimInstance::UnInitializeAnimation()
{
	if (bIsInitialized)
	{
		EndAllNotifyStates();

		// 파생 클래스가 정리할 때는 아직 소유 컴포넌트에 접근 가능하다.
		NativeUnInitializeAnimation();
	}

	RootNode = nullptr;
	OwningComponent = nullptr;
	bIsInitialized = false;
}

USkeletalMesh* UAnimInstance::GetSkeletalMesh() const
{
	return OwningComponent
	           ? OwningComponent->GetSkeletalMesh()
	           : nullptr;
}

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	if (!bIsInitialized || !GetSkeletalMesh())
	{
		assert(false);
		return;
	}

	// 역재생은 음수 DeltaSeconds 대신 음수 PlayRate를 사용한다.
	const float SafeDeltaSeconds =
	    DeltaSeconds > 0.0f ? DeltaSeconds : 0.0f;

	NotifyQueue.Empty();
	NotifyStateQueue.Empty();

	// 먼저 그래프에 전달할 변수를 갱신한다.
	NativeUpdateAnimation(SafeDeltaSeconds);

	if (!RootNode)
	{
		TriggerAnimNotifies();
		return;
	}

	FAnimationUpdateContext Context;
	Context.AnimInstance = this;
	Context.DeltaSeconds = SafeDeltaSeconds;

	RootNode->Update(Context);

	TriggerAnimNotifies();
}

void UAnimInstance::TriggerAnimNotifies()
{
	USkeletalMeshComponent* MeshComp = GetSkeletalMeshComponent();

	auto IsSameState = [](const FQueuedAnimNotify& A, const FQueuedAnimNotify& B)
	{
		return A.Sequence == B.Sequence && A.Event.TriggerTime == B.Event.TriggerTime &&
			A.Event.Duration == B.Event.Duration && A.Event.NotifyName == B.Event.NotifyName;
	};
	auto ContainsState = [&IsSameState](const TArray<FQueuedAnimNotify>& States, const FQueuedAnimNotify& State)
	{
		for (const FQueuedAnimNotify& Other : States)
		{
			if (IsSameState(Other, State))
			{
				return true;
			}
		}
		return false;
	};

	// 같은 State가 루프 경계 등으로 두 번 들어와도 한 번만 활성으로 본다.
	TArray<FQueuedAnimNotify> CurrentStates;
	for (const FQueuedAnimNotify& State : NotifyStateQueue)
	{
		if (!ContainsState(CurrentStates, State))
		{
			CurrentStates.Add(State);
		}
	}

	const TArray<FQueuedAnimNotify> PreviousStates = ActiveNotifyStates;
	ActiveNotifyStates = CurrentStates;

	// 루프로 같은 구간에 다시 들어갈 때 End가 Begin보다 먼저 나가도록 End부터 보낸다.
	for (const FQueuedAnimNotify& State : PreviousStates)
	{
		if (!ContainsState(CurrentStates, State) && MeshComp)
		{
			MeshComp->HandleAnimNotifyStateEnd(State.Event);
			MeshComp->OnAnimNotifyStateEnd.Broadcast(State.Event, State.Sequence);
		}
	}
	for (const FQueuedAnimNotify& State : CurrentStates)
	{
		if (!ContainsState(PreviousStates, State) && MeshComp)
		{
			MeshComp->HandleAnimNotifyStateBegin(State.Event);
			MeshComp->OnAnimNotifyStateBegin.Broadcast(State.Event, State.Sequence);
		}
	}

	// Notify가 게임 코드를 실행하다 애니메이션을 다시 갱신하면 NotifyQueue가 바뀌므로 복사본을 돈다.
	const TArray<FQueuedAnimNotify> Pending = NotifyQueue;
	for (const FQueuedAnimNotify& Queued : Pending)
	{
		if (Queued.Event.Notify)
		{
			Queued.Event.Notify->Notify(MeshComp, Queued.Sequence);
		}
		else if (MeshComp)
		{
			MeshComp->HandleAnimNotify(Queued.Event);
		}

		if (MeshComp)
		{
			MeshComp->OnAnimNotify.Broadcast(Queued.Event, Queued.Sequence);
		}
	}
}

void UAnimInstance::QueueAnimNotifyState(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence, float Weight)
{
	FQueuedAnimNotify& Queued = NotifyStateQueue.AddDefaulted_GetRef();
	Queued.Event = Event;
	Queued.Sequence = Sequence;
	Queued.Weight = Weight;
}

void UAnimInstance::EndAllNotifyStates()
{
	USkeletalMeshComponent* MeshComp = GetSkeletalMeshComponent();
	const TArray<FQueuedAnimNotify> PreviousStates = ActiveNotifyStates;
	ActiveNotifyStates.Empty();
	NotifyStateQueue.Empty();

	if (!MeshComp)
	{
		return;
	}
	for (const FQueuedAnimNotify& State : PreviousStates)
	{
		MeshComp->HandleAnimNotifyStateEnd(State.Event);
		MeshComp->OnAnimNotifyStateEnd.Broadcast(State.Event, State.Sequence);
	}
}

void UAnimInstance::QueueAnimNotify(const FAnimNotifyEvent& Event, UAnimSequenceBase* Sequence, float Weight)
{
	FQueuedAnimNotify& Queued = NotifyQueue.AddDefaulted_GetRef();
	Queued.Event = Event;
	Queued.Sequence = Sequence;
	Queued.Weight = Weight;
}

void UAnimInstance::EvaluateAnimation(FPoseContext& Output)
{
	Output.AnimInstance = this;
	Output.Curve.Reset();

	const USkeletalMesh* Mesh = GetSkeletalMesh();

	if (!bIsInitialized || !Mesh)
	{
		assert(false);

		// 이번 프레임의 결과를 남기지 않는다.
		Output.Pose = FAnimationPose();
		return;
	}

	Output.Curve.Reset();

	if (Output.Pose.TargetMesh != Mesh)
	{
		// Initialize에서 ResetToRefPose를 호출하므로,
		// 여기서는 Pose를 초기화만 한다.

		Output.Pose.Initialize(*Mesh);
	}
	else
	{
		Output.Pose.ResetToRefPose();
	}


	if (RootNode)
	{
		RootNode->Evaluate(Output);
	}
}

USkeletalMeshComponent* UAnimInstance::GetSkeletalMeshComponent() const
{
	return OwningComponent;
}

bool UAnimInstance::IsInitialized() const
{
	return bIsInitialized;
}


void UAnimSingleNodeInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	RootNode = &SequencePlayer;

	// 재초기화 시 클립과 재생 설정은 유지하고 시간은 진행 방향의 시작점으로 돌린다.
	SequencePlayer.ResetPlayback();
}

void UAnimSingleNodeInstance::SetAnimationAsset(
    UAnimSequence* InSequence)
{
	if (SequencePlayer.Sequence == InSequence)
		return;

	SequencePlayer.Sequence = InSequence;

	// 역재생 중에 클립을 바꾸면 끝(1)에서 시작해야 바로 멈추지 않는다
	SequencePlayer.ResetPlayback();

	// 클립 교체와 재생/일시정지는 별도 제어로 둔다.
	// bPlyaing, bLoop, PlayRate는 유지한다.
}

UAnimSequence* UAnimSingleNodeInstance::GetAnimationAsset() const
{
	return SequencePlayer.Sequence;
}

void UAnimSingleNodeInstance::SetPlaying(bool bInPlaying)
{
	SequencePlayer.bPlaying = bInPlaying;
}

bool UAnimSingleNodeInstance::IsPlaying() const
{
	return SequencePlayer.Sequence != nullptr && SequencePlayer.bPlaying;
}

void UAnimSingleNodeInstance::SetLooping(bool bInLooping)
{
	SequencePlayer.bLoop = bInLooping;
}

bool UAnimSingleNodeInstance::IsLooping() const
{
	return SequencePlayer.bLoop;
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
	SequencePlayer.PlayRate = InPlayRate;
}

float UAnimSingleNodeInstance::GetPlayRate() const
{
	return SequencePlayer.PlayRate;
}

void UAnimSingleNodeInstance::SetReversePlay(bool bReverse)
{
	SequencePlayer.SetReversePlay(bReverse);
}

bool UAnimSingleNodeInstance::IsReversePlay() const
{
	return SequencePlayer.IsReversePlay();
}

void UAnimSingleNodeInstance::SetPosition(float InPosition)
{
	const float Length = GetLength();

	// 클립이 없으면 0/0이 NaN이 되어 SetNormalizedTime이 무시하므로 처음으로 둔다
	if (Length <= 0.0f)
	{
		SequencePlayer.SetNormalizedTime(0.0f);
		return;
	}

	// 범위 밖 값은 SetNormalizedTime이 [0, 1]로 클램프한다
	SequencePlayer.SetNormalizedTime(InPosition / Length);
}

float UAnimSingleNodeInstance::GetPosition() const
{
	return SequencePlayer.GetNormalizedTime() * GetLength();
}

float UAnimSingleNodeInstance::GetLength() const
{
	if (SequencePlayer.Sequence)
	{
		return SequencePlayer.Sequence->GetPlayLength();
	}
	return 0.f;
}

bool UAnimSingleNodeInstance::IsFinished() const
{
	return SequencePlayer.IsFinished();
}

const FDeltaTimeRecord& UAnimSingleNodeInstance::GetDeltaTimeRecord() const
{
	return SequencePlayer.GetDeltaTimeRecord();
}