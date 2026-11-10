#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Animation/Nodes/AnimNodes.h"

struct FAnimNode_Base;
class USkeletalMeshComponent;
class UAnimSequence;
struct FPoseContext;

class UAnimInstance : public UObject
{
	DECLARE_CLASS(UAnimInstance, UObject)
public:
	void InitializeAnimation(USkeletalMeshComponent* InComponent);
	void UnInitializeAnimation();

	void UpdateAnimation(float DeltaSeconds);
	void EvaluateAnimation(FPoseContext& Output);

	USkeletalMeshComponent* GetSkeletalMeshComponent() const;
	USkeletalMesh* GetSkeletalMesh() const;

	bool IsInitialized() const;

protected:
	virtual void NativeInitializeAnimation() {};               // 그래프를 만든다 (한 번)
	virtual void NativeUnInitializeAnimation() {};             // 그래프를 정리한다 (한 번)
	virtual void NativeUpdateAnimation(float DeltaSeconds) {}; // 변수를 갱신한다 (매 프레임)


	// 파생 클래스가 소유한 노드를 참조한다. 여기서 delete하지 않는다.
	FAnimNode_Base* RootNode = nullptr; // 그래프의 시작 노드 (StateMachine 등)

private:
	// 비소유 참조. 컴포넌트가 인스턴스의 수명을 관리한다.
	USkeletalMeshComponent* OwningComponent = nullptr;
	bool bIsInitialized = false;
};

class UAnimSingleNodeInstance : public UAnimInstance
{
	DECLARE_CLASS(UAnimSingleNodeInstance, UAnimInstance)
public:
	void SetAnimationAsset(UAnimSequence* InSequence);
	UAnimSequence* GetAnimationAsset() const;

	void SetPlaying(bool bInPlaying);
	bool IsPlaying() const;
	void SetLooping(bool bInLooping);
	bool IsLooping() const;

	void SetPlayRate(float InPlayRate);
	float GetPlayRate() const;

	void SetPosition(float InPosition);
	float GetPosition() const;

	float GetLength() const;

protected:
	void NativeInitializeAnimation() override;

private:
	FAnimNode_SequencePlayer SequencePlayer;
};