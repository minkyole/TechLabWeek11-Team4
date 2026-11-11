#pragma once
#include "Animation/AnimGraph.h"
#include "imgui_node_editor.h"
#include <set>

// Same authoring widget for the top-level graph and a state's pose graph.
class FAnimPoseGraphEditor
{
public:
    ~FAnimPoseGraphEditor();
    void Reset();
    bool Draw(FAnimPoseGraph& Graph, const std::vector<FAnimParameter>& Parameters, const FString& MeshKey);
    bool DrawProperties(FAnimPoseGraph& Graph, const std::vector<FAnimParameter>& Parameters, const FString& MeshKey);
private:
    ax::NodeEditor::EditorContext* Context = nullptr;
    std::set<int> Placed;
    int Selected = 0, AddType = 0;
    bool Fit = true;
};
bool AnimGraphTextEdit(const char* Label, FString& Value);
bool DrawAnimGraphParameters(std::vector<FAnimParameter>& Parameters);
