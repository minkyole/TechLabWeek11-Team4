#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "AnimPoseGraphEditor.h"
class FSkeletalMeshViewerPanel;
class FAnimGraphEditorPanel : public IEditorPanel
{
public:
    bool Init() override { IEditorPanel::SetOpen(false); return true; }
    void Tick(float) override {}
    void OnRender() override;
    const char* GetPanelName() const override { return "AnimGraph Editor"; }
    void SetOpen(bool Open) override;
    void OpenAsset(const FString& Path);
    void NewAsset(const FString& Folder);
    void SetViewer(FSkeletalMeshViewerPanel* V) { Viewer=V; }
    bool IsKeyboardFocused() const { return IsOpen() && Focused; }
    void ReleaseResources();
private:
    void LoadPending();
    bool Save();
    void DrawPreview();
    FAnimGraphAssetData Working;
    FAnimPoseGraphEditor Editor;
    FSkeletalMeshViewerPanel* Viewer = nullptr;
    FString Path, PendingPath, PendingFolder, Error;
    char SavePath[512] = "Assets/NewAnimGraph.animgraph";
    bool Dirty=false, Focused=false, PendingClose=false;
};
// Rebuild live components depending directly or transitively on the saved asset.
void ReloadAnimationAssetInstances(const FString& Path);
