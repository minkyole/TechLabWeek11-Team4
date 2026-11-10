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

	void GetAnimationPose(float Time, FAnimationPose& OutPose) const;

	void BuildFromTracks(const FFrameRate& FrameRate, int32 NumFrames, const TArray<FBoneAnimationTrack>& Tracks);

private:
	FAnimKeyLookup FindKeyLookup(float Time) const;

	static void SampleTrack(const FRawAnimSequenceTrack& Track, const FAnimKeyLookup& Lookup, FAnimBoneTransform& OutTransform);

	void RebuildBoneTrackMap(const USkeletalMesh& Mesh) const;

	mutable const USkeletalMesh* CachedMesh = nullptr;
	mutable TArray<int32> BoneToTrackIndex;
};