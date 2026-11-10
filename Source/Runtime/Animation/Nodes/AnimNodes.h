#pragma once

#include "Animation/AnimationRuntime.h"
#include "Animation/AnimationContext.h"
#include "Animation/Nodes/PoseLink.h"
#include "ANimation/BlendSpace.h"

struct FAnimationUpdateContext;
struct FPoseContext;
class UAnimSequence;
class UAnimationAsset;

// 모든 노드의 뿌리. 프레임마다 Update -> Evaluate 순서로 불린다.
struct FAnimNode_Base
{
	virtual ~FAnimNode_Base() = default;

	virtual void OnEnter() {}
	virtual void OnExit() {}

	virtual void Update(const FAnimationUpdateContext& Context) {}
	virtual void Evaluate(FPoseContext& Output) {}

	virtual bool IsFinished() const { return false; }
};

// 두 포즈를 Alpha 로 섞는다.
struct FAnimNode_TwoWayBlend : public FAnimNode_Base
{
	FPoseLink A; // 입력 포즈 (다른 노드에 연결)
	FPoseLink B;
	float Alpha = 0.0f; // 0 = A, 1 = B

	void OnEnter() override;
	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};

// 단일 포즈를 그대로 출력한다. 디버깅용.
struct FAnimNode_Constant : public FAnimNode_Base
{
	FPoseContext ConstantPose;

	void Update(const FAnimationUpdateContext& Context) override;
	void Evaluate(FPoseContext& Output) override;
};

struct FAnimNode_AssetPlayerBase : public FAnimNode_Base
{
	float PlayRate = 1.0f;

	bool bPlaying = true;
	bool bLoop = true;

	// 파생 클래스가 재생하는 에셋을 반환한다.
	virtual const UAnimationAsset* GetAnimationAsset() const = 0;

	// 이번 프레임에 사용할 재생 길이.
	// Sequence: 시퀀스 길이
	// BlendSpace: 현재 샘플 가중치를 반영한 길이
	virtual float GetEffectivePlayLength() const = 0;

	void OnEnter() override;

	void Update(const FAnimationUpdateContext& Context) override;

	// 포즈 생성은 각 재생 노드에서 구현한다.
	void Evaluate(FPoseContext& Output) override = 0;

	float GetNormalizedTime() const { return NormalizedTime; }

	void SetNormalizedTime(float InTime);

	void ResetPlayback();

	// Notify·RootMotion이 이번 프레임에 지나간 구간을 읽는다.
	const FDeltaTimeRecord& GetDeltaTimeRecord() const { return DeltaTimeRecord; }
	ETypeAdvanceAnim GetLastAdvanceResult() const { return LastAdvanceResult; }

protected:
	// 시간 진행 전에 필요한 데이터를 갱신한다.
	// BlnedSpace는 여기서 입력에 따른 가중치를 계산한다.
	virtual void UpdateAssetPlayer(
	    const FAnimationUpdateContext& Context) {}

private:
	float NormalizedTime = 0.0f;
	FDeltaTimeRecord DeltaTimeRecord;
	ETypeAdvanceAnim LastAdvanceResult = ETypeAdvanceAnim::Default;
};

// 클립 하나를 재생한다.
struct FAnimNode_SequencePlayer : public FAnimNode_AssetPlayerBase
{
	UAnimSequence* Sequence = nullptr;

	const UAnimationAsset* GetAnimationAsset() const override;
	float GetEffectivePlayLength() const override;

	void Evaluate(FPoseContext& Output) override;

	bool IsFinished() const override;

};

struct FAnimNode_BlendSpacePlayer : public FAnimNode_AssetPlayerBase
{
	UBlendSpace* BlendSpace = nullptr;
	float X = 0.0f;

	const UAnimationAsset* GetAnimationAsset() const override;
	float GetEffectivePlayLength() const override;

	void Evaluate(FPoseContext& Output) override;

protected:
	void UpdateAssetPlayer(
	    const FAnimationUpdateContext& Context) override;

private:
	TArray<FBlendSampleWeight> SampleWeights;
};