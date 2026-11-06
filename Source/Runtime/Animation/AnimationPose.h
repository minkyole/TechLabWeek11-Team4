#pragma once

#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Render/Mesh.h"

struct FAnimBoneTransform
{
	FVector Translation = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity();
	FVector Scale = FVector::OneVector;
};

struct FAnimationPose
{
	// 비소유 참조. 평가하는 동안 유효해야 한다.
	const USkeletalMesh* TargetMesh = nullptr;

	// 부모 본 기준 로컬 변환.
	// 인덱스는 TargetMesh->MeshData.Bones와 동일하다.
	TArray<FAnimBoneTransform> LocalTransforms;

	void Initialize(const USkeletalMesh& InMesh);
	void ResetToRefPose();

	bool IsCompatibleWith(const FAnimationPose& Other) const;
};