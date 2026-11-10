#include "EnginePCH.h"
#include "LuaAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Asset/AssetManager.h"

#include "Engine/Engine.h"
#include "Scripting/LuaVM.h"

#include <cmath>
#include <utility>

ULuaAnimInstance::~ULuaAnimInstance()
{
	ReleaseAnimationScript();
}

bool ULuaAnimInstance::Fail(const FString& Message)
{
	LastError = Message;
	return false;
}

bool ULuaAnimInstance::RequireBuilding()
{
	if (!bBuilding)
		return Fail("Graph is not build mode");

	return true;
}

void ULuaAnimInstance::ResetGraph()
{
	RootNode = nullptr;

	bGraphReady = false;
	bBuilding = false;

	// 상태 머신이 가진 정의/노드 참조를 먼저 제거한다.
	StateMachine = FAnimNode_StateMachine{};

	// 그 다음 실제 노드와 정의를 제거한다.
	Players.Reset();
	Definition = FAnimStateMachineDefinition{};
}

bool ULuaAnimInstance::BeginBuild()
{
	if (!IsInitialized() || !GetSkeletalMesh())
		return Fail("AnimInstance must be initialized before building.");

	ResetGraph();

	LastError.clear();
	bBuilding = true;
	return true;
}

void ULuaAnimInstance::NativeInitializeAnimation()
{
	ResetGraph();
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
		UAnimSequence* JumpClip = CreateRootTestClip(RootBoneIndex, 0.8f, FVector(0, 0, 1.0f));
		UAnimSequence* AttackClip = CreateRootTestClip(RootBoneIndex, 0.6f, FVector(3.0f, 0, 0));
		UAnimSequence* HitClip = CreateRootTestClip(RootBoneIndex, 0.4f, FVector(-1.5f, 0, 0));
		UBlendSpace1D* TestBlendSpace = NewObject<UBlendSpace1D>(this);
		TestBlendSpace->SetSkeletalMesh(Mesh);
		FBlendParameter SpeedAxis;
		SpeedAxis.Name = FName("Speed");
		SpeedAxis.Min = 0.0f;
		SpeedAxis.Max = 600.0f;
		if (!IdleClip || !WalkClip || !RunClip || !AttackClip || !HitClip)
		{
			Fail("Failed to create test animation clips.");
			// 이미 생성된 테스트 에셋 정리도 필요
			return;
		}

		const auto AddSample = [&](UAnimSequence* Clip, float Speed)
		{
			FBlendSample Sample;
			Sample.Animation = Clip;
			Sample.SampleValue = FVector(Speed, 0.0f, 0.0f);

			return TestBlendSpace->AddSample(Sample);
		};

		if (!TestBlendSpace->SetBlendParameter(0, SpeedAxis) ||
		    !AddSample(IdleClip, 0.0f) ||
		    !AddSample(WalkClip, 300.0f) ||
		    !AddSample(RunClip, 600.0f))
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
			    LuaInitializeFunction(this);

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

//void ULuaAnimInstance::SetUpdateCallback(
//    FUpdateCallback Callback)
//{
//	UpdateCallback = std::move(Callback);
//}

void ULuaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (!bGraphReady || !LuaUpdateFunction.valid())
		return;

	bool bFailed = false;

	{
		sol::protected_function_result Result =
		    LuaUpdateFunction(this, DeltaSeconds);

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
		LuaUpdateFunction = {};

	//UpdateCallback(*this, DeltaSeconds);
}

bool ULuaAnimInstance::RegisterAssetAlias(
    const FString& Name,
    const FString& AssetKey)
{
	if (bGraphReady)
		return Fail("Cannot change asset aliases while graph is running.");

	if (Name.empty() || AssetKey.empty())
		return Fail("Asset alias and asset key must not be empty.");

	for (const FAssetAlias& Alias : AssetAliases)
	{
		if (Alias.Name == Name)
			return Fail("Duplicate asset alias.");
	}

	FAssetAlias Alias;
	Alias.Name = Name;
	Alias.AssetKey = AssetKey;

	AssetAliases.Add(Alias);
	return true;
}

FString ULuaAnimInstance::ResolveAssetKey(
    const FString& Name) const
{
	for (const FAssetAlias& Alias : AssetAliases)
	{
		if (Alias.Name == Name)
			return Alias.AssetKey;
	}

	return Name;
}

bool ULuaAnimInstance::ValidateAsset(
    const UAnimationAsset* Asset)
{
	if (!Asset)
		return Fail("Animation asset was not found or has the wrong type.");

	// 첫 구현은 동일한 메시를 사용하는 에셋으로 제한한다.
	if (Asset->GetSkeletalMesh() != GetSkeletalMesh())
		return Fail("Animation asset uses a different skeletal mesh.");

	const float Length = Asset->GetPlayLength();

	if (!std::isfinite(Length) || Length <= 0.0f)
		return Fail("Animation asest has an invalid play length.");

	return true;
}

FAnimNodeId ULuaAnimInstance::CreateSequencePlayer(
    const FString& ClipName,
    const FAnimPlayerOptions& Options)
{
	if (!RequireBuilding())
		return InvalidAnimNodeId;

	// 현재 SequencePlayer::IsFinished()는 정방향 완료 판정이다.
	// 첫 구현에서는 역재생을 허용하지 않는다.
	if (!std::isfinite(Options.PlayRate) || Options.PlayRate < 0.0f)
	{
		Fail("Play rate must be finite and non-negative.");
		return InvalidAnimNodeId;
	}

	UAnimSequence* Sequence =
	    UAssetManager::GetAssetByPath<UAnimSequence>(
	        ResolveAssetKey(ClipName));

	if (!ValidateAsset(Sequence))
		return InvalidAnimNodeId;

	auto Node = MakeUnique<FAnimNode_SequencePlayer>();

	Node->Sequence = Sequence;
	Node->bLoop = Options.bLoop;
	Node->bPlaying = Options.bPlaying;
	Node->PlayRate = Options.PlayRate;

	FPlayerEntry Entry;
	Entry.Kind = EPlayerKind::Sequence;
	Entry.Node = std::move(Node);

	const FAnimNodeId Id =
	    static_cast<FAnimNodeId>(Players.size());

	Players.Add(std::move(Entry));
	return Id;
}

FAnimNodeId ULuaAnimInstance::CreateBlendSpacePlayer(
    const FString& BlendSpaceName,
    const FAnimPlayerOptions& Options)
{
	if (!RequireBuilding())
		return InvalidAnimNodeId;

	if (!std::isfinite(Options.PlayRate) || Options.PlayRate < 0.0f)
	{
		Fail("Play rate must be finite and non-negative.");
		return InvalidAnimNodeId;
	}

	UBlendSpace* BlendSpace =
	    UAssetManager::GetAssetByPath<UBlendSpace>(
	        ResolveAssetKey(BlendSpaceName));

	if (!ValidateAsset(BlendSpace))
		return InvalidAnimNodeId;

	if (BlendSpace->GetDimension() != 1 ||
	    BlendSpace->GetNumSamples() == 0)
	{
		Fail("A non-empty 1D BlendSpace is required.");
		return InvalidAnimNodeId;
	}

	// BlendSpace에 포함된 각 Clip도 검증한다.
	for (const FBlendSample& Sample : BlendSpace->GetSamples())
	{
		if (!ValidateAsset(Sample.Animation))
			return InvalidAnimNodeId;
	}

	auto Node = MakeUnique<FAnimNode_BlendSpacePlayer>();

	Node->BlendSpace = BlendSpace;
	Node->bLoop = Options.bLoop;
	Node->bPlaying = Options.bPlaying;
	Node->PlayRate = Options.PlayRate;

	FPlayerEntry Entry;
	Entry.Kind = EPlayerKind::BlendSpace;
	Entry.Node = std::move(Node);

	const FAnimNodeId Id =
	    static_cast<FAnimNodeId>(Players.size());

	Players.Add(std::move(Entry));
	return Id;
}

ULuaAnimInstance::FPlayerEntry* ULuaAnimInstance::FindPlayer(
    FAnimNodeId NodeId)
{
	if (NodeId < 0 ||
	    static_cast<size_t>(NodeId) >= Players.size())
	{
		return nullptr;
	}

	return &Players[NodeId];
}

FAnimStateId ULuaAnimInstance::AddState(
    const FString& Name,
    FAnimNodeId NodeId)
{
	if (!RequireBuilding())
		return InvalidAnimStateId;

	if (Name.empty() || !FindPlayer(NodeId))
	{
		Fail("State requires a name and a valid node.");
		return InvalidAnimStateId;
	}

	const FName StateName(Name.c_str());

	for (const FAnimStateDefinition& Existing : Definition.States)
	{
		if (Existing.Name == StateName)
		{
			Fail("Duplicate state name.");
			return InvalidAnimStateId;
		}

		// 상태마다 독립적인 재생 시간을 갖게 한다.
		// 같은 에셋을 쓰더라도 플레이어는 별도로 생성한다.
		if (Existing.RootNodeIndex == NodeId)
		{
			Fail("A player node cannot be shared by multiple states.");
			return InvalidAnimStateId;
		}
	}

	FAnimStateDefinition State;
	State.Name = StateName;
	State.RootNodeIndex = NodeId;

	const FAnimStateId Id = Definition.States.Num();
	Definition.States.Add(State);

	return Id;
}

bool ULuaAnimInstance::SetEntryState(FAnimStateId StateId)
{
	if (!RequireBuilding())
		return false;

	if (!Definition.States.IsValidIndex(StateId))
		return Fail("Invalid entry state.");

	Definition.EntryState = StateId;
	return true;
}

FAnimConditionId ULuaAnimInstance::AddCondition(
    const FAnimConditionDefinition& Condition)
{
	if (!RequireBuilding())
		return InvalidAnimConditionId;

	const FAnimConditionId Id = Definition.Conditions.Num();

	// 기존 상태 머신은 자식 조건이 먼저 등록되어야 한다.
	for (FAnimConditionId Child : Condition.Children)
	{
		if (Child < 0 || Child >= Id)
		{
			Fail("Condition children must refer to earlier conditions.");
			return InvalidAnimConditionId;
		}
	}

	if (Condition.Type == EAnimConditionType::Custom &&
	    !Condition.Callback)
	{
		Fail("Custom condition requires a callback.");
		return InvalidAnimConditionId;
	}

	Definition.Conditions.Add(Condition);
	return Id;
}

FAnimConditionId ULuaAnimInstance::AddCustomCondition(
    FAnimConditionCallback Callback)
{
	FAnimConditionDefinition Condition;
	Condition.Type = EAnimConditionType::Custom;
	Condition.Callback = std::move(Callback);

	return AddCondition(Condition);
}

FAnimTransitionId ULuaAnimInstance::AddTransition(
    FAnimStateId From,
    FAnimStateId To,
    const FLuaAnimTransitionOptions& Options)
{
	return AddTransitionInternal(From, To, false, Options);
}

FAnimTransitionId ULuaAnimInstance::AddAnyStateTransition(
    FAnimStateId To,
    const FLuaAnimTransitionOptions& Options)
{
	return AddTransitionInternal(
	    InvalidAnimStateId, To, true, Options);
}

FAnimTransitionId ULuaAnimInstance::AddTransitionInternal(
    FAnimStateId From,
    FAnimStateId To,
    bool bAnyState,
    const FLuaAnimTransitionOptions& Options)
{
	if (!RequireBuilding())
		return InvalidAnimTransitionId;

	if (!Definition.States.IsValidIndex(To) ||
	    (!bAnyState && !Definition.States.IsValidIndex(From)))
	{
		Fail("Transition refers to an invalid state.");
		return InvalidAnimTransitionId;
	}

	if (!Definition.Conditions.IsValidIndex(Options.Condition))
	{
		Fail("Transition requires a valid condition.");
		return InvalidAnimTransitionId;
	}

	if (!std::isfinite(Options.Blend.Duration) ||
	    Options.Blend.Duration < 0.0f)
	{
		Fail("Blend duration must be finite and non-negative.");
		return InvalidAnimTransitionId;
	}

	FAnimTransitionDefinition Transition;
	Transition.From = From;
	Transition.To = To;
	Transition.bFromAnyState = bAnyState;

	Transition.Condition = Options.Condition;
	Transition.Priority = Options.Priority;
	Transition.Blend = Options.Blend;

	Transition.bRestartIfSameState =
	    Options.bRestartIfSameState;

	Transition.InterruptPolicy = Options.InterruptPolicy;
	Transition.ConsumeTriggers = Options.ConsumeTriggers;

	const FAnimTransitionId Id = Definition.Transitions.Num();
	Definition.Transitions.Add(Transition);

	return Id;
}

bool ULuaAnimInstance::SetBool(
    const FString& Name,
    bool Value)
{
	if ((!bBuilding && !bGraphReady) || Name.empty())
		return Fail("Invalid parameter write.");

	if (!StateMachine.Parameters.SetBool(FName(Name.c_str()), Value))
		return Fail("Parameter type mismatch.");

	return true;
}

bool ULuaAnimInstance::SetFloat(
    const FString& Name,
    float Value)
{
	if ((!bBuilding && !bGraphReady) || Name.empty())
		return Fail("Invalid parameter write.");

	if (!StateMachine.Parameters.SetFloat(FName(Name.c_str()), Value))
		return Fail("Invalid float value or parameter type mismatch.");

	return true;
}

bool ULuaAnimInstance::DefineTrigger(const FString& Name)
{
	if (!RequireBuilding())
		return false;

	if (Name.empty())
		return Fail("Trigger name must not be emptry.");

	const FName Key(Name.c_str());

	// 현재 ParameterSet에는 비활성 Trigger 생성 API가 없으므로
	// 생성한 뒤 즉시 비활성화한다.
	if (!StateMachine.Parameters.Trigger(Key))
		return Fail("Parameter type mismatch");

	StateMachine.Parameters.ConsumeTrigger(Key);
	return true;
}

bool ULuaAnimInstance::SetTrigger(const FString& Name)
{
	if ((!bBuilding && !bGraphReady) || Name.empty())
		return Fail("Invalid trigger write.");

	const FName Key(Name.c_str());
	bool Value = false;

	// 오타로 새로운 Trigger가 생기지 않도록 사전 정의를 요구한다.
	if (!StateMachine.Parameters.TryGetTrigger(Key, Value))
		return Fail("Trigger has not been defined.");

	return StateMachine.Parameters.Trigger(Key);
}

bool ULuaAnimInstance::ResetTrigger(const FString& Name)
{
	if ((!bBuilding && !bGraphReady) || Name.empty())
		return Fail("Invalid trigger write.");

	const FName Key(Name.c_str());
	bool Value = false;

	if (!StateMachine.Parameters.TryGetTrigger(Key, Value))
		return Fail("Trigger has not been defined.");

	StateMachine.Parameters.ConsumeTrigger(Key);
	return true;
}

bool ULuaAnimInstance::TryGetBool(
    const FString& Name,
    bool& OutValue) const
{
	return StateMachine.Parameters.TryGetBool(
	    FName(Name.c_str()), OutValue);
}

bool ULuaAnimInstance::TryGetFloat(
    const FString& Name,
    float& OutValue) const
{
	return StateMachine.Parameters.TryGetFloat(
	    FName(Name.c_str()), OutValue);
}

bool ULuaAnimInstance::TryGetTrigger(
    const FString& Name,
    bool& OutValue) const
{
	return StateMachine.Parameters.TryGetTrigger(
	    FName(Name.c_str()), OutValue);
}

bool ULuaAnimInstance::ValidateParameters()
{
	for (const FAnimConditionDefinition& Condition :
	     Definition.Conditions)
	{
		bool BoolValue = false;
		float FloatValue = 0.0f;
		bool bValid = true;

		switch (Condition.Type)
		{
		case EAnimConditionType::BoolEquals:
			bValid = StateMachine.Parameters.TryGetBool(
			    Condition.ParameterName, BoolValue);
			break;

		case EAnimConditionType::FloatCompare:
			bValid = StateMachine.Parameters.TryGetFloat(
			    Condition.ParameterName, FloatValue);
			break;

		case EAnimConditionType::TriggerActive:
			bValid = StateMachine.Parameters.TryGetTrigger(
			    Condition.ParameterName, BoolValue);
			break;

		default:
			break;
		}

		if (!bValid)
			return Fail("Condition parameter is missing or has the wrong type.");
	}

	for (const FAnimTransitionDefinition& Transition :
	     Definition.Transitions)
	{
		for (const FName& Name : Transition.ConsumeTriggers)
		{
			bool Value = false;

			if (!StateMachine.Parameters.TryGetTrigger(Name, Value))
				return Fail("Consumed trigger has not been defined.");
		}
	}

	return true;
}

bool ULuaAnimInstance::Commit()
{
	if (!RequireBuilding())
		return false;

	if (!IsInitialized() || !GetSkeletalMesh())
		return Fail("AnimInstance is not initialized.");

	if (!ValidateParameters())
		return false;

	TArray<FAnimNode_Base*> NodePointers;

	for (const FPlayerEntry& Entry : Players)
		NodePointers.Add(Entry.Node.get());

	if (!StateMachine.Initialize(Definition, NodePointers))
		return Fail("State machine definition validation failed.");

	StateMachine.OnEnter();

	// 최초 Evaluate 전 BlendSpace의 샘플 가중치 등을 준비한다.
	// StateMachine.Update()를 호출하면 전환까지 평가되므로
	// 여기서는 재생 노드들만 DeltaSeconds=0으로 갱신한다.
	FAnimationUpdateContext Context;
	Context.AnimInstance = this;
	Context.DeltaSeconds = 0.0f;

	for (const FPlayerEntry& Entry : Players)
		Entry.Node->Update(Context);

	RootNode = &StateMachine;

	bBuilding = false;
	bGraphReady = true;

	LastError.clear();
	return true;
}

bool ULuaAnimInstance::SetBlendSpaceInput(
    FAnimNodeId NodeId,
    float Value)
{
	if (!std::isfinite(Value))
		return Fail("BlendSpace input must be finite.");

	FPlayerEntry* Entry = FindPlayer(NodeId);

	if (!Entry || Entry->Kind != EPlayerKind::BlendSpace)
		return Fail("Node is not a BlendSpace player.");

	auto* Node =
	    static_cast<FAnimNode_BlendSpacePlayer*>(Entry->Node.get());

	Node->X = Value;
	return true;
}

bool ULuaAnimInstance::SetPlayRate(
    FAnimNodeId NodeId,
    float Value)
{
	if (!std::isfinite(Value) || Value < 0.0f)
		return Fail("Play rate must be finite and non-negative.");

	FPlayerEntry* Entry = FindPlayer(NodeId);

	if (!Entry)
		return Fail("Invalid player node.");

	Entry->Node->PlayRate = Value;
	return true;
}

bool ULuaAnimInstance::SetPlaying(
    FAnimNodeId NodeId,
    bool bPlaying)
{
	FPlayerEntry* Entry = FindPlayer(NodeId);

	if (!Entry)
		return Fail("Invalid player node.");

	Entry->Node->bPlaying = bPlaying;
	return true;
}

FAnimStateId ULuaAnimInstance::GetCurrentState() const
{
	return bGraphReady
	           ? StateMachine.GetCurrentState()
	           : InvalidAnimStateId;
}

bool ULuaAnimInstance::IsSourceFinished() const
{
	return bGraphReady && StateMachine.IsClipFinished();
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
	FLuaVM* VM = GEngine ? GEngine->GetLuaVM() : nullptr;
	sol::state* State = VM ? VM->GetState() : nullptr;

	if (!State)
		return Fail("Lua VM is not initialized.");

	ScriptEnvironment = sol::environment(
	    *State, sol::create, State->globals());

	sol::protected_function_result Loaded =
	    State->safe_script_file(
	        Path,
	        ScriptEnvironment,
	        sol::script_pass_on_error);

	if (!Loaded.valid())
	{
		const sol::error Error = Loaded;

		HTR_LOG(
		    Error,
		    "[LuaAnim] Failed to load '{}': {}",
		    Path,
		    Error.what());

		return Fail(Error.what());
	}

	sol::object Initialize =
	    ScriptEnvironment["InitializeAnimation"];

	if (Initialize.get_type() != sol::type::function)
		return Fail("InitializeAnimation function is required.");

	sol::object Update =
	    ScriptEnvironment["UpdateAnimation"];

	if (Update.get_type() != sol::type::nil &&
	    Update.get_type() != sol::type::function)
	{
		return Fail("UpdateAnimation must be a function.");
	}

	LuaInitializeFunction =
	    Initialize.as<sol::protected_function>();

	if (Update.get_type() == sol::type::function)
		LuaUpdateFunction = Update.as<sol::protected_function>();

	return true;
}

void ULuaAnimInstance::ReleaseAnimationScript()
{
	UpdateCallback = {};

	// Definition의 Custom 콜백에 캡처된 Lua 함수도 해제한다.
	ResetGraph();

	LuaInitializeFunction = {};
	LuaUpdateFunction = {};
	ScriptEnvironment = {};
}
