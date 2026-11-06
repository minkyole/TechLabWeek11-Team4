#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/Nodes/AnimNodes.h"

class UTestAnimInstance : public UAnimInstance
{
	DECLARE_CLASS(UTestAnimInstance, UAnimInstance)

public:
	// 기준 포즈를 기준으로 지정 본에 위치 오프셋을 적용한다.
	// false: 초기화되지 않았거나 잘못된 본 인덱스
	bool SetTestBoneTranslation(
	    int32 BoneIndex,
	    const FVector& TranslationOffset);

	// 테스트 포즈를 기준 포즈로 되돌린다.
	void ResetTestPose();

protected:
	void NativeInitializeAnimation() override;
	void NativeUnInitializeAnimation() override;

private:
	// 이 인스턴스가 노드를 직접 소유한다.
	FAnimNode_Constant ConstantNode;
};