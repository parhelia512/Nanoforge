#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "XtblDescription.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

class GuiState;
class XtblFile;
struct XtblFlag
{
    string Name;
    bool Value;
};
using XtblValue = std::variant<string, i32, f32, Vec3, u32, XtblFlag>;

//Interface used by all xtbl nodes. Includes data that all nodes have. Separate implementations must be written for each node data type (e.g. float, int, reference, etc)
class IXtblNode
{
public:
    //IXtblNode interface functions
    virtual ~IXtblNode()
    {
        DeleteSubnodes();
        Parent = nullptr;
    }
    //Draw editor for node using Dear ImGui. Returns true if the node was edited.
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr) = 0;
    //Initialize node with default values for it's data type
    virtual void InitDefault() = 0;
    //Write edited node data to modinfo.xml. Returns false if it encounters an error and true if it succeeds.
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml) = 0;
    //Write node data as xml. If writeNanoforgeMetadata data only used by Nanoforge will be written to each node.
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata = true) = 0;

    //Functions that all nodes have
    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
    void DeleteSubnodes();
    std::optional<string> GetSubnodeValueString(const string& subnodeName);
    IXtblNode* GetSubnode(const string& subnodeName);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();
    //Create deep copy of node and subnodes
    IXtblNode* DeepCopy(IXtblNode* parent = nullptr);


    //Values that all nodes have
    string Name;
    XtblValue Value;
    XtblType Type = XtblType::None;
    std::vector<IXtblNode*> Subnodes;
    IXtblNode* Parent = nullptr;
    bool CategorySet = false; //Used by XtblFile to track which nodes are uncategorized
    bool Enabled = true; //Whether or not the node should be included in the file xtbl (for non-required nodes)
    bool HasDescription = false; //Whether or not the XtblNode has a description. Either in the <TableDescription> block or one provided by modders
    bool Edited = false; //Set to true if edited
    bool NewEntry = false; //Entry isn't in the base game. Needs special treatment during modinfo.xml generation

protected:
    //Calculates desc_, nameNoId_, and name_ if editorValuesInitialized_ == false
    void CalculateEditorValues(Handle<XtblFile> xtbl, const char* nameOverride = nullptr);

    //Values needed by xtbl editor. Cached so they don't need to be recalculated each frame for hundreds of nodes
    bool editorValuesInitialized_ = false;
    Handle<XtblDescription> desc_ = nullptr; //Description from <TableDescription> section
    std::optional<string> nameNoId_; //Node name without the dear imgui label
    std::optional<string> name_; //Node name with dear imgui label. Used to unique identify it. Necessary when there are multiple UI elements with the same name
};

static IXtblNode* a;