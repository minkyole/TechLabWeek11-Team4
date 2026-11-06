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
	throw std::logic_error("Not Implemented");
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