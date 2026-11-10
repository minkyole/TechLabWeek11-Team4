#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Render/RenderPacket.h"

#include "Animation/AnimInstance.h"
#include "Core/Stats/SkinningStats.h"
#include "UObject/UObjectGlobals.h"
#include "Animation/AnimSequence.h"

// Debug
#include "Animation/TestAnimInstance.h"
#include "Animation/MyAnimInstance.h"
#include "Animation/AnimTestClips.h"
#include "Asset/AssetManager.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	AnimationMode = EAnimationMode::AnimationBlueprint;
	AnimClass = UMyAnimInstance::StaticClass();
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
	// 인스턴스의 SequencePlayer가 클립을 가리키고 있으므로 인스턴스를 먼저 지운다.
	ClearAnimInstance();
	AnimTestClips::DestroyClip(DebugTestClip);
}

void USkeletalMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	// ---- Debug [3-4]: SingleNode 재생 경로 확인 ----
	if (bDebugPlayTestClip && GetSkeletalMesh() && !DebugTestClip)
	{
		// 본 11을 1초 동안 로컬 Z축으로 90도. 루프면 1초마다 되돌아가고, 비루프면 90도에서 멈춘다.
		DebugTestClip = AnimTestClips::CreateSwingClip(GetSkeletalMesh(), 11, FVector(0.0f, 0.0f, 1.0f), 90.0f, 1.0f);

		if (DebugTestClip)
		{
			PlayAnimation(DebugTestClip, bDebugTestClipLoop);
			return;
		}
	}
	// ---- Debug [3-4] 끝 ----

	// ---- Debug [4-4]: FBX 클립 재생 확인 ----
	if (bDebugPlayFbxClip && GetSkeletalMesh())
	{
		// 메시의 경로가 곧 FBX 에셋 세트의 키라서 같은 파일의 클립을 바로 찾을 수 있다.
		const FFbxAssetSet* Assets = UAssetManager::Get().FindFbxAssets(GetSkeletalMesh()->GetPath());
		if (Assets && Assets->Animations.IsValidIndex(DebugFbxClipIndex))
		{
			PlayAnimation(Assets->Animations[DebugFbxClipIndex], bDebugTestClipLoop);
			return;
		}
	}
	// ---- Debug [4-4] 끝 ----

	UMyAnimInstance* MyInstance =
	    Cast<UMyAnimInstance>(GetAnimInstance());

	if (!MyInstance || !MyInstance->IsInitialized())
		return;

	MyInstance->Speed = DebugSpeed;
	MyInstance->bIsAttacking = bDebugAttack;
	MyInstance->bIsHit = bDebugHit;

	// Evaluate 전에 현재 속도의 가중치를 준비한다.
	MyInstance->UpdateAnimation(0.0f);

	// BeginPlay 안에서 스키닝 결과까지 바로 반영.
	if (RefreshAnimationPose())
	{
		FlushPendingPose();
	}
}

void USkeletalMeshComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void USkeletalMeshComponent::InitializeComponent()
{
	Super::InitializeComponent();

	InitializeAnimInstance();
}

void USkeletalMeshComponent::InitializeAnimInstance()
{
	// 컴포넌트 초기화 시점에 메시가 없을 수도 있다.
	if (!GetSkeletalMesh())
		return;

	if (!AnimInstance)
	{
		if (AnimationMode == EAnimationMode::AnimationSingleNode)
		{
			AnimInstance.reset(NewObject<UAnimSingleNodeInstance>(this));
		}
		else
		{
			UClass* ClassToSpawn =
			    (AnimClass && AnimClass->IsChildOf(UAnimInstance::StaticClass()))
			        ? AnimClass
			        : UAnimInstance::StaticClass();

			AnimInstance.reset(NewObject<UAnimInstance>(this, ClassToSpawn));
		}
	}

	if (!AnimInstance->IsInitialized())
	{
		AnimInstance->InitializeAnimation(this);

		// InitializeAnimation이 CurrentTime을 0으로 되돌리므로 그 뒤에 요청을 덮어쓴다.
		// 이미 초기화된 인스턴스에는 적용하지 않는다. 진행 중인 재생 시간을 저장값으로 되감게 된다.
		if (UAnimSingleNodeInstance* SingleNode =
		        Cast<UAnimSingleNodeInstance>(AnimInstance.get()))
		{
			AnimationData.Initialize(SingleNode);
		}

		// Tick이 돌지 않는 에디터 월드·뷰어에서도 첫 화면이 RefPose가 아니라 요청한 포즈가 되게 한다.
		if (RefreshAnimationPose())
		{
			FlushPendingPose();
		}
	}
}

void USkeletalMeshComponent::ClearAnimInstance()
{
	if (AnimInstance)
	{
		// 파생 클래스의 종료 훅을 먼저 실행한다.
		AnimInstance->UnInitializeAnimation();

		// 그 다음 객체를 삭제한다.
		AnimInstance.reset();
	}

	// 이전 인스턴스, 메시를 가리키는 평가 결과도 제거한다.
	EvaluatedPose = FPoseContext();
}

void USkeletalMeshComponent::SetSkeletalMesh(
    USkeletalMesh* InMesh)
{
	if (GetSkeletalMesh() == InMesh)
	{
		// 같은 메시라도 인스턴스가 없다면 준비한다.
		InitializeAnimInstance();
		return;
	}

	// 이전 메시가 유효한 동안 종료 훅을 실행한다.
	ClearAnimInstance();

	// 기존 부모 구현: 메시 교체, 기준 포즈,스키닝 갱신.
	Super::SetSkeletalMesh(InMesh);

	// nullptr이면 InitializeAnimInstance 내부에서 종료된다.
	InitializeAnimInstance();
}

UAnimSingleNodeInstance* USkeletalMeshComponent::GetSingleNodeInstance() const
{
	return Cast<UAnimSingleNodeInstance>(AnimInstance.get());
}

void USkeletalMeshComponent::SetAnimationMode(EAnimationMode InMode)
{
	if (AnimationMode == InMode)
		return;

	AnimationMode = InMode;

	// 인스턴스 타입이 모드에 묶여 있으므로 기존 실행체를 버린다.
	// AnimationData는 컴포넌트에 남아 있어 SingleNode로 돌아오면 그대로 복원된다.
	ClearAnimInstance();
	InitializeAnimInstance();
}

void USkeletalMeshComponent::SetAnimInstanceClass(UClass* InClass)
{
	if (AnimClass == InClass)
		return;

	AnimClass = InClass;

	// SingleNode 모드 중에는 클래스만 기억해 두고, Blueprint 모드로 바뀔 때 쓴다.
	if (AnimationMode == EAnimationMode::AnimationBlueprint)
	{
		ClearAnimInstance();
		InitializeAnimInstance();
	}
}

void USkeletalMeshComponent::SetAnimation(UAnimSequence* NewAnimToPlay)
{
	AnimationData.AnimToPlay = NewAnimToPlay;
	AnimationData.SavedPosition = 0.0f;

	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		SingleNode->SetAnimationAsset(NewAnimToPlay);

		// SetAnimationAsset은 같은 클립이면 아무것도 안 하므로,
		// "다시 지정 = 처음부터"를 보장하려고 위치를 명시적으로 되돌린다.
		SingleNode->SetPosition(0.0f);
	}
}

void USkeletalMeshComponent::Play(bool bLooping)
{
	AnimationData.bSavedLooping = bLooping;
	AnimationData.bSavedPlaying = true;

	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		SingleNode->SetLooping(bLooping);
		SingleNode->SetPlaying(true);
	}
}

void USkeletalMeshComponent::Stop()
{
	AnimationData.bSavedPlaying = false;

	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		SingleNode->SetPlaying(false);
	}
}

bool USkeletalMeshComponent::IsPlaying() const
{
	// 인스턴스가 아직 없으면 "재생 요청이 걸려 있는가"를 답한다.
	if (UAnimSingleNodeInstance* SingleNode = GetSingleNodeInstance())
	{
		return SingleNode->IsPlaying();
	}
	return AnimationData.AnimToPlay != nullptr && AnimationData.bSavedPlaying;
}

void USkeletalMeshComponent::PlayAnimation(UAnimSequence* NewAnimToPlay, bool bLooping)
{
	// 모드 전환이 인스턴스를 재생성할 수 있으므로 먼저 실행체를 확정하고, 그 실행체에 요청을 건다.
	SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SetAnimation(NewAnimToPlay);
	Play(bLooping);
}

void USkeletalMeshComponent::TickPose(float DeltaTime)
{
	if (!AnimInstance || !AnimInstance->IsInitialized())
		return;

	SCOPE_CYCLE_COUNTER(SkinningStats::STAT_AnimEvaluate);

	if (UMyAnimInstance* MyInstance =
	        Cast<UMyAnimInstance>(AnimInstance.get()))
	{
		MyInstance->Speed = DebugSpeed;
		if (bDebugAttack)
		{
			MyInstance->bIsAttacking = true;
			bDebugAttack = false; // 한 번만 반영
		}
		if (bDebugHit)
		{
			MyInstance->bIsHit = true;
			bDebugHit = false; // 한 번만 반영
		}
	}

	AnimInstance->UpdateAnimation(DeltaTime);
	RefreshAnimationPose();
}

bool USkeletalMeshComponent::RefreshAnimationPose()
{
	if (!GetSkeletalMesh() ||
	    !AnimInstance ||
	    !AnimInstance->IsInitialized())
	{
		return false;
	}

	AnimInstance->EvaluateAnimation(EvaluatedPose);

	return ApplyPose(EvaluatedPose.Pose);
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	USkeletalMesh* SkeletalMesh = GetSkeletalMesh();
	const FPrimitiveSceneProxy* Proxy = GetSceneProxy();
	if (!SkeletalMesh || !Proxy || !Proxy->IsVisible()) return;
	SyncSkinningMode();
	// GPU 모드면 바인드 포즈 정점 버퍼와 본 행렬 버퍼를, CPU 모드면 CPU가 스키닝한 정점 버퍼를 넘긴다.
	const bool bGPUSkinned = UsesGPUSkinning();
	FVertexBuffer* VertexBuffer = bGPUSkinned ? SkeletalMesh->VertexBuffer.get() : GetSkinnedVertexBuffer();
	FIndexBuffer* IndexBuffer = SkeletalMesh->IndexBuffer.get();
	if (!VertexBuffer || !IndexBuffer || !VertexBuffer->GetBuffer() || !IndexBuffer->GetBuffer()) return;

	// 합친 버퍼 기준으로 보정된 Section마다 기존 Queue에 패킷을 하나씩 추가한다.
	const uint32 TotalIndices = IndexBuffer->GetIndexCount();
	for (const FSkeletalMeshSection& Section : SkeletalMesh->Sections)
	{
		if (Section.IndexCount == 0 || Section.StartIndex > TotalIndices || Section.IndexCount > TotalIndices - Section.StartIndex) continue;
		if (Section.MaterialSlotIndex >= static_cast<uint32>(GetNumMaterials())) continue;
		UMaterial* Material = GetMaterial(static_cast<int32>(Section.MaterialSlotIndex));
		if (!Material) continue;

		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.VertexBuffer = VertexBuffer;
		Packet.IndexBuffer = IndexBuffer;
		if (bGPUSkinned)
		{
			Packet.SkinMatrices = GetSkinMatrixBuffer();
			Packet.NormalMatrices = GetNormalMatrixBuffer();
		}
		Packet.Material = Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
	}
}
