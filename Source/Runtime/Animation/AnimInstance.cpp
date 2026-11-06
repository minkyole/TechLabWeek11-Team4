#include "EnginePCH.h"
#include "AnimInstance.h"

#include "Animation/Nodes/AnimNodes.h"
#include "Components/SkeletalMeshComponent.h"

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
}

void UAnimInstance::UnInitializeAnimation()
{
	if (bIsInitialized)
	{
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

	// 먼저 그래프에 전달할 변수를 갱신한다.
	NativeUpdateAnimation(SafeDeltaSeconds);

	if (!RootNode)
		return;

	FAnimationUpdateContext Context;
	Context.AnimInstance = this;
	Context.DeltaSeconds = SafeDeltaSeconds;

	RootNode->Update(Context);
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

	if (Output.Pose.TargetMesh != Mesh)
	{
		Output.Pose.Initialize(*Mesh);
	}

	Output.ResetToRefPose();

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

	// 재초기화 시 클립과 재생 설정은 유지하고 시간은 처음으로 돌린다.
	SequencePlayer.CurrentTime = 0.0f;
}

void UAnimSingleNodeInstance::SetAnimationAsset(
	UAnimSequence* InSequence)
{
	if (SequencePlayer.Sequence == InSequence)
		return;

	SequencePlayer.Sequence = InSequence;
	SequencePlayer.CurrentTime = 0.0f;

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

void UAnimSingleNodeInstance::SetPosition(float InPosition)
{
	const float Length = GetLength();

	if (InPosition < 0.0f)
	{
		InPosition = 0.0f;
	}
	else if (InPosition > Length)
	{
		InPosition = Length;
	}

	SequencePlayer.CurrentTime = InPosition;
}

float UAnimSingleNodeInstance::GetPosition() const
{
	return SequencePlayer.CurrentTime;
}

float UAnimSingleNodeInstance::GetLength() const
{
	// TODO
	throw std::logic_error("Not implemented");
}