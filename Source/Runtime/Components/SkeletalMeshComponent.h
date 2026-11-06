#pragma once

#include "EnginePCH.h"
#include "SkinnedMeshComponent.h"
#include "Engine/SkeletonSceneProxy.h"
#include "Render/Mesh.h"
#include "Animation/AnimationContext.h"

class UAnimInstance;

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

public:
	USkeletalMeshComponent();
	~USkeletalMeshComponent() override;

	void BeginPlay() override;

	void InitializeComponent() override;
	//void TickComponent(float DeltaTime) override;

	void SetSkeletalMesh(USkeletalMesh* InMesh) override;

	// 메시가 준비되면 생성, 초기화. 이미 초기화됐다면 유지.
	void InitializeAnimInstance();

	// 종료 처리 후 인스턴스 삭제.
	void ClearAnimInstance();

	// 시간 진행 없이 포즈만 평가, 적용. 뷰어 시크에 사용.
	bool RefreshAnimationPose();

	// 비소유 포인터. 호출자는 delete하지 않는다.
	UAnimInstance* GetAnimInstance() const { return AnimInstance.get(); }

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void SubmitToRenderQueue(FRenderQueue& RenderQueue) override;

protected:
	// 시간 진행 + 포즈 받기 평가, 적용.
	void TickPose(float DeltaTime) override final;

private:
	TUniquePtr<UAnimInstance> AnimInstance;

	// 매 프레임 포즈 배열을 새로 만들지 않고 재사용.
	FPoseContext EvaluatedPose;
};
