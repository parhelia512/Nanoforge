#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include <tinyxml2/tinyxml2.h>
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include <filesystem>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* wndProcAppPtr = nullptr;

Application::Application(HINSTANCE hInstance)
{
    LoadSettings();
    wndProcAppPtr = this;
    hInstance_ = hInstance;
    fontManager_ = new ImGuiFontManager;
    packfileVFS_ = new PackfileVFS(packfileFolderPath_);
    camera_ = new Camera({ -2573.0f, 2337.0f, 963.0f }, 80.0f, { (f32)windowWidth_, (f32)windowHeight_ }, 1.0f, 10000.0f);
    
    InitRenderer();
    gui_ = new MainGui(fontManager_, packfileVFS_, camera_, renderer_->GetSystemWindowHandle());
    gui_->HandleResize();

    //Init frame timing variables
    deltaTime_ = maxFrameRateDelta;
    frameTimer_.Start();
}

Application::~Application()
{
    delete fontManager_;
    delete packfileVFS_;
    delete renderer_;
    delete camera_;
    delete gui_;
    wndProcAppPtr = nullptr;
}

void Application::Run()
{
    //MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    while(msg.message != WM_QUIT) //Loop while there are messages available
    {
        frameTimer_.Reset();

        //Dispatch messages if present
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (Paused)
        {
            Sleep(100);
        }
        else
        {
            camera_->DoFrame(deltaTime_);
            NewFrame();
            UpdateGui();

            //Check for newly streamed terrain instances that need to be initialized
            if (gui_->NewTerrainInstanceAdded)
            {
                std::lock_guard<std::mutex> lock(gui_->ResourceLock);
                renderer_->InitTerrainMeshes(&gui_->TerrainInstances);
            }

            renderer_->DoFrame(deltaTime_);
        }

        while (frameTimer_.ElapsedSecondsPrecise() < maxFrameRateDelta)
            deltaTime_ = frameTimer_.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    if(renderer_)
        renderer_->HandleResize();
    if (gui_)
        gui_->HandleResize();
}

void Application::InitRenderer()
{
    renderer_ = new DX11Renderer(hInstance_, WndProc, windowWidth_, windowHeight_, fontManager_, camera_);
}

void Application::NewFrame()
{
    //Start new imgui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    renderer_->NewFrame(deltaTime_);
}

void Application::UpdateGui()
{
    gui_->Update(deltaTime_);
}

void Application::LoadSettings()
{
    namespace fs = std::filesystem;
    //Load settings file if it exists
    if (fs::exists("./Settings.xml"))
    {
        tinyxml2::XMLDocument settings;
        settings.LoadFile("./Settings.xml");
        const char* dataPath = settings.FirstChildElement("DataPath")->GetText();
        if (!dataPath)
            throw std::exception("Error! Failed to get DataPath from Settings.xml");

        packfileFolderPath_ = string(dataPath);
    }
    else //Otherwise recreate it with the default values
    {
        tinyxml2::XMLDocument settings;
        auto* dataPath = settings.NewElement("DataPath");
        settings.InsertFirstChild(dataPath);
        dataPath->SetText(packfileFolderPath_.c_str());
        settings.SaveFile("./Settings.xml");
    }
}

//Todo: Pass key & mouse messages to InputManager and have it send input messages to other parts of code via callbacks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    wndProcAppPtr->camera_->HandleInput(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_KEYDOWN: //Key down
        //If escape key pressed display popup box
        if (wParam == VK_ESCAPE) 
        {
            if (MessageBox(0, "Are you sure you want to exit?",
                "Confirm exit", MB_YESNO | MB_ICONQUESTION) == IDYES)

                //Release the windows allocated memory  
                DestroyWindow(hwnd);
        }
        if (wParam == VK_F1)
        {
            wndProcAppPtr->gui_->CanStartInit = true;
        }
        return 0;
    case WM_DESTROY: //Exit if window close button pressed
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            wndProcAppPtr->Paused = true;
        }
        else
        {
            wndProcAppPtr->Paused = false;
            wndProcAppPtr->frameTimer_.Reset();
        }
        return 0;
    case WM_SIZE:
        wndProcAppPtr->HandleResize();
        return 0;
    }

    //Return the message for windows to handle it
    return DefWindowProc(hwnd, msg, wParam, lParam);
}