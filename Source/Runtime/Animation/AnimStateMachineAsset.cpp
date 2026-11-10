#include "EnginePCH.h"
#include "AnimStateMachineAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Asset/AssetManager.h"
#include "Core/Misc/FileHelper.h"
#include "Render/Mesh.h"
#include <cmath>
#include <set>
#include <unordered_map>

namespace
{
int StateIndex(const FAnimStateMachineAssetData &D, int Id)
{
	for (int i = 0; i < (int)D.States.size(); ++i)
		if (D.States[i].Id == Id)
			return i;
	return -1;
}
bool AssetPath(const FString &Path, fs::path &Native, FString &Key, FString &Error)
{
	std::error_code ec;
	Native = fs::weakly_canonical(FFileHelper::ToNativePath(Path), ec);
	auto root = fs::weakly_canonical("Assets", ec);
	auto relative = Native.lexically_relative(root);
	if (ec || relative.empty() || relative.is_absolute() || *relative.begin() == ".." ||
		_stricmp(Native.extension().string().c_str(), ".animsm"))
	{
		Error = "Choose an .animsm file inside Assets.";
		return false;
	}
	Key = FFileHelper::ToUtf8Path(relative);
	return true;
}
} // namespace
bool FAnimStateMachineAssetData::Validate(FString &Error) const
{
	Error.clear();
	auto fail = [&](const FString &s) {
		Error = s;
		return false;
	};
	auto *mesh = UAssetManager::GetAssetByPath<USkeletalMesh>(MeshKey);
	if (!mesh)
		return fail("Missing skeletal mesh: " + MeshKey);
	if (StateIndex(*this, Entry) < 0)
		return fail("Choose a valid entry state.");
	std::set<int> ids;
	std::set<FString> names, params;
	FAnimParameterSet parameterSet;
	for (const auto &p : Parameters)
	{
		const FString name = p.Name.ToString();
		if (name.empty() || !params.insert(name).second)
			return fail("Empty or duplicate parameter name.");
		if (!std::isfinite(p.FloatValue))
			return fail("Parameter value must be finite.");
		bool ok = false;
		switch (p.Type)
		{
		case EAnimParameterType::Bool:
			ok = parameterSet.SetBool(p.Name, p.BoolValue);
			break;
		case EAnimParameterType::Float:
			ok = parameterSet.SetFloat(p.Name, p.FloatValue);
			break;
		case EAnimParameterType::Trigger:
			ok = parameterSet.Trigger(p.Name);
			break;
		}
		if (!ok)
			return fail("Invalid parameter type.");
	}
	FAnimStateMachineDefinition def;
	def.EntryState = StateIndex(*this, Entry);
	FAnimNode_Base dummy;
	TArray<FAnimNode_Base *> nodes;
	for (const auto &s : States)
	{
		if (s.Id < 0 || !ids.insert(s.Id).second || s.Name.empty() || !names.insert(s.Name).second)
			return fail("States require unique IDs and names.");
		if (!std::isfinite(s.Options.PlayRate) || s.Options.PlayRate < 0 || !std::isfinite(s.X) || !std::isfinite(s.Y))
			return fail("Invalid state numeric value.");
		const UAnimationAsset *a =
			s.BlendSpace ? static_cast<UAnimationAsset *>(UAssetManager::GetAssetByPath<UBlendSpace1D>(s.AssetKey))
						 : static_cast<UAnimationAsset *>(UAssetManager::GetAssetByPath<UAnimSequence>(s.AssetKey));
		if (!a || a->GetSkeletalMesh() != mesh || !std::isfinite(a->GetPlayLength()) || a->GetPlayLength() <= 0)
			return fail("Missing or incompatible animation in state: " + s.Name);
		if (s.BlendSpace)
		{
			const auto *bs = static_cast<const UBlendSpace1D *>(a);
			if (bs->GetNumSamples() == 0)
				return fail("BlendSpace must contain samples.");
			for (const auto &sample : bs->GetSamples())
				if (!sample.Animation || sample.Animation->GetSkeletalMesh() != mesh ||
					!std::isfinite(sample.Animation->GetPlayLength()) || sample.Animation->GetPlayLength() <= 0)
					return fail("BlendSpace contains an invalid sample.");
		}
		if (s.BlendSpace && !s.BlendParameter.empty())
		{
			float v;
			if (!parameterSet.TryGetFloat(FName(s.BlendParameter.c_str()), v))
				return fail("BlendSpace requires a declared Float parameter: " + s.Name);
		}
		FAnimStateDefinition state;
		state.Name = FName(s.Name.c_str());
		state.RootNodeIndex = nodes.Num();
		def.States.Add(state);
		nodes.Add(&dummy);
	}
	for (const auto &c : Conditions)
	{
		if (c.Type == EAnimConditionType::Custom)
			return fail("Lua callback conditions are not supported in assets.");
		bool b;
		float f;
		if ((c.Type == EAnimConditionType::BoolEquals && !parameterSet.TryGetBool(c.ParameterName, b)) ||
			(c.Type == EAnimConditionType::FloatCompare && !parameterSet.TryGetFloat(c.ParameterName, f)) ||
			(c.Type == EAnimConditionType::TriggerActive && !parameterSet.TryGetTrigger(c.ParameterName, b)))
			return fail("Condition references an undeclared parameter or wrong type.");
		def.Conditions.Add(c);
	}
	for (const auto &t : Transitions)
	{
		FAnimTransitionDefinition tr;
		tr.From = StateIndex(*this, t.From);
		tr.To = StateIndex(*this, t.To);
		tr.bFromAnyState = t.From == -1;
		tr.Condition = t.Options.Condition;
		tr.Priority = t.Options.Priority;
		tr.Blend = t.Options.Blend;
		tr.InterruptPolicy = t.Options.InterruptPolicy;
		tr.bRestartIfSameState = t.Options.bRestartIfSameState;
		tr.ConsumeTriggers = t.Options.ConsumeTriggers;
		for (auto n : tr.ConsumeTriggers)
		{
			bool v;
			if (!parameterSet.TryGetTrigger(n, v))
				return fail("Consumed trigger is not declared.");
		}
		def.Transitions.Add(tr);
	}
	FAnimNode_StateMachine sm;
	if (!sm.Initialize(def, nodes))
		return fail("Invalid transition or condition. Condition children must reference earlier conditions; check "
					"entry, endpoints and blend settings.");
	return true;
}
bool FAnimStateMachineAssetData::Build(UStateMachineAnimInstance &I, FString &Error) const
{
	if (!Validate(Error))
		return false;
	auto failed = [&]() {
		Error = I.GetLastError();
		return false;
	};
	if (!I.BeginBuild())
		return failed();
	for (const auto &p : Parameters)
	{
		bool ok = false;
		auto name = p.Name.ToString();
		switch (p.Type)
		{
		case EAnimParameterType::Bool:
			ok = I.SetBool(name, p.BoolValue);
			break;
		case EAnimParameterType::Float:
			ok = I.SetFloat(name, p.FloatValue);
			break;
		case EAnimParameterType::Trigger:
			ok = I.DefineTrigger(name);
			break;
		}
		if (!ok)
			return failed();
	}
	for (const auto &s : States)
	{
		int node = s.BlendSpace ? I.CreateBlendSpacePlayer(s.AssetKey, s.Options)
								: I.CreateSequencePlayer(s.AssetKey, s.Options);
		if (node < 0 || I.AddState(s.Name, node) < 0)
			return failed();
	}
	if (!I.SetEntryState(StateIndex(*this, Entry)))
		return failed();
	for (const auto &c : Conditions)
		if (I.AddCondition(c) < 0)
			return failed();
	for (const auto &t : Transitions)
	{
		int id = t.From == -1 ? I.AddAnyStateTransition(StateIndex(*this, t.To), t.Options)
							  : I.AddTransition(StateIndex(*this, t.From), StateIndex(*this, t.To), t.Options);
		if (id < 0)
			return failed();
	}
	return I.Commit() || failed();
}

UAnimStateMachineAsset *UAssetManager::LoadAnimStateMachine(const FString &Path, FString &Error)
{
	fs::path native;
	FString key;
	if (!AssetPath(Path, native, key, Error))
		return nullptr;
	if (AssetMap.Contains(key) && !GetAssetByPath<UAnimStateMachineAsset>(key))
	{
		Error = "Asset key belongs to a different asset type.";
		return nullptr;
	}
	FString text;
	if (!FFileHelper::LoadFileToString(text, FFileHelper::ToUtf8Path(native)))
	{
		Error = "Cannot read state machine file.";
		return nullptr;
	}
	try
	{
		auto j = json::parse(text);
		if (j.at("Version") != 1 || j.at("Type") != "AnimStateMachine")
		{
			Error = "Unsupported state machine version/type.";
			return nullptr;
		}
		FAnimStateMachineAssetData d;
		d.MeshKey = j.at("Mesh");
		d.Entry = j.at("Entry");
		d.UpdateScript = j.value("UpdateScript", FString{});
		for (auto &v : j.at("Parameters"))
		{
			FAnimParameter p;
			p.Name = FName(v.at("Name").get<FString>().c_str());
			p.Type = (EAnimParameterType)v.at("Type").get<int>();
			p.BoolValue = v.at("Bool");
			p.FloatValue = v.at("Float");
			d.Parameters.push_back(p);
		}
		for (auto &v : j.at("States"))
		{
			FAnimAssetState s;
			s.Id = v.at("Id");
			s.Name = v.at("Name");
			s.AssetKey = v.at("Asset");
			s.BlendSpace = v.at("BlendSpace");
			s.BlendParameter = v.at("Parameter");
			s.Options.bLoop = v.at("Loop");
			s.Options.bPlaying = v.value("Playing", true);
			s.Options.PlayRate = v.at("Rate");
			s.X = v.at("X");
			s.Y = v.at("Y");
			d.States.push_back(s);
		}
		for (auto &v : j.at("Conditions"))
		{
			FAnimConditionDefinition c;
			c.Type = (EAnimConditionType)v.at("Type").get<int>();
			c.ParameterName = FName(v.at("Parameter").get<FString>().c_str());
			c.BoolValue = v.at("Bool");
			c.FloatValue = v.at("Float");
			c.Tolerance = v.at("Tolerance");
			c.CompareOp = (EAnimCompareOp)v.at("Op").get<int>();
			for (auto &n : v.at("Children"))
				c.Children.Add(n.get<int>());
			d.Conditions.push_back(c);
		}
		for (auto &v : j.at("Transitions"))
		{
			FAnimAssetTransition t;
			t.From = v.at("From");
			t.To = v.at("To");
			t.Options.Condition = v.at("Condition");
			t.Options.Priority = v.at("Priority");
			t.Options.Blend.Duration = v.at("Duration");
			t.Options.Blend.Curve = (EAnimBlendCurve)v.at("Curve").get<int>();
			t.Options.InterruptPolicy = (EAnimTransitionInterruptPolicy)v.at("Interrupt").get<int>();
			t.Options.bRestartIfSameState = v.at("Restart");
			for (auto &n : v.at("Consume"))
				t.Options.ConsumeTriggers.Add(FName(n.get<FString>().c_str()));
			d.Transitions.push_back(t);
		}
		if (!d.Validate(Error))
			return nullptr;
		auto *asset = GetAssetByPath<UAnimStateMachineAsset>(key);
		if (!asset)
		{
			asset = NewObject<UAnimStateMachineAsset>();
			RegisterAsset(key, asset);
		}
		asset->Data = std::move(d);
		AssetPathMap.Add(key, FFileHelper::ToUtf8Path(native));
		return asset;
	}
	catch (const std::exception &e)
	{
		Error = FString("Invalid state machine file: ") + e.what();
		return nullptr;
	}
}
bool UAssetManager::SaveAnimStateMachine(const FString &Path, const FAnimStateMachineAssetData &d, FString &Error,
										 bool CreateOnly)
{
	fs::path native;
	FString key;
	if (!AssetPath(Path, native, key, Error) || !d.Validate(Error))
		return false;
	if (AssetMap.Contains(key) && !GetAssetByPath<UAnimStateMachineAsset>(key))
	{
		Error = "Asset key belongs to a different asset type.";
		return false;
	}
	std::error_code ec;
	if (CreateOnly && fs::exists(native, ec))
	{
		Error = "File already exists.";
		return false;
	}
	json j = {{"Version", 1},
			  {"Type", "AnimStateMachine"},
			  {"Mesh", d.MeshKey},
			  {"Entry", d.Entry},
			  {"UpdateScript", d.UpdateScript},
			  {"States", json::array()},
			  {"Parameters", json::array()},
			  {"Conditions", json::array()},
			  {"Transitions", json::array()}};
	for (auto &p : d.Parameters)
		j["Parameters"].push_back(
			{{"Name", p.Name.ToString()}, {"Type", (int)p.Type}, {"Bool", p.BoolValue}, {"Float", p.FloatValue}});
	for (auto &s : d.States)
		j["States"].push_back({{"Id", s.Id},
							   {"Name", s.Name},
							   {"Asset", s.AssetKey},
							   {"BlendSpace", s.BlendSpace},
							   {"Parameter", s.BlendParameter},
							   {"Loop", s.Options.bLoop},
							   {"Playing", s.Options.bPlaying},
							   {"Rate", s.Options.PlayRate},
							   {"X", s.X},
							   {"Y", s.Y}});
	for (auto &c : d.Conditions)
	{
		json children = json::array();
		for (auto n : c.Children)
			children.push_back(n);
		j["Conditions"].push_back({{"Type", (int)c.Type},
								   {"Parameter", c.ParameterName.ToString()},
								   {"Bool", c.BoolValue},
								   {"Float", c.FloatValue},
								   {"Tolerance", c.Tolerance},
								   {"Op", (int)c.CompareOp},
								   {"Children", children}});
	}
	for (auto &t : d.Transitions)
	{
		json consume = json::array();
		for (auto n : t.Options.ConsumeTriggers)
			consume.push_back(n.ToString());
		j["Transitions"].push_back({{"From", t.From},
									{"To", t.To},
									{"Condition", t.Options.Condition},
									{"Priority", t.Options.Priority},
									{"Duration", t.Options.Blend.Duration},
									{"Curve", (int)t.Options.Blend.Curve},
									{"Interrupt", (int)t.Options.InterruptPolicy},
									{"Restart", t.Options.bRestartIfSameState},
									{"Consume", consume}});
	}
	wchar_t temp[MAX_PATH];
	if (!GetTempFileNameW(native.parent_path().c_str(), L"asm", 0, temp))
	{
		Error = "Cannot create save file.";
		return false;
	}
	bool ok = false;
	try
	{
		ok = FFileHelper::SaveStringToFile(j.dump(2), FFileHelper::ToUtf8Path(temp));
	}
	catch (const std::exception &e)
	{
		Error = e.what();
	}
	if (ok)
		ok = MoveFileExW(temp, native.c_str(), MOVEFILE_WRITE_THROUGH | (CreateOnly ? 0 : MOVEFILE_REPLACE_EXISTING)) !=
			 FALSE;
	if (!ok)
	{
		fs::remove(temp, ec);
		Error = "Cannot save state machine file.";
		return false;
	}
	auto *asset = GetAssetByPath<UAnimStateMachineAsset>(key);
	if (!asset)
	{
		asset = NewObject<UAnimStateMachineAsset>();
		RegisterAsset(key, asset);
	}
	asset->Data = d;
	AssetPathMap.Add(key, FFileHelper::ToUtf8Path(native));
	Error.clear();
	return true;
}
