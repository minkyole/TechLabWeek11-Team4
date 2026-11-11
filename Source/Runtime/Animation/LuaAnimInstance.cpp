#include "EnginePCH.h"
#include "LuaAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/CommonAnimClips.h"
#include "Asset/AssetManager.h"

#include "Engine/Engine.h"
#include "Scripting/LuaVM.h"

#include <cmath>
#include <utility>

ULuaAnimInstance::~ULuaAnimInstance()
{
	ReleaseAnimationScript();
}

void ULuaAnimInstance::NativeInitializeAnimation()
{
	ResetGraph();
    AnimationScriptPath = GetSkeletalMeshComponent()->GetAnimationScriptPath();
	LastError.clear();

	// Lua 바인딩 추가 후 연결할 위치:
	//
	// BeginBuild();
	// Lua InitializeAnimation(this);
	// Commit();
	//
	// 현재 단계에서는 호스트 C++ 코드가 BeginBuild/Commit을 호출한다.


	// Debug
	{
		const USkeletalMesh* Mesh = GetSkeletalMesh();
		if (!Mesh)
			return;

		const int32 RootBoneIndex = 1;

		UAnimSequence* IdleClip = CreateRootTestClip(RootBoneIndex, 1.0f, FVector(0, 0, 0));
		UAnimSequence* WalkClip = CreateRootTestClip(RootBoneIndex, 1.0f, FVector(0, 0, 0.3f));
		UAnimSequence* RunClip = CreateRootTestClip(RootBoneIndex, 0.5f, FVector(0, 0, 0.4f));
		UAnimSequence* JumpClip = CreateRootTestClip(RootBoneIndex, 0.1f, FVector(0, 0, 0.1f));
		UAnimSequence* AttackClip = CreateRootTestClip(RootBoneIndex, 0.6f, FVector(3.0f, 0, 0));
		UAnimSequence* HitClip = CreateRootTestClip(RootBoneIndex, 0.4f, FVector(-1.5f, 0, 0));
		UBlendSpace1D* TestBlendSpace = NewObject<UBlendSpace1D>(this);
		TestBlendSpace->SetSkeletalMesh(Mesh);
		FBlendParameter SpeedAxis;
		SpeedAxis.Name = FName("Speed");
		SpeedAxis.Min = 0.0f;
		SpeedAxis.Max = 10.0f;
		if (!IdleClip || !WalkClip || !RunClip || !AttackClip || !HitClip)
		{
			Fail("Failed to create test animation clips.");
			// 이미 생성된 테스트 에셋 정리도 필요
			return;
		}

		// 실제 클립이 같은 메시로 로드됐을 때만 별칭을 건다. ValidateAsset이 다른 메시 클립을 거부하기 때문이다.
		for (const FCommonAnimClip& Entry : CommonAnimClips::Clips)
		{
			const UAnimSequence* FbxClip = UAssetManager::GetAssetByPath<UAnimSequence>(Entry.AssetKey);
			// 다시 초기화될 때 같은 별칭을 두 번 넣으면 Fail이 나서 아직 별칭이 없을 때만 넣는다.
			if (FbxClip && FbxClip->GetSkeletalMesh() == Mesh && ResolveAssetKey(Entry.Alias) == Entry.Alias)
				RegisterAssetAlias(Entry.Alias, Entry.AssetKey);
		}

		// 블렌드 스페이스는 이름이 아닌 포인터로 샘플을 받아서 별칭을 직접 풀어 준다.
		const auto PickClip = [&](const char* Alias, UAnimSequence* Fallback)
		{
			const FString Resolved = ResolveAssetKey(Alias);
			UAnimSequence* FbxClip = Resolved != Alias ? UAssetManager::GetAssetByPath<UAnimSequence>(Resolved) : nullptr;
			return FbxClip ? FbxClip : Fallback;
		};

		const auto AddSample = [&](UAnimSequence* Clip, float Speed)
		{
			FBlendSample Sample;
			Sample.Animation = Clip;
			Sample.SampleValue = FVector(Speed, 0.0f, 0.0f);

			return TestBlendSpace->AddSample(Sample);
		};

		if (!TestBlendSpace->SetBlendParameter(0, SpeedAxis) ||
		    !AddSample(PickClip("Idle", IdleClip), 0.0f) ||
		    !AddSample(PickClip("Walk", WalkClip), 5.0f) ||
		    !AddSample(PickClip("Run", RunClip), 10.0f))
		{
			Fail("Failed to configure test BlendSpace.");
			// 생성된 테스트 에셋 정리도 필요
			return;
		}

		UAssetManager& Assets = UAssetManager::Get();

		Assets.RegisterAsset("Idle", IdleClip);
		Assets.RegisterAsset("Walk", WalkClip);
		Assets.RegisterAsset("Run", RunClip);
		Assets.RegisterAsset("Jump", JumpClip);
		Assets.RegisterAsset("Attack", AttackClip);
		Assets.RegisterAsset("Hit", HitClip);
		Assets.RegisterAsset("Locomotion", TestBlendSpace);

		if (!BeginBuild())
			return;

		if (!LoadAnimationScript(AnimationScriptPath))
		{
			ReleaseAnimationScript();
			return;
		}

		// 함수와 결과의 Lua 참조가 정리된 뒤 실패 정리를 수행한다.
		const bool bInitialized = [this]()
		{
			sol::protected_function_result Result =
			    Script.Initialize(this);

			if (!Result.valid())
			{
				const sol::error Error = Result;

				HTR_LOG(
				    Error,
				    "[LuaAnim] InitializeAnimation failed: {}",
				    Error.what());

				return Fail(Error.what());
			}

			return true;
		}();

		if (!bInitialized)
		{
			ReleaseAnimationScript();
			return;
		}

		if (!Commit())
		{
			HTR_LOG(
			    Error,
			    "[LuaAnim] Commit failed: {}",
			    GetLastError());

			ReleaseAnimationScript();
			return;
		}

		//FAnimPlayerOptions Looping;
		//Looping.bLoop = true;

		//FAnimPlayerOptions OneShot;
		//OneShot.bLoop = false;

		//const FAnimNodeId LocomotionPlayer =
		//    CreateBlendSpacePlayer("Locomotion", Looping);

		//const FAnimNodeId AttackPlayer =
		//    CreateSequencePlayer("Attack", OneShot);

		//const FAnimNodeId HitPlayer =
		//    CreateSequencePlayer("Hit", OneShot);

		//if (LocomotionPlayer == InvalidAnimNodeId ||
		//    AttackPlayer == InvalidAnimNodeId ||
		//    HitPlayer == InvalidAnimNodeId)
		//{
		//	return;
		//}

		//const FAnimStateId Locomotion =
		//    AddState("Locomotion", LocomotionPlayer);

		//const FAnimStateId Attack =
		//    AddState("Attack", AttackPlayer);

		//const FAnimStateId Hit =
		//    AddState("Hit", HitPlayer);

		//if (Locomotion == InvalidAnimStateId ||
		//    Attack == InvalidAnimStateId ||
		//    Hit == InvalidAnimStateId)
		//{
		//	return;
		//}

		//if (!SetEntryState(Locomotion) ||
		//    !SetFloat("Speed", 0.0f) ||
		//    !SetBool("IsGrounded", true) ||
		//    !DefineTrigger("AttackRequested") ||
		//	!DefineTrigger("HitRequested"))
		//{
		//	return;
		//}

		//const FAnimConditionId CanAttack =
		//    AddCustomCondition(
		//        [](const FAnimConditionContext& Context)
		//        {
		//	        bool bGrounded = false;
		//	        bool bRequested = false;

		//	        return Context.Parameters.TryGetBool(
		//	                   FName("IsGrounded"), bGrounded) &&
		//	               Context.Parameters.TryGetTrigger(
		//	                   FName("AttackRequested"), bRequested) &&
		//	               bGrounded &&
		//	               bRequested;
		//        });

		//FAnimConditionDefinition HitRequested;
		//HitRequested.Type = EAnimConditionType::TriggerActive;
		//HitRequested.ParameterName = FName("HitRequested");

		//const FAnimConditionId HitConditionId =
		//    AddCondition(HitRequested);

		//if (HitConditionId == InvalidAnimConditionId ||
		//    CanAttack == InvalidAnimConditionId)
		//{
		//	return;
		//}

		//FAnimConditionDefinition Finished;
		//Finished.Type = EAnimConditionType::SourceFinished;

		//const FAnimConditionId FinishedId =
		//    AddCondition(Finished);

		//FLuaAnimTransitionOptions AttackOptions;
		//AttackOptions.Condition = CanAttack;
		//AttackOptions.Priority = 50;
		//AttackOptions.Blend.Duration = 0.2f;
		//AttackOptions.ConsumeTriggers.Add(FName("AttackRequested"));

		//FLuaAnimTransitionOptions ReturnOptions;
		//ReturnOptions.Condition = FinishedId;
		//ReturnOptions.Blend.Duration = 0.2f;

		//// 어느 상태에서든 피격으로 전환.
		//// 피격 중 다시 맞으면 Hit 애니메이션 재시작.
		//FLuaAnimTransitionOptions HitOptions;
		//HitOptions.Condition = HitConditionId;
		//HitOptions.Priority = 100;
		//HitOptions.Blend.Duration = 0.1f;
		//HitOptions.bRestartIfSameState = true;
		//HitOptions.InterruptPolicy = EAnimTransitionInterruptPolicy::Always;
		//HitOptions.ConsumeTriggers.Add(FName("HitRequested"));

		//// 피격 재생 완료 후 이동 상태로 복귀.
		//FLuaAnimTransitionOptions HitReturnOptions;
		//HitReturnOptions.Condition = FinishedId;
		//HitReturnOptions.Priority = 0;
		//HitReturnOptions.Blend.Duration = 0.2f;

		//if (AddTransition(Locomotion, Attack, AttackOptions) ==
		//        InvalidAnimTransitionId ||
		//    AddTransition(Attack, Locomotion, ReturnOptions) ==
		//        InvalidAnimTransitionId ||
		//    AddAnyStateTransition(Hit, HitOptions) ==
		//        InvalidAnimTransitionId ||
		//    AddTransition(Hit, Locomotion, HitReturnOptions) ==
		//        InvalidAnimTransitionId)
		//{
		//	return;
		//}

		//if (!Commit())
		//	return;

		//SetUpdateCallback(
		//    [LocomotionPlayer](ULuaAnimInstance& Instance, float)
		//    {
		//	    float Speed = 0.0f;

		//	    if (Instance.TryGetFloat("Speed", Speed))
		//		    Instance.SetBlendSpaceInput(LocomotionPlayer, Speed);
		//    });

		//return;
	}
}

void ULuaAnimInstance::NativeUnInitializeAnimation()
{
	ReleaseAnimationScript();
}


void ULuaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!bGraphReady || !Script.Update.valid())
		return;

	bool bFailed = false;

	{
		sol::protected_function_result Result =
		    Script.Update(this, DeltaSeconds);

		if (!Result.valid())
		{
			const sol::error Error = Result;

			HTR_LOG(
			    Error,
			    "[LuaAnim] UpdateAnimation failed: {}",
			    Error.what());

			Fail(Error.what());
			bFailed = true;
		}
	}

	if (bFailed)
		Script.Update = {};


}

UAnimSequence* ULuaAnimInstance::CreateRootTestClip(
    int32 RootBoneIndex, float DurationSeconds, const FVector& Offset)
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh || !Mesh->MeshData.Bones.IsValidIndex(RootBoneIndex))
		return nullptr;

	UAnimSequence* Clip = NewObject<UAnimSequence>(this);
	Clip->SetSkeletalMesh(Mesh);
	UAnimDataModel* Model = Clip->GetDataModel();
	constexpr int32 FPS = 30;
	Model->SetFrameRate(FFrameRate(FPS, 1));
	Model->SetNumberOfFrames(std::max(
	    1, static_cast<int32>(std::round(DurationSeconds * FPS))));

	const FSkeletalBone& RootBone = Mesh->MeshData.Bones[RootBoneIndex];
	const int32 TrackIndex = Model->AddBoneTrack(FName(RootBone.Name));
	TArray<FVector> Positions;
	const int32 NumKeys = Model->GetNumberOfKeys();
	Positions.Reserve(NumKeys);
	constexpr float TwoPi = 6.28318530718f;
	for (int32 Key = 0; Key < NumKeys; ++Key)
	{
		const float Phase = static_cast<float>(Key) / (NumKeys - 1);
		// 시작/끝은 기준 위치, 중간은 Offset. 반복 경계에서도 연속적이다.
		const float Weight = (Key == 0 || Key == NumKeys - 1)
		                         ? 0.0f
		                         : 0.5f - 0.5f * std::cos(TwoPi * Phase);
		Positions.Add(RootBone.RefPoseTransform.Location + Offset * Weight);
	}
	// 회전/스케일과 나머지 본의 로컬 변환은 기준 포즈를 유지한다.
	Model->SetBoneTrackKeys(TrackIndex, Positions, {}, {});
	return Clip;
}

bool ULuaAnimInstance::LoadAnimationScript(const FString& Path)
{
 FString Error;
 return Script.Load(Path,true,false,Error) || Fail(Error);
}

void ULuaAnimInstance::ReleaseAnimationScript()
{


	// Definition의 Custom 콜백에 캡처된 Lua 함수도 해제한다.
	ResetGraph();

	Script.Initialize = {};
	Script.Update = {};
	Script.Environment = {};
}
