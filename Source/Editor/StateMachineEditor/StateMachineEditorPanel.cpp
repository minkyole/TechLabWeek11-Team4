#include "EnginePCH.h"
#include "StateMachineEditorPanel.h"
#include "Animation/AnimSequence.h"
#include "Animation/AssetAnimInstance.h"
#include "Animation/BlendSpace.h"
#include "Asset/AssetManager.h"
#include "Core/Misc/FileHelper.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshViewerPanel.h"
#include "Render/Mesh.h"
#include <algorithm>
#include <cmath>

namespace
{
bool TextEdit(const char *Label, FString &Value)
{
	char buffer[1024];
	strncpy_s(buffer, Value.c_str(), _TRUNCATE);
	if (ImGui::InputText(Label, buffer, sizeof(buffer)))
	{
		Value = buffer;
		return true;
	}
	return false;
}
bool NameEdit(const char *Label, FName &Value)
{
	auto text = Value.ToString();
	if (TextEdit(Label, text))
	{
		Value = FName(text.c_str());
		return true;
	}
	return false;
}
bool EnumEdit(const char *Label, int &Value, const char *Choices)
{
	return ImGui::Combo(Label, &Value, Choices);
}
} // namespace
FAnimAssetState *FStateMachineEditorPanel::FindState(int Id)
{
	for (auto &s : Working.States)
		if (s.Id == Id)
			return &s;
	return nullptr;
}
const char *FStateMachineEditorPanel::StateName(int Id)
{
	auto *s = FindState(Id);
	return s ? s->Name.c_str() : (Id == -1 ? "AnyState" : "Missing");
}
bool FStateMachineEditorPanel::StateCombo(const char *Label, int &Id, bool Any)
{
	bool changed = false;
	if (ImGui::BeginCombo(Label, StateName(Id)))
	{
		if (Any && ImGui::Selectable("AnyState", Id == -1))
		{
			Id = -1;
			changed = true;
		}
		for (auto &s : Working.States)
			if (ImGui::Selectable(s.Name.c_str(), s.Id == Id))
			{
				Id = s.Id;
				changed = true;
			}
		ImGui::EndCombo();
	}
	return changed;
}
void FStateMachineEditorPanel::SetOpen(bool Open)
{
	if (!Open && Dirty)
	{
		PendingClose = true;
		return;
	}
	if (!Open && Viewer)
		Viewer->StopStateMachinePreview();
	IEditorPanel::SetOpen(Open);
}
void FStateMachineEditorPanel::ReleaseResources()
{
	if (Viewer)
		Viewer->StopStateMachinePreview();
	Working = {};
	Dirty = false;
}
void FStateMachineEditorPanel::OpenAsset(const FString &P)
{
	PendingPath = P;
	PendingFolder.clear();
	IEditorPanel::SetOpen(true);
	if (!Dirty)
		LoadPending();
}
void FStateMachineEditorPanel::NewAsset(const FString &Folder)
{
	PendingPath.clear();
	PendingFolder = Folder;
	IEditorPanel::SetOpen(true);
	if (!Dirty)
		LoadPending();
}
void FStateMachineEditorPanel::LoadPending()
{
	if (!PendingPath.empty())
	{
		FString error;
		auto *a = UAssetManager::Get().LoadAnimStateMachine(PendingPath, error);
		if (!a)
		{
			Error = error;
			PendingPath.clear();
			return;
		}
		if (Viewer)
			Viewer->StopStateMachinePreview();
		Working = a->Data;
		Path = PendingPath;
		PendingPath.clear();
		Dirty = false;
	}
	else if (!PendingFolder.empty())
	{
		if (Viewer)
			Viewer->StopStateMachinePreview();
		Working = {};
		Path.clear();
		auto p = FFileHelper::ToUtf8Path(FFileHelper::ToNativePath(PendingFolder) / "NewStateMachine.animsm");
		strncpy_s(SavePath, p.c_str(), _TRUNCATE);
		PendingFolder.clear();
		Dirty = true;
	}
	Selected = SelectedTransition = -1;
	LinkFrom = -2;
	NextId = 1;
	for (auto &s : Working.States)
		NextId = std::max(NextId, s.Id + 1);
	Pan = ImVec2(30, 70);
	Zoom = 1;
	Error.clear();
}
bool FStateMachineEditorPanel::Save()
{
	FString target = Path.empty() ? FString(SavePath) : Path;
	if (!UAssetManager::Get().SaveAnimStateMachine(target, Working, Error, Path.empty()))
		return false;
	Path = target;
	Dirty = false;
	return true;
}
void FStateMachineEditorPanel::AddState()
{
	FAnimAssetState s;
	s.Id = NextId++;
	s.Name = "State_" + std::to_string(s.Id);
	s.X = 40.0f * (s.Id % 5);
	s.Y = 90.0f * (s.Id % 5);
	auto *mesh = UAssetManager::GetAssetByPath<USkeletalMesh>(Working.MeshKey);
	if (mesh)
	{
		auto assets = UAssetManager::Get().GetAnimationAssets(mesh);
		if (assets.Num())
			s.AssetKey = assets[0]->GetPath();
	}
	Working.States.push_back(s);
	Selected = s.Id;
	SelectedTransition = -1;
	if (Working.Entry == -1)
		Working.Entry = s.Id;
	Dirty = true;
}
void FStateMachineEditorPanel::DeleteSelection()
{
	if (SelectedTransition >= 0 && SelectedTransition < (int)Working.Transitions.size())
	{
		Working.Transitions.erase(Working.Transitions.begin() + SelectedTransition);
		SelectedTransition = -1;
		Dirty = true;
	}
	else if (Selected >= 0)
	{
		int id = Selected;
		std::erase_if(Working.States, [&](auto &s) { return s.Id == id; });
		std::erase_if(Working.Transitions, [&](auto &t) { return t.From == id || t.To == id; });
		if (Working.Entry == id)
			Working.Entry = -1;
		Selected = -1;
		SelectedTransition = -1;
		LinkFrom = -2;
		Dirty = true;
	}
}
void FStateMachineEditorPanel::DrawGraph()
{
	ImGui::TextDisabled("Drag: move state | Middle drag: pan | Wheel: zoom | Select source, Link, then destination");
	if (ImGui::Button("Add State"))
		AddState();
	ImGui::SameLine();
	if (ImGui::Button("Link selected"))
	{
		if (Selected >= 0)
			LinkFrom = Selected;
	}
	ImGui::SameLine();
	if (ImGui::Button("AnyState link"))
		LinkFrom = -1;
	ImGui::SameLine();
	if (ImGui::Button("Delete"))
		DeleteSelection();
	if (LinkFrom != -2)
	{
		ImGui::SameLine();
		ImGui::Text("Select destination (%s)", StateName(LinkFrom));
		ImGui::SameLine();
		if (ImGui::SmallButton("Cancel link"))
			LinkFrom = -2;
	}
	ImGui::BeginChild("Canvas", ImVec2(0, 0), ImGuiChildFlags_Borders,
					  ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
	auto origin = ImGui::GetCursorScreenPos();
	auto size = ImGui::GetContentRegionAvail();
	auto *draw = ImGui::GetWindowDrawList();
	auto &io = ImGui::GetIO();
	if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		Pan.x += io.MouseDelta.x;
		Pan.y += io.MouseDelta.y;
	}
	if (ImGui::IsWindowHovered() && io.MouseWheel != 0)
	{
		float old = Zoom;
		Zoom = std::clamp(Zoom + io.MouseWheel * .1f, .35f, 2.f);
		auto m = io.MousePos;
		Pan.x = (Pan.x - (m.x - origin.x)) * Zoom / old + (m.x - origin.x);
		Pan.y = (Pan.y - (m.y - origin.y)) * Zoom / old + (m.y - origin.y);
	}
	auto pos = [&](const FAnimAssetState &s) {
		return ImVec2(origin.x + Pan.x + s.X * Zoom, origin.y + Pan.y + s.Y * Zoom);
	};
	auto nodeCenter = [&](int id) {
		auto *s = FindState(id);
		if (s)
		{
			auto p = pos(*s);
			return ImVec2(p.x + 80 * Zoom, p.y + 25 * Zoom);
		}
		return ImVec2(origin.x + Pan.x - 20, origin.y + Pan.y - 40);
	};
	draw->AddText(ImVec2(origin.x + Pan.x - 40, origin.y + Pan.y - 55), IM_COL32(210, 160, 90, 255), "AnyState");
	UAssetAnimInstance *runtime = Viewer ? Viewer->GetStateMachinePreview() : nullptr;
	int current = runtime ? runtime->GetCurrentAssetState() : -1;
	for (int i = 0; i < (int)Working.Transitions.size(); ++i)
	{
		auto &t = Working.Transitions[i];
		auto a = nodeCenter(t.From), b = nodeCenter(t.To);
		float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
		if (len < 1)
		{
			draw->AddCircle(ImVec2(a.x, a.y - 30 * Zoom), 30 * Zoom, IM_COL32(180, 180, 180, 255));
			b = ImVec2(a.x + 25 * Zoom, a.y - 45 * Zoom);
		}
		ImVec2 mid = len < 1 ? ImVec2(a.x, a.y - 70 * Zoom)
            : ImVec2((a.x + b.x) * .5f - dy / len * 22, (a.y + b.y) * .5f + dx / len * 22);
		bool active = runtime && runtime->GetActiveTransitionIndex() == i;
		ImU32 color = active ? IM_COL32(80, 230, 130, 255)
							 : (i == SelectedTransition ? IM_COL32(255, 190, 70, 255) : IM_COL32(150, 160, 185, 255));
		draw->AddLine(a, mid, color, 2);
		draw->AddLine(mid, b, color, 2);
		float angle = std::atan2(b.y - mid.y, b.x - mid.x);
		draw->AddTriangleFilled(mid, ImVec2(mid.x - 12 * std::cos(angle - .5f), mid.y - 12 * std::sin(angle - .5f)),
								ImVec2(mid.x - 12 * std::cos(angle + .5f), mid.y - 12 * std::sin(angle + .5f)), color);
		ImGui::SetCursorScreenPos(ImVec2(mid.x - 9, mid.y - 9));
		ImGui::PushID(100000 + i);
		if (ImGui::InvisibleButton("edge", ImVec2(18, 18)))
		{
			SelectedTransition = i;
			Selected = -1;
		}
		ImGui::PopID();
	}
	for (auto &s : Working.States)
	{
		auto p = pos(s);
		ImVec2 end(p.x + 160 * Zoom, p.y + 50 * Zoom);
		ImU32 color = s.Id == current ? IM_COL32(45, 125, 75, 255) : IM_COL32(50, 60, 85, 255);
		draw->AddRectFilled(p, end, color, 5);
		draw->AddRect(p, end, s.Id == Selected ? IM_COL32(255, 190, 70, 255) : IM_COL32(110, 120, 150, 255), 5.0f, 2.0f, ImDrawFlags_None);
		draw->AddText(ImVec2(p.x + 8, p.y + 6), IM_COL32(255, 255, 255, 255), s.Name.c_str());
		draw->AddText(ImVec2(p.x + 8, p.y + 25), IM_COL32(180, 195, 210, 255),
					  s.BlendSpace ? "BlendSpace" : "Sequence");
		if (s.Id == Working.Entry)
		{
			draw->AddText(ImVec2(p.x, p.y - 20), IM_COL32(110, 210, 255, 255), "Entry");
		}
		ImGui::SetCursorScreenPos(p);
		ImGui::PushID(s.Id);
		if (ImGui::InvisibleButton("state", ImVec2(160 * Zoom, 50 * Zoom)))
		{
			if (LinkFrom != -2)
			{
				FAnimAssetTransition t;
				t.From = LinkFrom;
				t.To = s.Id;
				FAnimConditionDefinition c;
				c.Type = EAnimConditionType::Always;
				t.Options.Condition = (int)Working.Conditions.size();
				Working.Conditions.push_back(c);
				Working.Transitions.push_back(t);
				SelectedTransition = (int)Working.Transitions.size() - 1;
				Selected = -1;
				LinkFrom = -2;
				Dirty = true;
			}
			else
			{
				Selected = s.Id;
				SelectedTransition = -1;
			}
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			s.X += io.MouseDelta.x / Zoom;
			s.Y += io.MouseDelta.y / Zoom;
			Dirty = true;
		}
		ImGui::PopID();
	}
	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy(size);
	ImGui::EndChild();
}
void FStateMachineEditorPanel::DrawDetails()
{
	if (auto *s = FindState(Selected))
	{
		ImGui::TextUnformatted("State");
		Dirty |= TextEdit("Name", s->Name);
		if (ImGui::Button("Set as Entry"))
		{
			Working.Entry = s->Id;
			Dirty = true;
		}
		if (ImGui::Checkbox("Use BlendSpace", &s->BlendSpace))
		{
			s->AssetKey.clear();
			Dirty = true;
		}
		auto *mesh = UAssetManager::GetAssetByPath<USkeletalMesh>(Working.MeshKey);
		if (ImGui::BeginCombo("Animation", s->AssetKey.c_str()))
		{
			if (s->BlendSpace)
			{
				for (auto *a : UAssetManager::Get().GetBlendSpaceAssets(mesh))
					if (ImGui::Selectable(a->GetPath().c_str(), a->GetPath() == s->AssetKey))
					{
						s->AssetKey = a->GetPath();
						Dirty = true;
					}
			}
			else
			{
				for (auto *a : UAssetManager::Get().GetAnimationAssets(mesh))
					if (ImGui::Selectable(a->GetPath().c_str(), a->GetPath() == s->AssetKey))
					{
						s->AssetKey = a->GetPath();
						Dirty = true;
					}
			}
			ImGui::EndCombo();
		}
		Dirty |= ImGui::Checkbox("Loop", &s->Options.bLoop);
		Dirty |= ImGui::Checkbox("Playing", &s->Options.bPlaying);
		Dirty |= ImGui::InputFloat("Play rate", &s->Options.PlayRate);
		if (s->BlendSpace && ImGui::BeginCombo("Axis parameter",
											   s->BlendParameter.empty() ? "Constant zero" : s->BlendParameter.c_str()))
		{
			if (ImGui::Selectable("Constant zero", s->BlendParameter.empty()))
			{
				s->BlendParameter.clear();
				Dirty = true;
			}
			for (auto &p : Working.Parameters)
				if (p.Type == EAnimParameterType::Float &&
					ImGui::Selectable(p.Name.ToString().c_str(), s->BlendParameter == p.Name.ToString()))
				{
					s->BlendParameter = p.Name.ToString();
					Dirty = true;
				}
			ImGui::EndCombo();
		}
	}
	else if (SelectedTransition >= 0 && SelectedTransition < (int)Working.Transitions.size())
	{
		auto &t = Working.Transitions[SelectedTransition];
		auto &o = t.Options;
		ImGui::TextUnformatted("Transition");
		Dirty |= StateCombo("From", t.From, true);
		Dirty |= StateCombo("To", t.To);
		Dirty |= ImGui::InputInt("Condition index", &o.Condition);
		Dirty |= ImGui::InputInt("Priority", &o.Priority);
		Dirty |= ImGui::InputFloat("Blend seconds", &o.Blend.Duration);
		int curve = (int)o.Blend.Curve;
		if (EnumEdit("Curve", curve, "Linear\0SmoothStep\0"))
		{
			o.Blend.Curve = (EAnimBlendCurve)curve;
			Dirty = true;
		}
		int policy = (int)o.InterruptPolicy;
		if (EnumEdit("Interrupt", policy, "Never\0HigherPriorityOnly\0Always\0"))
		{
			o.InterruptPolicy = (EAnimTransitionInterruptPolicy)policy;
			Dirty = true;
		}
		Dirty |= ImGui::Checkbox("Restart same state", &o.bRestartIfSameState);
		ImGui::TextUnformatted("Consume triggers");
		for (auto &p : Working.Parameters)
			if (p.Type == EAnimParameterType::Trigger)
			{
				bool enabled = false;
				for (auto n : o.ConsumeTriggers)
					enabled |= n == p.Name;
				if (ImGui::Checkbox(p.Name.ToString().c_str(), &enabled))
				{
					if (enabled)
						o.ConsumeTriggers.Add(p.Name);
					else
					{
						TArray<FName> next;
						for (auto n : o.ConsumeTriggers)
							if (n != p.Name)
								next.Add(n);
						o.ConsumeTriggers = std::move(next);
					}
					Dirty = true;
				}
			}
	}
	else
		ImGui::TextDisabled("Select a state or transition.");
}
void FStateMachineEditorPanel::DrawParameters()
{
	if (ImGui::Button("Add Parameter"))
	{
		FAnimParameter p;
		p.Name = FName(("Parameter_" + std::to_string(Working.Parameters.size() + 1)).c_str());
		Working.Parameters.push_back(p);
		Dirty = true;
	}
	for (int i = 0; i < (int)Working.Parameters.size(); ++i)
	{
		auto &p = Working.Parameters[i];
		ImGui::PushID(i);
		Dirty |= NameEdit("Name", p.Name);
		int type = (int)p.Type;
		if (EnumEdit("Type", type, "Bool\0Float\0Trigger\0"))
		{
			p.Type = (EAnimParameterType)type;
			Dirty = true;
		}
		if (p.Type == EAnimParameterType::Bool)
			Dirty |= ImGui::Checkbox("Default", &p.BoolValue);
		if (p.Type == EAnimParameterType::Float)
			Dirty |= ImGui::InputFloat("Default", &p.FloatValue);
		bool remove = ImGui::SmallButton("Remove parameter");
		ImGui::Separator();
		ImGui::PopID();
		if (remove)
		{
			Working.Parameters.erase(Working.Parameters.begin() + i);
			Dirty = true;
			break;
		}
	}
}
void FStateMachineEditorPanel::DrawConditions()
{
	ImGui::TextWrapped("Conditions use indices. AND/OR/NOT may reference earlier conditions only.");
	if (ImGui::Button("Add Condition"))
	{
		Working.Conditions.emplace_back();
		Dirty = true;
	}
	for (int i = 0; i < (int)Working.Conditions.size(); ++i)
	{
		ImGui::PushID(i);
		auto &c = Working.Conditions[i];
		auto label = "Condition " + std::to_string(i);
		if (ImGui::TreeNode(label.c_str()))
		{
			int type = (int)c.Type;
			if (EnumEdit("Type", type,
						 "Always\0BoolEquals\0FloatCompare\0TriggerActive\0SourceFinished\0StateElapsedCompare\0And\0Or"
						 "\0Not\0"))
			{
				c.Type = (EAnimConditionType)type;
				c.Children.Reset();
				Dirty = true;
			}
			if (c.Type == EAnimConditionType::BoolEquals || c.Type == EAnimConditionType::FloatCompare ||
				c.Type == EAnimConditionType::TriggerActive)
				Dirty |= NameEdit("Parameter", c.ParameterName);
			if (c.Type == EAnimConditionType::BoolEquals)
				Dirty |= ImGui::Checkbox("Equals", &c.BoolValue);
			if (c.Type == EAnimConditionType::FloatCompare || c.Type == EAnimConditionType::StateElapsedCompare)
			{
				int op = (int)c.CompareOp;
				if (EnumEdit("Compare", op, "Less\0LessEqual\0Greater\0GreaterEqual\0NearlyEqual\0NotNearlyEqual\0"))
				{
					c.CompareOp = (EAnimCompareOp)op;
					Dirty = true;
				}
				Dirty |= ImGui::InputFloat("Value", &c.FloatValue);
				Dirty |= ImGui::InputFloat("Tolerance", &c.Tolerance);
			}
			if (c.Type == EAnimConditionType::And || c.Type == EAnimConditionType::Or ||
				c.Type == EAnimConditionType::Not)
			{
				for (int n = 0; n < i; ++n)
				{
					bool used = false;
					for (int child : c.Children)
						used |= child == n;
					auto l = "Child " + std::to_string(n);
					if (ImGui::Checkbox(l.c_str(), &used))
					{
						TArray<int> next;
						for (int child : c.Children)
							if (child != n && !(used && c.Type == EAnimConditionType::Not))
								next.Add(child);
						if (used)
							next.Add(n);
						c.Children = std::move(next);
						Dirty = true;
					}
				}
			}
			if (ImGui::SmallButton("Remove condition"))
			{
				bool referenced = false;
				for (auto &t : Working.Transitions)
					referenced |= t.Options.Condition == i;
				for (auto &condition : Working.Conditions)
					for (int n : condition.Children)
						referenced |= n == i;
				if (referenced)
					Error = "Condition is referenced. Change its references before removing it.";
				else
				{
					Working.Conditions.erase(Working.Conditions.begin() + i);
					for (auto &t : Working.Transitions)
						if (t.Options.Condition > i)
							--t.Options.Condition;
					for (auto &condition : Working.Conditions)
						for (auto &n : condition.Children)
							if (n > i)
								--n;
					Dirty = true;
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}
void FStateMachineEditorPanel::DrawPreview()
{
	if (ImGui::Button("Apply to 3D Preview") && Viewer)
	{
		if (Working.Validate(Error))
		{
			Viewer->PreviewStateMachine(Working, Error);
		}
	}
	auto *instance = Viewer ? Viewer->GetStateMachinePreview() : nullptr;
	if (!instance)
		return;
	ImGui::SameLine();
	if (ImGui::Button(instance->IsPaused() ? "Play" : "Pause"))
		instance->SetPaused(!instance->IsPaused());
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
	{
		auto data = instance->GetAssetData();
		instance->Configure(data, false);
	}
	ImGui::Text("Current state ID: %d", instance->GetCurrentAssetState());
	if (instance->IsTransitioning())
	{
		int idx = instance->GetActiveTransitionIndex();
		const auto &data = instance->GetAssetData();
		if (idx >= 0 && idx < (int)data.Transitions.size())
		{
			const auto &t = data.Transitions[idx];
			ImGui::Text("Transition: %d -> %d", t.From, t.To);
		}
		ImGui::ProgressBar(instance->GetTransitionProgress());
	}
	for (auto &p : instance->GetAssetData().Parameters)
	{
		ImGui::PushID(p.Name.ToString().c_str());
		auto name = p.Name.ToString();
		if (p.Type == EAnimParameterType::Float)
		{
			float v = 0;
			instance->TryGetFloat(name, v);
			if (ImGui::InputFloat(name.c_str(), &v))
				instance->SetFloat(name, v);
		}
		else if (p.Type == EAnimParameterType::Bool)
		{
			bool v = false;
			instance->TryGetBool(name, v);
			if (ImGui::Checkbox(name.c_str(), &v))
				instance->SetBool(name, v);
		}
		else if (ImGui::Button(name.c_str()))
			instance->SetTrigger(name);
		ImGui::PopID();
	}
	ImGui::TextDisabled("Manual parameters only; Lua update is disabled in preview.");
}
void FStateMachineEditorPanel::OnRender()
{
	if (!IsOpen())
		return;
	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
	bool visible = ImGui::Begin(GetPanelName(), &open, ImGuiWindowFlags_NoCollapse);
	Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (!open)
		SetOpen(false);
	if (visible)
	{
		if (ImGui::Button("Save"))
			Save();
		ImGui::SameLine();
		ImGui::Text("%s%s", Path.empty() ? "New state machine" : Path.c_str(), Dirty ? " *" : "");
		if (Path.empty())
			ImGui::InputText("Save path", SavePath, sizeof(SavePath));
		if (ImGui::BeginCombo("Skeletal Mesh", Working.MeshKey.c_str()))
		{
			for (auto *m : UAssetManager::Get().GetSkeletalMeshAssets())
				if (ImGui::Selectable(m->GetPath().c_str(), Working.MeshKey == m->GetPath()))
				{
					Working.MeshKey = m->GetPath();
					Dirty = true;
				}
			ImGui::EndCombo();
		}
		Dirty |= TextEdit("Optional update script", Working.UpdateScript);
		if (!Error.empty())
			ImGui::TextWrapped("%s", Error.c_str());
		if (ImGui::BeginTable("Layout", 2, ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch, 2);
			ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch, 1);
			ImGui::TableNextColumn();
			DrawGraph();
			ImGui::TableNextColumn();
			ImGui::BeginChild("Properties");
			if (ImGui::CollapsingHeader("Selection", ImGuiTreeNodeFlags_DefaultOpen))
				DrawDetails();
			if (ImGui::CollapsingHeader("Parameters"))
			{
				ImGui::PushID("Parameters");
				DrawParameters();
				ImGui::PopID();
			}
			if (ImGui::CollapsingHeader("Conditions"))
			{
				ImGui::PushID("Conditions");
				DrawConditions();
				ImGui::PopID();
			}
			if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen))
				DrawPreview();
			ImGui::EndChild();
			ImGui::EndTable();
		}
		if (Focused && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete))
			DeleteSelection();
		if (Focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
			Save();
	}
	if (Dirty && (PendingClose || !PendingPath.empty() || !PendingFolder.empty()))
		ImGui::OpenPopup("Unsaved state machine");
	if (ImGui::BeginPopupModal("Unsaved state machine", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Save changes before continuing?");
		auto finish = [&]() {
			if (PendingClose)
			{
				PendingClose = false;
				IEditorPanel::SetOpen(false);
				if (Viewer)
					Viewer->StopStateMachinePreview();
			}
			else
				LoadPending();
			ImGui::CloseCurrentPopup();
		};
		if (ImGui::Button("Save and continue"))
		{
			if (Save())
				finish();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard"))
		{
			Dirty = false;
			finish();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			PendingClose = false;
			PendingPath.clear();
			PendingFolder.clear();
			ImGui::CloseCurrentPopup();
		}
		if (!Error.empty())
			ImGui::TextWrapped("%s", Error.c_str());
		ImGui::EndPopup();
	}
	ImGui::End();
}
