#pragma once
#include "common/Typedefs.h"
#include "rfg/TerrainHelpers.h"
#include "render/resources/Scene.h"
#include <ext/WindowsWrapper.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <array>
#include <filesystem>
#include <span>

class ImGuiFontManager;
class Camera;
using ImTextureID = void*;

class DX11Renderer
{
public:
    void Init(HINSTANCE hInstance, WNDPROC wndProc, u32 WindowWidth, u32 WindowHeight, ImGuiFontManager* fontManager);
    ~DX11Renderer();

    void NewFrame(f32 deltaTime) const;
    void DoFrame(f32 deltaTime);
    void HandleResize();
    HWND GetSystemWindowHandle() { return hwnd_; }
    
    //Creates a shader resource view from the provided input and returns it. Up to user to free it once they're done
    ImTextureID TextureDataToHandle(std::span<u8> data, DXGI_FORMAT format, u32 width, u32 height);
    u32 WindowWidth() { return windowWidth_; }
    u32 WindowHeight() { return windowHeight_; }

    void CreateScene();
    void DeleteScene(u32 index);

    std::vector<Scene> Scenes = {};

private:
    //Todo: Add a callback so viewport windows can be written outside of the renderer
    void ImGuiDoFrame();
    [[nodiscard]] bool InitWindow(WNDPROC wndProc);
    [[nodiscard]] bool InitDx11();
    //Initialize swapchain and resources it uses. Run each time the window is resized
    [[nodiscard]] bool InitSwapchainAndResources();
    [[nodiscard]] bool InitImGui() const;

    [[nodiscard]] bool CreateDevice();
    [[nodiscard]] bool CreateSwapchain();
    [[nodiscard]] bool CreateRenderTargetView();
    [[nodiscard]] bool AcquireDxgiFactoryInstance();
    void UpdateWindowDimensions();


    ImGuiFontManager* fontManager_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    u32 windowWidth_ = 800;
    u32 windowHeight_ = 800;

    IDXGIFactory* dxgiFactory_ = nullptr;
    ID3D11Device* d3d11Device_ = nullptr;
    ID3D11DeviceContext* d3d11Context_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* renderTargetView_ = nullptr;

    ID3D11RasterizerState* rasterizerState_ = nullptr;
    ID3D11BlendState* blendState_ = nullptr;
    ID3D11DepthStencilState* depthStencilState_ = nullptr;
    D3D11_VIEWPORT viewport;
    DirectX::XMFLOAT4 clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };

    bool initialized_ = false;
};