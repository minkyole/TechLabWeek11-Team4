#include "EnginePCH.h"
#include "AnimNodes.h"

#include "Animation/AnimationContext.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"

void FAnimNode_TwoWayBlend::OnEnter()
{
	A.OnEnter();
	B.OnEnter();
}

void FAnimNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
	// 각 입력이 자기 재생 시간을 흘린다. 루트 이동도 포즈와 같은 비율로 나눠 넘긴다
	A.Update(Context.FractionalWeight(1.0f - Alpha));
	B.Update(Context.FractionalWeight(Alpha));
}

void FAnimNode_TwoWayBlend::Evaluate(FPoseContext& Output)
{
	FPoseContext PoseA(Output);
	FPoseContext PoseB(Output);
	A.Evaluate(PoseA);
	B.Evaluate(PoseB);

	// 본마다 위치 / 스케일은 Lerp, 회전은 Quaternion Slerp
	FAnimationRuntime::BlendTwoPosesTogether(
	    PoseA.Pose, PoseB.Pose, PoseA.Curve, PoseB.Curve,
	    1.0f - Alpha, // 첫 번째 포즈(A) 의 가중치
	    Output.Pose, Output.Curve);
}

void FAnimNode_Constant::Update(const FAnimationUpdateContext& Context)
{
	// 아무것도 하지 않는다. ConstantPose는 외부에서 설정한다.
}

void FAnimNode_Constant::Evaluate(FPoseContext& Output)
{
	// Pose와 Curve를 그대로 복사한다.
	Output.Pose = ConstantPose.Pose;
	Output.Curve = ConstantPose.Curve;
}

void FAnimNode_AssetPlayerBase::SetNormalizedTime(float InTime)
{
	if (!std::isfinite(InTime))
	{
		return;
	}
	NormalizedTime = std::clamp(InTime, 0.0f, 1.0f);

	// 스크럽·점프는 재생이 아니라 순간이동이라 지나간 구간이 없다
	DeltaTimeRecord.Set(NormalizedTime, 0.0f);
	LastAdvanceResult = ETypeAdvanceAnim::Default;
}

void FAnimNode_AssetPlayerBase::ResetPlayback()
{
	// 루프 역재생도 1에서 시작해야 첫 프레임에 Looped가 떠서 끝 지점 Notify가 울리지 않는다
	SetNormalizedTime(IsReversePlay() ? 1.0f : 0.0f);
}

void FAnimNode_AssetPlayerBase::SetReversePlay(bool bReverse)
{
	// 속도 크기는 유지하고 부호만 바꿔서, 뷰어의 배속 설정과 충돌하지 않게 한다
	const float Speed = std::abs(PlayRate);
	PlayRate = bReverse ? -Speed : Speed;
}

void FAnimNode_AssetPlayerBase::OnEnter()
{
	// 재생 시작 시점에 호출된다. 재생 시간을 초기화한다.
	ResetPlayback();
}

void FAnimNode_AssetPlayerBase::Update(
    const FAnimationUpdateContext& Context)
{
	// 정지 중에도 입력값 변경은 포즈에 반영되어야 한다.
	// 따라서 bPlyaing 검사보다 먼저 호출한다.
	UpdateAssetPlayer(Context);

	// 멈춘 프레임도 "안 움직임"으로 덮어써야 지난 프레임 구간으로 Notify가 또 울리지 않는다
	DeltaTimeRecord.Set(NormalizedTime, 0.0f);
	LastAdvanceResult = ETypeAdvanceAnim::Default;

	if (!bPlaying || GetAnimationAsset() == nullptr)
	{
		return;
	}

	const float Length = GetEffectivePlayLength();

	if (!std::isfinite(Length) || Length <= 0.0f)
	{
		return;
	}

	if (!std::isfinite(Context.DeltaSeconds) ||
	    Context.DeltaSeconds < 0.0f ||
	    !std::isfinite(PlayRate) ||
	    PlayRate == 0.0f)
	{
		return;
	}

	// 초 단위를 길이로 나눠 정규화한다. BlendSpace는 이 길이가 가중치에 따라 매 프레임 바뀐다
	const float MoveDelta = static_cast<float>(
	    static_cast<double>(Context.DeltaSeconds) * PlayRate / Length);

	float MovedDelta = 0.0f;
	LastAdvanceResult = FAnimationRuntime::AdvanceTime(
	    bLoop, MoveDelta, NormalizedTime, MovedDelta);

	// Previous는 위에서 움직이기 전 값으로 이미 넣어 두었다
	DeltaTimeRecord.Set(DeltaTimeRecord.Previous, MovedDelta);

	// Notify와 같은 구간 기록을 써야 이벤트와 이동이 같은 시간 범위를 본다
	if (bUseRootMotion && DeltaTimeRecord.IsMoved() && Context.AnimInstance)
	{
		ExtractRootMotion(Context, DeltaTimeRecord.Previous, DeltaTimeRecord.Delta);
	}
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	if (!Sequence)
	{
		return;
	}
	Sequence->GetAnimationPose(GetNormalizedTime() * Sequence->GetPlayLength(), Output.Pose, bUseRootMotion);
}

bool FAnimNode_SequencePlayer::IsFinished() const
{
	if (!Sequence)
	{
		return true;
	}
	if (bLoop)
	{
		return false;
	}

	// 일시정지나 SetPosition으로 끝에 놓인 경우도 맞게 판정하려고 결과값 대신 시간을 본다
	return IsReversePlay() ? GetNormalizedTime() <= 0.0f : GetNormalizedTime() >= 1.0f;
}

void FAnimNode_SequencePlayer::ExtractRootMotion(
    const FAnimationUpdateContext& Context, float PrevTime, float MovedDelta)
{
	const USkeletalMesh* Mesh = Context.AnimInstance->GetSkeletalMesh();
	if (!Sequence || !Mesh)
	{
		return;
	}

	const FRootMotionParams Delta = Sequence->ExtractRootMotion(*Mesh, PrevTime, MovedDelta, bLoop);
	Context.AnimInstance->AccumulateRootMotion(Delta, Context.BlendWeight);
}

const UAnimationAsset* FAnimNode_SequencePlayer::GetAnimationAsset() const
{
	return Sequence;
}

float FAnimNode_SequencePlayer::GetEffectivePlayLength() const
{
	if (Sequence == nullptr)
		return 0.0f;
	return Sequence->GetPlayLength();
}

const UAnimationAsset* FAnimNode_BlendSpacePlayer::GetAnimationAsset() const
{
	return BlendSpace;
}

void FAnimNode_BlendSpacePlayer::UpdateAssetPlayer(
    const FAnimationUpdateContext& Context)
{
	SampleWeights.Reset();

	if (BlendSpace == nullptr)
	{
		return;
	}

	if (!BlendSpace->GetSampleWeights(
	        FVector(X, 0.0f, 0.0f),
	        SampleWeights))
	{
		SampleWeights.Reset();
	}
}

void FAnimNode_BlendSpacePlayer::ExtractRootMotion(
    const FAnimationUpdateContext& Context, float PrevTime, float MovedDelta)
{
	const USkeletalMesh* Mesh = Context.AnimInstance->GetSkeletalMesh();
	if (!BlendSpace || !Mesh)
	{
		return;
	}

	// Evaluate와 같은 비율로 섞어야 포즈와 이동이 어긋나지 않으니 합이 1이 되게 맞춘다
	float WeightSum = 0.0f;
	for (const FBlendSampleWeight& Entry : SampleWeights)
	{
		WeightSum += Entry.Weight > 0.0f ? Entry.Weight : 0.0f;
	}
	if (WeightSum <= 0.0f)
	{
		return;
	}

	for (const FBlendSampleWeight& Entry : SampleWeights)
	{
		const FBlendSample* Sample = BlendSpace->GetSample(Entry.SampleIndex);
		if (!Sample || !Sample->Animation || Entry.Weight <= 0.0f)
		{
			continue;
		}

		// 샘플들은 같은 정규화 시간을 공유하니 구간도 그대로 쓴다
		const FRootMotionParams Delta = Sample->Animation->ExtractRootMotion(*Mesh, PrevTime, MovedDelta, bLoop);
		Context.AnimInstance->AccumulateRootMotion(Delta, Context.BlendWeight * (Entry.Weight / WeightSum));
	}
}

float FAnimNode_BlendSpacePlayer::GetEffectivePlayLength() const
{
	if (BlendSpace == nullptr)
		return 0.0f;

	return BlendSpace->GetEffectivePlayLength(SampleWeights);
}

void FAnimNode_BlendSpacePlayer::Evaluate(FPoseContext& Output)
{
	// 정상적인 AnimInstance 평가 경로에서는 대상 메시가 설정되어 있다.
	if (!Output.Pose.TargetMesh)
	{
		assert(false);
		return;
	}

	// 샘플 평가에 실패하면 기준 포즈와 빈 커브를 출력한다.
	Output.ResetToRefPose();

	if (!BlendSpace || SampleWeights.IsEmpty())
	{
		return;
	}

	// 현재 구현은 최대 두 샘플을 섞는 1D 전용이다.
	if (BlendSpace->GetDimension() != 1 || SampleWeights.Num() > 2)
	{
		assert(false);
		return;
	}

	const float NormalizedTime = GetNormalizedTime();

	// 선택된 샘플을 같은 정규화 진행률에서 평가한다.
	const auto EvaluateSample =
	    [this, NormalizedTime](
	        const FBlendSampleWeight& Entry,
	        FPoseContext& SamplePose) -> bool
	{
		if (!std::isfinite(Entry.Weight) || Entry.Weight <= 0.0f)
		{
			return false;
		}

		const FBlendSample* Sample =
		    BlendSpace->GetSample(Entry.SampleIndex);

		if (!Sample || !Sample->Animation)
		{
			return false;
		}

		UAnimSequence* Sequence = Sample->Animation;

		// GetPlayLength()가 DataModel을 직접 참조하므로 먼저 검사.
		if (!Sequence->GetDataModel())
		{
			return false;
		}

		const float Length = Sequence->GetPlayLength();

		if (!std::isfinite(Length) || Length <= 0.0f)
		{
			return false;
		}

		const float TimeSeconds = NormalizedTime * Length;

		// SamplePose는 Output과 같은 대상 메시를 가진다.
		// GetAnimationPose 내부에서 본 이름 매핑과 보간을 수행한다.
		Sequence->GetAnimationPose(TimeSeconds, SamplePose.Pose, bUseRootMotion);

		// 현재 시퀀스 API는 본 포즈만 평가한다.
		SamplePose.Curve.Reset();

		return true;
	};

	// FPoseContext 복사 생성자는 같은 평가 환경과 대상 메시를 가진 기준 포즈 버퍼를 만든다.
	FPoseContext PoseA(Output);

	if (!EvaluateSample(SampleWeights[0], PoseA))
	{
		return;
	}

	// 샘플 좌표와 일치하거나 입력이 양 끝에 있으면 하나의 샘플만 사용한다.
	if (SampleWeights.Num() == 1)
	{
		Output.Pose = PoseA.Pose;
		Output.Curve = PoseA.Curve;
		return;
	}

	FPoseContext PoseB(Output);

	if (!EvaluateSample(SampleWeights[1], PoseB))
	{
		return;
	}

	// 두 가중치는 앞에서 유효한 양수인지 검사한다.
	const double WeightA = SampleWeights[0].Weight;
	const double WeightB = SampleWeights[1].Weight;

	const float NormalizedWeightA =
	    static_cast<float>(WeightA / (WeightA + WeightB));

	// 이 함수는 Alpha(B의 비율)가 아닌 A의 가중치를 받는다.
	FAnimationRuntime::BlendTwoPosesTogether(
	    PoseA.Pose, PoseB.Pose, PoseA.Curve, PoseB.Curve,
	    NormalizedWeightA, Output.Pose, Output.Curve);
}
