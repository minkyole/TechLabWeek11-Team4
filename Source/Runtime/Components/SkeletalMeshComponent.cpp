#include "EnginePCH.h"
#include "SkeletalMeshComponent.h"
#include "Render/RenderPacket.h"

#include "Animation/AnimInstance.h"
#include "UObject/UObjectGlobals.h"
#include "Animation/AnimSequence.h"

USkeletalMeshComponent::USkeletalMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USkeletalMeshComponent::~USkeletalMeshComponent()
{
	ClearAnimInstance();
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
		AnimInstance.reset(
		    NewObject<UAnimSingleNodeInstance>(this));
	}

	if (AnimInstance && !AnimInstance->IsInitialized())
	{
		AnimInstance->InitializeAnimation(this);
	}

	// ---- [TEMP 2-5] 샘플링 확인용. 확인 후 삭제 ----
	{
		UAnimSingleNodeInstance* SingleNode = Cast<UAnimSingleNodeInstance>(AnimInstance.get());
		const TArray<FSkeletalBone>& Bones = GetSkeletalMesh()->MeshData.Bones;
		if (SingleNode && Bones.Num() > 1)
		{
			UAnimSequence* TestClip = NewObject<UAnimSequence>();
			TestClip->SetSkeletalMesh(GetSkeletalMesh());

			UAnimDataModel* Model = TestClip->GetDataModel();
			Model->SetFrameRate(FFrameRate(30, 1));
			Model->SetNumberOfFrames(30); // 키 31개, 1초

			const int32 TestBoneIndex = 1;
			const int32 TrackIndex = Model->AddBoneTrack(FName(Bones[TestBoneIndex].Name));

			const FQuat RefRot = FQuat(Bones[TestBoneIndex].RefPoseTransform.Rotation).Normalized();
			const FQuat EndRot = (RefRot * FQuat::MakeFromAxisAngle(FVector(0.0f, 0.0f, 1.0f), 3.14159265f * 0.5f)).Normalized();

			TArray<FQuat> RotKeys;
			const int32 NumKeys = Model->GetNumberOfKeys();
			for (int32 Key = 0; Key < NumKeys; ++Key)
			{
				const float T = static_cast<float>(Key) / static_cast<float>(NumKeys - 1);
				RotKeys.Add(FMath::SlerpShortestPath(RefRot, EndRot, T));
			}
			Model->SetBoneTrackKeys(TrackIndex, {}, RotKeys, {});

			SingleNode->SetAnimationAsset(TestClip);
			SingleNode->SetPosition(0.5f); // 기대: 45°
			RefreshAnimationPose();
			FlushPendingPose();
		}
	}
	// ---- [TEMP 2-5] 끝 ----
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

void USkeletalMeshComponent::TickPose(float DeltaTime)
{
	if (!AnimInstance || !AnimInstance->IsInitialized()) 
		return;

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
	FVertexBuffer* VertexBuffer = GetSkinnedVertexBuffer();
	FIndexBuffer* IndexBuffer = SkeletalMesh->IndexBuffer.get();
	if (!VertexBuffer || !IndexBuffer || !VertexBuffer->GetBuffer() || !IndexBuffer->GetBuffer()) return;

	// 합친 버퍼 기준으로 보정된 Section마다 기존 Queue에 패킷을 하나씩 추가한다.
	const uint32 TotalIndices = IndexBuffer->GetIndexCount();
	for (const FSkeletalMeshSection& Section : SkeletalMesh->Sections)
	{
		if (Section.IndexCount == 0 || Section.StartIndex > TotalIndices
			|| Section.IndexCount > TotalIndices - Section.StartIndex) continue;
		if (Section.MaterialSlotIndex >= static_cast<uint32>(GetNumMaterials())) continue;
		UMaterial* Material = GetMaterial(static_cast<int32>(Section.MaterialSlotIndex));
		if (!Material) continue;

		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.VertexBuffer = VertexBuffer;
		Packet.IndexBuffer = IndexBuffer;
		Packet.Material = Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
	}
}

