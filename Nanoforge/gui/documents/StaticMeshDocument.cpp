#include "StaticMeshDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/RfgUtil.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "Log.h"
#include "gui/documents/PegHelpers.h"
#include "render/imgui/imgui_ext.h"
#include "gui/util/WinUtil.h"
#include "PegHelpers.h"
#include "render/Render.h"
#include <imgui_internal.h>
#include <optional>
#include "util/Profiler.h"
#include "rfg/TextureIndex.h"

StaticMeshDocument::StaticMeshDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    state_ = state;

    //Create scene instance and store index
    Scene = state->Renderer->CreateScene();
    if (!Scene)
        THROW_EXCEPTION("Failed to create scene for static mesh document \"{}\"", filename);

    //Init scene and camera
    Scene->Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->Cam.Speed = 0.25f;
    Scene->Cam.SprintSpeed = 0.4f;
    Scene->Cam.LookAt({ 0.0f, 0.0f, 0.0f });
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 2.5f;

    //Create worker thread to load terrain meshes in background
    meshLoadTask_ = Task::Create(fmt::format("Loading {}...", filename));
    TaskScheduler::QueueTask(meshLoadTask_, std::bind(&StaticMeshDocument::WorkerThread, this, meshLoadTask_, state));
}

StaticMeshDocument::~StaticMeshDocument()
{
    //Wait for worker thread to so we don't destroy resources it's using
    Open = false;
    meshLoadTask_->CancelAndWait();

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
    Scene = nullptr;
}

void StaticMeshDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Camera only handles input if window is focused
    Scene->Cam.InputActive = ImGui::IsWindowFocused();
    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if(contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
        Scene->HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(Scene->ClearColor.x, Scene->ClearColor.y, Scene->ClearColor.z, Scene->ClearColor.w));
    ImGui::Image(Scene->GetView(), ImVec2(static_cast<f32>(Scene->Width()), static_cast<f32>(Scene->Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    DrawOverlayButtons(state);
}

void StaticMeshDocument::Save(GuiState* state)
{

}

void StaticMeshDocument::DrawOverlayButtons(GuiState* state)
{
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_CAMERA))
        ImGui::OpenPopup("##CameraSettingsPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##CameraSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Camera");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        f32 fov = Scene->Cam.GetFovDegrees();
        f32 nearPlane = Scene->Cam.GetNearPlane();
        f32 farPlane = Scene->Cam.GetFarPlane();
        f32 lookSensitivity = Scene->Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) Scene->Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) Scene->Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) Scene->Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) Scene->Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) Scene->Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) Scene->Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &Scene->Cam.Speed);
        ImGui::InputFloat("Sprint speed", &Scene->Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::SliderFloat("Fov", &fov, 40.0f, 120.0f))
            Scene->Cam.SetFovDegrees(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            Scene->Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&Scene->Cam.camPosition))
        {
            Scene->Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitchDegrees()));
        gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYawDegrees()));

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_SUN))
        ImGui::OpenPopup("##SceneSettingsPopup");
    state->FontManager->FontL.Pop();

    if (ImGui::BeginPopup("##SceneSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Render settings");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        ImGui::SetNextItemWidth(175.0f);
        static float tempScale = 1.0f;
        ImGui::InputFloat("Scale", &tempScale);
        ImGui::SameLine();
        if (ImGui::Button("Set all"))
        {
            Scene->Objects[0]->Scale.x = tempScale;
            Scene->Objects[0]->Scale.y = tempScale;
            Scene->Objects[0]->Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&Scene->Objects[0]->Scale, 0.01, 1.0f, 100.0f);

        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
        ImGui::SliderFloat("Diffuse intensity", &Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 3.0f);

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_INFO_CIRCLE))
        ImGui::OpenPopup("##MeshInfoPopup");
    state->FontManager->FontL.Pop();

    if (ImGui::BeginPopup("##MeshInfoPopup"))
    {
        //Header / general data
        state->FontManager->FontL.Push();
        ImGui::Text("Mesh info");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        gui::LabelAndValue("Header size:", std::to_string(StaticMesh.MeshInfo.CpuDataSize) + " bytes");
        gui::LabelAndValue("Data size:", std::to_string(StaticMesh.MeshInfo.GpuDataSize) + " bytes");
        gui::LabelAndValue("Num LODs:", std::to_string(StaticMesh.NumLods));
        gui::LabelAndValue("Num submeshes:", std::to_string(StaticMesh.MeshInfo.NumSubmeshes));
        gui::LabelAndValue("Num materials:", std::to_string(StaticMesh.Header.NumMaterials));
        gui::LabelAndValue("Num vertices:", std::to_string(StaticMesh.MeshInfo.NumVertices));
        gui::LabelAndValue("Num indices:", std::to_string(StaticMesh.MeshInfo.NumIndices));
        gui::LabelAndValue("Vertex format:", to_string(StaticMesh.MeshInfo.VertFormat));
        gui::LabelAndValue("Vertex size:", std::to_string(StaticMesh.MeshInfo.VertexStride0));
        gui::LabelAndValue("Index size:", std::to_string(StaticMesh.MeshInfo.IndexSize));

        //Submesh data
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CUBES "Submeshes");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //LOD level selector
        auto& mesh = Scene->Objects[0]->ObjectMesh;
        u32 lodLevel = mesh.GetLodLevel();
        u32 min = 0;
        u32 max = mesh.NumLods() - 1;
        bool lodChanged = ImGui::SliderScalar("LOD Level", ImGuiDataType_U32, &lodLevel, &min, &max, nullptr);
        ImGui::SameLine();
        gui::HelpMarker("Lower is higher quality.", state->FontManager->FontDefault.GetFont());
        if (lodChanged)
        {
            mesh.SetLodLevel(lodLevel);
            Scene->NeedsRedraw = true;
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_FILE_EXPORT))
        ImGui::OpenPopup("##MeshExportPopup");
    state->FontManager->FontL.Pop();

    if (ImGui::BeginPopup("##MeshExportPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Export mesh");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //Todo: Support other export options
        //Output format radio selector
        ImGui::Text("Format: ");
        ImGui::SameLine();
        ImGui::RadioButton("Obj", true);

        static string MeshExportPath;
        ImGui::InputText("Export path", &MeshExportPath);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            auto result = OpenFolder();
            if (!result)
                return;

            MeshExportPath = result.value();
        }

        //Disable mesh export button if export is disabled
        if (!meshExportEnabled_)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Export"))
        {
            if (!std::filesystem::exists(MeshExportPath))
            {
                LOG_ERROR("Failed to export {} to obj. Output folder \"{}\" does not exist.", StaticMesh.Name, MeshExportPath);
            }
            else
            {
                //Extract textures used by mesh and get their names
                string diffuseMapName = "";
                string specularMapPath = "";
                string normalMapPath = "";
                //if (DiffuseMapPegPath != "")
                //{
                //    string cpuFilePath = DiffuseMapPegPath;
                //    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                //    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, DiffuseTextureName, MeshExportPath + "\\");
                //    diffuseMapName = String::Replace(DiffuseTextureName, ".tga", ".dds");
                //}
                //if (SpecularMapPegPath != "")
                //{
                //    string cpuFilePath = SpecularMapPegPath;
                //    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                //    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, SpecularTextureName, MeshExportPath + "\\");
                //    specularMapPath = String::Replace(SpecularTextureName, ".tga", ".dds");
                //}
                //if (NormalMapPegPath != "")
                //{
                //    string cpuFilePath = NormalMapPegPath;
                //    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                //    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, NormalTextureName, MeshExportPath + "\\");
                //    normalMapPath = String::Replace(NormalTextureName, ".tga", ".dds");
                //}

                //Write mesh to obj
                //StaticMesh.WriteToObj(GpuFilePath, MeshExportPath, diffuseMapName, specularMapPath, normalMapPath);
            }
        }
        if (!meshExportEnabled_)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        gui::TooltipOnPrevious("You must wait for the mesh to be fully loaded to export it. See the progress bar to the right of the export panel.", state->FontManager->FontDefault.GetFont());

        ImGui::EndPopup();
    }

    //Progress bar and text state of worker thread
    ImGui::SameLine();
    ImVec2 tempPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(tempPos.x, tempPos.y - 5.0f));
    ImGui::Text(WorkerStatusString.c_str());
    ImGui::SetCursorPos(ImVec2(tempPos.x, tempPos.y + ImGui::GetFontSize() + 6.0f));
    ImGui::ProgressBar(WorkerProgressFraction, ImVec2(230.0f, ImGui::GetFontSize() * 1.1f));
}

//Used to end mesh load task early if it was cancelled
#define TaskEarlyExitCheck() if (meshLoadTask_->Cancelled()) return;

void StaticMeshDocument::WorkerThread(Handle<Task> task, GuiState* state)
{
    WorkerStatusString = "Parsing header...";

    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);
    TaskEarlyExitCheck();

    //Read packfile holding the mesh
    Packfile3* packfile = InContainer ? state->PackfileVFS->GetContainer(ParentName, VppName) : state->PackfileVFS->GetPackfile(VppName);
    defer(if (InContainer) delete packfile);
    if (!packfile)
    {
        LOG_ERROR("Failed to get packfile {}/{} for {}", VppName, ParentName, Filename);
        Open = false;
        return;
    }
    packfile->ReadMetadata();

    //Read mesh data
    std::span<u8> cpuFileBytes = packfile->ExtractSingleFile(Filename, true).value();
    std::span<u8> gpuFileBytes = packfile->ExtractSingleFile(gpuFileName, true).value();
    defer(delete[] cpuFileBytes.data());
    defer(delete[] gpuFileBytes.data());

    //Read mesh header
    BinaryReader cpuFileReader(cpuFileBytes);
    BinaryReader gpuFileReader(gpuFileBytes);
    string ext = Path::GetExtension(Filename);
    if (ext == ".csmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xC0FFEE11, 5); //Todo: Move signature + version into class or helper function.
    else if (ext == ".ccmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xFAC351A9, 4);

    Log->info("Mesh vertex format: {}", to_string(StaticMesh.MeshInfo.VertFormat));

    TaskEarlyExitCheck();

    //Get material based on vertex format
    WorkerStatusString = "Setting up scene...";
    VertexFormat format = StaticMesh.MeshInfo.VertFormat;

    //Two steps for each submesh: Get index/vertex buffers and find textures
    u32 numSteps = 2;
    f32 stepSize = 1.0f / (f32)numSteps;

    TaskEarlyExitCheck();

    WorkerStatusString = "Loading mesh...";

    //Read index and vertex buffers from gpu file
    auto maybeMeshData = StaticMesh.ReadMeshData(gpuFileReader);
    if (!maybeMeshData)
    {
        LOG_ERROR("Failed to read mesh data for static mesh document {}", Filename);
        Open = false;
        return;
    }

    //Load mesh and create render object from it
    MeshInstanceData meshData = maybeMeshData.value();
    defer(delete[] meshData.IndexBuffer.data());
    defer(delete[] meshData.VertexBuffer.data());
    Handle<RenderObject> renderObject = Scene->CreateRenderObject(to_string(format), Mesh{ Scene->d3d11Device_, meshData, StaticMesh.NumLods });

    //Set camera position to get a better view of the mesh
    {
        auto& submesh0 = StaticMesh.MeshInfo.Submeshes[0];
        Vec3 pos = submesh0.Bmax - submesh0.Bmin;
        Scene->Cam.SetPosition(pos.z, pos.y, pos.x); //x and z intentionally switched since that usually has a better result
        Scene->Cam.SetNearPlane(0.1f);
        Scene->Cam.Speed = 0.05f;
    }

    WorkerProgressFraction += stepSize;
    meshExportEnabled_ = true;

    bool foundDiffuse = false;
    bool foundSpecular = false;
    bool foundNormal = false;

    //Todo: Fully support RFGs material system. Currently just takes diffuse, normal, and specular textures
    //Remove duplicates
    std::vector<string> textures = StaticMesh.TextureNames;
    std::sort(textures.begin(), textures.end());
    textures.erase(std::unique(textures.begin(), textures.end()), textures.end());
    for (auto& texture : textures)
        texture = String::ToLower(texture);

    //Remove textures that don't fit the current material system of Nanoforge
    textures.erase(std::remove_if(textures.begin(), textures.end(),
        [](string& str)
        {
            return !(String::EndsWith(str, "_d.tga") || String::EndsWith(str, "_n.tga") || String::EndsWith(str, "_s.tga") ||
                     String::EndsWith(str, "_d_low.tga") || String::EndsWith(str, "_n_low.tga") || String::EndsWith(str, "_s_low.tga"));
        }), textures.end());

    //Load textures
    for (auto& textureName : textures)
    {
        //Check if the document was closed. If so, end worker thread early
        if (meshLoadTask_->Cancelled())
            return;

        string textureNameLower = String::ToLower(textureName);
        if (!foundDiffuse && String::Contains(textureNameLower, "_d"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found diffuse texture {} for {}", textureName, Filename);
                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[0] = texture.value();
                DiffuseTextureName = textureName;
                foundDiffuse = true;
            }
            else
            {
                Log->warn("Failed to find diffuse texture {} for {}", textureName, Filename);
            }
        }
        else if (!foundNormal && String::Contains(textureNameLower, "_n"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found normal map {} for {}", textureName, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[2] = texture.value();
                NormalTextureName = textureName;
                foundNormal = true;
            }
            else
            {
                Log->warn("Failed to find normal map {} for {}", textureName, Filename);
            }
        }
        else if (!foundSpecular && String::Contains(textureNameLower, "_s"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found specular map {} for {}", textureName, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[1] = texture.value();
                SpecularTextureName = textureName;
                foundSpecular = true;
            }
            else
            {
                Log->warn("Failed to find specular map {} for {}", textureName, Filename);
            }
        }
    }

    WorkerProgressFraction += stepSize;
    WorkerStatusString = "Done! " ICON_FA_CHECK;
    Log->info("Worker thread for {} finished.", Title);
}