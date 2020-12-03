#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"
#include "property_panel/PropertyPanelContent.h"
#include "RfgTools++/formats/zones/properties/primitive/StringProperty.h"

void ZoneObjectsList_DrawObjectNode(GuiState* state, ZoneObjectNode36& object);
static u32 ZoneObjectList_ObjectIndex = 0;

//Todo: May want to move this to be under each zone in the zone window list since we'll have a third panel for per-object properties
void ZoneObjectsList_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Zone objects", open))
    {
        ImGui::End();
        return;
    }

    if (state->SelectedZone == InvalidZoneIndex || state->SelectedZone >= state->CurrentTerritory->ZoneFiles.size())
    {
        ImGui::Text("%s Select a zone to see the objects it contains.", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else if (!state->CurrentTerritory->Ready())
    {
        ImGui::Text("%s Zone data still loading. ", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        if (ImGui::CollapsingHeader(ICON_FA_FILTER " Filters##CollapsingHeader"))
        {
            if (ImGui::Button("Show all types"))
            {
                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    objectClass.Show = true;
                }
                state->CurrentTerritoryUpdateDebugDraw = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Hide all types"))
            {
                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    objectClass.Show = false;
                }
                state->CurrentTerritoryUpdateDebugDraw = true;
            }

            //Draw object filters sub-window
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.134f, 0.160f, 0.196f, 1.0f));
            if (ImGui::BeginChild("##Zone object filters list", ImVec2(0, 200.0f), true))
            {
                ImGui::Text(" " ICON_FA_EYE);
                gui::TooltipOnPrevious("Toggles whether bounding boxes are drawn for the object class", nullptr);

                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    if (ImGui::Checkbox((string("##showBB") + objectClass.Name).c_str(), &objectClass.Show))
                        state->CurrentTerritoryUpdateDebugDraw = true;

                    ImGui::SameLine();
                    ImGui::Text(objectClass.Name);
                    ImGui::SameLine();
                    ImGui::TextColored(gui::SecondaryTextColor, "|  %d objects", objectClass.NumInstances);
                    ImGui::SameLine();

                    //Todo: Use a proper string formatting lib here
                    if (ImGui::ColorEdit3(string("##" + objectClass.Name).c_str(), (f32*)&objectClass.Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        state->CurrentTerritoryUpdateDebugDraw = true;
                }
                ImGui::EndChild();
            }
            ImGui::PopStyleColor();
        }

        //Draw zone objects list
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_BOXES " Zone objects");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //Draw object list
        ZoneObjectList_ObjectIndex = 0;
        auto& zone = state->CurrentTerritory->ZoneFiles[state->SelectedZone].Zone;
        if (ImGui::BeginChild("##Zone object list", ImVec2(0, 0), true))
        {
            //Todo: Separate node structure from ZoneObject36 class. This should really be independent of the format since it's only relevant to Nanoforge
            //Draw each node
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
            for (auto& object : zone.ObjectsHierarchical)
            {
                ZoneObjectsList_DrawObjectNode(state, object);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }
    }

    ImGui::End();
}

void ZoneObjectsList_DrawObjectNode(GuiState* state, ZoneObjectNode36& object)
{
    //Dont show node or child nodes if the object class is being hidden
    auto& objectClass = state->CurrentTerritory->GetObjectClass(object.Self->ClassnameHash);
    if (!objectClass.Show)
        return;

    //Update node index and selection state
    ZoneObjectList_ObjectIndex++; //Incremented for each node so they all have a unique id within dear imgui
    object.Selected = &object == state->SelectedObject;

    //Attempt to find a human friendly name for the object
    string name = "";
    auto* displayName = object.Self->GetProperty<StringProperty>("display_name");
    auto* chunkName = object.Self->GetProperty<StringProperty>("chunk_name");
    auto* animationType = object.Self->GetProperty<StringProperty>("animation_type");
    auto* activityType = object.Self->GetProperty<StringProperty>("activity_type");
    auto* raidType = object.Self->GetProperty<StringProperty>("raid_type");
    auto* courierType = object.Self->GetProperty<StringProperty>("courier_type");
    auto* spawnSet = object.Self->GetProperty<StringProperty>("spawn_set");
    auto* itemType = object.Self->GetProperty<StringProperty>("item_type");
    auto* dummyType = object.Self->GetProperty<StringProperty>("dummy_type");
    auto* weaponType = object.Self->GetProperty<StringProperty>("weapon_type");
    auto* regionKillType = object.Self->GetProperty<StringProperty>("region_kill_type");
    auto* deliveryType = object.Self->GetProperty<StringProperty>("delivery_type");
    auto* squadDef = object.Self->GetProperty<StringProperty>("squad_def");
    auto* missionInfo = object.Self->GetProperty<StringProperty>("mission_info");
    if (displayName)
        name = displayName->Data;
    else if (chunkName)
        name = chunkName->Data;
    else if (animationType)
        name = animationType->Data;
    else if (activityType)
        name = activityType->Data;
    else if (raidType)
        name = raidType->Data;
    else if (courierType)
        name = courierType->Data;
    else if (spawnSet)
        name = spawnSet->Data;
    else if (itemType)
        name = itemType->Data;
    else if (dummyType)
        name = dummyType->Data;
    else if (weaponType)
        name = weaponType->Data;
    else if (regionKillType)
        name = regionKillType->Data;
    else if (deliveryType)
        name = deliveryType->Data;
    else if (squadDef)
        name = squadDef->Data;
    else if (missionInfo)
        name = missionInfo->Data;

    if (name != "")
        name = " |   " + name;

    //Draw node
    if (ImGui::TreeNodeEx((string(objectClass.LabelIcon) + object.Self->Classname  + "##" + std::to_string(ZoneObjectList_ObjectIndex)).c_str(), ImGuiTreeNodeFlags_SpanAvailWidth |
        (object.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None) | (object.Selected ? ImGuiTreeNodeFlags_Selected : 0)))
    {
        //Update selection state
        if (ImGui::IsItemClicked())
        {
            state->SetSelectedZoneObject(&object);
            state->PropertyPanelContentFuncPtr = &PropertyPanel_ZoneObject;
        }

        if (name != "")
        {
            ImGui::SameLine();
            ImGui::TextColored(gui::SecondaryTextColor, name);
        }

        //Draw child nodes
        for (auto& childObject : object.Children)
        {
            ZoneObjectsList_DrawObjectNode(state, childObject);
        }
        ImGui::TreePop();
    }
    else
    {
        if (name != "")
        {
            ImGui::SameLine();
            ImGui::TextColored(gui::SecondaryTextColor, name);
        }
    }
}