#include "EnginePCH.h"
#include "TestAnimInstance.h"

void UTestAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	const USkeletalMesh* Mesh = GetSkeletalMesh();

	if (!Mesh)
	{
		assert(false);
		return;
	}

	// Constant 노드가 출력할 포즈를 준비한다.
	ConstantA.ConstantPose.AnimInstance = this;
	ConstantA.ConstantPose.Pose.Initialize(*Mesh);
	ConstantA.ConstantPose.Curve.Reset();

	ConstantB.ConstantPose.AnimInstance = this;
	ConstantB.ConstantPose.Pose.Initialize(*Mesh);
	ConstantB.ConstantPose.Curve.Reset();

	BlendNode.A.LinkedNode = &ConstantA;
	BlendNode.B.LinkedNode = &ConstantB;
	BlendNode.Alpha = 0.0f;

	// AnimInstance의 평가 시작점을 연결한다.
	RootNode = &BlendNode;
}

void UTestAnimInstance::NativeUnInitializeAnimation()
{
	// 노드를 가리키는 참조를 먼저 해제한다.
	RootNode = nullptr;

	// 이전 메시와 인스턴스를 참조하는 데이터를 제거한다.
	BlendNode.A.LinkedNode = nullptr;
	BlendNode.B.LinkedNode = nullptr;

	ConstantA.ConstantPose.AnimInstance = nullptr;
	ConstantA.ConstantPose.Pose = FAnimationPose{};
	ConstantA.ConstantPose.Curve.Reset();

	ConstantB.ConstantPose.AnimInstance = nullptr;
	ConstantB.ConstantPose.Pose = FAnimationPose{};
	ConstantB.ConstantPose.Curve.Reset();

	Super::NativeUnInitializeAnimation();
}

bool UTestAnimInstance::SetTestBoneTranslation(
    int32 BoneIndex,
    const FVector& TranslationOffset)
{
	if (!IsInitialized())
		return false;

	FAnimationPose& PoseA = ConstantA.ConstantPose.Pose;
	FAnimationPose& PoseB = ConstantB.ConstantPose.Pose;

	if (!PoseA.TargetMesh ||
		!PoseA.LocalTransforms.IsValidIndex(BoneIndex) ||
		!PoseB.TargetMesh ||
		!PoseB.LocalTransforms.IsValidIndex(BoneIndex))
	{
		return false;
	}

	// 이전 테스트 결과에 누적하지 않고 기준 포즈에서 다시 시작한다.
	ConstantA.ConstantPose.ResetToRefPose();
	ConstantB.ConstantPose.ResetToRefPose();

	PoseB.LocalTransforms[BoneIndex].Translation +=
	    TranslationOffset;

	return true;
}

void UTestAnimInstance::SetBlendAlpha(float InAlpha)
{
	BlendNode.Alpha = FMath::Clamp01(InAlpha);
}

void UTestAnimInstance::ResetTestPose()
{
	if (!IsInitialized() ||
		!ConstantB.ConstantPose.Pose.TargetMesh)
	{
		return;
	}

	ConstantB.ConstantPose.ResetToRefPose();
}
