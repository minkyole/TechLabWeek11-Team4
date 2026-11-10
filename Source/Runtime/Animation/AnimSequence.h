#pragma once
#include "AnimationAsset.h"
#include "AnimDataModel.h"
#include "AnimationPose.h" 
#include "AnimNotifyEvent.h"

class UAnimSequenceBase : public UAnimationAsset
{
	DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)

public: 
	UAnimSequenceBase();
	~UAnimSequenceBase() override;

	void RemoveNotifyAt(int32 Index);
	void ClearNotifies();
	void SetNotifyObject(int32 Index, UAnimNotify* NewNotify);
	void OnNotifyObjectDestroyed(const UAnimNotify* DestroyedNotify);

	UAnimDataModel* GetDataModel() const { return DataModel; }
	float GetPlayLength() const override { return DataModel->GetPlayLength(); }


	TArray<FAnimNotifyEvent> Notifies;
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