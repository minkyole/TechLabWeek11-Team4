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
	ConstantNode.ConstantPose.AnimInstance = this;
	ConstantNode.ConstantPose.Pose.Initialize(*Mesh);
	ConstantNode.ConstantPose.Curve.Reset();

	// AnimInstance의 평가 시작점을 연결한다.
	RootNode = &ConstantNode;
}

void UTestAnimInstance::NativeUnInitializeAnimation()
{
	// 노드를 가리키는 참조를 먼저 해제한다.
	RootNode = nullptr;

	// 이전 메시와 인스턴스를 참조하는 데이터를 제거한다.
	ConstantNode.ConstantPose.AnimInstance = nullptr;
	ConstantNode.ConstantPose.Pose = FAnimationPose{};
	ConstantNode.ConstantPose.Curve.Reset();

	Super::NativeUnInitializeAnimation();
}

bool UTestAnimInstance::SetTestBoneTranslation(
    int32 BoneIndex,
    const FVector& TranslationOffset)
{
	if (!IsInitialized())
		return false;

	FAnimationPose& Pose = ConstantNode.ConstantPose.Pose;

	if (!Pose.TargetMesh ||
		!Pose.LocalTransforms.IsValidIndex(BoneIndex))
	{
		return false;
	}

	// 이전 테스트 결과에 누적하지 않고 기준 포즈에서 다시 시작한다.
	ConstantNode.ConstantPose.ResetToRefPose();

	Pose.LocalTransforms[BoneIndex].Translation +=
	    TranslationOffset;

	return true;
}

void UTestAnimInstance::ResetTestPose()
{
	if (!IsInitialized() ||
		!ConstantNode.ConstantPose.Pose.TargetMesh)
	{
		return;
	}

	ConstantNode.ConstantPose.ResetToRefPose();
}
