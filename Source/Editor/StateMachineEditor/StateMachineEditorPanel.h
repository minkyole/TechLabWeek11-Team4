#pragma once
#include "Animation/AnimStateMachineAsset.h"
#include "Editor/EditorUI/EditorPanel.h"
class FSkeletalMeshViewerPanel;
class UAssetAnimInstance;
class FStateMachineEditorPanel : public IEditorPanel
{
  public:
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
	void LoadPending();
	bool Save();
	void DrawGraph();
	void DrawDetails();
	void DrawParameters();
	void DrawConditions();
	void DrawPreview();
	void DeleteSelection();
	FAnimAssetState *FindState(int Id);
	const char *StateName(int Id);
	bool StateCombo(const char *Label, int &Id, bool Any = false);
	void AddState();
	FAnimStateMachineAssetData Working;
	FString Path, PendingPath, PendingFolder, Error;
	char SavePath[512] = "Assets/NewStateMachine.animsm";
	bool Dirty = false, PendingClose = false, Focused = false;
	int Selected = -1, SelectedTransition = -1, LinkFrom = -2, NextId = 1;
	float Zoom = 1;
	ImVec2 Pan{30, 70};
	FSkeletalMeshViewerPanel *Viewer = nullptr;
};
