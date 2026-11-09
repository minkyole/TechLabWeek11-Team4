#pragma once

#include "Animation/AnimSequence.h"
#include "Render/Mesh.h"
#include "UObject/UObjectGlobals.h"

#include <algorithm>
#include <cmath>

// FBX 임포트(Phase 4) 전까지 재생 경로를 확인하기 위한 코드 생성 클립.
// 실제 에셋과 같은 UAnimSequence 경로를 타므로 SequencePlayer/SingleNode는 이 클립과 FBX 클립을 구분하지 않는다.
namespace AnimTestClips
{
	inline UAnimSequence* CreateSwingClip(
	    const USkeletalMesh* Mesh,
	    int32 BoneIndex,
	    const FVector& LocalAxis,
	    float SwingDegrees,
	    float DurationSeconds)
	{
		if (!Mesh || !Mesh->MeshData.Bones.IsValidIndex(BoneIndex) || DurationSeconds <= 0.0f)
			return nullptr;

		// Outer를 두지 않는다. 클립은 인스턴스보다 오래 사는 공유 에셋이어야 하고,
		// 해제 책임은 이 함수를 부른 쪽(DestroyClip)에 있다.
		UAnimSequence* Clip = NewObject<UAnimSequence>();
		Clip->SetSkeletalMesh(Mesh);

		constexpr int32 FPS = 30;
		const int32 NumFrames = std::max(1, static_cast<int32>(std::round(DurationSeconds * FPS)));

		UAnimDataModel* Model = Clip->GetDataModel();
		Model->SetFrameRate(FFrameRate(FPS, 1));
		Model->SetNumberOfFrames(NumFrames);

		const FSkeletalBone& Bone = Mesh->MeshData.Bones[BoneIndex];
		const int32 TrackIndex = Model->AddBoneTrack(FName(Bone.Name));

		// 델타를 오른쪽에 곱해 "본 자신의 로컬 축" 기준으로 돈다. 왼쪽에 곱하면 부모 축 기준이 된다.
		const FQuat RefRotation = FQuat(Bone.RefPoseTransform.Rotation).Normalized();
		const float SwingRadians = SwingDegrees * (3.14159265f / 180.0f);

		TArray<FQuat> RotationKeys;
		const int32 NumKeys = Model->GetNumberOfKeys();
		RotationKeys.Reserve(NumKeys);
		for (int32 Key = 0; Key < NumKeys; ++Key)
		{
			const float Phase = static_cast<float>(Key) / static_cast<float>(NumKeys - 1);
			const FQuat Delta = FQuat::MakeFromAxisAngle(LocalAxis, SwingRadians * Phase);
			RotationKeys.Add((RefRotation * Delta).Normalized());
		}

		// 위치·스케일 키를 비워 두면 샘플러가 RefPose 값을 쓴다.
		Model->SetBoneTrackKeys(TrackIndex, {}, RotationKeys, {});
		return Clip;
	}

	// UAnimSequence 소멸자가 DataModel을 해제하지 않으므로 함께 지운다.
	inline void DestroyClip(UAnimSequence*& Clip)
	{
		if (!Clip)
			return;

		delete Clip->GetDataModel();
		delete Clip;
		Clip = nullptr;
	}
}
