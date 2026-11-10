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


FAnimNode_AssetPlayerBase& UAnimSingleNodeInstance::ActivePlayer()
{
	return BlendSpacePlayer.BlendSpace ? static_cast<FAnimNode_AssetPlayerBase&>(BlendSpacePlayer) : SequencePlayer;
}

const FAnimNode_AssetPlayerBase& UAnimSingleNodeInstance::ActivePlayer() const
{
	return BlendSpacePlayer.BlendSpace ? static_cast<const FAnimNode_AssetPlayerBase&>(BlendSpacePlayer) : SequencePlayer;
}

void UAnimSingleNodeInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RootNode = &ActivePlayer();
	ActivePlayer().ResetPlayback();
	if (IsInitialized()) UpdateAnimation(0.0f);
}

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequence* InSequence)
{
	if (!BlendSpacePlayer.BlendSpace && SequencePlayer.Sequence == InSequence) return;
	const bool Playing = ActivePlayer().bPlaying;
	const bool Looping = ActivePlayer().bLoop;
	const float Rate = ActivePlayer().PlayRate;
	BlendSpacePlayer.BlendSpace = nullptr;
	SequencePlayer.Sequence = InSequence;
	SequencePlayer.bPlaying = Playing; SequencePlayer.bLoop = Looping; SequencePlayer.PlayRate = Rate;
	SequencePlayer.ResetPlayback();
	RootNode = &SequencePlayer;
}

UAnimSequence* UAnimSingleNodeInstance::GetAnimationAsset() const { return SequencePlayer.Sequence; }

void UAnimSingleNodeInstance::SetBlendSpaceAsset(UBlendSpace1D* InBlendSpace)
{
	if (!InBlendSpace) { SetAnimationAsset(nullptr); return; }
	if (BlendSpacePlayer.BlendSpace == InBlendSpace) return;
	const bool Playing = ActivePlayer().bPlaying;
	const bool Looping = ActivePlayer().bLoop;
	const float Rate = ActivePlayer().PlayRate;
	SequencePlayer.Sequence = nullptr;
	BlendSpacePlayer.BlendSpace = InBlendSpace;
	BlendSpacePlayer.bPlaying = Playing; BlendSpacePlayer.bLoop = Looping; BlendSpacePlayer.PlayRate = Rate;
	BlendSpacePlayer.ResetPlayback();
	const FBlendParameter* Axis = InBlendSpace->GetBlendParameter(0);
	BlendSpacePlayer.X = Axis ? Axis->Min : 0.0f;
	RootNode = &BlendSpacePlayer;
	if (IsInitialized()) UpdateAnimation(0.0f);
}

UBlendSpace1D* UAnimSingleNodeInstance::GetBlendSpaceAsset() const
{
	return Cast<UBlendSpace1D>(BlendSpacePlayer.BlendSpace);
}

void UAnimSingleNodeInstance::SetBlendSpaceInput(float X)
{
	if (!std::isfinite(X) || !BlendSpacePlayer.BlendSpace) return;
	const FBlendParameter* Axis = BlendSpacePlayer.BlendSpace->GetBlendParameter(0);
	if (!Axis || !std::isfinite(Axis->Min) || !std::isfinite(Axis->Max) || Axis->Min >= Axis->Max) return;
	BlendSpacePlayer.X = std::clamp(X, Axis->Min, Axis->Max);
	if (IsInitialized()) UpdateAnimation(0.0f);
}

void UAnimSingleNodeInstance::SetPlaying(bool Value) { ActivePlayer().bPlaying = Value; }
bool UAnimSingleNodeInstance::IsPlaying() const { return ActivePlayer().GetAnimationAsset() && ActivePlayer().bPlaying; }
void UAnimSingleNodeInstance::SetLooping(bool Value) { ActivePlayer().bLoop = Value; }
bool UAnimSingleNodeInstance::IsLooping() const { return ActivePlayer().bLoop; }
void UAnimSingleNodeInstance::SetPlayRate(float Value) { if (std::isfinite(Value)) ActivePlayer().PlayRate = Value; }
float UAnimSingleNodeInstance::GetPlayRate() const { return ActivePlayer().PlayRate; }

void UAnimSingleNodeInstance::SetPosition(float Value)
{
	if (!std::isfinite(Value)) return;
	const float Length = GetLength();
	ActivePlayer().SetNormalizedTime(Length > 0.0f ? std::clamp(Value / Length, 0.0f, 1.0f) : 0.0f);
}

float UAnimSingleNodeInstance::GetPosition() const { return ActivePlayer().GetNormalizedTime() * GetLength(); }

float UAnimSingleNodeInstance::GetLength() const
{
	if (!BlendSpacePlayer.BlendSpace && SequencePlayer.Sequence && !SequencePlayer.Sequence->GetDataModel()) return 0.0f;
	const float Length = ActivePlayer().GetEffectivePlayLength();
	return std::isfinite(Length) && Length > 0.0f ? Length : 0.0f;
}

bool UAnimSingleNodeInstance::IsFinished() const
{
	return ActivePlayer().IsFinished();
}

const FDeltaTimeRecord& UAnimSingleNodeInstance::GetDeltaTimeRecord() const
{
	return ActivePlayer().GetDeltaTimeRecord();
}

void UAnimSingleNodeInstance::SetReversePlay(bool bReverse)
{
	ActivePlayer().SetReversePlay(bReverse);
}

bool UAnimSingleNodeInstance::IsReversePlay() const
{
	return ActivePlayer().IsReversePlay();
}