#pragma once
#include "Animation/StateMachineAnimInstance.h"
#include "Asset/RenderAsset.h"
#include <vector>

struct FAnimAssetState
{
	int Id = 0;
	FString Name = "State", AssetKey, BlendParameter;
	bool BlendSpace = false;
	FAnimPlayerOptions Options;
	float X = 0, Y = 0;
};
struct FAnimAssetTransition
{
	int From = -1, To = -1; // -1 source means AnyState
	FAnimTransitionOptions Options;
};
struct FAnimStateMachineAssetData
{
	FString MeshKey, UpdateScript;
	int Entry = -1;
	std::vector<FAnimAssetState> States;
	std::vector<FAnimParameter> Parameters;
	std::vector<FAnimConditionDefinition> Conditions;
	std::vector<FAnimAssetTransition> Transitions;
	bool Validate(FString &Error) const;
	bool Build(UStateMachineAnimInstance &Instance, FString &Error) const;
};
class UAnimStateMachineAsset : public URenderAsset
{
	DECLARE_CLASS(UAnimStateMachineAsset, URenderAsset)
  public:
	FAnimStateMachineAssetData Data;
};
