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

	// SingleNode 모드가 아니면 nullptr.
	UAnimSingleNodeInstance* GetSingleNodeInstance() const;

	// ---- 재생 입구 ----
	// 모드나 클래스가 바뀌면 인스턴스를 버리고 설계도로 다시 만든다.
	void SetAnimationMode(EAnimationMode InMode);
	EAnimationMode GetAnimationMode() const { return AnimationMode; }
	void SetAnimInstanceClass(UClass* InClass);

	// ---- SingleNode 모드 전용 ----
	void SetAnimation(UAnimSequence* NewAnimToPlay);
	void Play(bool bLooping);
	void Stop();
	bool IsPlaying() const;

	// SingleNode 모드로 전환 → 클립 지정 → 처음부터 재생.
	void PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping);

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
