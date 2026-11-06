#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/Nodes/AnimNodes.h"

class UTestAnimInstance : public UAnimInstance
{
	DECLARE_CLASS(UTestAnimInstance, UAnimInstance)

public:
	// A는 기존 포즈, B는 지정 본에 오프셋을 적용한 포즈.
	bool SetTestBoneTranslation(
	    int32 BoneIndex,
	    const FVector& TranslationOffset);

	void SetBlendAlpha(float InAlpha);

	// 테스트 포즈를 기준 포즈로 되돌린다.
	void ResetTestPose();

protected:
	void NativeInitializeAnimation() override;
	void NativeUnInitializeAnimation() override;

private:
	// 이 인스턴스가 노드를 직접 소유한다.
	FAnimNode_Constant ConstantA;
	FAnimNode_Constant ConstantB;
	FAnimNode_TwoWayBlend BlendNode;
};