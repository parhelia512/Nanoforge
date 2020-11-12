#include "StaticMeshDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/RfgUtil.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "Log.h"
#include "gui/documents/PegHelpers.h"
#include <optional>

#ifdef DEBUG_BUILD
    const string shaderFolderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/";
#else
    const string shaderFolderPath_ = "./Assets/shaders/";
#endif

void StaticMeshDocument_WorkerThread(GuiState* state, Document& doc);
//Finds a texture and creates a directx texture resource from it. textureName is the textureName of a texture inside a cpeg/cvbm. So for example, sledgehammer_high_n.tga, which is in sledgehammer_high.cpeg_pc
//Will try to find a high res version of the texture first if lookForHighResVariant is true.
//Will return a default texture if the target isn't found.
std::optional<Texture2D> FindTexture(GuiState* state, Document& doc, const string& name, bool lookForHighResVariant = false);

void StaticMeshDocument_Init(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;

    //Create scene instance and store index
    data->SceneIndex = state->Renderer->Scenes.size();
    state->Renderer->CreateScene();

    //Create worker thread to load terrain meshes in background
    //data->WorkerFuture = std::async(std::launch::async, &StaticMeshDocument_WorkerThread, state, doc);

    //Todo: Move into worker thread once working. Just here for prototyping
    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(data->Filename);

    //Get path to cpu file and gpu file in cache
    data->CpuFilePath = data->InContainer ?
        state->PackfileVFS->GetFile(data->VppName, data->ParentName, data->Filename) :
        state->PackfileVFS->GetFile(data->VppName, data->Filename);
    data->GpuFilePath = data->InContainer ?
        state->PackfileVFS->GetFile(data->VppName, data->ParentName, gpuFileName) :
        state->PackfileVFS->GetFile(data->VppName, gpuFileName);

    //Read cpu file
    BinaryReader cpuFileReader(data->CpuFilePath);
    BinaryReader gpuFileReader(data->GpuFilePath);
    string ext = Path::GetExtension(data->Filename);
    //Todo: Move signature + version into class or helper function. Users of StaticMesh::Read shouldn't need to know these to use it
    if (ext == ".csmesh_pc")
        data->StaticMesh.Read(cpuFileReader, data->Filename, 0xC0FFEE11, 5);
    else if (ext == ".ccmesh_pc")
        data->StaticMesh.Read(cpuFileReader, data->Filename, 0xFAC351A9, 4);

    Log->info("Mesh vertex format: {}", to_string(data->StaticMesh.VertexBufferConfig.Format));

    //Init scene and camera
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];
    scene.Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)scene.Width(), (f32)scene.Height() }, 1.0f, 10000.0f);
    scene.Cam.Speed = 0.25f;
    scene.Cam.SprintSpeed = 0.4f;
    scene.Cam.LookAt({ 0.0f, 0.0f, 0.0f });

    //Pixlit0Uv = 30,
    //Pixlit1Uv = 31,
    //Pixlit1UvCa = 32,
    //          Pixlit1UvNmap = 33,
    //          Pixlit1UvNmapCa = 34,
    //Pixlit2Uv = 35,
    //Pixlit2UvCa = 36,
    //          Pixlit2UvNmap = 37,
    //Pixlit2UvNmapCa = 38,
    //Pixlit3Uv = 39,
    //Pixlit3UvCa = 40,
    //Pixlit3UvNmap = 41,
    //          Pixlit3UvNmapCa = 42,
    //Pixlit4Uv = 43,
    //Pixlit4UvCa = 44,
    //Pixlit4UvNmap = 45,
    //Pixlit4UvNmapCa = 46,

    //Todo: Put this in renderer / RenderObject code somewhere so it can be reused by other mesh code
    //Vary input and shader based on vertex format
    VertexFormat format = data->StaticMesh.VertexBufferConfig.Format;
    scene.SetShader(shaderFolderPath_ + to_string(format) + ".fx");
    if (format == VertexFormat::Pixlit1UvNmap)
    {
        scene.SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        });
    }
    else if (format == VertexFormat::Pixlit1UvNmapCa)
    {
        scene.SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        });
    }
    else if (format == VertexFormat::Pixlit2UvNmap)
    {
        scene.SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        });
    }
    else if (format == VertexFormat::Pixlit3UvNmap)
    {
        scene.SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit3UvNmapCa)
    {
        scene.SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        });
    }

    for (u32 i = 0; i < data->StaticMesh.SubMeshes.size(); i++)
    {
        //Read index and vertex buffers from gpu file
        auto maybeMeshData = data->StaticMesh.ReadSubmeshData(gpuFileReader, i);
        if (!maybeMeshData)
            THROW_EXCEPTION("Failed to get mesh data for static mesh doc in StaticMesh::ReadSubmeshData()");

        MeshInstanceData meshData = maybeMeshData.value();
        auto& renderObject = scene.Objects.emplace_back();
        Mesh mesh;
        mesh.Create(scene.d3d11Device_, scene.d3d11Context_, meshData.VertexBuffer, meshData.IndexBuffer,
            data->StaticMesh.VertexBufferConfig.NumVerts, DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        renderObject.Create(mesh, Vec3{ 0.0f, 0.0f, 0.0f });
        renderObject.SetScale(25.0f);

        for (auto& textureName : data->StaticMesh.TextureNames)
        {
            string textureNameLower = String::ToLower(textureName);
            bool isLowResTexture = String::Contains(textureNameLower, "_low_");
            string highResName = isLowResTexture ? String::Replace(textureNameLower, "_low_", "_") : textureNameLower;
            if (String::Contains(textureNameLower, "_d"))
            {
                auto texture = FindTexture(state, doc, textureNameLower, true);
                if (texture)
                {
                    Log->info("Found diffuse texture {} for {}", textureNameLower, data->Filename);
                    data->DiffuseTexture = texture.value();
                    renderObject.UseTextures = true;
                    renderObject.DiffuseTexture = texture.value();
                }
                else
                {
                    Log->warn("Failed to find diffuse texture {} for {}", textureNameLower, data->Filename);
                }
            }
            else if (String::Contains(textureNameLower, "_n"))
            {
                auto texture = FindTexture(state, doc, textureNameLower, true);
                if (texture)
                {
                    Log->info("Found normal map {} for {}", textureNameLower, data->Filename);
                    data->NormalTexture = texture.value();
                    renderObject.UseTextures = true;
                    renderObject.NormalTexture = texture.value();
                }
                else
                {
                    Log->warn("Failed to find normal map {} for {}", textureNameLower, data->Filename);
                }
            }
            else if (String::Contains(textureNameLower, "_s"))
            {
                auto texture = FindTexture(state, doc, textureNameLower, true);
                if (texture)
                {
                    Log->info("Found specular map {} for {}", textureNameLower, data->Filename);
                    data->SpecularTexture = texture.value();
                    renderObject.UseTextures = true;
                    renderObject.SpecularTexture = texture.value();
                }
                else
                {
                    Log->warn("Failed to find specular map {} for {}", textureNameLower, data->Filename);
                }
            }
        }

        //Clear mesh data
        delete[] meshData.IndexBuffer.data();
        delete[] meshData.VertexBuffer.data();
    }
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, Document& doc);

void StaticMeshDocument_Update(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }

    //Camera only handles input if window is focused
    scene.Cam.InputActive = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    scene.HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(scene.ClearColor.x, scene.ClearColor.y, scene.ClearColor.z, scene.ClearColor.w));
    ImGui::Image(scene.GetView(), ImVec2(static_cast<f32>(scene.Width()), static_cast<f32>(scene.Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    StaticMeshDocument_DrawOverlayButtons(state, doc);

    ImGui::End();
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

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

        f32 fov = scene.Cam.GetFov();
        f32 nearPlane = scene.Cam.GetNearPlane();
        f32 farPlane = scene.Cam.GetFarPlane();
        f32 lookSensitivity = scene.Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) scene.Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) scene.Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) scene.Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) scene.Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) scene.Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) scene.Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &scene.Cam.Speed);
        ImGui::InputFloat("Sprint speed", &scene.Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::InputFloat("Fov", &fov))
            scene.Cam.SetFov(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            scene.Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            scene.Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            scene.Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&scene.Cam.camPosition))
        {
            scene.Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch: ", std::to_string(scene.Cam.GetPitch()));
        gui::LabelAndValue("Yaw: ", std::to_string(scene.Cam.GetYaw()));

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

        ImGui::SetNextItemWidth(175.0f);
        static float tempScale = 1.0f;
        ImGui::InputFloat("Scale", &tempScale);
        ImGui::SameLine();
        if (ImGui::Button("Set all"))
        {
            scene.Objects[0].Scale.x = tempScale;
            scene.Objects[0].Scale.y = tempScale;
            scene.Objects[0].Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&scene.Objects[0].Scale, 0.01, 1.0f, 100.0f);

        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&scene.perFrameStagingBuffer_.DiffuseColor));
        ImGui::SliderFloat("Diffuse intensity", &scene.perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 1.0f);

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_INFO_CIRCLE))
        ImGui::OpenPopup("##MeshInfoPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##MeshInfoPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Mesh info");
        state->FontManager->FontL.Pop();
        ImGui::Separator();



        ImGui::EndPopup();
    }
}

//Tries to open a cpeg/cvbm pegName in the provided packfile and create a Texture2D from a sub-texture with the name textureName
std::optional<Texture2D> GetTextureFromPeg(GuiState* state, Document& doc, Packfile3* parent, const string& pegName, const string& textureName)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //Get gpu filename
    string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(pegName);

    //Get bytes for cpu file and gpu file
    auto cpuFileBytes = parent->ExtractSingleFile(pegName, true);
    if (!cpuFileBytes)
        return {};
    auto gpuFileBytes = parent->ExtractSingleFile(gpuFilename, true);
    if (!gpuFileBytes)
    {
        //Must release alllocated cpuFileBytes
        delete[] cpuFileBytes.value().data();
        return {};
    }

    //Parse peg file
    std::optional<Texture2D> out = {};
    BinaryReader cpuFileReader(cpuFileBytes.value());
    BinaryReader gpuFileReader(gpuFileBytes.value());
    PegFile10 peg;
    peg.Read(cpuFileReader, gpuFileReader);
    
    //See if target texture is in peg. If so extract it and create a Texture2D from it
    for (auto& entry : peg.Entries)
    {
        if (String::EqualIgnoreCase(entry.Name, textureName))
        {
            peg.ReadTextureData(gpuFileReader, entry);
            std::span<u8> textureData = entry.RawData;
            
            //Create and setup texture2d
            Texture2D texture2d;
            DXGI_FORMAT dxgiFormat = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat);
            D3D11_SUBRESOURCE_DATA textureSubresourceData;
            textureSubresourceData.pSysMem = textureData.data();
            textureSubresourceData.SysMemSlicePitch = 0;
            textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, entry.Width, entry.Height);
            texture2d.Create(scene.d3d11Device_, entry.Width, entry.Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
            texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
            texture2d.CreateSampler(); //Need sampler too

            out = texture2d;
        }
    }

    //Release allocated memory and return output
    peg.Cleanup();
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();
    return out;
}

//Tries to find a cpeg with a subtexture with the provided name and create a Texture2D from it. Searches all cpeg/cvbm files in packfile. First checks pegs then searches in str2s
std::optional<Texture2D> GetTextureFromPackfile(GuiState* state, Document& doc, Packfile3* packfile, const string& textureName, bool isStr2 = false)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //First search top level cpeg/cvbm files
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        Packfile3Entry& entry = packfile->Entries[i];
        const char* entryName = packfile->EntryNames[i];
        string ext = Path::GetExtension(entryName);

        //Try to get texture from each cpeg/cvbm
        if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
        {
            auto texture = GetTextureFromPeg(state, doc, packfile, entryName, textureName);
            if (texture)
                return texture;
        }
    }

    //Then search inside each str2 if this packfile isn't a str2
    if (!isStr2)
    {
        for (u32 i = 0; i < packfile->Entries.size(); i++)
        {
            Packfile3Entry& entry = packfile->Entries[i];
            const char* entryName = packfile->EntryNames[i];
            string ext = Path::GetExtension(entryName);

            //Try to get texture from each str2
            if (ext == ".str2_pc")
            {
                //Find container
                auto containerBytes = packfile->ExtractSingleFile(entryName, false);
                if (!containerBytes)
                    continue;

                //Parse container and get file byte buffer
                Packfile3 container(containerBytes.value());
                container.ReadMetadata();
                auto texture = GetTextureFromPackfile(state, doc, &container, textureName, true);
                if (texture)
                    return texture;
            }
        }
    }

    //If didn't find texture at this point then failed. Return empty
    return {};
}

std::optional<Texture2D> GetTexture(GuiState* state, Document& doc, const string& textureName)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //First search current str2_pc if the mesh is inside one
    if (data->InContainer)
    {
        Packfile3* container = state->PackfileVFS->GetContainer(data->ParentName, data->VppName);
        auto texture = GetTextureFromPackfile(state, doc, container, textureName, true);
        delete container; //Delete container since they're loaded on demand

        //Return texture if it was found
        if (texture)
            return texture;
    }

    //Then search parent vpp
    Packfile3* parentPackfile = state->PackfileVFS->GetPackfile(data->VppName);
    return GetTextureFromPackfile(state, doc, parentPackfile, textureName); //Return regardless here since it's our last search option
}

std::optional<Texture2D> FindTexture(GuiState* state, Document& doc, const string& name, bool lookForHighResVariant)
{
    //Look for high res variant if requested and string fits high res search requirements
    if (lookForHighResVariant && String::Contains(name, "_low_"))
    {
        //Replace _low_ with _. This is the naming scheme I've seen many high res variants follow
        string highResName = String::Replace(name, "_low_", "_");
        auto texture = GetTexture(state, doc, highResName);
        
        //Return high res variant if it was found
        if (texture)
        {
            Log->info("Found high res variant of {}: {}", name, highResName);
            return texture;
        }
    }

    //Else look for the specified texture
    return GetTexture(state, doc, name);
}

void StaticMeshDocument_OnClose(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //Delete scene and free its resources
    state->Renderer->DeleteScene(data->SceneIndex);

    //Free document data
    delete data;
}

void StaticMeshDocument_WorkerThread(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];


}