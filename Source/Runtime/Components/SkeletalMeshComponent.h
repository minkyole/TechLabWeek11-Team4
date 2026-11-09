#pragma once

#include "EnginePCH.h"
#include "SkinnedMeshComponent.h"
#include "Engine/SkeletonSceneProxy.h"
#include "Render/Mesh.h"
#include "Animation/AnimationContext.h"
#include "Animation/SingleAnimationPlayData.h"

class UAnimInstance;
class UClass;

enum class EAnimationMode : uint8
{
	AnimationBlueprint,  // AnimClass로 지정한 그래프 인스턴스 (StateMachine 등)
	AnimationSingleNode, // UAnimSingleNodeInstance로 클립 하나만 재생
};

class USkeletalMeshComponent : public USkinnedMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	REFLECT_START(USkeletalMeshComponent)
		PROPERTY(DebugSpeed)
	REFLECT_END()

public:
	USkeletalMeshComponent();
	~USkeletalMeshComponent() override;

	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;

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

	EAnimationMode AnimationMode = EAnimationMode::AnimationSingleNode;

	// AnimationBlueprint 모드에서 생성할 클래스. 
	UClass* AnimClass = nullptr;

	// AnimationSingleNode 모드의 재생 요청.
	FSingleAnimationPlayData AnimationData;

	// 매 프레임 포즈 배열을 새로 만들지 않고 재사용.
	FPoseContext EvaluatedPose;

	float DebugSpeed = 0.0f;
};
