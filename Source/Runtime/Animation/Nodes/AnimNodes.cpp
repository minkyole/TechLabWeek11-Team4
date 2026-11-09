#include "EnginePCH.h"
#include "AnimNodes.h"

#include "Animation/AnimationContext.h"
#include "Animation/AnimSequence.h"

void FAnimNode_TwoWayBlend::Update(const FAnimationUpdateContext& Context)
{
	A.Update(Context); // 각 입력이 자기 재생 시간을 흘린다
	B.Update(Context);
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
}

void FAnimNode_AssetPlayerBase::Update(
    const FAnimationUpdateContext& Context)
{
	// 정지 중에도 입력값 변경은 포즈에 반영되어야 한다.
	// 따라서 bPlyaing 검사보다 먼저 호출한다.
	UpdateAssetPlayer(Context);

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

	const double DeltaNormalizedtime =
	    static_cast<double>(Context.DeltaSeconds) * PlayRate / Length;

	double NewTime =
	    static_cast<double>(NormalizedTime) + DeltaNormalizedtime;

	if (bLoop)
	{
		// 정방향과 역방향 재생 모두 [0, 1)로 순환한다.
		NewTime -= std::floor(NewTime);
	}
	else
	{
		// 끝에 도달하면 마지막 포즈를 유지한다.
		NewTime = std::clamp(NewTime, 0.0, 1.0);
	}

	NormalizedTime = static_cast<float>(NewTime);
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	if (!Sequence)
	{
		return;
	}
	Sequence->GetAnimationPose(GetNormalizedTime() * Sequence->GetPlayLength(), Output.Pose);
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
		Sequence->GetAnimationPose(TimeSeconds, SamplePose.Pose);

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
