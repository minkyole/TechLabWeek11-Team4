#include "EnginePCH.h"
#include "AnimPoseGraphEditor.h"
#include "Asset/AssetManager.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Render/Mesh.h"
#include <algorithm>
namespace ed = ax::NodeEditor;
namespace
{
const char* Names[] = {"Sequence Player","BlendSpace Player","Two-way Blend","State Machine","Output Pose"};
uintptr_t Pin(int Node, int Slot) { return uintptr_t(Node)*4+Slot; }
int Inputs(EAnimGraphNodeType Type) { return Type==EAnimGraphNodeType::TwoWayBlend?2:Type==EAnimGraphNodeType::Output?1:0; }
}
bool AnimGraphTextEdit(const char* Label, FString& Value)
{
    char Buffer[1024]; strncpy_s(Buffer,Value.c_str(),_TRUNCATE);
    if(!ImGui::InputText(Label,Buffer,sizeof(Buffer))) return false;
    Value=Buffer; return true;
}
bool DrawAnimGraphParameters(std::vector<FAnimParameter>& Parameters)
{
    bool Dirty=false; int Remove=-1;
    for(int I=0;I<(int)Parameters.size();++I)
    {
        auto& P=Parameters[I]; ImGui::PushID(I);
        FString Name=P.Name.ToString(); if(AnimGraphTextEdit("Name",Name)) { P.Name=FName(Name.c_str()); Dirty=true; }
        int Type=(int)P.Type; if(ImGui::Combo("Type",&Type,"Bool\0Float\0Trigger\0")) { P.Type=(EAnimParameterType)Type; Dirty=true; }
        if(P.Type==EAnimParameterType::Float) Dirty|=ImGui::InputFloat("Default",&P.FloatValue);
        if(P.Type==EAnimParameterType::Bool) Dirty|=ImGui::Checkbox("Default",&P.BoolValue);
        if(ImGui::Button("Remove parameter")) Remove=I;
        ImGui::Separator(); ImGui::PopID();
    }
    if(Remove>=0) { Parameters.erase(Parameters.begin()+Remove); Dirty=true; }
    if(ImGui::Button("Add parameter")) { FAnimParameter P; P.Name=FName(("Parameter"+std::to_string(Parameters.size()+1)).c_str()); Parameters.push_back(P); Dirty=true; }
    return Dirty;
}
FAnimPoseGraphEditor::~FAnimPoseGraphEditor() { Reset(); }
void FAnimPoseGraphEditor::Reset() { if(Context) ed::DestroyEditor(Context); Context=nullptr; Placed.clear(); Selected=0; Fit=true; }
bool FAnimPoseGraphEditor::Draw(FAnimPoseGraph& Graph, const std::vector<FAnimParameter>& Parameters, const FString& MeshKey)
{
    bool Dirty=false;
    if(!Context) { ed::Config Config; Config.SettingsFile=nullptr; Context=ed::CreateEditor(&Config); }
    if(Graph.Nodes.empty()) { FAnimGraphNode N; N.Type=EAnimGraphNodeType::Output; N.X=500; Graph.Nodes.push_back(N); Dirty=true; }
    ImGui::SetNextItemWidth(165); ImGui::Combo("##NodeType",&AddType,Names,4); ImGui::SameLine();
    auto* Previous=ed::GetCurrentEditor(); ed::SetCurrentEditor(Context);
    if(ImGui::Button("Add node")) { FAnimGraphNode N; N.Id=Graph.NextNodeId(); N.Type=(EAnimGraphNodeType)AddType; N.Y=60.0f*(N.Id%5); Graph.Nodes.push_back(N); Dirty=true; }
    ImGui::SameLine(); if(ImGui::Button("Frame all")) Fit=true;
    FString Error; int ErrorNode=0; Graph.Validate(Error,&ErrorNode);
    ImGui::TextDisabled("Drag pose pins to connect | Select and Delete to remove | Middle drag: pan");
    ed::EnableShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&!ImGui::GetIO().WantTextInput);
    ed::Begin("PoseGraphCanvas",ImVec2(0,std::max(260.0f,ImGui::GetContentRegionAvail().y-65)));
    for(auto& N:Graph.Nodes)
    {
        if(Placed.insert(N.Id).second) ed::SetNodePosition(ed::NodeId(N.Id),ImVec2(N.X,N.Y));
        if(N.Id==ErrorNode) ed::PushStyleColor(ed::StyleColor_NodeBorder,ImVec4(1,.2f,.2f,1));
        ed::BeginNode(ed::NodeId(N.Id)); ImGui::Text("%s #%d",Names[(int)N.Type],N.Id);
        if(!N.AssetKey.empty()) ImGui::TextUnformatted(N.AssetKey.c_str());
        if(!N.Parameter.empty()) ImGui::Text("Parameter: %s",N.Parameter.c_str());
        for(int I=0;I<Inputs(N.Type);++I) { ed::BeginPin(ed::PinId(Pin(N.Id,I+1)),ed::PinKind::Input); ImGui::Text("-> %s",N.Type==EAnimGraphNodeType::TwoWayBlend?(I==0?"A":"B"):"Pose"); ed::EndPin(); }
        if(N.Type!=EAnimGraphNodeType::Output) { ed::BeginPin(ed::PinId(Pin(N.Id,0)),ed::PinKind::Output); ImGui::TextUnformatted("Pose ->"); ed::EndPin(); }
        ed::EndNode(); if(N.Id==ErrorNode) ed::PopStyleColor();
        auto Position=ed::GetNodePosition(ed::NodeId(N.Id)); if(Position.x!=N.X || Position.y!=N.Y) { N.X=Position.x; N.Y=Position.y; Dirty=true; }
    }
    for(auto& L:Graph.Links) ed::Link(ed::LinkId(L.Id),ed::PinId(Pin(L.From,0)),ed::PinId(Pin(L.To,L.Input+1)),ImVec4(.5f,.8f,1,1),2);
    if(ed::BeginCreate())
    {
        ed::PinId A,B;
        if(ed::QueryNewLink(&A,&B))
        {
            auto From=A.Get(),To=B.Get(); if(From%4) std::swap(From,To);
            bool Valid=From && To && From%4==0 && To%4>0 && Graph.CanConnect(int(From/4),int(To/4),int(To%4)-1);
            if(Valid) { if(ed::AcceptNewItem()) { Graph.Links.push_back({Graph.NextLinkId(),int(From/4),int(To/4),int(To%4)-1}); Dirty=true; } }
            else ed::RejectNewItem(ImVec4(1,.2f,.2f,1));
        }
    }
    ed::EndCreate();
    if(ed::BeginDelete())
    {
        ed::LinkId Link; while(ed::QueryDeletedLink(&Link)) if(ed::AcceptDeletedItem()) { std::erase_if(Graph.Links,[&](auto& L){return L.Id==(int)Link.Get();}); Dirty=true; }
        ed::NodeId Node; while(ed::QueryDeletedNode(&Node))
        {
            auto* N=Graph.Find((int)Node.Get());
            if(!N || N->Type==EAnimGraphNodeType::Output) { ed::RejectDeletedItem(); continue; }
            if(ed::AcceptDeletedItem()) { int Id=N->Id; std::erase_if(Graph.Links,[&](auto& L){return L.From==Id||L.To==Id;}); std::erase_if(Graph.Nodes,[&](auto& V){return V.Id==Id;}); Placed.erase(Id); if(Selected==Id) Selected=0; Dirty=true; }
        }
    }
    ed::EndDelete();
    if(ed::HasSelectionChanged()) { ed::NodeId Node; Selected=ed::GetSelectedNodes(&Node,1)?(int)Node.Get():0; }
    if(Fit) { ed::NavigateToContent(0); Fit=false; }
    ed::End(); ed::SetCurrentEditor(Previous);
    if(!Error.empty()) ImGui::TextWrapped("%s",Error.c_str());
    return Dirty;
}
bool FAnimPoseGraphEditor::DrawProperties(FAnimPoseGraph& Graph, const std::vector<FAnimParameter>& Parameters, const FString& MeshKey)
{
    auto* N=Graph.Find(Selected); if(!N) { ImGui::TextDisabled("Select a pose node."); return false; }
    bool Dirty=false; ImGui::Text("%s #%d",Names[(int)N->Type],N->Id);
    auto* Mesh=UAssetManager::GetAssetByPath<USkeletalMesh>(MeshKey);
    if(N->Type==EAnimGraphNodeType::Sequence || N->Type==EAnimGraphNodeType::BlendSpace)
    {
        if(ImGui::BeginCombo("Animation",N->AssetKey.c_str()))
        {
            if(N->Type==EAnimGraphNodeType::Sequence) { for(auto* A:UAssetManager::Get().GetAnimationAssets(Mesh)) if(ImGui::Selectable(A->GetPath().c_str(),N->AssetKey==A->GetPath())) { N->AssetKey=A->GetPath(); Dirty=true; } }
            else { for(auto* A:UAssetManager::Get().GetBlendSpaceAssets(Mesh)) if(ImGui::Selectable(A->GetPath().c_str(),N->AssetKey==A->GetPath())) { N->AssetKey=A->GetPath(); Dirty=true; } }
            ImGui::EndCombo();
        }
        Dirty|=AnimGraphTextEdit("Asset key",N->AssetKey);
        Dirty|=ImGui::Checkbox("Loop",&N->Loop); Dirty|=ImGui::Checkbox("Playing",&N->Playing); Dirty|=ImGui::InputFloat("Play rate",&N->PlayRate);
    }
    if(N->Type==EAnimGraphNodeType::StateMachine)
    {
        Dirty|=AnimGraphTextEdit("StateMachine (.animsm)",N->AssetKey);
        ImGui::TextWrapped("Parameters map by name and type. Only the owning graph's UpdateScript runs; the referenced StateMachine's script is disabled.");
    }
    if(N->Type==EAnimGraphNodeType::TwoWayBlend || N->Type==EAnimGraphNodeType::BlendSpace)
    {
        if(ImGui::BeginCombo("Float parameter",N->Parameter.empty()?"Constant":N->Parameter.c_str()))
        {
            if(ImGui::Selectable("Constant",N->Parameter.empty())) { N->Parameter.clear(); Dirty=true; }
            for(auto& P:Parameters) if(P.Type==EAnimParameterType::Float && ImGui::Selectable(P.Name.ToString().c_str(),N->Parameter==P.Name.ToString())) { N->Parameter=P.Name.ToString(); Dirty=true; }
            ImGui::EndCombo();
        }
        // Embedded StateMachines may introduce a Float not yet declared at this level.
        Dirty|=AnimGraphTextEdit("Parameter name",N->Parameter);
        if(N->Parameter.empty()) Dirty|=ImGui::InputFloat(N->Type==EAnimGraphNodeType::TwoWayBlend?"Alpha":"Axis",&N->Value);
    }
    return Dirty;
}
