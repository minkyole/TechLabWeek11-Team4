#pragma once
#include "Animation/AnimationScriptContext.h"
#include "Animation/StateMachineAnimInstance.h"
class ULuaAnimInstance : public UStateMachineAnimInstance
{
	DECLARE_CLASS(ULuaAnimInstance, UStateMachineAnimInstance)
  public:
	~ULuaAnimInstance() override;

  protected:
	void NativeInitializeAnimation() override;
	void NativeUnInitializeAnimation() override;
	void NativeUpdateAnimation(float DeltaSeconds) override;

  private:
	bool LoadAnimationScript(const FString &Path);
	void ReleaseAnimationScript();
	UAnimSequence *CreateRootTestClip(int32 RootBoneIndex, float DurationSeconds, const FVector &Offset);
	FAnimationScriptContext Script;
	FString AnimationScriptPath = "Assets/Scripts/Animation/CharacterAnim.lua";
};
