#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/xtbl/Xtbl.h"
#include <vector>

class XtblManager;

class XtblDocument final : public IDocument
{
public:
    XtblDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer, IXtblNode* startingNode = nullptr);
    ~XtblDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

    IXtblNode* SelectedNode = nullptr;
private:
    //Used to check if items on the sidebar match the search term
    bool AnyChildMatchesSearchTerm(Handle<XtblCategory> category);
    //Used to draw the sidebar
    void DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault = false);
    void DrawXtblNodeEntry(IXtblNode* node); //Draw xtbl node in entry list

    //Behavior for the buttons on the sidebar
    void AddEntry();
    void AddCategory();
    void DuplicateEntry(IXtblNode* entry);

    //Sidebar right click context menu behavior
    void DrawRenameCategoryWindow();
    void DrawRenameEntryWindow();

    string filename_;
    string parentName_;
    string vppName_;
    bool inContainer_;
    string searchTerm_ = "";

    Handle<XtblFile> xtbl_ = nullptr;
    XtblManager* xtblManager_ = nullptr;
    GuiState* state_ = nullptr;

    //Note: Bools are used for popups to bypass needing ImGui::OpenPopup() and ImGui::BeginPopup() to be called at around the same gui stack level
    //Category rename popup data
    const char* renameCategoryPopupId_ = "Rename category";
    bool renameCategoryWindowOpen_ = false;
    Handle<XtblCategory> renameCategory_ = nullptr;
    string renameCategoryName_ = "";

    //Entry rename popup data
    const char* renameEntryPopupId_ = "Rename entry";
    bool renameEntryWindowOpen_ = false;
    IXtblNode* renameEntry_ = nullptr;
    string renameEntryName_ = "";
};