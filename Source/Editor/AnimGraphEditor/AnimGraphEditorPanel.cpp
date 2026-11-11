#include "EnginePCH.h"
#include "AnimGraphEditorPanel.h"
#include "Editor/SkeletalMeshViewer/SkeletalMeshViewerPanel.h"
#include "Asset/AssetManager.h"
#include "Core/Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"
#include "Components/SkeletalMeshComponent.h"
#include "Render/Mesh.h"
#include "Animation/AssetAnimInstance.h"

void ReloadAnimationAssetInstances(const FString& Path)
{
    // Collect first: recreating instances mutates GUObjectArray during iteration.
    std::vector<USkeletalMeshComponent*> Components;
    for(TObjectIterator<USkeletalMeshComponent> It;It;++It)
        if(!It->IsTemplate() && It->UsesAnimationAsset(Path)) Components.push_back(*It);
    for(auto* Component:Components)
    {
        if (auto* Graph = Cast<UAnimGraphInstance>(Component->GetAnimInstance()); Graph && Component->GetAnimGraphAssetPath().empty())
        { auto Snapshot=Graph->GetAssetData(); Graph->Configure(Snapshot,false); Component->RefreshAnimationPose(); }
        else if (auto* SM = Cast<UAssetAnimInstance>(Component->GetAnimInstance()); SM && Component->GetStateMachineAssetPath().empty())
        { auto Snapshot=SM->GetAssetData(); SM->Configure(Snapshot,false); Component->RefreshAnimationPose(); }
        else { Component->ClearAnimInstance(); Component->InitializeAnimInstance(); }
    }
}
void FAnimGraphEditorPanel::ReleaseResources()
{
    if(Viewer) Viewer->StopAnimGraphPreview();
    Editor.Reset(); Working={}; Dirty=false;
}
void FAnimGraphEditorPanel::SetOpen(bool Open)
{
    if(!Open && Dirty) { PendingClose=true; return; }
    if(!Open && Viewer) Viewer->StopAnimGraphPreview();
    IEditorPanel::SetOpen(Open);
}
void FAnimGraphEditorPanel::OpenAsset(const FString& P)
{
    PendingPath=P; PendingFolder.clear(); IEditorPanel::SetOpen(true); if(!Dirty) LoadPending();
}
void FAnimGraphEditorPanel::NewAsset(const FString& Folder)
{
    PendingFolder=Folder; PendingPath.clear(); IEditorPanel::SetOpen(true); if(!Dirty) LoadPending();
}
void FAnimGraphEditorPanel::LoadPending()
{
    if(!PendingPath.empty())
    {
        auto* Asset=UAssetManager::Get().LoadAnimGraph(PendingPath,Error);
        if(!Asset) { PendingPath.clear(); return; }
        Working=Asset->Data; Path=PendingPath;
    }
    else if(!PendingFolder.empty())
    {
        Working={}; Path.clear();
        auto Meshes=UAssetManager::Get().GetSkeletalMeshAssets();
        if(Meshes.Num()) Working.MeshKey=Meshes[0]->GetPath();
        FAnimGraphNode Output; Output.Type=EAnimGraphNodeType::Output; Output.X=500; Working.Graph.Nodes.push_back(Output);
        auto Target=FFileHelper::ToUtf8Path(FFileHelper::ToNativePath(PendingFolder)/"NewAnimGraph.animgraph"); strncpy_s(SavePath,Target.c_str(),_TRUNCATE);
    }
    if(Viewer) Viewer->StopAnimGraphPreview();
    PendingPath.clear(); PendingFolder.clear(); Error.clear(); Dirty=false; Editor.Reset();
}
bool FAnimGraphEditorPanel::Save()
{
    auto Target=Path.empty()?FString(SavePath):Path;
    if(!UAssetManager::Get().SaveAnimGraph(Target,Working,Error,Path.empty())) return false;
    Path=Target; Dirty=false; ReloadAnimationAssetInstances(Path); return true;
}
void FAnimGraphEditorPanel::DrawPreview()
{
    if(ImGui::Button("Apply to 3D Preview") && Viewer) Viewer->PreviewAnimGraph(Working,Error);
    auto* Instance=Viewer?Viewer->GetAnimGraphPreview():nullptr;
    if(!Instance) return;
    ImGui::SameLine(); if(ImGui::Button(Instance->IsPaused()?"Play":"Pause")) Instance->SetPaused(!Instance->IsPaused());
    if(ImGui::Button("Reset preview")) { auto Data=Instance->GetAssetData(); if(!Instance->Configure(Data,false)) Error=Instance->GetLastError(); }
    ImGui::TextDisabled("Manual parameters; Lua update is disabled in preview.");
    // Includes parameters imported by referenced StateMachine assets.
    for(auto P:Instance->GetParameters().Values)
    {
        auto Name=P.Name.ToString(); ImGui::PushID(Name.c_str());
        if(P.Type==EAnimParameterType::Float) { float V=P.FloatValue; if(ImGui::InputFloat(Name.c_str(),&V)) Instance->SetFloat(Name,V); }
        else if(P.Type==EAnimParameterType::Bool) { bool V=P.BoolValue; if(ImGui::Checkbox(Name.c_str(),&V)) Instance->SetBool(Name,V); }
        else if(ImGui::Button(Name.c_str())) Instance->SetTrigger(Name);
        ImGui::PopID();
    }
    if(auto* Runtime=Instance->GetRuntime()) for(auto& M:Runtime->GetMachines())
    {
        ImGui::TextWrapped("%s: state #%d",M.Asset.c_str(),M.State);
        if(M.Transitioning) ImGui::ProgressBar(M.Progress);
    }
}
void FAnimGraphEditorPanel::OnRender()
{
    if(!IsOpen()) return;
    ImGui::SetNextWindowSize(ImVec2(1100,750),ImGuiCond_FirstUseEver);
    bool Open=true;
    bool Visible=ImGui::Begin(GetPanelName(),&Open,ImGuiWindowFlags_NoCollapse);
    Focused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if(!Open) SetOpen(false);
    if(Visible && IsOpen())
    {
        if(ImGui::Button("Save")) Save(); ImGui::SameLine(); ImGui::Text("%s%s",Path.empty()?"New AnimGraph":Path.c_str(),Dirty?" *":"");
        if(Path.empty()) ImGui::InputText("Save path",SavePath,sizeof(SavePath));
        if(ImGui::BeginCombo("Skeletal Mesh",Working.MeshKey.c_str()))
        {
            for(auto* Mesh:UAssetManager::Get().GetSkeletalMeshAssets()) if(ImGui::Selectable(Mesh->GetPath().c_str(),Working.MeshKey==Mesh->GetPath())) { Working.MeshKey=Mesh->GetPath(); Dirty=true; }
            ImGui::EndCombo();
        }
        Dirty|=AnimGraphTextEdit("Optional UpdateScript",Working.UpdateScript);
        if(!Error.empty()) ImGui::TextWrapped("%s",Error.c_str());
        if(ImGui::BeginTable("AnimGraphLayout",2,ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Graph",ImGuiTableColumnFlags_WidthStretch,2); ImGui::TableSetupColumn("Properties",ImGuiTableColumnFlags_WidthStretch,1);
            ImGui::TableNextColumn(); Dirty|=Editor.Draw(Working.Graph,Working.Parameters,Working.MeshKey);
            ImGui::TableNextColumn();
            if(ImGui::BeginChild("AnimGraphProperties"))
            {
                if(ImGui::CollapsingHeader("Selection",ImGuiTreeNodeFlags_DefaultOpen)) Dirty|=Editor.DrawProperties(Working.Graph,Working.Parameters,Working.MeshKey);
                if(ImGui::CollapsingHeader("Parameters")) Dirty|=DrawAnimGraphParameters(Working.Parameters);
                if(ImGui::CollapsingHeader("Preview",ImGuiTreeNodeFlags_DefaultOpen)) DrawPreview();
            }
            ImGui::EndChild(); ImGui::EndTable();
        }
        if(Focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) Save();
    }
    if(Dirty && (PendingClose || !PendingPath.empty() || !PendingFolder.empty())) ImGui::OpenPopup("Unsaved AnimGraph");
    if(ImGui::BeginPopupModal("Unsaved AnimGraph",nullptr,ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Save changes before continuing?");
        auto Finish=[&] { Dirty=false; if(PendingClose) { PendingClose=false; SetOpen(false); } else LoadPending(); ImGui::CloseCurrentPopup(); };
        if(ImGui::Button("Save and continue") && Save()) Finish();
        ImGui::SameLine(); if(ImGui::Button("Discard")) Finish();
        ImGui::SameLine(); if(ImGui::Button("Cancel")) { PendingClose=false; PendingPath.clear(); PendingFolder.clear(); ImGui::CloseCurrentPopup(); }
        if(!Error.empty()) ImGui::TextWrapped("%s",Error.c_str()); ImGui::EndPopup();
    }
    ImGui::End();
}
