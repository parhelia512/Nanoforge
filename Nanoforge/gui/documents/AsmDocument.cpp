#include "AsmDocument.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"
#include "Log.h"

AsmDocument::AsmDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer)
{
    //Get packfile. All asm_pc files are in .vpp_pc files
    Packfile3* container = state->PackfileVFS->GetPackfile(vppName);

    //Find asm_pc file in parent packfile
    for (auto& asmFile : container->AsmFiles)
        if (String::EqualIgnoreCase(asmFile.Name, filename))
            asmFile_ = &asmFile;

    //Report error if asm_pc file isn't found
    if (!asmFile_)
    {
        LOG_ERROR("Failed to find {}. Closing asm document.", filename_);
        Open = false;
        return;
    }
}

AsmDocument::~AsmDocument()
{

}

void AsmDocument::Update(GuiState* state)
{
    if (!asmFile_)
        return;

    const f32 indent = 30.0f;
    ImGui::Separator();
    state->FontManager->FontMedium.Push();
    ImGui::Text(fmt::format("{} {}", ICON_FA_DATABASE, Title.c_str()));
    state->FontManager->FontMedium.Pop();
    ImGui::Separator();

    //Header data
    ImGui::Indent(indent);
    gui::LabelAndValue("Name:", asmFile_->Name);
    gui::LabelAndValue("Signature:", std::to_string(asmFile_->Version));
    gui::LabelAndValue("Version:", std::to_string(asmFile_->Version));
    gui::LabelAndValue("# Containers:", std::to_string(asmFile_->ContainerCount));
    ImGui::Unindent(indent);

    ImGui::Separator();
    state->FontManager->FontMedium.Push();
    ImGui::Text(ICON_FA_BOX " Containers");
    state->FontManager->FontMedium.Pop();
    ImGui::Separator();

    //Container data
    ImGui::Indent(indent);
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.25f); //Increase spacing to differentiate leaves from expanded contents.
    for (auto& container : asmFile_->Containers)
    {
        if (ImGui::TreeNodeEx(container.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            gui::LabelAndValue("Type: ", to_string(container.Type));
            gui::LabelAndValue("Flags: ", std::to_string(container.Flags));
            gui::LabelAndValue("# Primitives: ", std::to_string(container.PrimitiveCount));
            gui::LabelAndValue("Data offset: ", std::to_string(container.DataOffset));
            gui::LabelAndValue("Compressed size: ", std::to_string(container.CompressedSize));

            //Primitive data
            if (ImGui::TreeNode(fmt::format("Primitives##{}", (u64)&container).c_str()))
            {
                for (auto& primitive : container.Primitives)
                {
                    if (ImGui::TreeNode(primitive.Name.c_str()))
                    {
                        gui::LabelAndValue("Type: ", to_string(primitive.Type));
                        gui::LabelAndValue("Allocator: ", to_string(primitive.Allocator));
                        gui::LabelAndValue("Flags: ", std::to_string(primitive.Flags));
                        gui::LabelAndValue("Split ext index: ", std::to_string(primitive.SplitExtIndex));
                        gui::LabelAndValue("Header size: ", std::to_string(primitive.HeaderSize));
                        gui::LabelAndValue("Data size: ", std::to_string(primitive.DataSize));
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopStyleVar();
    ImGui::Unindent(indent);
}

#pragma warning(disable:4100)
void AsmDocument::Save(GuiState* state)
{

}
#pragma warning(default:4100)