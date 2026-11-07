#include "EnginePCH.h"
#include "AnimNodes.h"

#include "Animation/AnimationContext.h"


void FAnimNode_SequencePlayer::Update(const FAnimationUpdateContext& Context)
{
	(void)Context; // TEMP(C): crash guard until B implements. Do not commit.
}

void FAnimNode_SequencePlayer::Evaluate(FPoseContext& Output)
{
	Output.Pose = FAnimationPose(); // TEMP(C): empty pose so ApplyPose is skipped and Bone Editor edits stay. Do not commit.
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