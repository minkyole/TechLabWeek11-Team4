#pragma once
#include "Animation/AnimStateMachineAsset.h"
#include "Editor/EditorUI/EditorPanel.h"
#include <map>
#include <set>
namespace ax
{
namespace NodeEditor
{
struct EditorContext;
}
} // namespace ax
class FSkeletalMeshViewerPanel;
class UAssetAnimInstance;
class FStateMachineEditorPanel : public IEditorPanel
{
  public:
	~FStateMachineEditorPanel() override;
	bool Init() override
	{
		SetOpen(false);
		return true;
	}
	void Tick(float) override
	{
	}
	void OnRender() override;
	const char *GetPanelName() const override
	{
		return "State Machine Editor";
	}
	void SetOpen(bool Open) override;
	void OpenAsset(const FString &Path);
	void NewAsset(const FString &Folder);
	void SetViewer(FSkeletalMeshViewerPanel *V)
	{
		Viewer = V;
	}
	bool IsKeyboardFocused() const
	{
		return IsOpen() && Focused;
	}
	void ReleaseResources();

  private:
	struct FGraphSession
	{
		ax::NodeEditor::EditorContext *Context = nullptr;
		std::set<uintptr_t> Placed;
		bool Fit = true;
	};
	void ClearContexts();
	FGraphSession &GraphSession();
	void LoadPending();
	bool Save();
	void DrawGraph();
	void DrawStateGraph(FGraphSession &Session);
	void DrawRuleGraph(FGraphSession &Session);
	void DrawDetails();
	void DrawParameters();
	void DrawPreview();
	void DrawRuleErrors();
	void DrawRuleProperties(FAnimRuleNode &Node, bool InCanvas = false);
	void DeleteSelection();
	void AddTransition(int From, int To);
	FAnimAssetState *FindState(int Id);
	FAnimAssetTransition *FindTransition(int Id);
	const char *StateName(int Id);
	bool StateCombo(const char *Label, int &Id, bool Any = false);
	bool ParameterCombo(const char *Label, FName &Name, EAnimParameterType Type, bool InCanvas);
	FString ParameterReferences(FName Name) const;
	void RenameParameter(FName Old, FName New);
	void AddState(ImVec2 Position = ImVec2(40, 40));
	FAnimStateMachineAssetData Working;
	FString Path, PendingPath, PendingFolder, Error;
	char SavePath[512] = "Assets/NewStateMachine.animsm";
	bool Dirty = false, PendingClose = false, Focused = false;
	int Selected = -1, SelectedTransition = -1, NextId = 1, ActiveRule = 0, SelectedRuleNode = 0;
	uintptr_t ContextObject = 0, CreateFromPin = 0;
	ImVec2 CreationPosition{};
	std::map<int, FGraphSession> Sessions;
	FSkeletalMeshViewerPanel *Viewer = nullptr;
};
