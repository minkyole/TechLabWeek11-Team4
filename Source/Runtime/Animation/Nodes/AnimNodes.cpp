#include "EnginePCH.h"
#include "AnimNodes.h"

#include "Animation/AnimationContext.h"
#include "Animation/AnimSequence.h"


void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
	if (!Sequence || !bPlaying)
	{
		return;
	}

	float Length = Sequence->GetPlayLength();
	if (Length <= 0)
	{
		CurrentTime = 0;
		return;
	}

	CurrentTime += Context.DeltaSeconds * PlayRate;
	if (bLoop)
	{
		if (CurrentTime >= Length)
		{
			CurrentTime = std::fmod(CurrentTime, Length);
		}
	}
	else
	{
		CurrentTime = FMath::Clamp(CurrentTime, 0.f, Length);
	}
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	if (!Sequence)
	{
		return;
	}
	Sequence->GetAnimationPose(CurrentTime, Output.Pose);
}

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