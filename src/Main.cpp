
#include <time.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <ctime>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#undef min
#undef max
#endif

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "GL/gl3w.h"
#include "tinydir.h"

#include "PTScene.h"
#include "Loader.h"
#include "GLTFLoader.h"
#include "PTRenderer.h"

#include "stb_image_write.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

using namespace std;
using namespace PathTraceAlg;

Scene* scene = nullptr;
Renderer* renderer = nullptr;

std::vector<string> availableSceneFiles;
std::vector<string> environmentMaps;

float mouseSensitivity = 0.01f;
int currentSceneIndex = 0;
int selectedInstance = 0;
double lastFrameTime = SDL_GetTicks();
int currentEnvironmentMapIndex = 0;
bool shouldExit = false;
bool uiInitialized = false;
bool uiAutoTrajectory = false;
int uiTrajectoryFrame = 0;
Vec3 uiHomeEye;
Vec3 uiHomePivot;
Vec3 uiHomeRight;
float uiHomeRadius = 1.0f;
std::string currentScenePath;

struct UserInterfaceState
{
    bool leftPanelOpen = true;
    bool rightPanelOpen = true;
    bool topBarOpen = true;
    bool statusBarOpen = true;
    bool focusMode = false;
    float leftPanelSlide = 1.0f;
    float rightPanelSlide = 1.0f;
    float topBarSlide = 1.0f;
    float statusBarSlide = 1.0f;
    float opacity = 0.50f;
    float ghostingProtection = 0.78f;
    int debugView = 0;
    std::string statusMessage = "Ready";
};

UserInterfaceState userInterface;

constexpr float kLeftPanelWidth = 330.0f;
constexpr float kRightPanelWidth = 430.0f;

// Mouse state for camera control
bool isLeftMouseButtonDown = false;
bool isMiddleMouseButtonDown = false;
int previousMouseX = 0;
int previousMouseY = 0;

std::string shaderDirectory = "../src/shaders/";
std::string assetsDirectory = "../assets/myscene/";
std::string environmentMapDirectory = "../assets/HDR/";

RenderOptions renderOptions;

struct ResearchExperiment
{
    bool enabled = false;
    std::string outputDirectory;
    std::string methodName = "proposed";
    int temporalMode = 3;
    int frame = 0;
    int totalFrames = 180;
    int pass = 0;
    int accumulatedSpp = 0;
    int spp = 1;
    int referenceSpp = 256;
    int width = 640;
    int height = 360;
    Vec3 initialEye;
    Vec3 initialPivot;
    Vec3 initialRight;
    float initialRadius = 1.0f;
    std::ofstream timingFile;
};

ResearchExperiment research;

struct LoopData
{
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mGLContext = nullptr;
};

float SmoothStep(float value);

bool UIWantsMouse()
{
    return uiInitialized && ImGui::GetIO().WantCaptureMouse;
}

bool UIWantsKeyboard()
{
    return uiInitialized && ImGui::GetIO().WantCaptureKeyboard;
}

void CaptureUICameraHome()
{
    if (!scene || !scene->camera) return;
    uiHomeEye = scene->camera->position;
    uiHomePivot = scene->camera->GetPivot();
    uiHomeRight = scene->camera->right;
    uiHomeRadius = std::max(Vec3::Distance(uiHomeEye, uiHomePivot), 0.05f);
    uiTrajectoryFrame = 0;
}

void ApplyUICameraTrajectory()
{
    if (!uiAutoTrajectory || research.enabled || !scene || !renderer) return;

    int canonicalFrame = uiTrajectoryFrame % 180;
    float lateral = 0.0f;
    float angleDegrees = 0.0f;
    if (canonicalFrame >= 30)
        lateral = 0.12f * uiHomeRadius * SmoothStep((canonicalFrame - 30) / 59.0f);
    if (canonicalFrame >= 90 && canonicalFrame < 120)
        angleDegrees = 8.0f * SmoothStep((canonicalFrame - 90) / 29.0f);
    else if (canonicalFrame >= 120 && canonicalFrame < 150)
        angleDegrees = 8.0f + 12.0f * SmoothStep((canonicalFrame - 120) / 29.0f);
    else if (canonicalFrame >= 150)
        angleDegrees = 20.0f;

    Vec3 shiftedPivot = uiHomePivot + uiHomeRight * lateral;
    Vec3 offset = uiHomeEye - uiHomePivot;
    float radians = angleDegrees * 3.14159265358979323846f / 180.0f;
    Vec3 rotatedOffset(cosf(radians) * offset.x + sinf(radians) * offset.z,
                       offset.y,
                       -sinf(radians) * offset.x + cosf(radians) * offset.z);
    scene->camera->SetLookAt(shiftedPivot + rotatedOffset, shiftedPivot);
    scene->dirty = true;
    uiTrajectoryFrame = (uiTrajectoryFrame + 1) % 180;
}

int TemporalModeFromName(const std::string& name)
{
    if (name == "none") return 0;
    if (name == "fixed") return 1;
    if (name == "geometry") return 2;
    if (name == "proposed") return 3;
    if (name == "inverse_variance") return 4;
    if (name == "no_luminance") return 5;
    if (name == "no_motion") return 6;
    if (name == "reference") return 7;
    return -1;
}

bool EnsureDirectory(const std::string& directory)
{
    if (directory.empty()) return false;
    std::string path = directory;
    std::replace(path.begin(), path.end(), '\\', '/');
    for (size_t index = 1; index <= path.size(); ++index)
    {
        if (index != path.size() && path[index] != '/') continue;
        std::string part = path.substr(0, index);
        if (part.empty() || part.back() == ':') continue;
        if (_mkdir(part.c_str()) != 0 && errno != EEXIST) return false;
    }
    return true;
}

float SmoothStep(float value)
{
    value = std::max(0.0f, std::min(value, 1.0f));
    return value * value * (3.0f - 2.0f * value);
}

void ApplyResearchCameraPose()
{
    if (!research.enabled) return;

    int canonicalFrame = research.totalFrames <= 1 ? 0 :
        (research.frame * 179) / (research.totalFrames - 1);
    float lateral = 0.0f;
    float angleDegrees = 0.0f;
    if (canonicalFrame >= 30)
        lateral = 0.12f * research.initialRadius *
                  SmoothStep((canonicalFrame - 30) / 59.0f);
    if (canonicalFrame >= 90 && canonicalFrame < 120)
        angleDegrees = 8.0f * SmoothStep((canonicalFrame - 90) / 29.0f);
    else if (canonicalFrame >= 120 && canonicalFrame < 150)
        angleDegrees = 8.0f + 12.0f * SmoothStep((canonicalFrame - 120) / 29.0f);
    else if (canonicalFrame >= 150)
        angleDegrees = 20.0f;

    Vec3 shiftedPivot = research.initialPivot + research.initialRight * lateral;
    Vec3 offset = research.initialEye - research.initialPivot;
    float radians = angleDegrees * 3.14159265358979323846f / 180.0f;
    Vec3 rotatedOffset(cosf(radians) * offset.x + sinf(radians) * offset.z,
                       offset.y,
                       -sinf(radians) * offset.x + cosf(radians) * offset.z);
    scene->camera->SetLookAt(shiftedPivot + rotatedOffset, shiftedPivot);
    scene->dirty = true;

    if (research.temporalMode == 7)
    {
        renderOptions.temporalSpp = std::max(1, std::min(8,
            research.referenceSpp - research.accumulatedSpp));
        scene->renderOptions.temporalSpp = renderOptions.temporalSpp;
        if (research.pass == 0)
            renderer->ResetTemporalHistory();
    }
}

void FinishResearchFrame()
{
    if (!research.enabled) return;

    research.timingFile << research.frame << ',' << research.pass << ','
        << std::fixed << std::setprecision(4)
        << renderer->GetPathTraceMilliseconds() << ','
        << renderer->GetTemporalMilliseconds() << ','
        << renderer->GetPathTraceMilliseconds() + renderer->GetTemporalMilliseconds()
        << '\n';

    bool poseComplete = true;
    if (research.temporalMode == 7)
    {
        research.accumulatedSpp += scene->renderOptions.temporalSpp;
        poseComplete = research.accumulatedSpp >= research.referenceSpp;
    }

    if (poseComplete)
    {
        std::ostringstream prefix;
        prefix << research.outputDirectory << "/frame_"
               << std::setw(4) << std::setfill('0') << research.frame;
        renderer->ExportResearchFrame(prefix.str());
        research.frame++;
        research.pass = 0;
        research.accumulatedSpp = 0;
        if (research.frame >= research.totalFrames)
        {
            research.timingFile.flush();
            printf("Research sequence complete: %s\n", research.outputDirectory.c_str());
            shouldExit = true;
        }
    }
    else
    {
        research.pass++;
    }
}

void LoadAvailableSceneFiles()
{
    tinydir_dir dir;
    int i;
    tinydir_open_sorted(&dir, assetsDirectory.c_str());

    for (i = 0; i < dir.n_files; i++)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        std::string ext = std::string(file.extension);
        if (ext == "scene" || ext == "gltf" || ext == "glb")
        {
            availableSceneFiles.push_back(assetsDirectory + std::string(file.name));
        }
    }

    tinydir_close(&dir);
}

void LoadEnvironmentMaps()
{
    tinydir_dir dir;
    int i;
    tinydir_open_sorted(&dir, environmentMapDirectory.c_str());

    for (i = 0; i < dir.n_files; i++)
    {
        tinydir_file file;
        tinydir_readfile_n(&dir, &file, i);

        std::string ext = std::string(file.extension);
        if (ext == "hdr")
        {
            environmentMaps.push_back(environmentMapDirectory + std::string(file.name));
        }
    }

    tinydir_close(&dir);
}

void LoadSceneFromFile(std::string sceneFilePath)
{
    delete scene;
    scene = new Scene();
    std::string fileExtension = sceneFilePath.substr(sceneFilePath.find_last_of(".") + 1);

    bool loadSuccess = false;
    Mat4 transform;

    if (fileExtension == "scene")
        loadSuccess = LoadSceneFromFile(sceneFilePath, scene, renderOptions);
    else if (fileExtension == "gltf")
        loadSuccess = LoadGLTF(sceneFilePath, scene, renderOptions, transform, false);
    else if (fileExtension == "glb")
        loadSuccess = LoadGLTF(sceneFilePath, scene, renderOptions, transform, true);

    if (!loadSuccess)
    {
        printf("Unable to load scene\n");
        exit(0);
    }

    selectedInstance = 0;

    // Add a default HDR if there are no lights in the scene
    if (!scene->envMap && !environmentMaps.empty())
    {
        scene->AddEnvMap(environmentMaps[currentEnvironmentMapIndex]);
        renderOptions.enableEnvMap = scene->lights.empty() ? true : false;
    }

    scene->renderOptions = renderOptions;
    currentScenePath = sceneFilePath;
    CaptureUICameraHome();
}

bool InitializePathTracerRenderer()
{
    delete renderer;
    renderer = new Renderer(scene, shaderDirectory);
    renderer->SetDebugView(userInterface.debugView);
    return true;
}

void SaveScreenshot(const std::string filename)
{
    unsigned char* imageData = nullptr;
    int width, height;
    renderer->GetOutputBuffer(&imageData, width, height);
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), width, height, 4, imageData, width * 4);
    printf("Frame saved: %s\n", filename.c_str());
    delete[] imageData;
}

std::string FileNameFromPath(const std::string& path)
{
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

void SetUIStatus(const std::string& message)
{
    userInterface.statusMessage = message;
}

void ApplyOptionsFromUI(bool reloadShaders = false, bool resetTemporalHistory = false)
{
    if (!scene || !renderer) return;
    scene->renderOptions = renderOptions;
    scene->dirty = true;
    if (reloadShaders)
        renderer->ReloadShaders();
    if (resetTemporalHistory)
        renderer->ResetTemporalHistory();
}

void ResetUICamera()
{
    if (!scene || !scene->camera) return;
    scene->camera->SetLookAt(uiHomeEye, uiHomePivot);
    scene->dirty = true;
    uiTrajectoryFrame = 0;
    SetUIStatus("Camera reset");
}

std::string MakeScreenshotPath()
{
    EnsureDirectory("../captures");
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream path;
    path << "../captures/render_" << std::put_time(&localTime, "%Y%m%d_%H%M%S")
         << "_spp" << (renderer ? renderer->GetSampleCount() : 0) << ".png";
    return path.str();
}

void SaveUIScreenshot()
{
    const std::string path = MakeScreenshotPath();
    SaveScreenshot(path);
    SetUIStatus("Screenshot saved to captures/");
}

#if defined(_WIN32)
bool OpenModelDialog(std::string& selectedPath)
{
    char filePath[4096] = {};
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = static_cast<DWORD>(sizeof(filePath));
    dialog.lpstrFilter = "3D scenes (*.glb;*.gltf;*.scene)\0*.glb;*.gltf;*.scene\0All files (*.*)\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrInitialDir = assetsDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&dialog))
        return false;
    selectedPath = filePath;
    return true;
}
#endif

void LoadSceneFromUI(const std::string& path)
{
    LoadSceneFromFile(path);
    InitializePathTracerRenderer();
    userInterface.debugView = 0;
    renderer->SetDebugView(0);
    SetUIStatus("Loaded " + FileNameFromPath(path));
}

void ConfigureUserInterfaceStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(13.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 7.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.067f, 0.086f, 1.0f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.22f, 0.27f, 0.78f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.18f, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.20f, 0.26f, 0.96f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.21f, 0.25f, 0.32f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.13f, 0.16f, 0.21f, 0.95f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.27f, 0.42f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.36f, 0.62f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.25f, 0.40f, 0.82f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.33f, 0.52f, 0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.40f, 0.68f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.48f, 0.63f, 1.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.48f, 0.63f, 1.0f, 0.88f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.72f, 1.0f, 1.0f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.23f, 0.34f, 0.58f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.36f, 0.60f, 0.90f);
    colors[ImGuiCol_Separator] = ImVec4(0.21f, 0.24f, 0.30f, 0.82f);
}

void HideAllUserInterface()
{
    userInterface.leftPanelOpen = false;
    userInterface.rightPanelOpen = false;
    userInterface.topBarOpen = false;
    userInterface.statusBarOpen = false;
    userInterface.focusMode = true;
}

void RestoreUserInterface()
{
    userInterface.leftPanelOpen = true;
    userInterface.rightPanelOpen = true;
    userInterface.topBarOpen = true;
    userInterface.statusBarOpen = true;
    userInterface.focusMode = false;
}

float AnimatePanel(float currentValue, bool open)
{
    const float target = open ? 1.0f : 0.0f;
    const float step = std::min(ImGui::GetIO().DeltaTime * 11.0f, 1.0f);
    return currentValue + (target - currentValue) * step;
}

void DrawTopBar(const ImVec2& displaySize)
{
    const float height = 46.0f;
    userInterface.topBarSlide = AnimatePanel(userInterface.topBarSlide, userInterface.topBarOpen);
    const float y = -height * (1.0f - userInterface.topBarSlide);

    ImGui::SetNextWindowPos(ImVec2(0.0f, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(userInterface.opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##PathTracerTopBar", nullptr, flags);

    if (ImGui::BeginTable("##TopBarLayout", 3, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Brand", ImGuiTableColumnFlags_WidthFixed, 185.0f);
        ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 370.0f);
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.68f, 1.0f, 1.0f));
        ImGui::TextUnformatted("PATH TRACER STUDIO");
        ImGui::PopStyleColor();

        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", FileNameFromPath(currentScenePath).c_str());

        ImGui::TableNextColumn();
        const float displayedFps = std::min(ImGui::GetIO().Framerate, 9999.0f);
        if (ImGui::BeginTable("##TopActions", 5,
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadInnerX))
        {
            ImGui::TableSetupColumn("FPS", ImGuiTableColumnFlags_WidthFixed, 76.0f);
            ImGui::TableSetupColumn("UILabel", ImGuiTableColumnFlags_WidthFixed, 24.0f);
            ImGui::TableSetupColumn("Opacity", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn("Collapse", ImGuiTableColumnFlags_WidthFixed, 32.0f);

            ImGui::TableNextColumn();
            ImGui::Text("%4.0f FPS", displayedFps);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("UI");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(94.0f);
            float opacityPercent = userInterface.opacity * 100.0f;
            if (ImGui::SliderFloat("##UIOpacity", &opacityPercent, 35.0f, 100.0f, "%.0f%%",
                                   ImGuiSliderFlags_NoInput))
                userInterface.opacity = opacityPercent / 100.0f;
            ImGui::TableNextColumn();
            if (ImGui::Button("Hide UI", ImVec2(72.0f, 0.0f)))
                HideAllUserInterface();
            ImGui::TableNextColumn();
            if (ImGui::Button("^##HideTop", ImVec2(28.0f, 0.0f)))
                userInterface.topBarOpen = false;
            ImGui::EndTable();
        }
        ImGui::EndTable();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void DrawTopRestoreHandle(const ImVec2& displaySize)
{
    if (userInterface.topBarOpen || userInterface.topBarSlide > 0.04f) return;
    ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f - 34.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(68.0f, 28.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##ShowTopHandle", nullptr, flags);
    if (ImGui::Button("v  UI", ImVec2(68.0f, 25.0f)))
    {
        userInterface.topBarOpen = true;
        userInterface.statusBarOpen = true;
        userInterface.focusMode = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawLeftPanel(float top, float bottom, const ImVec2& displaySize)
{
    const float width = kLeftPanelWidth;
    userInterface.leftPanelSlide = AnimatePanel(userInterface.leftPanelSlide, userInterface.leftPanelOpen);
    const float x = -width * (1.0f - userInterface.leftPanelSlide);

    ImGui::SetNextWindowPos(ImVec2(x, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, std::max(160.0f, displaySize.y - top - bottom)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(userInterface.opacity);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##SceneCameraPanel", nullptr, flags);

    ImGui::SeparatorText("SCENE");
#if defined(_WIN32)
    if (ImGui::Button("Open Model...", ImVec2(-1.0f, 0.0f)))
    {
        std::string selectedPath;
        if (OpenModelDialog(selectedPath))
        {
            if (std::find(availableSceneFiles.begin(), availableSceneFiles.end(), selectedPath) == availableSceneFiles.end())
                availableSceneFiles.push_back(selectedPath);
            currentSceneIndex = static_cast<int>(std::find(availableSceneFiles.begin(), availableSceneFiles.end(), selectedPath) - availableSceneFiles.begin());
            LoadSceneFromUI(selectedPath);
        }
    }
#endif

    const std::string scenePreview = availableSceneFiles.empty() ? "No scenes" : FileNameFromPath(currentScenePath);
    if (ImGui::BeginCombo("Active scene", scenePreview.c_str()))
    {
        for (int index = 0; index < static_cast<int>(availableSceneFiles.size()); ++index)
        {
            const bool selected = index == currentSceneIndex;
            if (ImGui::Selectable(FileNameFromPath(availableSceneFiles[index]).c_str(), selected))
            {
                currentSceneIndex = index;
                LoadSceneFromUI(availableSceneFiles[index]);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const std::string environmentPreview = environmentMaps.empty() ? "None" :
        FileNameFromPath(environmentMaps[currentEnvironmentMapIndex]);
    if (ImGui::BeginCombo("Environment", environmentPreview.c_str()))
    {
        for (int index = 0; index < static_cast<int>(environmentMaps.size()); ++index)
        {
            const bool selected = index == currentEnvironmentMapIndex;
            if (ImGui::Selectable(FileNameFromPath(environmentMaps[index]).c_str(), selected))
            {
                currentEnvironmentMapIndex = index;
                scene->AddEnvMap(environmentMaps[index]);
                ApplyOptionsFromUI(false, true);
                SetUIStatus("Environment map changed");
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (scene)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Triangles"); ImGui::SameLine(112.0f); ImGui::Text("%d", static_cast<int>(scene->vertIndices.size()));
        ImGui::TextDisabled("Materials"); ImGui::SameLine(112.0f); ImGui::Text("%d", static_cast<int>(scene->materials.size()));
        ImGui::TextDisabled("Textures"); ImGui::SameLine(112.0f); ImGui::Text("%d", static_cast<int>(scene->textures.size()));
        ImGui::TextDisabled("BVH nodes"); ImGui::SameLine(112.0f); ImGui::Text("%d", static_cast<int>(scene->bvhTranslator.nodes.size()));
    }

    ImGui::Spacing();
    ImGui::SeparatorText("OUTPUT");
    if (ImGui::Checkbox("OIDN denoiser", &renderOptions.enableDenoiser))
        ApplyOptionsFromUI();

    int toneMapMode = !renderOptions.enableTonemap ? 2 : (renderOptions.enableAces ? 0 : 1);
    const char* toneMapNames[] = { "ACES", "Reinhard", "Linear" };
    if (ImGui::Combo("Tone mapping", &toneMapMode, toneMapNames, 3))
    {
        renderOptions.enableTonemap = toneMapMode != 2;
        renderOptions.enableAces = toneMapMode == 0;
        ApplyOptionsFromUI();
    }
    if (ImGui::SliderFloat("Exposure", &renderOptions.exposure, -5.0f, 5.0f, "%.1f EV"))
        ApplyOptionsFromUI();
    if (renderOptions.enableAces && ImGui::Checkbox("Fast ACES approximation", &renderOptions.simpleAcesFit))
        ApplyOptionsFromUI();

    ImGui::Spacing();
    ImGui::SeparatorText("CAMERA");
    if (scene && scene->camera)
    {
        float fieldOfView = Math::Degrees(scene->camera->fov);
        if (ImGui::SliderFloat("Field of view", &fieldOfView, 20.0f, 90.0f, "%.0f deg"))
        {
            scene->camera->SetFov(fieldOfView);
            ApplyOptionsFromUI(false, true);
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("NAVIGATION");
    ImGui::TextDisabled("Left drag"); ImGui::SameLine(100.0f); ImGui::TextUnformatted("Orbit");
    ImGui::TextDisabled("Middle drag"); ImGui::SameLine(100.0f); ImGui::TextUnformatted("Pan");
    ImGui::TextDisabled("Mouse wheel"); ImGui::SameLine(100.0f); ImGui::TextUnformatted("Zoom");
    ImGui::TextDisabled("Tab"); ImGui::SameLine(100.0f); ImGui::TextUnformatted("Toggle UI");
    ImGui::End();
}

void DrawRightPanel(float top, float bottom, const ImVec2& displaySize)
{
    const float width = kRightPanelWidth;
    userInterface.rightPanelSlide = AnimatePanel(userInterface.rightPanelSlide, userInterface.rightPanelOpen);
    const float x = displaySize.x - width * userInterface.rightPanelSlide;

    ImGui::SetNextWindowPos(ImVec2(x, top), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, std::max(160.0f, displaySize.y - top - bottom)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(userInterface.opacity);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##RenderSettingsPanel", nullptr, flags);
    ImGui::PushItemWidth(205.0f);

    ImGui::SeparatorText("PATH TRACING");
            if (ImGui::SliderInt("Maximum bounces", &renderOptions.maxDepth, 1, 10))
                ApplyOptionsFromUI();

            static int finiteTargetSamples = 128;
            bool unlimitedSamples = renderOptions.maxSpp < 0;
            if (ImGui::Checkbox("Unlimited samples", &unlimitedSamples))
            {
                renderOptions.maxSpp = unlimitedSamples ? -1 : finiteTargetSamples;
                ApplyOptionsFromUI();
            }
            if (!unlimitedSamples)
            {
                finiteTargetSamples = std::max(1, renderOptions.maxSpp);
                if (ImGui::SliderInt("Target samples", &finiteTargetSamples, 1, 512))
                {
                    renderOptions.maxSpp = finiteTargetSamples;
                    ApplyOptionsFromUI();
                }
            }

            if (ImGui::Checkbox("Russian roulette", &renderOptions.enableRR))
                ApplyOptionsFromUI(true, true);
            ImGui::BeginDisabled();
            bool progressiveAccumulation = true;
            ImGui::Checkbox("Progressive accumulation", &progressiveAccumulation);
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::SeparatorText("STABLE PREVIEW");
            bool motionAware = renderOptions.temporalMode == 3;
            if (ImGui::Checkbox("Motion-aware reconstruction", &motionAware))
            {
                renderOptions.useAdvancedTAA = motionAware;
                renderOptions.temporalMode = motionAware ? 3 : 0;
                ApplyOptionsFromUI(false, true);
                SetUIStatus(motionAware ? "Stable preview enabled" : "Stable preview disabled");
            }
            float noiseReductionPercent = renderOptions.spatialFilterStrength * 100.0f;
            if (ImGui::SliderFloat("Noise reduction", &noiseReductionPercent,
                                   0.0f, 100.0f, "%.0f%%"))
            {
                renderOptions.spatialFilterStrength = noiseReductionPercent / 100.0f;
                ApplyOptionsFromUI(false, true);
            }
            float historyStrengthPercent = renderOptions.historyWeight * 100.0f;
            if (ImGui::SliderFloat("History strength", &historyStrengthPercent,
                                   0.0f, 98.0f, "%.0f%%"))
            {
                renderOptions.historyWeight = historyStrengthPercent / 100.0f;
                ApplyOptionsFromUI(false, true);
            }
            float ghostingProtectionPercent = userInterface.ghostingProtection * 100.0f;
            if (ImGui::SliderFloat("Ghosting protection", &ghostingProtectionPercent,
                                   0.0f, 100.0f, "%.0f%%"))
            {
                userInterface.ghostingProtection = ghostingProtectionPercent / 100.0f;
                const float protection = userInterface.ghostingProtection;
                renderOptions.depthThreshold = 0.05f + (0.005f - 0.05f) * protection;
                renderOptions.normalThreshold = 0.70f + (0.97f - 0.70f) * protection;
                renderOptions.luminanceTolerance = 3.5f + (0.70f - 3.5f) * protection;
                ApplyOptionsFromUI(false, true);
            }

            ImGui::Spacing();
            ImGui::SeparatorText("LIGHTING");
            if (ImGui::Checkbox("Environment lighting", &renderOptions.enableEnvMap))
                ApplyOptionsFromUI(true, true);
            if (ImGui::SliderFloat("Environment intensity", &renderOptions.envMapIntensity,
                                   0.0f, 10.0f, "%.2f"))
                ApplyOptionsFromUI();
            if (ImGui::SliderFloat("Environment rotation", &renderOptions.envMapRot,
                                   0.0f, 360.0f, "%.0f deg"))
                ApplyOptionsFromUI();

            ImGui::Spacing();
            ImGui::SeparatorText("ADVANCED TEMPORAL SETTINGS");
            {
                bool advancedChanged = false;
                advancedChanged |= ImGui::SliderInt("Samples per moving frame", &renderOptions.temporalSpp, 1, 8);
                advancedChanged |= ImGui::SliderFloat("Depth threshold", &renderOptions.depthThreshold, 0.001f, 0.08f, "%.3f");
                advancedChanged |= ImGui::SliderFloat("Normal threshold", &renderOptions.normalThreshold, 0.50f, 0.99f, "%.2f");
                advancedChanged |= ImGui::SliderFloat("Luminance tolerance", &renderOptions.luminanceTolerance, 0.25f, 5.0f, "%.2f");
                advancedChanged |= ImGui::SliderFloat("Motion decay", &renderOptions.motionDecay, 0.0001f, 0.02f, "%.4f", ImGuiSliderFlags_Logarithmic);
                if (advancedChanged) ApplyOptionsFromUI(false, true);
                if (ImGui::Button("Reset temporal history", ImVec2(-1.0f, 0.0f)))
                {
                    renderer->ResetTemporalHistory();
                    SetUIStatus("Temporal history reset");
                }
            }
    ImGui::PopItemWidth();
    ImGui::End();
}

void DrawSideHandles(const ImVec2& displaySize, float top, float bottom)
{
    const float leftWidth = kLeftPanelWidth;
    const float rightWidth = kRightPanelWidth;
    const float handleWidth = 24.0f;
    const float handleHeight = 58.0f;
    const float centerY = top + (displaySize.y - top - bottom) * 0.5f - handleHeight * 0.5f;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings;

    const float leftX = leftWidth * userInterface.leftPanelSlide;
    ImGui::SetNextWindowPos(ImVec2(leftX, centerY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(handleWidth, handleHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##LeftPanelHandle", nullptr, flags);
    if (ImGui::Button(userInterface.leftPanelOpen ? "<##Left" : ">##Left", ImVec2(handleWidth, handleHeight)))
    {
        userInterface.leftPanelOpen = !userInterface.leftPanelOpen;
        userInterface.focusMode = false;
    }
    ImGui::End();

    const float rightX = displaySize.x - rightWidth * userInterface.rightPanelSlide - handleWidth;
    ImGui::SetNextWindowPos(ImVec2(rightX, centerY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(handleWidth, handleHeight), ImGuiCond_Always);
    ImGui::Begin("##RightPanelHandle", nullptr, flags);
    if (ImGui::Button(userInterface.rightPanelOpen ? ">##Right" : "<##Right", ImVec2(handleWidth, handleHeight)))
    {
        userInterface.rightPanelOpen = !userInterface.rightPanelOpen;
        userInterface.focusMode = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void DrawStatusBar(const ImVec2& displaySize)
{
    const float height = 28.0f;
    userInterface.statusBarSlide = AnimatePanel(userInterface.statusBarSlide, userInterface.statusBarOpen);
    const float y = displaySize.y - height * userInterface.statusBarSlide;
    ImGui::SetNextWindowPos(ImVec2(0.0f, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(userInterface.opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##StatusBar", nullptr, flags);
    ImGui::Text("SPP %d", renderer ? renderer->GetSampleCount() : 0);
    ImGui::SameLine(75.0f);
    ImGui::Text("Path trace %.2f ms", renderer ? renderer->GetPathTraceMilliseconds() : 0.0);
    ImGui::SameLine(220.0f);
    ImGui::Text("Temporal %.2f ms", renderer ? renderer->GetTemporalMilliseconds() : 0.0);
    ImGui::SameLine(355.0f);
    ImGui::Text("%d x %d", renderOptions.renderResolution.x, renderOptions.renderResolution.y);
    const float messageWidth = ImGui::CalcTextSize(userInterface.statusMessage.c_str()).x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 16.0f, displaySize.x - messageWidth - 16.0f));
    ImGui::TextDisabled("%s", userInterface.statusMessage.c_str());
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void DrawUserInterface()
{
    if (!uiInitialized || research.enabled) return;
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const float top = 46.0f * userInterface.topBarSlide;
    const float bottom = 28.0f * userInterface.statusBarSlide;

    DrawTopBar(displaySize);
    DrawLeftPanel(top, bottom, displaySize);
    DrawRightPanel(top, bottom, displaySize);
    DrawSideHandles(displaySize, top, bottom);
    DrawStatusBar(displaySize);
    DrawTopRestoreHandle(displaySize);
}

void RenderFrame()
{
    renderer->Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, renderOptions.windowResolution.x, renderOptions.windowResolution.y);
    renderer->Present();
}

void UpdateFrame(float deltaTime)
{
    // Camera control
    if (!research.enabled && isLeftMouseButtonDown)
    {
        int currentMouseX, currentMouseY;
        SDL_GetMouseState(&currentMouseX, &currentMouseY);
        float deltaX = (currentMouseX - previousMouseX) * mouseSensitivity * 10.0f;
        float deltaY = (currentMouseY - previousMouseY) * mouseSensitivity * 10.0f;
        scene->camera->OffsetOrientation(deltaX, deltaY);
        previousMouseX = currentMouseX;
        previousMouseY = currentMouseY;
        scene->dirty = true;
    }
    else if (!research.enabled && isMiddleMouseButtonDown)
    {
        int currentMouseX, currentMouseY;
        SDL_GetMouseState(&currentMouseX, &currentMouseY);
        float deltaX = (currentMouseX - previousMouseX) * mouseSensitivity;
        float deltaY = (currentMouseY - previousMouseY) * mouseSensitivity;
        scene->camera->Strafe(deltaX, deltaY);
        previousMouseX = currentMouseX;
        previousMouseY = currentMouseY;
        scene->dirty = true;
    }

    renderer->Update(deltaTime);
}

void ProcessKeyboardInput(SDL_Event& event, LoopData& loopData)
{
    if (event.type == SDL_KEYDOWN)
    {
        SDL_Keycode key = event.key.keysym.sym;
        bool optionsChanged = false;
        bool reloadShaders = false;

        // Save screenshot
        if (key == SDLK_s && (SDL_GetModState() & KMOD_CTRL))
        {
            SaveUIScreenshot();
        }
        // Switch scenes with number keys
        else if (key >= SDLK_1 && key <= SDLK_9)
        {
            int sceneIndex = key - SDLK_1;
            if (sceneIndex < availableSceneFiles.size())
            {
                currentSceneIndex = sceneIndex;
                LoadSceneFromFile(availableSceneFiles[currentSceneIndex]);
                SDL_RestoreWindow(loopData.mWindow);
                SDL_SetWindowSize(loopData.mWindow, renderOptions.windowResolution.x, renderOptions.windowResolution.y);
                int windowWidth, windowHeight;
                SDL_GL_GetDrawableSize(loopData.mWindow, &windowWidth, &windowHeight);
                renderOptions.windowResolution.x = windowWidth;
                renderOptions.windowResolution.y = windowHeight;
                InitializePathTracerRenderer();
            }
        }
        // Switch environment maps with E key
        else if (key == SDLK_e)
        {
            if (!environmentMaps.empty())
            {
                currentEnvironmentMapIndex = (currentEnvironmentMapIndex + 1) % environmentMaps.size();
                scene->AddEnvMap(environmentMaps[currentEnvironmentMapIndex]);
                optionsChanged = true;
            }
        }
        // Toggle options with various keys
        else if (key == SDLK_r)
        {
            renderOptions.enableRR = !renderOptions.enableRR;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_t)
        {
            renderOptions.enableTonemap = !renderOptions.enableTonemap;
            optionsChanged = true;
        }
        else if (key == SDLK_u)
        {
            renderOptions.enableUniformLight = !renderOptions.enableUniformLight;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_m)
        {
            renderOptions.enableEnvMap = !renderOptions.enableEnvMap;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_h)
        {
            renderOptions.hideEmitters = !renderOptions.hideEmitters;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_b)
        {
            renderOptions.enableBackground = !renderOptions.enableBackground;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_n)
        {
            renderOptions.transparentBackground = !renderOptions.transparentBackground;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_a)
        {
            renderOptions.enableAces = !renderOptions.enableAces;
            optionsChanged = true;
        }
        else if (key == SDLK_v)
        {
            renderOptions.enableVolumeMIS = !renderOptions.enableVolumeMIS;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_o)
        {
            renderOptions.enableRoughnessMollification = !renderOptions.enableRoughnessMollification;
            reloadShaders = true;
            optionsChanged = true;
        }
        else if (key == SDLK_y)
        {
            renderOptions.useAdvancedTAA = !renderOptions.useAdvancedTAA;
            renderOptions.temporalMode = renderOptions.useAdvancedTAA ? 3 : 0;
            renderer->ResetTemporalHistory();
            optionsChanged = true;
        }
        // Adjust values with +/- keys
        else if (key == SDLK_EQUALS || key == SDLK_PLUS)
        {
            if (SDL_GetModState() & KMOD_ALT)
            {
                renderOptions.envMapRot = fmodf(renderOptions.envMapRot + 5.0f, 360.0f);
            }
            else if (SDL_GetModState() & KMOD_SHIFT)
            {
                renderOptions.maxSpp = (renderOptions.maxSpp < 0) ? 1 : renderOptions.maxSpp + 1;
                if (renderOptions.maxSpp > 256) renderOptions.maxSpp = 256;
            }
            else if (SDL_GetModState() & KMOD_CTRL)
            {
                renderOptions.maxDepth = (renderOptions.maxDepth < 10) ? renderOptions.maxDepth + 1 : 10;
            }
            else
            {
                renderOptions.envMapIntensity = (renderOptions.envMapIntensity < 10.0f) ? renderOptions.envMapIntensity + 0.1f : 10.0f;
            }
            optionsChanged = true;
        }
        else if (key == SDLK_MINUS)
        {
            if (SDL_GetModState() & KMOD_ALT)
            {
                renderOptions.envMapRot = fmodf(renderOptions.envMapRot - 5.0f + 360.0f, 360.0f);
            }
            else if (SDL_GetModState() & KMOD_SHIFT)
            {
                renderOptions.maxSpp = (renderOptions.maxSpp > -1) ? renderOptions.maxSpp - 1 : -1;
            }
            else if (SDL_GetModState() & KMOD_CTRL)
            {
                renderOptions.maxDepth = (renderOptions.maxDepth > 1) ? renderOptions.maxDepth - 1 : 1;
            }
            else
            {
                renderOptions.envMapIntensity = (renderOptions.envMapIntensity > 0.1f) ? renderOptions.envMapIntensity - 0.1f : 0.1f;
            }
            optionsChanged = true;
        }

        if (optionsChanged)
        {
            scene->renderOptions = renderOptions;
            scene->dirty = true;
        }

        if (reloadShaders)
        {
            scene->dirty = true;
            renderer->ReloadShaders();
        }
    }
}

void MainEventLoop(void* arg)
{
    LoopData& loopData = *(LoopData*)arg;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (uiInitialized)
            ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
        {
            shouldExit = true;
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                renderOptions.windowResolution = iVec2(event.window.data1, event.window.data2);
                int windowWidth, windowHeight;
                SDL_GL_GetDrawableSize(loopData.mWindow, &windowWidth, &windowHeight);
                renderOptions.windowResolution.x = windowWidth;
                renderOptions.windowResolution.y = windowHeight;

                if (!renderOptions.independentRenderSize)
                    renderOptions.renderResolution = renderOptions.windowResolution;

                scene->renderOptions = renderOptions;
                renderer->Resize();
            }

            if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(loopData.mWindow))
            {
                shouldExit = true;
            }
        }
        else if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            if (!UIWantsMouse() && event.button.button == SDL_BUTTON_LEFT)
            {
                isLeftMouseButtonDown = true;
                SDL_GetMouseState(&previousMouseX, &previousMouseY);
            }
            else if (!UIWantsMouse() && event.button.button == SDL_BUTTON_MIDDLE)
            {
                isMiddleMouseButtonDown = true;
                SDL_GetMouseState(&previousMouseX, &previousMouseY);
            }
        }
        else if (event.type == SDL_MOUSEBUTTONUP)
        {
            if (event.button.button == SDL_BUTTON_LEFT)
                isLeftMouseButtonDown = false;
            else if (event.button.button == SDL_BUTTON_MIDDLE)
                isMiddleMouseButtonDown = false;
        }
        else if (event.type == SDL_MOUSEWHEEL && !research.enabled && !UIWantsMouse())
        {
            float wheelSteps = static_cast<float>(event.wheel.y);
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                wheelSteps = -wheelSteps;
            scene->camera->Zoom(wheelSteps);
            scene->dirty = true;
        }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
        {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB && !research.enabled)
            {
                if (userInterface.focusMode)
                    RestoreUserInterface();
                else
                    HideAllUserInterface();
            }
            else if (!UIWantsKeyboard())
                ProcessKeyboardInput(event, loopData);
        }
    }

    if (uiInitialized && !research.enabled)
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    double currentTime = SDL_GetTicks();
    ApplyResearchCameraPose();
    ApplyUICameraTrajectory();
    UpdateFrame((float)(currentTime - lastFrameTime));
    lastFrameTime = currentTime;
    glClearColor(0., 0., 0., 0.);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    RenderFrame();
    if (uiInitialized && !research.enabled)
    {
        DrawUserInterface();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    FinishResearchFrame();
    SDL_GL_SwapWindow(loopData.mWindow);
}

int main(int argc, char** argv)
{
    srand((unsigned int)time(0));

    std::string sceneFile;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "-s" || arg == "--scene")
        {
            if (i + 1 >= argc) { printf("Missing value for %s\n", arg.c_str()); return 1; }
            sceneFile = argv[++i];
        }
        else if (arg == "--experiment-dir")
        {
            if (i + 1 >= argc) { printf("Missing experiment directory\n"); return 1; }
            research.enabled = true;
            research.outputDirectory = argv[++i];
        }
        else if (arg == "--method")
        {
            if (i + 1 >= argc) { printf("Missing method name\n"); return 1; }
            research.methodName = argv[++i];
            research.temporalMode = TemporalModeFromName(research.methodName);
            if (research.temporalMode < 0)
            {
                printf("Unknown method. Use none, fixed, geometry, proposed, inverse_variance, no_luminance, no_motion, or reference.\n");
                return 1;
            }
        }
        else if (arg == "--frames" || arg == "--seed" || arg == "--spp" ||
                 arg == "--reference-spp" || arg == "--width" || arg == "--height" ||
                 arg == "--max-depth")
        {
            if (i + 1 >= argc) { printf("Missing value for %s\n", arg.c_str()); return 1; }
            int value = atoi(argv[++i]);
            if (arg == "--frames") research.totalFrames = std::max(1, value);
            else if (arg == "--seed") renderOptions.temporalSeed = value;
            else if (arg == "--spp") research.spp = std::max(1, std::min(8, value));
            else if (arg == "--reference-spp") research.referenceSpp = std::max(1, value);
            else if (arg == "--width") research.width = std::max(64, value);
            else if (arg == "--height") research.height = std::max(64, value);
            else if (arg == "--max-depth") renderOptions.maxDepth = std::max(1, value);
        }
        else if (arg == "--help")
        {
            printf("Research mode:\n"
                   "  --scene FILE --experiment-dir DIR --method METHOD\n"
                   "  [--frames 180] [--seed 1] [--spp 1] [--width 640] [--height 360]\n"
                   "  Reference: --method reference [--reference-spp 256]\n");
            return 0;
        }
        else if (arg[0] == '-')
        {
            printf("Unknown option %s\n", arg.c_str());
            return 1;
        }
    }

    if (!sceneFile.empty())
    {
        scene = new Scene();
        LoadEnvironmentMaps();
        LoadSceneFromFile(sceneFile);
    }
    else
    {
        LoadAvailableSceneFiles();
        LoadEnvironmentMaps();
        LoadSceneFromFile(availableSceneFiles[currentSceneIndex]);
    }

    if (research.enabled)
    {
        if (!EnsureDirectory(research.outputDirectory))
        {
            printf("Unable to create experiment directory: %s\n", research.outputDirectory.c_str());
            return 1;
        }
        renderOptions.renderResolution = iVec2(research.width, research.height);
        renderOptions.windowResolution = iVec2(research.width, research.height);
        renderOptions.independentRenderSize = true;
        renderOptions.temporalMode = research.temporalMode;
        renderOptions.temporalSpp = research.spp;
        renderOptions.enableDenoiser = false;
        scene->renderOptions = renderOptions;
    }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    LoopData loopData;

    // OpenGL 3.3 core is required by the renderer and UI shaders.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_DisplayMode currentDisplayMode;
    SDL_GetCurrentDisplayMode(0, &currentDisplayMode);
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    loopData.mWindow = SDL_CreateWindow("PathTracer Studio", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, renderOptions.windowResolution.x, renderOptions.windowResolution.y, windowFlags);

    // Query actual drawable window size
    int windowWidth, windowHeight;
    SDL_GL_GetDrawableSize(loopData.mWindow, &windowWidth, &windowHeight);
    renderOptions.windowResolution.x = windowWidth;
    renderOptions.windowResolution.y = windowHeight;

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    loopData.mGLContext = SDL_GL_CreateContext(loopData.mWindow);
    if (!loopData.mGLContext)
    {
        fprintf(stderr, "Failed to initialize GL context!\n");
        return 1;
    }
    SDL_GL_SetSwapInterval(0); // Disable vsync

    // Initialize
    if (gl3wInit() != 0)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    if (!research.enabled)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#if defined(_WIN32)
        const char* interfaceFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        if (GetFileAttributesA(interfaceFontPath) != INVALID_FILE_ATTRIBUTES)
        {
            ImFont* interfaceFont = io.Fonts->AddFontFromFileTTF(interfaceFontPath, 16.0f);
            if (interfaceFont) io.FontDefault = interfaceFont;
        }
#endif
        ConfigureUserInterfaceStyle();
        if (!ImGui_ImplSDL2_InitForOpenGL(loopData.mWindow, loopData.mGLContext) ||
            !ImGui_ImplOpenGL3_Init("#version 330"))
        {
            fprintf(stderr, "Failed to initialize the user interface.\n");
            return 1;
        }
        uiInitialized = true;
    }

    if (!InitializePathTracerRenderer())
        return 1;

    if (research.enabled)
    {
        research.initialEye = scene->camera->position;
        research.initialPivot = scene->camera->GetPivot();
        research.initialRight = scene->camera->right;
        research.initialRadius = Vec3::Distance(research.initialEye, research.initialPivot);
        research.timingFile.open(research.outputDirectory + "/timings.csv");
        if (!research.timingFile)
        {
            printf("Unable to create timings.csv\n");
            return 1;
        }
        research.timingFile << "frame,pass,path_trace_ms,temporal_ms,total_ms\n";
        renderer->ResetTemporalHistory();
        printf("Research experiment: method=%s, frames=%d, seed=%d, spp=%d, output=%s\n",
               research.methodName.c_str(), research.totalFrames, renderOptions.temporalSeed,
               research.spp, research.outputDirectory.c_str());
    }

    printf("\n=== Keyboard Controls ===\n");
    printf("Mouse Controls:\n");
    printf("  LMB + drag: Rotate camera\n");
    printf("  MMB + drag: Pan camera\n");
    printf("  Mouse wheel: Zoom in/out\n");
    printf("\nKeyboard Controls:\n");
    printf("  CTRL+S: Save screenshot\n");
    printf("  1-9: Switch scenes\n");
    printf("  E: Switch environment maps\n");
    printf("  R: Toggle Russian Roulette\n");
    printf("  T: Toggle Tonemap\n");
    printf("  Denoiser: Always enabled in interactive mode\n");
    printf("  U: Toggle Uniform Light\n");
    printf("  M: Toggle Environment Map\n");
    printf("  H: Toggle Hide Emitters\n");
    printf("  B: Toggle Background\n");
    printf("  N: Toggle Transparent Background\n");
    printf("  A: Toggle ACES\n");
    printf("  V: Toggle Volume MIS\n");
    printf("  O: Toggle Roughness Mollification\n");
    printf("  Y: Toggle motion-aware stable preview\n");
    printf("  Tab: Hide or restore the UI\n");
    printf("  +/-: Adjust environment map intensity\n");
    printf("  ALT+/-: Adjust environment map rotation (degrees)\n");
    printf("  SHIFT+/-: Adjust max SPP\n");
    printf("  CTRL+/-: Adjust max depth\n");
    printf("========================\n\n");

    while (!shouldExit)
    {
        MainEventLoop(&loopData);
    }

    delete renderer;
    delete scene;

    // Cleanup
    if (uiInitialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        uiInitialized = false;
    }
    SDL_GL_DeleteContext(loopData.mGLContext);
    SDL_DestroyWindow(loopData.mWindow);
    SDL_Quit();
    return 0;
    }
