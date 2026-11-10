#include "EnginePCH.h"
#include "StateMachineEditorPanel.h"
#include "Animation/AssetAnimInstance.h"
#include "Asset/AssetManager.h"
#include "Animation/AnimSequence.h"
#include "Render/Mesh.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshViewerPanel.h"
#include "imgui_node_editor.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace ed = ax::NodeEditor;
namespace
{
	enum EGraphKind : uintptr_t
	{
		StateNode = 1,
		SpecialNode,
		TransitionNode,
		StatePin,
		TransitionPin,
		TransitionLink,
		EntryLink,
		RuleNode,
		RulePin,
		RuleLink,
		SpecialPin
	};
	uintptr_t Key(EGraphKind Kind, int Id = 0, int Slot = 0)
	{
		return (uintptr_t(Kind) << 56) | (uintptr_t(uint32_t(Id)) << 16) | uint16_t(Slot);
	}
	int Kind(uintptr_t Id)
	{
		return int(Id >> 56);
	}
	int Object(uintptr_t Id)
	{
		return int((Id >> 16) & 0xffffffff);
	}
	int Slot(uintptr_t Id)
	{
		return int(Id & 0xffff);
	}
	ed::NodeId Node(EGraphKind k, int id = 0)
	{
		return ed::NodeId(Key(k, id));
	}
	ed::PinId Pin(EGraphKind k, int id = 0, int slot = 0)
	{
		return ed::PinId(Key(k, id, slot));
	}
	const char* TypeName(EAnimConditionType Type)
	{
		switch (Type)
		{
		case EAnimConditionType::Always:
			return "Bool Constant";
		case EAnimConditionType::BoolEquals:
			return "Bool Compare";
		case EAnimConditionType::FloatCompare:
			return "Float Compare";
		case EAnimConditionType::TriggerActive:
			return "Trigger Active";
		case EAnimConditionType::SourceFinished:
			return "Source Finished";
		case EAnimConditionType::StateElapsedCompare:
			return "State Elapsed";
		case EAnimConditionType::And:
			return "AND";
		case EAnimConditionType::Or:
			return "OR";
		case EAnimConditionType::Not:
			return "NOT";
		default:
			return "Invalid";
		}
	}
	void PinLabel(ed::PinId Id, ed::PinKind Kind, const char* Text)
	{
		ed::BeginPin(Id, Kind);
		ImGui::TextColored(ImVec4(.55f, .85f, 1, 1), "%s %s", Kind == ed::PinKind::Input ? ">" : "o", Text);
		ed::EndPin();
	}
	void Place(std::set<uintptr_t>& Placed, ed::NodeId Id, float X, float Y)
	{
		if (Placed.insert(Id.Get()).second)
			ed::SetNodePosition(Id, ImVec2(X, Y));
	}
	bool Capture(ed::NodeId Id, float& X, float& Y)
	{
		auto p = ed::GetNodePosition(Id);
		if (std::abs(p.x - std::floor(X)) > .01f || std::abs(p.y - std::floor(Y)) > .01f)
		{
			X = p.x;
			Y = p.y;
			return true;
		}
		return false;
	}
} // namespace
void FStateMachineEditorPanel::ClearContexts()
{
	for (auto& [id, s] : Sessions)
	{
		if (ed::GetCurrentEditor() == s.Context)
			ed::SetCurrentEditor(nullptr);
		ed::DestroyEditor(s.Context);
	}
	Sessions.clear();
	CreateFromPin = ContextObject = 0;
}
FStateMachineEditorPanel::FGraphSession& FStateMachineEditorPanel::GraphSession()
{
	auto& session = Sessions[ActiveRule];
	if (!session.Context)
	{
		ed::Config config;
		config.SettingsFile = nullptr;
		config.NavigateButtonIndex = 2;
		config.ContextMenuButtonIndex = 1;
		config.EnableSmoothZoom = true;
		session.Context = ed::CreateEditor(&config);
	}
	return session;
}
void FStateMachineEditorPanel::AddTransition(int From, int To)
{
	if (!FindState(To) || (From != -1 && !FindState(From)))
		return;
	FAnimAssetTransition t;
	t.From = From;
	t.To = To;
	int next = 1;
	for (auto& existing : Working.Transitions)
		next = std::max(next, existing.Id + 1);
	t.Id = next;
	t.Rule = FAnimRuleGraph::Disabled();
	Working.Transitions.push_back(t);
	Working.EnsureRuleGraphs();
	Selected = -1;
	SelectedTransition = (int)Working.Transitions.size() - 1;
	Dirty = true;
}
void FStateMachineEditorPanel::DeleteSelection()
{
	auto& session = GraphSession();
	auto* previous = ed::GetCurrentEditor();
	ed::SetCurrentEditor(session.Context);
	std::vector<ed::NodeId> nodes(ed::GetSelectedObjectCount());
	int count = ed::GetSelectedNodes(nodes.data(), (int)nodes.size());
	for (int i = 0; i < count; ++i)
		ed::DeleteNode(nodes[i]);
	std::vector<ed::LinkId> links(ed::GetSelectedObjectCount());
	count = ed::GetSelectedLinks(links.data(), (int)links.size());
	for (int i = 0; i < count; ++i)
		ed::DeleteLink(links[i]);
	ed::SetCurrentEditor(previous);
}
void FStateMachineEditorPanel::DrawGraph()
{
	if (ActiveRule && !FindTransition(ActiveRule))
		ActiveRule = 0;
	if (ImGui::Button("State Machine"))
	{
		ActiveRule = 0;
		SelectedRuleNode = 0;
	}
	if (auto* t = FindTransition(ActiveRule))
	{
		ImGui::SameLine();
		FString title = FString(" > ") + StateName(t->From) + " -> " + StateName(t->To);
		ImGui::TextUnformatted(title.c_str());
	}
	auto& session = GraphSession();
	auto* previous = ed::GetCurrentEditor();
	ed::SetCurrentEditor(session.Context);
	if (ImGui::Button("Frame All"))
		session.Fit = true;
	ImGui::SameLine();
	if (ImGui::Button("Frame Selection"))
		ed::NavigateToSelection();
	ImGui::SameLine();
	if (ImGui::Button("Delete Selection"))
		DeleteSelection();
	ImGui::TextDisabled("Drag pins to connect | Middle drag: pan | Wheel: zoom | Right click: actions");
	ed::EnableShortcuts(Focused && !ImGui::GetIO().WantTextInput);
	ed::Begin(ActiveRule ? "Transition Rule" : "State Machine Canvas",
	          ImVec2(0, std::max(260.0f, ImGui::GetContentRegionAvail().y)));
	if (ActiveRule)
		DrawRuleGraph(session);
	else
		DrawStateGraph(session);
	if (session.Fit)
	{
		ed::NavigateToContent(0);
		session.Fit = false;
	}
	ed::End();
	ed::SetCurrentEditor(previous);
}
void FStateMachineEditorPanel::DrawStateGraph(FGraphSession& session)
{
	auto* runtime = Viewer ? Viewer->GetStateMachinePreview() : nullptr;
	int current = runtime ? runtime->GetCurrentAssetState() : -1;
	int activeTransition = 0;
	if (runtime)
	{
		int index = runtime->GetActiveTransitionIndex();
		const auto& data = runtime->GetAssetData();
		if (index >= 0 && index < (int)data.Transitions.size())
			activeTransition = data.Transitions[index].Id;
	}
	for (int special = 0; special < 2; ++special)
	{
		auto id = Node(SpecialNode, special);
		Place(session.Placed, id, special ? Working.AnyX : Working.EntryX, special ? Working.AnyY : Working.EntryY);
		ed::PushStyleColor(ed::StyleColor_NodeBg, special ? ImVec4(.3f, .19f, .1f, 1) : ImVec4(.12f, .25f, .32f, 1));
		ed::BeginNode(id);
		ImGui::TextUnformatted(special ? "AnyState" : "Entry");
		PinLabel(Pin(SpecialPin, special), ed::PinKind::Output, "Transition");
		ed::EndNode();
		ed::PopStyleColor();
	}
	for (auto& state : Working.States)
	{
		auto id = Node(StateNode, state.Id);
		Place(session.Placed, id, state.X, state.Y);
		if (state.Id == current)
			ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(.13f, .36f, .22f, 1));
		ed::BeginNode(id);
		ImGui::TextUnformatted(state.Name.c_str());
		ImGui::Separator();
		ImGui::TextDisabled("%s", state.BlendSpace ? "BlendSpace" : "Sequence");
		FString asset = state.AssetKey;
		auto slash = asset.find_last_of("/\\");
		if (slash != FString::npos)
			asset = asset.substr(slash + 1);
		ImGui::TextUnformatted(asset.empty() ? "Select animation in Details" : asset.c_str());
		PinLabel(Pin(StatePin, state.Id, 1), ed::PinKind::Input, "In");
		ImGui::SameLine(120);
		PinLabel(Pin(StatePin, state.Id, 0), ed::PinKind::Output, "Out");
		ed::EndNode();
		if (state.Id == current)
			ed::PopStyleColor();
	}
	auto sourcePin = [&](int from)
	{ return from == -1 ? Pin(SpecialPin, 1) : Pin(StatePin, from, 0); };
	if (FindState(Working.Entry))
		ed::Link(ed::LinkId(Key(EntryLink)), Pin(SpecialPin, 0), Pin(StatePin, Working.Entry, 1),
		         ImVec4(.3f, .7f, 1, 1), 2);
	for (auto& t : Working.Transitions)
	{
		auto id = Node(TransitionNode, t.Id);
		Place(session.Placed, id, t.X, t.Y);
		bool active = activeTransition == t.Id;
		ed::PushStyleColor(ed::StyleColor_NodeBg, active ? ImVec4(.12f, .4f, .22f, 1) : ImVec4(.24f, .22f, .14f, 1));
		ed::BeginNode(id);
		FString summary = t.Rule.Summary();
		if (summary.size() > 54)
			summary = summary.substr(0, 51) + "...";
		ImGui::TextUnformatted(summary.c_str());
		PinLabel(Pin(TransitionPin, t.Id, 0), ed::PinKind::Input, "");
		ImGui::SameLine();
		PinLabel(Pin(TransitionPin, t.Id, 1), ed::PinKind::Output, "");
		ed::EndNode();
		ed::PopStyleColor();
		ImVec4 color = active ? ImVec4(.3f, 1, .5f, 1) : ImVec4(.7f, .75f, .85f, 1);
		auto left = ed::LinkId(Key(TransitionLink, t.Id, 0)), right = ed::LinkId(Key(TransitionLink, t.Id, 1));
		ed::Link(left, sourcePin(t.From), Pin(TransitionPin, t.Id, 0), color, 2);
		ed::Link(right, Pin(TransitionPin, t.Id, 1), Pin(StatePin, t.To, 1), color, 2);
		if (active)
		{
			ed::Flow(left);
			ed::Flow(right);
		}
	}
	auto selectTransition = [&](int id)
	{
		for (int i = 0; i < (int)Working.Transitions.size(); ++i)
			if (Working.Transitions[i].Id == id)
			{
				SelectedTransition = i;
				Selected = -1;
				return;
			}
	};
	if (ed::HasSelectionChanged())
	{
		int count = ed::GetSelectedObjectCount();
		std::vector<ed::NodeId> nodes(count);
		int n = ed::GetSelectedNodes(nodes.data(), count);
		Selected = -1;
		SelectedTransition = -1;
		if (n)
		{
			auto value = nodes[n - 1].Get();
			if (Kind(value) == StateNode)
				Selected = Object(value);
			else if (Kind(value) == TransitionNode)
				selectTransition(Object(value));
		}
		std::vector<ed::LinkId> links(count);
		n = ed::GetSelectedLinks(links.data(), count);
		if (n && Kind(links[n - 1].Get()) == TransitionLink)
			selectTransition(Object(links[n - 1].Get()));
	}
	auto doubleNode = ed::GetDoubleClickedNode();
	auto doubleLink = ed::GetDoubleClickedLink();
	int openRule = 0;
	if (Kind(doubleNode.Get()) == TransitionNode)
		openRule = Object(doubleNode.Get());
	if (Kind(doubleLink.Get()) == TransitionLink)
		openRule = Object(doubleLink.Get());
	if (ed::BeginCreate())
	{
		ed::PinId a, b;
		if (ed::QueryNewLink(&a, &b))
		{
			auto from = a.Get(), to = b.Get();
			if (Kind(to) == SpecialPin || (Kind(to) == StatePin && Slot(to) == 0))
				std::swap(from, to);
			bool validSource = Kind(from) == SpecialPin || (Kind(from) == StatePin && Slot(from) == 0);
			bool valid = validSource && Kind(to) == StatePin && Slot(to) == 1;
			if (valid)
			{
				if (ed::AcceptNewItem())
				{
					int target = Object(to);
					if (Kind(from) == SpecialPin && Object(from) == 0)
					{
						Working.Entry = target;
						Dirty = true;
					}
					else
						AddTransition(Kind(from) == SpecialPin ? -1 : Object(from), target);
				}
			}
			else
				ed::RejectNewItem(ImVec4(1, .3f, .3f, 1));
		}
		ed::PinId pin;
		if (ed::QueryNewNode(&pin))
		{
			if (Kind(pin.Get()) == StatePin || Kind(pin.Get()) == SpecialPin)
			{
				if (ed::AcceptNewItem())
				{
					CreateFromPin = pin.Get();
					CreationPosition = ed::ScreenToCanvas(ImGui::GetMousePos());
					ed::Suspend();
					ImGui::OpenPopup("Create State");
					ed::Resume();
				}
			}
			else
				ed::RejectNewItem();
		}
	}
	// BeginCreate marks the action active even when it returns false.
	ed::EndCreate();
	auto removeTransition = [&](int id)
	{
		session.Placed.erase(Key(TransitionNode, id));
		auto it = Sessions.find(id);
		if (it != Sessions.end())
		{
			ed::DestroyEditor(it->second.Context);
			Sessions.erase(it);
		}
		std::erase_if(Working.Transitions, [&](auto& t)
		              { return t.Id == id; });
		SelectedTransition = -1;
		Dirty = true;
	};
	if (ed::BeginDelete())
	{
		ed::LinkId link;
		while (ed::QueryDeletedLink(&link))
		{
			auto value = link.Get();
			if (Kind(value) == EntryLink)
			{
				if (ed::AcceptDeletedItem())
				{
					Working.Entry = -1;
					Dirty = true;
				}
			}
			else if (Kind(value) == TransitionLink)
			{
				if (ed::AcceptDeletedItem())
					removeTransition(Object(value));
			}
			else
				ed::RejectDeletedItem();
		}
		ed::NodeId node;
		while (ed::QueryDeletedNode(&node))
		{
			auto value = node.Get();
			if (Kind(value) == SpecialNode)
				ed::RejectDeletedItem();
			else if (ed::AcceptDeletedItem())
			{
				if (Kind(value) == TransitionNode)
					removeTransition(Object(value));
				else if (Kind(value) == StateNode)
				{
					int id = Object(value);
					std::vector<int> dependencies;
					for (auto& t : Working.Transitions)
						if (t.From == id || t.To == id)
							dependencies.push_back(t.Id);
					for (int t : dependencies)
						removeTransition(t);
					std::erase_if(Working.States, [&](auto& s)
					              { return s.Id == id; });
					if (Working.Entry == id)
						Working.Entry = -1;
					Selected = -1;
					Dirty = true;
				}
			}
		}
	}
	ed::EndDelete();
	ed::Suspend();
	ed::NodeId menuNode;
	ed::LinkId menuLink;
	if (ed::ShowNodeContextMenu(&menuNode))
	{
		ContextObject = menuNode.Get();
		ImGui::OpenPopup("State Actions");
	}
	if (ed::ShowLinkContextMenu(&menuLink))
	{
		ContextObject = menuLink.Get();
		ImGui::OpenPopup("State Actions");
	}
	if (ed::ShowBackgroundContextMenu())
	{
		CreateFromPin = 0;
		CreationPosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		ImGui::OpenPopup("Create State");
	}
	if (ImGui::BeginPopup("Create State"))
	{
		if (ImGui::MenuItem("Add State"))
		{
			AddState(CreationPosition);
			int target = Selected;
			if (CreateFromPin)
			{
				auto p = CreateFromPin;
				if (Kind(p) == SpecialPin && Object(p) == 0)
				{
					Working.Entry = target;
					Dirty = true;
				}
				else if (Kind(p) == SpecialPin)
					AddTransition(-1, target);
				else if (Slot(p) == 0)
					AddTransition(Object(p), target);
				else
					AddTransition(target, Object(p));
			}
			CreateFromPin = 0;
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup("State Actions"))
	{
		int kind = Kind(ContextObject), id = Object(ContextObject);
		bool transition = kind == TransitionNode || kind == TransitionLink;
		if (transition && ImGui::MenuItem("Edit Transition Rule"))
			openRule = id;
		if (kind == StateNode && ImGui::MenuItem("Set as Entry"))
		{
			Working.Entry = id;
			Dirty = true;
		}
		if (kind != SpecialNode && ImGui::MenuItem("Delete"))
		{
			if (kind == TransitionLink || kind == EntryLink)
				ed::DeleteLink(ed::LinkId(ContextObject));
			else
				ed::DeleteNode(ed::NodeId(ContextObject));
		}
		ImGui::EndPopup();
	}
	ed::Resume();
	for (auto& s : Working.States)
		if (session.Placed.contains(Key(StateNode, s.Id)))
			Dirty |= Capture(Node(StateNode, s.Id), s.X, s.Y);
	for (auto& t : Working.Transitions)
		if (session.Placed.contains(Key(TransitionNode, t.Id)))
			Dirty |= Capture(Node(TransitionNode, t.Id), t.X, t.Y);
	Dirty |= Capture(Node(SpecialNode, 0), Working.EntryX, Working.EntryY);
	Dirty |= Capture(Node(SpecialNode, 1), Working.AnyX, Working.AnyY);
	if (openRule && FindTransition(openRule))
	{
		selectTransition(openRule);
		ActiveRule = openRule;
		SelectedRuleNode = 0;
	}
}
void FStateMachineEditorPanel::DrawRuleGraph(FGraphSession& session)
{
	auto* transition = FindTransition(ActiveRule);
	if (!transition)
		return;
	auto& graph = transition->Rule;
	FString compileError;
	std::vector<FAnimConditionDefinition> temporary;
	int root = -1, errorNode = 0;
	graph.Compile(Working.Parameters, temporary, root, compileError, &errorNode);
	for (auto& n : graph.Nodes)
	{
		auto id = Node(RuleNode, n.Id);
		Place(session.Placed, id, n.X, n.Y);
		bool invalid = n.Id == errorNode;
		ed::PushStyleColor(ed::StyleColor_NodeBg, n.Result ? ImVec4(.28f, .13f, .14f, 1) : ImVec4(.13f, .21f, .29f, 1));
		if (invalid)
			ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(1, .25f, .2f, 1));
		ed::BeginNode(id);
		ImGui::PushID(n.Id);
		ImGui::TextUnformatted(n.Result ? "Can Enter Transition" : TypeName(n.Condition.Type));
		ImGui::Separator();
		ImGui::PushItemWidth(180);
		if (!n.Result)
			DrawRuleProperties(n, true);
		ImGui::PopItemWidth();
		for (int slot = 0; slot < n.InputCount; ++slot)
		{
			auto label = "Input " + std::to_string(slot + 1);
			PinLabel(Pin(RulePin, n.Id, slot + 1), ed::PinKind::Input, n.Result ? "Result" : label.c_str());
		}
		if (!n.Result)
			PinLabel(Pin(RulePin, n.Id, 0), ed::PinKind::Output, "Bool");
		if (invalid)
			ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "%s", compileError.c_str());
		ImGui::PopID();
		ed::EndNode();
		if (invalid)
			ed::PopStyleColor();
		ed::PopStyleColor();
	}
	for (auto& link : graph.Links)
		ed::Link(ed::LinkId(Key(RuleLink, link.Id)), Pin(RulePin, link.From, 0), Pin(RulePin, link.To, link.Input + 1),
		         ImVec4(.45f, .85f, .55f, 1), 2);
	if (ed::HasSelectionChanged())
	{
		std::vector<ed::NodeId> nodes(ed::GetSelectedObjectCount());
		int count = ed::GetSelectedNodes(nodes.data(), (int)nodes.size());
		SelectedRuleNode = count ? Object(nodes[count - 1].Get()) : 0;
	}
	auto connect = [&](int from, int to, int slot)
	{
		if (!graph.CanConnect(from, to, slot))
			return false;
		int id = graph.NextLinkId();
		std::erase_if(graph.Links, [&](auto& l)
		              { return l.To == to && l.Input == slot; });
		graph.Links.push_back({ id, from, to, slot });
		Dirty = true;
		return true;
	};
	if (ed::BeginCreate())
	{
		ed::PinId a, b;
		if (ed::QueryNewLink(&a, &b))
		{
			auto from = a.Get(), to = b.Get();
			if (Slot(from) != 0)
				std::swap(from, to);
			bool valid = Kind(from) == RulePin && Kind(to) == RulePin && Slot(from) == 0 && Slot(to) > 0 &&
			             graph.CanConnect(Object(from), Object(to), Slot(to) - 1);
			if (valid)
			{
				if (ed::AcceptNewItem())
					connect(Object(from), Object(to), Slot(to) - 1);
			}
			else
				ed::RejectNewItem(ImVec4(1, .25f, .2f, 1));
		}
		ed::PinId pin;
		if (ed::QueryNewNode(&pin) && ed::AcceptNewItem())
		{
			CreateFromPin = pin.Get();
			CreationPosition = ed::ScreenToCanvas(ImGui::GetMousePos());
			ed::Suspend();
			ImGui::OpenPopup("Create Rule Node");
			ed::Resume();
		}
	}
	// BeginCreate marks the action active even when it returns false.
	ed::EndCreate();
	if (ed::BeginDelete())
	{
		ed::LinkId link;
		while (ed::QueryDeletedLink(&link))
			if (ed::AcceptDeletedItem())
			{
				int id = Object(link.Get());
				std::erase_if(graph.Links, [&](auto& l)
				              { return l.Id == id; });
				Dirty = true;
			}
		ed::NodeId node;
		while (ed::QueryDeletedNode(&node))
		{
			int id = Object(node.Get());
			auto* n = graph.Find(id);
			if (!n || n->Result)
			{
				ed::RejectDeletedItem();
				continue;
			}
			if (ed::AcceptDeletedItem())
			{
				std::erase_if(graph.Links, [&](auto& l)
				              { return l.From == id || l.To == id; });
				std::erase_if(graph.Nodes, [&](auto& x)
				              { return x.Id == id; });
				session.Placed.erase(node.Get());
				if (SelectedRuleNode == id)
					SelectedRuleNode = 0;
				Dirty = true;
			}
		}
	}
	ed::EndDelete();
	ed::Suspend();
	ed::NodeId menuNode;
	ed::LinkId menuLink;
	if (ed::ShowNodeContextMenu(&menuNode))
	{
		ContextObject = menuNode.Get();
		ImGui::OpenPopup("Rule Actions");
	}
	if (ed::ShowLinkContextMenu(&menuLink))
	{
		ContextObject = menuLink.Get();
		ImGui::OpenPopup("Rule Actions");
	}
	if (ed::ShowBackgroundContextMenu())
	{
		CreateFromPin = 0;
		CreationPosition = ed::ScreenToCanvas(ImGui::GetMousePos());
		ImGui::OpenPopup("Create Rule Node");
	}
	if (ImGui::BeginPopup("Create Rule Node"))
	{
		for (int value = 0; value <= (int)EAnimConditionType::Not; ++value)
		{
			auto type = (EAnimConditionType)value;
			int inputs = (type == EAnimConditionType::And || type == EAnimConditionType::Or)
			                 ? 2
			                 : (type == EAnimConditionType::Not ? 1 : 0);
			bool allowed = !CreateFromPin || Slot(CreateFromPin) != 0 || inputs > 0;
			if (ImGui::MenuItem(TypeName(type), nullptr, false, allowed))
			{
				FAnimRuleNode n;
				n.Id = graph.NextNodeId();
				n.Condition.Type = type;
				n.Condition.BoolValue = type != EAnimConditionType::Always;
				n.InputCount = inputs;
				n.X = CreationPosition.x;
				n.Y = CreationPosition.y;
				if (type == EAnimConditionType::BoolEquals || type == EAnimConditionType::FloatCompare ||
				    type == EAnimConditionType::TriggerActive)
				{
					auto required = type == EAnimConditionType::BoolEquals
					                    ? EAnimParameterType::Bool
					                    : (type == EAnimConditionType::FloatCompare ? EAnimParameterType::Float
					                                                                : EAnimParameterType::Trigger);
					for (auto& p : Working.Parameters)
						if (p.Type == required)
						{
							n.Condition.ParameterName = p.Name;
							break;
						}
				}
				graph.Nodes.push_back(n);
				SelectedRuleNode = n.Id;
				if (CreateFromPin)
				{
					if (Slot(CreateFromPin) == 0)
						connect(Object(CreateFromPin), n.Id, 0);
					else
						connect(n.Id, Object(CreateFromPin), Slot(CreateFromPin) - 1);
				}
				CreateFromPin = 0;
				Dirty = true;
			}
		}
		ImGui::EndPopup();
	}
	if (ImGui::BeginPopup("Rule Actions"))
	{
		bool canDelete = Kind(ContextObject) == RuleLink;
		auto* n = graph.Find(Object(ContextObject));
		if (n && !n->Result)
			canDelete = true;
		if (ImGui::MenuItem("Delete", nullptr, false, canDelete))
		{
			if (Kind(ContextObject) == RuleLink)
				ed::DeleteLink(ed::LinkId(ContextObject));
			else
				ed::DeleteNode(ed::NodeId(ContextObject));
		}
		ImGui::EndPopup();
	}
	ed::Resume();
	for (auto& n : graph.Nodes)
		if (session.Placed.contains(Key(RuleNode, n.Id)))
			Dirty |= Capture(Node(RuleNode, n.Id), n.X, n.Y);
}
bool FStateMachineEditorPanel::ParameterCombo(const char* Label, FName& Name, EAnimParameterType Type, bool InCanvas)
{
	bool changed = false;
	bool valid = false;
	for (auto& p : Working.Parameters)
		valid |= p.Name == Name && p.Type == Type;
	if (!valid)
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, .4f, .3f, 1));
	// IsActive() reports focus, not whether Begin()/End() is open.
	bool suspended = InCanvas;
	if (suspended)
		ed::Suspend();
	if (ImGui::BeginCombo(Label, valid ? Name.ToString().c_str() : "Select parameter"))
	{
		for (auto& p : Working.Parameters)
			if (p.Type == Type && ImGui::Selectable(p.Name.ToString().c_str(), p.Name == Name))
			{
				Name = p.Name;
				changed = true;
			}
		ImGui::EndCombo();
	}
	if (suspended)
		ed::Resume();
	if (!valid)
		ImGui::PopStyleColor();
	return changed;
}
void FStateMachineEditorPanel::DrawRuleProperties(FAnimRuleNode& n, bool InCanvas)
{
	if (n.Result)
	{
		ImGui::TextDisabled("Connect a Bool rule to allow this transition.");
		return;
	}
	auto& c = n.Condition;
	auto type = c.Type;
	if (type == EAnimConditionType::BoolEquals || type == EAnimConditionType::FloatCompare ||
	    type == EAnimConditionType::TriggerActive)
	{
		auto required =
		    type == EAnimConditionType::BoolEquals
		        ? EAnimParameterType::Bool
		        : (type == EAnimConditionType::FloatCompare ? EAnimParameterType::Float : EAnimParameterType::Trigger);
		Dirty |= ParameterCombo("Parameter", c.ParameterName, required, InCanvas);
	}
	if (type == EAnimConditionType::Always)
		Dirty |= ImGui::Checkbox("Value", &c.BoolValue);
	if (type == EAnimConditionType::BoolEquals)
		Dirty |= ImGui::Checkbox("Equals true", &c.BoolValue);
	if (type == EAnimConditionType::FloatCompare || type == EAnimConditionType::StateElapsedCompare)
	{
		int op = (int)c.CompareOp;
		bool suspended = InCanvas;
		if (suspended)
			ed::Suspend();
		if (ImGui::Combo("Compare", &op, "<\0<=\0>\0>=\0Nearly Equal\0Not Nearly Equal\0"))
		{
			c.CompareOp = (EAnimCompareOp)op;
			Dirty = true;
		}
		if (suspended)
			ed::Resume();
		Dirty |= ImGui::InputFloat("Value", &c.FloatValue);
		if (c.CompareOp == EAnimCompareOp::NearlyEqual || c.CompareOp == EAnimCompareOp::NotNearlyEqual)
			Dirty |= ImGui::InputFloat("Tolerance", &c.Tolerance);
	}
	if (type == EAnimConditionType::And || type == EAnimConditionType::Or)
	{
		if (n.InputCount < 64 && ImGui::SmallButton("+ Input"))
		{
			++n.InputCount;
			Dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(n.InputCount <= 1);
		if (ImGui::SmallButton("- Input"))
		{
			--n.InputCount;
			if (auto* t = FindTransition(ActiveRule))
				std::erase_if(t->Rule.Links, [&](auto& l)
				              { return l.To == n.Id && l.Input >= n.InputCount; });
			Dirty = true;
		}
		ImGui::EndDisabled();
	}
}
FString FStateMachineEditorPanel::ParameterReferences(FName Name) const
{
	FString references;
	for (auto& s : Working.States)
		if (s.BlendParameter == Name.ToString())
			references += "\nBlendSpace: " + s.Name;
	for (auto& t : Working.Transitions)
	{
		bool uses = false;
		for (auto& n : t.Rule.Nodes)
		{
			auto type = n.Condition.Type;
			if (!n.Result &&
			    (type == EAnimConditionType::BoolEquals || type == EAnimConditionType::FloatCompare ||
			     type == EAnimConditionType::TriggerActive) &&
			    n.Condition.ParameterName == Name)
				uses = true;
		}
		for (auto trigger : t.Options.ConsumeTriggers)
			uses |= trigger == Name;
		if (uses)
		{
			FString from = "AnyState", to = "Missing";
			for (auto& s : Working.States)
			{
				if (s.Id == t.From)
					from = s.Name;
				if (s.Id == t.To)
					to = s.Name;
			}
			references += "\nTransition: " + from + " -> " + to;
		}
	}
	return references;
}
void FStateMachineEditorPanel::RenameParameter(FName Old, FName New)
{
	for (auto& s : Working.States)
		if (s.BlendParameter == Old.ToString())
			s.BlendParameter = New.ToString();
	for (auto& t : Working.Transitions)
	{
		for (auto& n : t.Rule.Nodes)
			if (n.Condition.ParameterName == Old)
				n.Condition.ParameterName = New;
		for (auto& trigger : t.Options.ConsumeTriggers)
			if (trigger == Old)
				trigger = New;
	}
	for (auto& c : Working.Conditions)
		if (c.ParameterName == Old)
			c.ParameterName = New;
}
void FStateMachineEditorPanel::DrawParameters()
{
	if (ImGui::Button("Add Parameter"))
	{
		int number = 1;
		FName name;
		bool exists;
		do
		{
			name = FName(("Parameter_" + std::to_string(number++)).c_str());
			exists = false;
			for (auto& p : Working.Parameters)
				exists |= p.Name == name;
		} while (exists);
		FAnimParameter p;
		p.Name = name;
		Working.Parameters.push_back(p);
		Dirty = true;
	}
	for (int i = 0; i < (int)Working.Parameters.size(); ++i)
	{
		auto& p = Working.Parameters[i];
		ImGui::PushID(i);
		char name[256];
		strncpy_s(name, p.Name.ToString().c_str(), _TRUNCATE);
		if (ImGui::InputText("Name", name, sizeof(name)))
		{
			FName next(name);
			bool duplicate = next.IsNone();
			for (int j = 0; j < (int)Working.Parameters.size(); ++j)
				if (j != i && Working.Parameters[j].Name == next)
					duplicate = true;
			if (duplicate)
				Error = "Choose a unique, nonempty parameter name.";
			else
			{
				RenameParameter(p.Name, next);
				p.Name = next;
				Dirty = true;
			}
		}
		int type = (int)p.Type;
		if (ImGui::Combo("Type", &type, "Bool\0Float\0Trigger\0"))
		{
			FString refs = ParameterReferences(p.Name);
			if (!refs.empty())
				Error = "Parameter is in use. Update these references before changing its type:" + refs;
			else
			{
				p.Type = (EAnimParameterType)type;
				Dirty = true;
			}
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
			FString refs = ParameterReferences(p.Name);
			if (!refs.empty())
				Error = "Parameter is in use. Update these references before removing it:" + refs;
			else
			{
				Working.Parameters.erase(Working.Parameters.begin() + i);
				Dirty = true;
				break;
			}
		}
	}
	ImGui::TextDisabled("Lua parameter names in script files must be updated manually after renaming.");
}
void FStateMachineEditorPanel::DrawRuleErrors()
{
	bool any = false;
	for (auto& t : Working.Transitions)
	{
		std::vector<FAnimConditionDefinition> conditions;
		int root = -1, bad = 0;
		FString error;
		if (!t.Rule.Compile(Working.Parameters, conditions, root, error, &bad))
		{
			if (!any)
			{
				ImGui::SeparatorText("Rule Errors");
				any = true;
			}
			FString title = FString(StateName(t.From)) + " -> " + StateName(t.To);
			ImGui::PushID(t.Id);
			if (ImGui::Selectable(title.c_str(), ActiveRule == t.Id))
			{
				ActiveRule = t.Id;
				SelectedRuleNode = bad;
				auto& session = GraphSession();
				auto* previous = ed::GetCurrentEditor();
				ed::SetCurrentEditor(session.Context);
				ed::ClearSelection();
				if (bad)
					ed::SelectNode(Node(RuleNode, bad));
				session.Fit = true;
				ed::SetCurrentEditor(previous);
			}
			ImGui::TextColored(ImVec4(1, .4f, .3f, 1), "%s", error.c_str());
			ImGui::PopID();
		}
	}
}
