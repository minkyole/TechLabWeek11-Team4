#pragma once
#include "AnimationAsset.h"
#include "AnimDataModel.h"
#include "AnimationPose.h" 

class UAnimSequenceBase : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)

public: 
	UAnimSequenceBase();
	~UAnimSequenceBase() override = default;

	UAnimDataModel* GetDataModel() const { return DataModel; }
	float GetPlayLength() const override { return DataModel->GetPlayLength(); }

protected:
	UAnimDataModel* DataModel = nullptr;
};

class UAnimSequence : public UAnimSequenceBase
{
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

public:
	UAnimSequence() = default;
	~UAnimSequence() override = default;

private:
	FAnimKeyLookup FindKeyLookup(float Time) const;

	static void SampleTrack(const FRawAnimSequenceTrack& Track, const FAnimKeyLookup& Lookup, FAnimBoneTransform& OutTransform);
};