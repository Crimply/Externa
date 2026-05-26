#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <gdiplus.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <tchar.h>
#include <thread>
#include <vector>
#include <commdlg.h>
#include <fstream>

#include "background_data.h"
#include "imguitheming.h"
#include "json.hpp"
#include "resizeing.h"
#include "uishit.h"
#include "winfind.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include <d3d11.h>
#include <dxgi.h>

#include "eyeframe.h"
#include "eyezoom.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")

using json = nlohmann::json;
using namespace Gdiplus;

// ----------------------------------------------
//  Enums & globals
// ----------------------------------------------
enum CurrentResize {
    Rezise_Thin,
    Rezise_Wide,
    Rezise_Eye,
    Rezise_Normal
};
static int activeReszie = Rezise_Normal;

bool g_eyeOverlayEnabled = true;
bool g_eyeOverlayCustom = false;
RECT g_eyeRect = {0,0,0,0};
RECT g_cutoutRect = {0,0,0,0};

char g_targetWindowTitle[256] = "Minecraft*";
char g_stateOutputLocation[256] = "";
bool g_doStateOutput = false;
std::string gamestate = "";

struct HotkeyConfig {
    int thinKey = VK_F1;
    int wideKey = VK_F2;
    int eyeKey = VK_F3;
    int normalKey = VK_F4;
    int deleteKey = VK_DELETE;
};
HotkeyConfig g_hotkeys;

struct ResizeDimensions {
    int thin_w = 340, thin_h = 1000;
    int wide_w = 1920, wide_h = 300;
    int eye_w = 384, eye_h = 16384;
    int normal_w = 1920, normal_h = 1080;
} g_resizeDims;

struct imguiSet {
    bool togglegui = true;
    bool overlay = false;
} imguiSettings;

struct waterMarkInfo {
    std::vector<std::string> stringinfo = {""};
} waterMarkInfos;

// Macro capture settings (4 slots, but only slot 0 used in this version)
struct CaptureSettings {
    bool enabled = false;
    RECT cropRect = {0, 0, 100, 100};
    int targetWidth = 100, targetHeight = 100;
    float rotation = 0.0f;
    bool preserveAspect = true;
    ImVec2 displayPos = ImVec2(0,0);
    ImVec2 displaySize = ImVec2(0,0);
    // Colour key fields
    bool colorKeyEnabled = false;
    COLORREF colorKey = RGB(0,255,0);
    int tolerance = 30;
    std::vector<COLORREF> multiColors;
    bool colorPassMode = false;
};
CaptureSettings g_captureSettings[4];

HWND g_targetHwnd = nullptr;
int g_activeMacro = -1;

// DirectX globals
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
HWND g_hWnd = nullptr;

ID3D11ShaderResourceView* g_captureTexture = nullptr;
int g_captureTexWidth = 0, g_captureTexHeight = 0;

ULONG_PTR g_gdiplusToken = 0;
int bg_texture_width = 16, bg_texture_height = 16;
ID3D11ShaderResourceView* bg_texture = nullptr;

struct GameInfo {
    int frameCount = 0;
    float frameRate = 0.0f;
    std::atomic<bool> isRunning = true;
} g_gameInfo;

bool g_capturingHotkey = false;
int* g_capturingHotkeyFor = nullptr;

// Custom capture definition (modified: outline instead of requireColor)
struct CustomCapture {
    std::string name;
    int x = 0, y = 0, width = 800, height = 420;
    bool enabled = true;
    int visibilityModes = (1<<Rezise_Thin)|(1<<Rezise_Wide)|(1<<Rezise_Eye)|(1<<Rezise_Normal);

    // Outline filter (replaces requireColorPresent)
    bool outlineEnabled = false;
    COLORREF outlineColor = RGB(255, 255, 255);

    // Cropping & transform
    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
    int targetWidth = 100, targetHeight = 100;
    float rotation = 0.0f;
    bool preserveAspect = true;

    // Colour key (chroma)
    bool colorKeyEnabled = false;
    COLORREF colorKey = RGB(0,255,0);
    int tolerance = 30;
    std::vector<COLORREF> multiColors;
    bool colorPassMode = false;

    // Display & shape
    ImVec2 displayPos = ImVec2(0,0);
    ImVec2 displaySize = ImVec2(0,0);
    bool circular = false;
    float circleRadius = 0.0f;

    char targetWindowTitle[256] = "";


    ID3D11ShaderResourceView* texture = nullptr;
    int texWidth = 0, texHeight = 0;

    // Runtime HWND cache,(not serialized). mutable so that a const& can refresh it. 
    mutable HWND cachedHwnd = nullptr;
    mutable char cachedHwndTitle[256] = {};

    void freeTexture() {
        if (texture) { texture->Release(); texture = nullptr; }
        texWidth = texHeight = 0;
    }
};
std::vector<CustomCapture> g_customCaptures;
int g_editingCustomCaptureIndex = -1;
int g_draggingCaptureIndex = -1;
bool g_draggingCaptureRect = false, g_draggingDisplayRect = false;
ImVec2 g_lastMousePos;

Gdiplus::Bitmap* g_eyeOverlayImage = nullptr;
CustomCapture g_eyeOverlayCapture;
bool g_eyeOverlayCaptureActive = false;

bool g_restrictToAllowedWindows = false;
std::vector<std::string> g_allowedWindows;

struct ResizeBackground {
    bool enabled = false;
    bool useImage = false;
    char imagePath[256] = "";
    float color[4] = {0.5f,0.0f,0.5f,1.0f};
    ID3D11ShaderResourceView* texture = nullptr;
    int texWidth = 0, texHeight = 0;
    void freeTexture() {
        if (texture) { texture->Release(); texture = nullptr; }
        texWidth = texHeight = 0;
    }
};
ResizeBackground g_backgrounds[4];

// ----------------------------------------------
//  Helper functions
// ----------------------------------------------
inline std::wstring toWide(const char* str) {
    return std::wstring(str, str + strlen(str));
}

std::string GetKeyName(int vk) {
    switch (vk) {
        case VK_LSHIFT:   return "Left Shift";
        case VK_RSHIFT:   return "Right Shift";
        case VK_LCONTROL: return "Left Ctrl";
        case VK_RCONTROL: return "Right Ctrl";
        case VK_LMENU:    return "Left Alt";
        case VK_RMENU:    return "Right Alt";
        case VK_LWIN:     return "Left Win";
        case VK_RWIN:     return "Right Win";
    }
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    char name[32] = {0};
    if (GetKeyNameTextA(scanCode << 16, name, sizeof(name)))
        return std::string(name);
    char fallback[16];
    sprintf_s(fallback, "Key %d", vk);
    return fallback;
}
//C:\Users\Crimp\AppData\Roaming\PrismLauncher\instances\1.16.1(1)\minecraft\wpstateout.txt
bool canResize()
{
    std::string temp;
    if (g_doStateOutput) return true;
    if (g_stateOutputLocation == "") return true;
    if (ReadTextFile(g_stateOutputLocation, temp))
    {
        if (temp == "inworld,paused" || temp == "inworld,unpaused" || temp == "inworld,gamescreenopen")
        {
            return true;
        }
        return false;
    }
    return true;
}

void UpdateCutoutRect() {
    if (!g_targetHwnd || !IsWindow(g_targetHwnd)) {
        g_cutoutRect = {0,0,0,0};
        return;
    }
    RECT clientRect;
    if (GetClientRect(g_targetHwnd, &clientRect)) {
        POINT topLeft = {clientRect.left, clientRect.top};
        POINT bottomRight = {clientRect.right, clientRect.bottom};
        ClientToScreen(g_targetHwnd, &topLeft);
        ClientToScreen(g_targetHwnd, &bottomRight);
        g_cutoutRect.left   = topLeft.x;
        g_cutoutRect.top    = topLeft.y;
        g_cutoutRect.right  = bottomRight.x;
        g_cutoutRect.bottom = bottomRight.y;
    } else {
        g_cutoutRect = {0,0,0,0};
    }
}

void LoadEyeOverlayImage() {
    if (g_eyeOverlayImage) return;
    EyeZoomConfig ezCfg;
    if (!g_eyeOverlayCustom)
        GenerateEyeZoomOverlay(ezCfg, g_eyeOverlayCapture.width, g_eyeOverlayCapture.height, L"overlay.png");
    std::wstring path = L"overlay.png";
    Gdiplus::Bitmap* temp = Gdiplus::Bitmap::FromFile(path.c_str());
    if (temp && temp->GetLastStatus() == Gdiplus::Ok) {
        int w = temp->GetWidth(), h = temp->GetHeight();
        g_eyeOverlayImage = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
        Gdiplus::Graphics g(g_eyeOverlayImage);
        g.DrawImage(temp, 0, 0, w, h);
        delete temp;
    } else {
        delete temp;
        g_eyeOverlayImage = nullptr;
        OutputDebugStringA("Failed to load overlay.png\n");
    }
}

void UpdateCustomCaptureTexture(CustomCapture& cap, Bitmap* bmp) {
    if (!bmp) { cap.freeTexture(); return; }
    int width = bmp->GetWidth(), height = bmp->GetHeight();
    if (width<=0 || height<=0) return;
    BitmapData bmpData;
    Rect rect(0,0,width,height);
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok) return;
    if (!cap.texture || cap.texWidth!=width || cap.texHeight!=height) {
        cap.freeTexture();
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC; desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* tex = nullptr;
        if (g_pd3dDevice->CreateTexture2D(&desc, nullptr, &tex) == S_OK) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format; srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &cap.texture);
            tex->Release();
        }
        cap.texWidth = width; cap.texHeight = height;
    }
    if (cap.texture) {
        ID3D11Resource* resource; cap.texture->GetResource(&resource);
        ID3D11Texture2D* tex = static_cast<ID3D11Texture2D*>(resource);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (g_pd3dDeviceContext->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK) {
            BYTE* dst = (BYTE*)mapped.pData; BYTE* src = (BYTE*)bmpData.Scan0;
            int srcPitch = bmpData.Stride;
            for (int y=0; y<height; ++y)
                memcpy(dst + y*mapped.RowPitch, src + y*srcPitch, width*4);
            g_pd3dDeviceContext->Unmap(tex,0);
        }
        resource->Release();
    }
    bmp->UnlockBits(&bmpData);
}

void SetOverlayOwner(HWND target) {
    if (target && IsWindow(target)) {
        SetWindowLongPtr(g_hWnd, GWLP_HWNDPARENT, (LONG_PTR)target);
        SetWindowPos(g_hWnd, HWND_TOP, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    } else {
        SetWindowLongPtr(g_hWnd, GWLP_HWNDPARENT, 0);
    }
}

void UpdateCustomCapturesVisibility() {
    for (auto& cap : g_customCaptures) {
        if (!cap.enabled || !(cap.visibilityModes & (1<<activeReszie)))
            cap.freeTexture();
    }
}

// ----------------------------------------------
//  Resize actions
// ----------------------------------------------
void SaveSettings();

void DoNormalResize() {
    if (!canResize()) return;
    HWND hWnd = FindWindowByPartialTitle(toWide(g_targetWindowTitle).c_str());
    if (!hWnd) return;
    Resizing::toggleResize(hWnd, g_resizeDims.normal_w, g_resizeDims.normal_h, false);
    activeReszie = Rezise_Normal;
    g_targetHwnd = hWnd;
    SetOverlayOwner(g_targetHwnd);
    UpdateCutoutRect();
    if (g_eyeOverlayCaptureActive) {
        g_eyeOverlayCapture.freeTexture();
        g_eyeOverlayCaptureActive = false;
    }
    UpdateCustomCapturesVisibility();
    SaveSettings();
}

void DoThinResize() {
    if (!canResize()) return;
    if (activeReszie == Rezise_Thin) { DoNormalResize(); return; }
    HWND hWnd = FindWindowByPartialTitle(toWide(g_targetWindowTitle).c_str());
    if (!hWnd) return;
    Resizing::toggleResize(hWnd, g_resizeDims.thin_w, g_resizeDims.thin_h, true);
    activeReszie = Rezise_Thin;
    g_targetHwnd = hWnd;
    SetOverlayOwner(g_targetHwnd);
    UpdateCutoutRect();
    if (g_eyeOverlayCaptureActive) {
        g_eyeOverlayCapture.freeTexture();
        g_eyeOverlayCaptureActive = false;
    }
    UpdateCustomCapturesVisibility();
    SaveSettings();
}

void DoWideResize() {
    if (!canResize()) return;
    if (activeReszie == Rezise_Wide) { DoNormalResize(); return; }
    HWND hWnd = FindWindowByPartialTitle(toWide(g_targetWindowTitle).c_str());
    if (!hWnd) return;
    Resizing::toggleResize(hWnd, g_resizeDims.wide_w, g_resizeDims.wide_h, true);
    activeReszie = Rezise_Wide;
    g_targetHwnd = hWnd;
    SetOverlayOwner(g_targetHwnd);
    UpdateCutoutRect();
    if (g_eyeOverlayCaptureActive) {
        g_eyeOverlayCapture.freeTexture();
        g_eyeOverlayCaptureActive = false;
    }
    UpdateCustomCapturesVisibility();
    SaveSettings();
}

void DoEyeResize() {
    if (!canResize()) return;
    if (activeReszie == Rezise_Eye) { DoNormalResize(); return; }
    HWND hWnd = FindWindowByPartialTitle(toWide(g_targetWindowTitle).c_str());
    if (!hWnd) return;
    Resizing::toggleResize(hWnd, g_resizeDims.eye_w, g_resizeDims.eye_h, true);
    activeReszie = Rezise_Eye;
    g_targetHwnd = hWnd;
    UpdateCutoutRect();
    if (g_eyeOverlayEnabled) {
        LoadEyeOverlayImage();
        if (g_eyeOverlayImage) {
            const int EYE_WIDTH = 768, EYE_HEIGHT = 432;
            g_eyeOverlayCapture.name = "EyeOverlay";
            g_eyeOverlayCapture.width = EYE_WIDTH;
            g_eyeOverlayCapture.height = EYE_HEIGHT;
            g_eyeOverlayCapture.targetWidth = EYE_WIDTH;
            g_eyeOverlayCapture.targetHeight = EYE_HEIGHT;
            g_eyeOverlayCapture.displaySize = ImVec2((float)EYE_WIDTH, (float)EYE_HEIGHT);
            g_eyeOverlayCapture.enabled = true;
            g_eyeOverlayCaptureActive = true;
            RECT gameRect;
            GetWindowRect(hWnd, &gameRect);
            int gameCenterY = (gameRect.top + gameRect.bottom)/2;
            int overlayTop = gameCenterY - EYE_HEIGHT/2;
            g_eyeOverlayCapture.displayPos = ImVec2((float)gameRect.left - EYE_WIDTH, (float)overlayTop);
        } else {
            g_eyeOverlayCaptureActive = false;
        }
    }
    UpdateCustomCapturesVisibility();
    SaveSettings();
}

// ----------------------------------------------
//  Settings serialisation (with new outline fields)
// ----------------------------------------------
void SaveSettings() {
    json j;
    j["target_window_title"] = g_targetWindowTitle;
    j["state_output_location"] = g_stateOutputLocation;
    j["state_output_check"] = g_doStateOutput;
    j["resize_dims"] = {
        {"thin_w", g_resizeDims.thin_w}, {"thin_h", g_resizeDims.thin_h},
        {"wide_w", g_resizeDims.wide_w}, {"wide_h", g_resizeDims.wide_h},
        {"eye_w", g_resizeDims.eye_w}, {"eye_h", g_resizeDims.eye_h},
        {"normal_w", g_resizeDims.normal_w}, {"normal_h", g_resizeDims.normal_h}
    };
    j["hotkeys"] = {
        {"thin", g_hotkeys.thinKey}, {"wide", g_hotkeys.wideKey},
        {"eye", g_hotkeys.eyeKey}, {"normal", g_hotkeys.normalKey},
        {"delete", g_hotkeys.deleteKey}
    };
    j["overlay"] = imguiSettings.overlay;
    j["eye_overlay"] = g_eyeOverlayEnabled;
    j["eye_overlay_custom"] = g_eyeOverlayCustom;
    j["restrict_to_allowed_windows"] = g_restrictToAllowedWindows;
    j["allowed_windows"] = g_allowedWindows;

    json captureArray = json::array();
    for (int i=0; i<4; ++i) {
        json cap;
        cap["enabled"] = g_captureSettings[i].enabled;
        cap["cropRect"] = {g_captureSettings[i].cropRect.left, g_captureSettings[i].cropRect.top,
                           g_captureSettings[i].cropRect.right, g_captureSettings[i].cropRect.bottom};
        cap["targetWidth"] = g_captureSettings[i].targetWidth;
        cap["targetHeight"] = g_captureSettings[i].targetHeight;
        cap["rotation"] = g_captureSettings[i].rotation;
        cap["preserveAspect"] = g_captureSettings[i].preserveAspect;
        cap["displayPos"] = {g_captureSettings[i].displayPos.x, g_captureSettings[i].displayPos.y};
        cap["displaySize"] = {g_captureSettings[i].displaySize.x, g_captureSettings[i].displaySize.y};
        cap["colorKeyEnabled"] = g_captureSettings[i].colorKeyEnabled;
        cap["colorKey"] = (int)g_captureSettings[i].colorKey;
        cap["tolerance"] = g_captureSettings[i].tolerance;
        json multi = json::array();
        for (auto c : g_captureSettings[i].multiColors) multi.push_back((int)c);
        cap["multiColors"] = multi;
        cap["colorPassMode"] = g_captureSettings[i].colorPassMode;
        captureArray.push_back(cap);
    }
    j["capture"] = captureArray;

    json customArray = json::array();
    for (const auto& cap : g_customCaptures) {
        json jcap;
        jcap["name"] = cap.name;
        jcap["x"] = cap.x; jcap["y"] = cap.y; jcap["width"] = cap.width; jcap["height"] = cap.height;
        jcap["enabled"] = cap.enabled;
        jcap["visibilityModes"] = cap.visibilityModes;
        jcap["cropX"] = cap.cropX; jcap["cropY"] = cap.cropY; jcap["cropW"] = cap.cropW; jcap["cropH"] = cap.cropH;
        jcap["targetWidth"] = cap.targetWidth; jcap["targetHeight"] = cap.targetHeight;
        jcap["rotation"] = cap.rotation; jcap["preserveAspect"] = cap.preserveAspect;
        jcap["displayPos"] = {cap.displayPos.x, cap.displayPos.y};
        jcap["displaySize"] = {cap.displaySize.x, cap.displaySize.y};
        jcap["cicular"] = cap.circular; jcap["circleRadius"] = cap.circleRadius;
        jcap["targetWindowTitle"] = std::string(cap.targetWindowTitle);
        // Outline fields (replaced requireColorPresent)
        jcap["outlineEnabled"] = cap.outlineEnabled;
        jcap["outlineColor"] = (int)cap.outlineColor;
        jcap["colorKeyEnabled"] = cap.colorKeyEnabled;
        jcap["colorKey"] = (int)cap.colorKey;
        jcap["tolerance"] = cap.tolerance;
        json multi = json::array();
        for (auto c : cap.multiColors) multi.push_back((int)c);
        jcap["multiColors"] = multi;
        jcap["colorPassMode"] = cap.colorPassMode;
        customArray.push_back(jcap);
    }
    j["custom_captures"] = customArray;

    json backgroundsArray = json::array();
    for (int i=0; i<4; ++i) {
        json bg;
        bg["enabled"] = g_backgrounds[i].enabled;
        bg["useImage"] = g_backgrounds[i].useImage;
        bg["imagePath"] = std::string(g_backgrounds[i].imagePath);
        bg["color"] = {g_backgrounds[i].color[0], g_backgrounds[i].color[1],
                       g_backgrounds[i].color[2], g_backgrounds[i].color[3]};
        backgroundsArray.push_back(bg);
    }
    j["backgrounds"] = backgroundsArray;

    std::ofstream file("settings.json");
    if (file.is_open()) file << j.dump(4);
}

void LoadSettings() {
    std::ifstream file("settings.json");
    if (!file.is_open()) return;
    json j;
    try {
        file >> j;

        if (j.contains("state_output_check")) {
            g_doStateOutput = j.value("enabled", false);
        }
        if (j.contains("target_window_title"))
            strcpy_s(g_targetWindowTitle, j["target_window_title"].get<std::string>().c_str());
        if (j.contains("state_output_location"))
            if (j["state_output_location"].is_string()) {
                strcpy_s(g_stateOutputLocation, j["state_output_location"].get<std::string>().c_str());
            }
        if (j.contains("resize_dims")) {
            auto& d = j["resize_dims"];
            if (d.contains("thin_w")) g_resizeDims.thin_w = d["thin_w"];
            if (d.contains("thin_h")) g_resizeDims.thin_h = d["thin_h"];
            if (d.contains("wide_w")) g_resizeDims.wide_w = d["wide_w"];
            if (d.contains("wide_h")) g_resizeDims.wide_h = d["wide_h"];
            if (d.contains("eye_w"))  g_resizeDims.eye_w  = d["eye_w"];
            if (d.contains("eye_h"))  g_resizeDims.eye_h  = d["eye_h"];
            if (d.contains("normal_w")) g_resizeDims.normal_w = d["normal_w"];
            if (d.contains("normal_h")) g_resizeDims.normal_h = d["normal_h"];
        }
        if (j.contains("hotkeys")) {
            auto& h = j["hotkeys"];
            if (h.contains("thin"))   g_hotkeys.thinKey = h["thin"];
            if (h.contains("wide"))   g_hotkeys.wideKey = h["wide"];
            if (h.contains("eye"))    g_hotkeys.eyeKey  = h["eye"];
            if (h.contains("normal")) g_hotkeys.normalKey = h["normal"];
            if (h.contains("delete")) g_hotkeys.deleteKey = h["delete"];
        }
        if (j.contains("overlay")) imguiSettings.overlay = j["overlay"];
        if (j.contains("eye_overlay")) g_eyeOverlayEnabled = j["eye_overlay"];
        if (j.contains("eye_overlay_custom")) g_eyeOverlayCustom = j["eye_overlay_custom"];
        if (j.contains("restrict_to_allowed_windows")) g_restrictToAllowedWindows = j["restrict_to_allowed_windows"];
        if (j.contains("allowed_windows")) {
            g_allowedWindows.clear();
            for (auto& w : j["allowed_windows"]) g_allowedWindows.push_back(w.get<std::string>());
        }

        if (j.contains("capture")) {
            auto& caps = j["capture"];
            for (size_t i=0; i<caps.size() && i<4; ++i) {
                auto& c = caps[i];
                g_captureSettings[i].enabled = c.value("enabled", false);
                if (c.contains("cropRect") && c["cropRect"].size()>=4) {
                    auto& cr = c["cropRect"];
                    g_captureSettings[i].cropRect = {cr[0], cr[1], cr[2], cr[3]};
                }
                g_captureSettings[i].targetWidth = c.value("targetWidth", 100);
                g_captureSettings[i].targetHeight = c.value("targetHeight", 100);
                g_captureSettings[i].rotation = c.value("rotation", 0.0f);
                g_captureSettings[i].preserveAspect = c.value("preserveAspect", true);
                if (c.contains("displayPos") && c["displayPos"].size()>=2) {
                    g_captureSettings[i].displayPos.x = c["displayPos"][0];
                    g_captureSettings[i].displayPos.y = c["displayPos"][1];
                }
                if (c.contains("displaySize") && c["displaySize"].size()>=2) {
                    g_captureSettings[i].displaySize.x = c["displaySize"][0];
                    g_captureSettings[i].displaySize.y = c["displaySize"][1];
                }
                g_captureSettings[i].colorKeyEnabled = c.value("colorKeyEnabled", false);
                g_captureSettings[i].colorKey = (COLORREF)c.value("colorKey", (int)RGB(0,255,0));
                g_captureSettings[i].tolerance = c.value("tolerance", 30);
                if (c.contains("multiColors")) {
                    g_captureSettings[i].multiColors.clear();
                    for (auto& col : c["multiColors"])
                        g_captureSettings[i].multiColors.push_back((COLORREF)col.get<int>());
                }
                g_captureSettings[i].colorPassMode = c.value("colorPassMode", false);
            }
        }

        if (j.contains("custom_captures")) {
            for (const auto& jc : j["custom_captures"]) {
                CustomCapture cap;
                cap.name = jc.value("name", "");
                cap.x = jc.value("x", 0); cap.y = jc.value("y", 0);
                cap.width = jc.value("width", 100); cap.height = jc.value("height", 100);
                cap.enabled = jc.value("enabled", true);
                cap.visibilityModes = jc.value("visibilityModes",
                    (1<<Rezise_Thin)|(1<<Rezise_Wide)|(1<<Rezise_Eye)|(1<<Rezise_Normal));
                cap.cropX = jc.value("cropX", 0); cap.cropY = jc.value("cropY", 0);
                cap.cropW = jc.value("cropW", 0); cap.cropH = jc.value("cropH", 0);
                cap.targetWidth = jc.value("targetWidth", 100);
                cap.targetHeight = jc.value("targetHeight", 100);
                cap.rotation = jc.value("rotation", 0.0f);
                cap.preserveAspect = jc.value("preserveAspect", true);
                if (jc.contains("displayPos") && jc["displayPos"].size()>=2) {
                    cap.displayPos.x = jc["displayPos"][0];
                    cap.displayPos.y = jc["displayPos"][1];
                }
                if (jc.contains("displaySize") && jc["displaySize"].size()>=2) {
                    cap.displaySize.x = jc["displaySize"][0];
                    cap.displaySize.y = jc["displaySize"][1];
                }
                cap.circular = jc.value("circular", false);
                cap.circleRadius = jc.value("circleRadius", 1.0f);
                std::string title = jc.value("targetWindowTitle", "");
                strcpy_s(cap.targetWindowTitle, title.c_str());
                // Outline fields
                cap.outlineEnabled = jc.value("outlineEnabled", false);
                cap.outlineColor = (COLORREF)jc.value("outlineColor", (int)RGB(255,255,255));
                cap.colorKeyEnabled = jc.value("colorKeyEnabled", false);
                cap.colorKey = (COLORREF)jc.value("colorKey", (int)RGB(0,255,0));
                cap.tolerance = jc.value("tolerance", 30);
                if (jc.contains("multiColors")) {
                    cap.multiColors.clear();
                    for (auto& col : jc["multiColors"])
                        cap.multiColors.push_back((COLORREF)col.get<int>());
                }
                cap.colorPassMode = jc.value("colorPassMode", false);
                g_customCaptures.push_back(cap);
            }
        }

        if (j.contains("backgrounds")) {
            auto& bgs = j["backgrounds"];
            for (size_t i=0; i<bgs.size() && i<4; ++i) {
                auto& bg = bgs[i];
                g_backgrounds[i].freeTexture();
                g_backgrounds[i].enabled = bg.value("enabled", false);
                g_backgrounds[i].useImage = bg.value("useImage", false);
                std::string path = bg.value("imagePath", "");
                strncpy_s(g_backgrounds[i].imagePath, path.c_str(), sizeof(g_backgrounds[i].imagePath)-1);
                if (bg.contains("color") && bg["color"].size()>=4) {
                    for (int k=0; k<4; ++k) g_backgrounds[i].color[k] = bg["color"][k];
                }
            }
        }
    } catch(...) {}
}

// ----------------------------------------------
//  Capture & transformation functions
// ----------------------------------------------
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num=0, size=0;
    GetImageEncodersSize(&num, &size);
    if (size==0) return -1;
    ImageCodecInfo* pCodec = (ImageCodecInfo*)malloc(size);
    if (!pCodec) return -1;
    GetImageEncoders(num, size, pCodec);
    for (UINT j=0; j<num; ++j) {
        if (wcscmp(pCodec[j].MimeType, format)==0) {
            *pClsid = pCodec[j].Clsid;
            free(pCodec);
            return j;
        }
    }
    free(pCodec);
    return -1;
}

void CaptureScreenArea(int x, int y, int width, int height, const std::string& filename) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return;
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDC, width, height);
    if (!bitmap) {
        DeleteDC(memDC); ReleaseDC(nullptr, screenDC); return;
    }
    SelectObject(memDC, bitmap);
    BitBlt(memDC, 0,0, width,height, screenDC, x,y, SRCCOPY);
    Bitmap* gdiBitmap = Bitmap::FromHBITMAP(bitmap, nullptr);
    if (gdiBitmap) {
        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
            std::wstring wfilename(filename.begin(), filename.end());
            gdiBitmap->Save(wfilename.c_str(), &pngClsid, nullptr);
        }
        delete gdiBitmap;
    }
    DeleteObject(bitmap); DeleteDC(memDC); ReleaseDC(nullptr, screenDC);
}

bool IsForegroundAllowed() {
    if (!g_restrictToAllowedWindows) return true;
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    if (fg == g_hWnd) return true;
    wchar_t title[256];
    GetWindowTextW(fg, title, 256);
    std::string titleStr(title, title+wcslen(title));
    for (const auto& allowed : g_allowedWindows)
        if (titleStr.find(allowed) != std::string::npos) return true;
    return false;
}

bool LoadTextureFromFile(const std::string& filename, ID3D11ShaderResourceView** outTex, int* outW, int* outH) {
    Bitmap* bmp = Bitmap::FromFile(std::wstring(filename.begin(), filename.end()).c_str());
    if (!bmp || bmp->GetLastStatus() != Ok) { delete bmp; return false; }
    int w = bmp->GetWidth(), h = bmp->GetHeight();
    BitmapData data;
    Rect rect(0,0,w,h);
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
        delete bmp; return false;
    }
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data.Scan0;
    init.SysMemPitch = data.Stride;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &init, &tex);
    bmp->UnlockBits(&data); delete bmp;
    if (FAILED(hr) || !tex) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = desc.Format; srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    if (FAILED(g_pd3dDevice->CreateShaderResourceView(tex, &srv, outTex))) {
        tex->Release(); return false;
    }
    tex->Release();
    if (outW) *outW = w; if (outH) *outH = h;
    return true;
}

void EnsureBackgroundTexture(ResizeBackground& bg) {
    if (!bg.enabled || !bg.useImage || bg.imagePath[0]=='\0') {
        bg.freeTexture(); return;
    }
    if (bg.texture) return;
    LoadTextureFromFile(bg.imagePath, &bg.texture, &bg.texWidth, &bg.texHeight);
}

void DrawResizeBackground() {
    ResizeBackground& bg = g_backgrounds[activeReszie];
    if (!bg.enabled) return;
    EnsureBackgroundTexture(bg);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 size = ImGui::GetIO().DisplaySize;
    if (bg.useImage && bg.texture)
        draw->AddImage((ImTextureID)bg.texture, ImVec2(0,0), size);
    else
        draw->AddRectFilled(ImVec2(0,0), size,
            IM_COL32((int)(bg.color[0]*255), (int)(bg.color[1]*255),
                     (int)(bg.color[2]*255), (int)(bg.color[3]*255)));
}

// Macro capture with colour key
Bitmap* CaptureTransformed(HWND targetHwnd, const CaptureSettings& s) {
    if (!targetHwnd) return nullptr;
    RECT client; GetClientRect(targetHwnd, &client);
    RECT crop = s.cropRect;
    int srcW = crop.right - crop.left, srcH = crop.bottom - crop.top;
    if (srcW<=0 || srcH<=0) return nullptr;
    HDC srcDC = GetDC(targetHwnd);
    HDC memDC = CreateCompatibleDC(srcDC);
    HBITMAP hbmp = CreateCompatibleBitmap(srcDC, srcW, srcH);
    if (!hbmp) { ReleaseDC(targetHwnd,srcDC); DeleteDC(memDC); return nullptr; }
    SelectObject(memDC, hbmp);
    BitBlt(memDC,0,0,srcW,srcH, srcDC,crop.left,crop.top, SRCCOPY);
    Bitmap* srcBitmap = Bitmap::FromHBITMAP(hbmp, nullptr);
    DeleteObject(hbmp); DeleteDC(memDC); ReleaseDC(targetHwnd,srcDC);
    if (!srcBitmap) return nullptr;

    int destW = s.targetWidth, destH = s.targetHeight;
    if (destW<=0 || destH<=0) return srcBitmap;
    Bitmap* destBitmap = new Bitmap(destW, destH, PixelFormat32bppARGB);
    Graphics g(destBitmap);
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.Clear(Color(0,0,0,0));
    float sx = (float)destW/srcW, sy = (float)destH/srcH;
    if (s.preserveAspect) { float sc = std::min(sx,sy); sx = sy = sc; }
    float cx = destW/2.0f, cy = destH/2.0f;
    g.TranslateTransform(cx,cy);
    g.RotateTransform(s.rotation);
    g.ScaleTransform(sx,sy);
    g.TranslateTransform(-srcW/2.0f, -srcH/2.0f);
    g.DrawImage(srcBitmap, 0,0, srcW,srcH);
    delete srcBitmap;

    // Colour key processing
    if (s.colorKeyEnabled) {
        BitmapData data;
        Rect rect(0,0,destW,destH);
        if (destBitmap->LockBits(&rect, ImageLockModeRead|ImageLockModeWrite,
                                 PixelFormat32bppARGB, &data) == Ok) {
            BYTE* pixels = (BYTE*)data.Scan0;
            int stride = data.Stride;
            auto within = [](BYTE a, BYTE b, int tol) { return abs(a-b)<=tol; };
            auto match = [&](BYTE r, BYTE g, BYTE b) -> bool {
                int tol = s.tolerance;
                BYTE kr=GetRValue(s.colorKey), kg=GetGValue(s.colorKey), kb=GetBValue(s.colorKey);
                if (within(r,kr,tol) && within(g,kg,tol) && within(b,kb,tol)) return true;
                for (COLORREF col : s.multiColors) {
                    BYTE cr=GetRValue(col), cg=GetGValue(col), cb=GetBValue(col);
                    if (within(r,cr,tol) && within(g,cg,tol) && within(b,cb,tol)) return true;
                }
                return false;
            };
            for (int y=0; y<destH; ++y) {
                BYTE* row = pixels + y*stride;
                for (int x=0; x<destW; ++x) {
                    BYTE* p = row + x*4;
                    BYTE r=p[2], g=p[1], b=p[0];
                    bool isKey = match(r,g,b);
                    if (s.colorPassMode) { if (!isKey) p[3]=0; }
                    else { if (isKey) p[3]=0; }
                }
            }
            destBitmap->UnlockBits(&data);
        }
    }
    return destBitmap;
}

void UpdateCaptureTexture(Bitmap* bmp) {
    if (!bmp) {
        if (g_captureTexture) { g_captureTexture->Release(); g_captureTexture=nullptr; }
        return;
    }
    int w = bmp->GetWidth(), h = bmp->GetHeight();
    if (w<=0 || h<=0) return;
    BitmapData data;
    Rect rect(0,0,w,h);
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return;
    if (!g_captureTexture || g_captureTexWidth!=w || g_captureTexHeight!=h) {
        if (g_captureTexture) g_captureTexture->Release();
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC; desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* tex = nullptr;
        if (g_pd3dDevice->CreateTexture2D(&desc, nullptr, &tex) == S_OK) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
            srv.Format = desc.Format; srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv.Texture2D.MipLevels = 1;
            g_pd3dDevice->CreateShaderResourceView(tex, &srv, &g_captureTexture);
            tex->Release();
        }
        g_captureTexWidth = w; g_captureTexHeight = h;
    }
    if (g_captureTexture) {
        ID3D11Resource* res; g_captureTexture->GetResource(&res);
        ID3D11Texture2D* tex = (ID3D11Texture2D*)res;
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (g_pd3dDeviceContext->Map(tex,0, D3D11_MAP_WRITE_DISCARD,0, &mapped) == S_OK) {
            BYTE* dst = (BYTE*)mapped.pData;
            BYTE* src = (BYTE*)data.Scan0;
            for (int y=0; y<h; ++y)
                memcpy(dst + y*mapped.RowPitch, src + y*data.Stride, w*4);
            g_pd3dDeviceContext->Unmap(tex,0);
        }
        res->Release();
    }
    bmp->UnlockBits(&data);
}

// Helper: apply outline to non‑transparent pixels
void ApplyOutlineToBitmap(Bitmap* bmp, COLORREF outlineColor) {
    if (!bmp) return;
    int w = bmp->GetWidth(), h = bmp->GetHeight();
    if (w <= 0 || h <= 0) return;

    BitmapData data;
    Rect rect(0, 0, w, h);
    if (bmp->LockBits(&rect, ImageLockModeRead | ImageLockModeWrite,
                      PixelFormat32bppARGB, &data) != Ok) return;

    BYTE* pixels = (BYTE*)data.Scan0;
    int stride = data.Stride;

    auto getAlpha = [&](int x, int y) -> BYTE {
        if (x < 0 || x >= w || y < 0 || y >= h) return 0;
        BYTE* p = pixels + y * stride + x * 4;
        return p[3];
    };

    BYTE r_out = GetRValue(outlineColor);
    BYTE g_out = GetGValue(outlineColor);
    BYTE b_out = GetBValue(outlineColor);
    for (int y = 0; y < h; ++y) {
        BYTE* row = pixels + y * stride;
        for (int x = 0; x < w; ++x) {
            BYTE* p = row + x * 4;
            if (p[3] == 0) continue;
            if (getAlpha(x-1, y) == 0 || getAlpha(x+1, y) == 0 ||
                getAlpha(x, y-1) == 0 || getAlpha(x, y+1) == 0) {
                p[0] = b_out;  // B
                p[1] = g_out;  // G
                p[2] = r_out;  // R
                // alpha unchanged - so neighbour edge-tests later in the loop stay correct
            }
        }
    }

    bmp->UnlockBits(&data);
}

static HDC      g_capScratchDC  = nullptr;
static HBITMAP  g_capScratchBmp = nullptr;
static int      g_capScratchW = 0, g_capScratchH = 0;

// Custom capture with colour key and outline (instead of requireColor)
Bitmap* CaptureWindowOrDesktop(const CustomCapture& cap) {
    HWND target = nullptr;
    RECT client = {0,0,0,0};
    if (cap.targetWindowTitle[0] != 0) {
      bool titleChanged = strcmp(cap.cachedHwndTitle, cap.targetWindowTitle) !=0;
      if (cap.cachedHwnd && !titleChanged && IsWindow(cap.cachedHwnd)) { 
         target = cap.cachedHwnd;
      } else {
         std::wstring wtitle(cap.targetWindowTitle, cap.targetWindowTitle+strlen(cap.targetWindowTitle));\
         target = FindWindowByPartialTitle(wtitle.c_str());
         cap.cachedHwnd = target;
         strcpy_s(cap.cachedHwndTitle, cap.targetWindowTitle);
      }
      if (target) GetClientRect(target, &client);
    }
    if (!target) return nullptr;
    if (client.right<=client.left || client.bottom<=client.top) return nullptr;
    POINT topLeft = {client.left, client.top};
    ClientToScreen(target, &topLeft);
    RECT clientScreen = {topLeft.x, topLeft.y,
                         topLeft.x + (client.right-client.left),
                         topLeft.y + (client.bottom-client.top)};
    int desiredX = clientScreen.left + cap.x;
    int desiredY = clientScreen.top  + cap.y;
    int captureX = std::max<int>(clientScreen.left, desiredX);
    int captureY = std::max<int>(clientScreen.top, desiredY);
    int captureR = std::min<int>(clientScreen.right, desiredX + cap.width);
    int captureB = std::min<int>(clientScreen.bottom, desiredY + cap.height);
    int captureW = captureR - captureX, captureH = captureB - captureY;
    if (captureW<=0 || captureH<=0) return nullptr;

    HDC screenDC = GetDC(nullptr);
    if (!g_capScratchDC) g_capScratchDC = CreateCompatibleDC(screenDC);
    if (g_capScratchW != captureW || g_capScratchH != captureH) {
        if (g_capScratchBmp) DeleteObject(g_capScratchBmp);
        g_capScratchBmp = CreateCompatibleBitmap(screenDC, captureW, captureH);
        g_capScratchW = captureW; g_capScratchH = captureH;
    }
    if (!g_capScratchBmp) { ReleaseDC(nullptr, screenDC); return nullptr; }
    HGDIOBJ oldObj = SelectObject(g_capScratchDC, g_capScratchBmp);
    BitBlt(g_capScratchDC, 0,0, captureW,captureH, screenDC, captureX,captureY, SRCCOPY);
    Bitmap* srcBitmap = Bitmap::FromHBITMAP(g_capScratchBmp, nullptr);
    SelectObject(g_capScratchDC, oldObj);
    ReleaseDC(nullptr, screenDC);
    if (!srcBitmap) return nullptr;

    int cropX = cap.cropX, cropY = cap.cropY, cropW = cap.cropW, cropH = cap.cropH;
    if (cropW<=0) cropW = captureW; if (cropH<=0) cropH = captureH;
    cropX = std::max(0, std::min(cropX, captureW)); cropY = std::max(0, std::min(cropY, captureH));
    cropW = std::min(cropW, captureW - cropX); cropH = std::min(cropH, captureH - cropY);
    if (cropW<=0 || cropH<=0) { delete srcBitmap; return nullptr; }
    Bitmap* cropped = srcBitmap->Clone(cropX, cropY, cropW, cropH, PixelFormat32bppARGB);
    delete srcBitmap;
    if (!cropped) return nullptr;

    // No requireColorPresent check anymore

    int destW = cap.targetWidth, destH = cap.targetHeight;
    if (destW<=0 || destH<=0) return cropped;
    Bitmap* dest = new Bitmap(destW, destH, PixelFormat32bppARGB);
    Graphics g(dest);
    g.SetInterpolationMode(InterpolationModeNearestNeighbor);
    g.Clear(Color(0,0,0,0));
    float sx = (float)destW/cropW, sy = (float)destH/cropH;
    if (cap.preserveAspect) { float sc = std::min(sx,sy); sx = sy = sc; }
    float cx = destW/2.0f, cy = destH/2.0f;
    g.TranslateTransform(cx,cy);
    g.RotateTransform(cap.rotation);
    g.ScaleTransform(sx,sy);
    g.TranslateTransform(-cropW/2.0f, -cropH/2.0f);
    g.DrawImage(cropped, 0,0, cropW,cropH);
    delete cropped;

    // Colour key
    if (cap.colorKeyEnabled) {
        BitmapData data;
        Rect rect(0,0,destW,destH);
        if (dest->LockBits(&rect, ImageLockModeRead|ImageLockModeWrite,
                           PixelFormat32bppARGB, &data) == Ok) {
            BYTE* pixels = (BYTE*)data.Scan0;
            int stride = data.Stride;
            auto within = [](BYTE a, BYTE b, int tol) { return abs(a-b)<=tol; };
            auto match = [&](BYTE r, BYTE g, BYTE b) -> bool {
                int tol = cap.tolerance;
                BYTE kr=GetRValue(cap.colorKey), kg=GetGValue(cap.colorKey), kb=GetBValue(cap.colorKey);
                if (within(r,kr,tol) && within(g,kg,tol) && within(b,kb,tol)) return true;
                for (COLORREF col : cap.multiColors) {
                    BYTE cr=GetRValue(col), cg=GetGValue(col), cb=GetBValue(col);
                    if (within(r,cr,tol) && within(g,cg,tol) && within(b,cb,tol)) return true;
                }
                return false;
            };
            for (int y=0; y<destH; ++y) {
                BYTE* row = pixels + y*stride;
                for (int x=0; x<destW; ++x) {
                    BYTE* p = row + x*4;
                    BYTE r=p[2], g=p[1], b=p[0];
                    bool isKey = match(r,g,b);
                    if (cap.colorPassMode) { if (!isKey) p[3]=0; }
                    else { if (isKey) p[3]=0; }
                }
            }
            dest->UnlockBits(&data);
        }
    }

    // Circular mask
    if (cap.circular) {
        int w = destW, h = destH;
        float cx2 = w/2.0f, cy2 = h/2.0f;
        float maxR = std::max(w,h)/2.0f;
        float radius = maxR * cap.circleRadius;
        BitmapData data;
        Rect rect(0,0,w,h);
        if (dest->LockBits(&rect, ImageLockModeRead|ImageLockModeWrite,
                           PixelFormat32bppARGB, &data) == Ok) {
            BYTE* pixels = (BYTE*)data.Scan0;
            int stride = data.Stride;
            for (int y=0; y<h; ++y) {
                BYTE* row = pixels + y*stride;
                for (int x=0; x<w; ++x) {
                    float dx = x - cx2, dy = y - cy2;
                    if (dx*dx+dy*dy > radius*radius)
                        row[x*4+3] = 0;
                }
            }
            dest->UnlockBits(&data);
        }
    }

    // Apply outline after all alpha-modifying operations
    if (cap.outlineEnabled) {
        ApplyOutlineToBitmap(dest, cap.outlineColor);
    }

    return dest;
}

// ----------------------------------------------
//  GUI pages
// ----------------------------------------------
enum eMenuPage { Page_Settings, Page_Resizing, Page_CustomCaptures };
static int currentPage = Page_Settings;

void RenderGUI(bool isAllowed) {
    // Dim the game window when in a non‑normal resize
    if (activeReszie != Rezise_Normal && g_targetHwnd && IsWindow(g_targetHwnd)) {
        RECT client;
        if (GetClientRect(g_targetHwnd, &client)) {
            POINT tl = {client.left, client.top}, br = {client.right, client.bottom};
            ClientToScreen(g_targetHwnd, &tl); ClientToScreen(g_targetHwnd, &br);
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            draw->AddRectFilled(ImVec2((float)tl.x,(float)tl.y),
                                ImVec2((float)br.x,(float)br.y),
                                IM_COL32(0,255,0,255));
        }
    }

    // Make overlay click‑through when GUI hidden, only touch the style when it changes.
    static int s_lastClickThrough = -1;   // -1 = unknown, forces apply on first frame
    int wantClickThrough = imguiSettings.togglegui ? 0 : 1;
    if (wantClickThrough != s_lastClickThrough) {
        LONG_PTR ex = GetWindowLongPtr(g_hWnd, GWL_EXSTYLE);
        if (wantClickThrough) ex |=  WS_EX_TRANSPARENT;
        else                  ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtr(g_hWnd, GWL_EXSTYLE, ex);
        s_lastClickThrough = wantClickThrough;
    }

    // Watermark overlay
    if (imguiSettings.overlay) {
        ImGui::SetNextWindowBgAlpha(0.1f);
        ImGui::Begin("Watermark", nullptr, ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        for (auto& t : waterMarkInfos.stringinfo) ImGui::Text("%s", t.c_str());
        ImGui::End();
    }

    // Main GUI window
    if (imguiSettings.togglegui)
    {
        ImGui::SetNextWindowPos(ImVec2(20,20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400,400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Externa", nullptr, ImGuiWindowFlags_NoTitleBar);
        if (bg_texture) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
            draw->AddImageRounded((ImTextureID)(intptr_t)bg_texture, pos,
                ImVec2(pos.x+size.x, pos.y+size.y), ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255,255,255,255), 6.0f, ImDrawFlags_RoundCornersAll);
        }
        ImGui::BeginChild("Sidebar", ImVec2(100,0), true);
        {
            if (ImGui::Button("Settings", ImVec2(80,30))) currentPage = Page_Settings;
            if (currentPage == Page_Settings) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("Resizing", ImVec2(80,30))) currentPage = Page_Resizing;
            if (currentPage == Page_Resizing) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("Custom Captures", ImVec2(80,30))) currentPage = Page_CustomCaptures;
            if (currentPage == Page_CustomCaptures) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("GoBorderless", ImVec2(80,30))) DoNormalResize();
            float wh = ImGui::GetWindowHeight();
            ImGui::SetCursorPosY(wh - 30 - ImGui::GetStyle().WindowPadding.y);
            if (ImGui::Button("Close", ImVec2(80,30))) {
                g_gameInfo.isRunning = false;
                PostQuitMessage(0);
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Content", ImVec2(0,0), true);
        {
            switch (currentPage) {
            case Page_Settings:
                {
                    ImGui::Text("Settings");
                    ImGui::Checkbox("Use StateOutput", &g_doStateOutput);
                    ImGui::InputText("State Pathaa", g_stateOutputLocation, sizeof(g_stateOutputLocation));
                    ImGui::SameLine();
                    if (ImGui::Button("Browseeee..."))
                    {
                        OPENFILENAMEA ofn = {sizeof(ofn)};
                        char szFile[260] = "";
                        ofn.lpstrFile = szFile;
                        ofn.nMaxFile = sizeof(szFile);
                        ofn.lpstrFilter = "Text Files\0*.txt;*.csv;*.log;*.ini\0All Files\0*.*\0";
                        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                        if (GetOpenFileNameA(&ofn))
                            strcpy_s(g_stateOutputLocation, szFile);
                    }
                    ImGui::Separator();
                    ImGui::Text("Hotkeys");
                    auto hotkeyBtn = [](const char* lbl, int& key) {
                        ImGui::Text("%s", lbl); ImGui::SameLine();
                        std::string btn = GetKeyName(key);
                        if (ImGui::Button(btn.c_str())) { g_capturingHotkey=true; g_capturingHotkeyFor=&key; }
                        ImGui::SameLine();
                        if (ImGui::Button(("Set##"+std::string(lbl)).c_str())) { g_capturingHotkey=true; g_capturingHotkeyFor=&key; }
                        ImGui::SameLine();
                        if (ImGui::Button(("Delete##"+std::string(lbl)).c_str())) key=0;
                    };
                    hotkeyBtn("Thin",  g_hotkeys.thinKey);
                    hotkeyBtn("Wide",  g_hotkeys.wideKey);
                    hotkeyBtn("Eye",   g_hotkeys.eyeKey);
                    hotkeyBtn("Normal",g_hotkeys.normalKey);
                    ImGui::Separator();
                    ImGui::Text("Delete Key"); ImGui::SameLine();
                    std::string delKey = GetKeyName(g_hotkeys.deleteKey);
                    if (ImGui::Button(delKey.c_str())) { g_capturingHotkey=true; g_capturingHotkeyFor=&g_hotkeys.deleteKey; }
                    ImGui::SameLine();
                    if (ImGui::Button("Set##Delete")) { g_capturingHotkey=true; g_capturingHotkeyFor=&g_hotkeys.deleteKey; }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete##Delete")) g_hotkeys.deleteKey=0;
                    ImGui::Separator();
                    ImGui::Checkbox("Eye Overlay", &g_eyeOverlayEnabled);
                    ImGui::Checkbox("Custom Overlay", &g_eyeOverlayCustom);
                    ImGui::Separator();
                    ImGui::Text("Window Restrictions");
                    ImGui::Checkbox("Restrict to allowed windows only", &g_restrictToAllowedWindows);
                    if (g_restrictToAllowedWindows) {
                        static char newWin[256]="";
                        ImGui::InputText("Add window title (partial)", newWin, sizeof(newWin));
                        ImGui::SameLine();
                        if (ImGui::Button("Add") && strlen(newWin)>0) {
                            g_allowedWindows.push_back(newWin);
                            newWin[0]=0;
                        }
                        for (int i=0; i<(int)g_allowedWindows.size(); ++i) {
                            ImGui::Text("%s", g_allowedWindows[i].c_str());
                            ImGui::SameLine();
                            if (ImGui::Button(("Remove##"+std::to_string(i)).c_str()))
                                g_allowedWindows.erase(g_allowedWindows.begin()+i--);
                        }
                    }
                    ImGui::Separator();
                    ImGui::Text("Resize Backgrounds");
                    static int bgMode=0;
                    const char* modeNames[]={"Thin","Wide","Eye","Normal"};
                    ImGui::Combo("Mode",&bgMode,modeNames,4);
                    ResizeBackground& bg = g_backgrounds[bgMode];
                    ImGui::Checkbox("Enable Background",&bg.enabled);
                    if (bg.enabled) {
                        ImGui::Checkbox("Use Image",&bg.useImage);
                        if (bg.useImage) {
                            ImGui::InputText("Image Path", bg.imagePath, sizeof(bg.imagePath));
                            ImGui::SameLine();
                            if (ImGui::Button("Browse...")) {
                                OPENFILENAMEA ofn={sizeof(ofn)};
                                char szFile[260]="";
                                ofn.lpstrFile=szFile; ofn.nMaxFile=sizeof(szFile);
                                ofn.lpstrFilter="Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
                                ofn.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
                                if (GetOpenFileNameA(&ofn)) {
                                    strcpy_s(bg.imagePath, szFile);
                                    bg.freeTexture();
                                }
                            }
                            if (bg.texture)
                                ImGui::Image((ImTextureID)bg.texture, ImVec2(100,100));
                        } else {
                            ImGui::ColorEdit4("Color", bg.color);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::Button("Save Settings")) SaveSettings();
                    break;
                }
            case Page_Resizing: {
                    ImGui::Text("Resizing Macros");
                    static bool resizeSuccess=false;
                    static std::string msg="";
                    ImGui::Separator();
                    ImGui::Text("Target Window");
                    ImGui::InputText("Window Title", g_targetWindowTitle, sizeof(g_targetWindowTitle));
                    ImGui::Separator();
                    ImGui::Text("Resize Macro Configuration");
                    ImGui::BeginTable("MacroTable",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg);
                    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed,100);
                    ImGui::TableSetupColumn("Dimensions", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,120);
                    ImGui::TableHeadersRow();

                    #define ROW(mode, w, h) \
                    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(#mode); \
                    ImGui::TableSetColumnIndex(1); \
                    ImGui::SetNextItemWidth(-1); ImGui::InputInt("##"#mode"_w", &w,10,50); \
                    ImGui::SetNextItemWidth(-1); ImGui::InputInt("##"#mode"_h", &h,10,50); \
                    ImGui::TableSetColumnIndex(2); \
                    if (ImGui::Button("Apply##"#mode, ImVec2(50,0))) { \
                    Do##mode##Resize(); msg = #mode " mode applied"; resizeSuccess=true; }

                    ROW(Thin, g_resizeDims.thin_w, g_resizeDims.thin_h);
                    ROW(Wide, g_resizeDims.wide_w, g_resizeDims.wide_h);
                    ROW(Eye,  g_resizeDims.eye_w,  g_resizeDims.eye_h);


                    // Normal row (clear)
                    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Normal");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-1); ImGui::InputInt("##normal_w", &g_resizeDims.normal_w,10,50);
                    ImGui::SetNextItemWidth(-1); ImGui::InputInt("##normal_h", &g_resizeDims.normal_h,10,50);
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Button("Clear##normal", ImVec2(50,0))) {
                        DoNormalResize(); msg = "Resize cleared"; resizeSuccess=true;
                    }
                    ImGui::EndTable();
                    if (!msg.empty()) {
                        ImGui::SameLine();
                        ImGui::TextColored(resizeSuccess ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "%s", msg.c_str());
                    }
                    break;
            }
            case Page_CustomCaptures: {
                    ImGui::Text("Custom Captures");
                    ImGui::Separator();
                    ImGui::Text("Add new capture:");
                    static char newName[256]="";
                    static int newX=0, newY=0, newW=100, newH=100;
                    static char newWindowTitle[256]="";
                    ImGui::InputText("Name", newName, sizeof(newName));
                    ImGui::InputInt("X",&newX); ImGui::InputInt("Y",&newY);
                    ImGui::InputInt("Width",&newW); ImGui::InputInt("Height",&newH);
                    ImGui::InputText("Window Title (optional)", newWindowTitle, sizeof(newWindowTitle));
                    if (ImGui::Button("Add Capture") && strlen(newName)>0 && newW>0 && newH>0) {
                        CustomCapture cap;
                        cap.name = newName; cap.x=newX; cap.y=newY; cap.width=newW; cap.height=newH;
                        cap.targetWidth=newW; cap.targetHeight=newH;
                        cap.displayPos=ImVec2((float)newX,(float)newY);
                        strcpy_s(cap.targetWindowTitle, newWindowTitle);
                        g_customCaptures.push_back(cap);
                        memset(newName,0,sizeof(newName)); newX=newY=0; newW=newH=100; newWindowTitle[0]=0;
                    }
                    ImGui::Separator();
                    ImGui::Text("Captures:");
                    int editingIdx = -1;
                    for (size_t i=0; i<g_customCaptures.size(); ++i) {
                        auto& cap = g_customCaptures[i];
                        ImGui::PushID(i);
                        ImGui::Checkbox("##enabled", &cap.enabled);
                        ImGui::SameLine();
                        ImGui::Text("%s: (%d,%d) %dx%d", cap.name.c_str(), cap.x, cap.y, cap.width, cap.height);
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            cap.freeTexture();
                            g_customCaptures.erase(g_customCaptures.begin()+i);
                            --i; ImGui::PopID(); continue;
                        }
                        if (ImGui::TreeNode(("Settings##"+cap.name).c_str())) {
                            editingIdx = (int)i;
                            ImGui::InputText("Target Window Title", cap.targetWindowTitle, sizeof(cap.targetWindowTitle));
                            ImGui::Text("Visible on:");
                            ImGui::CheckboxFlags("Thin",&cap.visibilityModes,1<<Rezise_Thin); ImGui::SameLine();
                            ImGui::CheckboxFlags("Wide",&cap.visibilityModes,1<<Rezise_Wide); ImGui::SameLine();
                            ImGui::CheckboxFlags("Eye",&cap.visibilityModes,1<<Rezise_Eye); ImGui::SameLine();
                            ImGui::CheckboxFlags("Normal",&cap.visibilityModes,1<<Rezise_Normal);
                            ImGui::Text("Crop (relative to capture area)");
                            ImGui::InputInt("Crop X",&cap.cropX); ImGui::InputInt("Crop Y",&cap.cropY);
                            ImGui::InputInt("Crop Width",&cap.cropW); ImGui::InputInt("Crop Height",&cap.cropH);
                            ImGui::InputInt("Capture Width",&cap.width); ImGui::InputInt("Capture Height",&cap.height);
                            int tsz[2] = {cap.targetWidth, cap.targetHeight};
                            if (ImGui::InputInt2("Target Size", tsz)) {
                                cap.targetWidth=tsz[0]; cap.targetHeight=tsz[1];
                            }
                            ImGui::Separator();
                            ImGui::SliderFloat("Rotation", &cap.rotation, -180,180);
                            ImGui::Checkbox("Preserve Aspect", &cap.preserveAspect);
                            // Outline controls (replaces requireColor)
                            ImGui::Checkbox("Apply Outline", &cap.outlineEnabled);
                            if (cap.outlineEnabled) {
                                float col[3] = { GetRValue(cap.outlineColor)/255.0f,
                                                 GetGValue(cap.outlineColor)/255.0f,
                                                 GetBValue(cap.outlineColor)/255.0f };
                                if (ImGui::ColorEdit3("Outline Colour", col))
                                    cap.outlineColor = RGB((int)(col[0]*255),(int)(col[1]*255),(int)(col[2]*255));
                            }
                            ImGui::Checkbox("Color Key", &cap.colorKeyEnabled);
                            if (cap.colorKeyEnabled) {
                                float col[3] = { GetRValue(cap.colorKey)/255.0f,
                                                 GetGValue(cap.colorKey)/255.0f,
                                                 GetBValue(cap.colorKey)/255.0f };
                                if (ImGui::ColorEdit3("Key Colour", col))
                                    cap.colorKey = RGB((int)(col[0]*255),(int)(col[1]*255),(int)(col[2]*255));
                                ImGui::SliderInt("Tolerance", &cap.tolerance, 0, 255);
                                ImGui::Checkbox("Color Pass Mode", &cap.colorPassMode);
                                ImGui::Text("Additional Colors:");
                                for (int j=0; j<(int)cap.multiColors.size(); ++j) {
                                    ImGui::PushID(j);
                                    float mcol[3] = { GetRValue(cap.multiColors[j])/255.0f,
                                                      GetGValue(cap.multiColors[j])/255.0f,
                                                      GetBValue(cap.multiColors[j])/255.0f };
                                    if (ImGui::ColorEdit3("##col", mcol))
                                        cap.multiColors[j] = RGB((int)(mcol[0]*255),(int)(mcol[1]*255),(int)(mcol[2]*255));
                                    ImGui::SameLine();
                                    if (ImGui::Button("Remove")) { cap.multiColors.erase(cap.multiColors.begin()+j); --j; }
                                    ImGui::PopID();
                                }
                                static COLORREF newCol = RGB(255,0,0);
                                if (ImGui::ColorEdit3("New Color", (float*)&newCol)) {}
                                ImGui::SameLine();
                                if (ImGui::Button("Add Color")) cap.multiColors.push_back(newCol);
                            }
                            ImGui::Checkbox("Circular", &cap.circular);
                            if (cap.circular) {
                                ImGui::TextWrapped("Image will be masked to a circle inscribed in the target size.");
                                ImGui::SliderFloat("Circle Radius", &cap.circleRadius, 0.1f, 1.0f);
                            }
                            ImGui::InputFloat2("Display Position", &cap.displayPos.x);
                            ImGui::InputFloat2("Display Size", &cap.displaySize.x);
                            if (cap.displaySize.x==0 && cap.displaySize.y==0)
                                ImGui::Text("Display size will be target size");
                            if (ImGui::Button("Set Display Pos from Capture"))
                                cap.displayPos = ImVec2((float)cap.x, (float)cap.y);
                            if (cap.texture) {
                                ImGui::Text("Preview:");
                                ImVec2 prev(200,200);
                                float asp = (float)cap.texHeight/cap.texWidth;
                                prev.y = prev.x * asp;
                                ImGui::Image((ImTextureID)cap.texture, prev);
                            } else {
                                ImGui::Text("Preview not available");
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    g_editingCustomCaptureIndex = editingIdx;
                    ImGui::Separator();
                    if (ImGui::Button("Capture All Enabled to Files")) {
                        for (const auto& cap : g_customCaptures) {
                            if (cap.enabled) {
                                std::string fname = cap.name + ".png";
                                if (cap.targetWindowTitle[0]!=0) {
                                    HWND hw = FindWindowByPartialTitle(std::wstring(cap.targetWindowTitle,
                                        cap.targetWindowTitle+strlen(cap.targetWindowTitle)).c_str());
                                    if (hw) {
                                        RECT cl; GetClientRect(hw,&cl);
                                        POINT tl = {cl.left,cl.top}; ClientToScreen(hw,&tl);
                                        CaptureScreenArea(tl.x+cap.x, tl.y+cap.y, cap.width, cap.height, fname);
                                    } else {
                                        CaptureScreenArea(cap.x, cap.y, cap.width, cap.height, fname);
                                    }
                                } else {
                                    CaptureScreenArea(cap.x, cap.y, cap.width, cap.height, fname);
                                }
                            }
                        }
                        ImGui::Text("Captures saved!");
                    }
                    break;
            }
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

    // Draw macro capture overlay
    if (isAllowed && g_activeMacro>=0 && g_captureSettings[g_activeMacro].enabled && g_captureTexture) {
        CaptureSettings& s = g_captureSettings[g_activeMacro];
        ImVec2 sz = (s.displaySize.x>0 && s.displaySize.y>0) ? s.displaySize : ImVec2((float)s.targetWidth,(float)s.targetHeight);
        ImGui::SetNextWindowPos(s.displayPos);
        ImGui::SetNextWindowSize(sz);
        ImGui::Begin("CaptureOverlay", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
                     ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|
                     ImGuiWindowFlags_NoInputs);
        ImGui::Image((ImTextureID)g_captureTexture, sz);
        ImGui::End();
    }

    // Draw custom capture overlays
    if (isAllowed) {
        for (auto& cap : g_customCaptures) {
            if (cap.enabled && (cap.visibilityModes & (1<<activeReszie)) && cap.texture) {
                ImVec2 sz = (cap.displaySize.x>0 && cap.displaySize.y>0) ? cap.displaySize : ImVec2((float)cap.targetWidth,(float)cap.targetHeight);
                ImGui::SetNextWindowPos(cap.displayPos);
                ImGui::SetNextWindowSize(sz);
                ImGui::Begin(("CustomOverlay_"+cap.name).c_str(), nullptr,
                             ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|
                             ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoInputs);
                ImGui::Image((ImTextureID)cap.texture, sz);
                ImGui::End();
            }
        }
        if (g_eyeOverlayCaptureActive && g_eyeOverlayCapture.texture) {
            ImVec2 sz = (g_eyeOverlayCapture.displaySize.x>0 && g_eyeOverlayCapture.displaySize.y>0) ?
                        g_eyeOverlayCapture.displaySize :
                        ImVec2((float)g_eyeOverlayCapture.targetWidth, (float)g_eyeOverlayCapture.targetHeight);
            ImGui::SetNextWindowPos(g_eyeOverlayCapture.displayPos);
            ImGui::SetNextWindowSize(sz);
            ImGui::Begin("EyeOverlay", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
                         ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBackground|
                         ImGuiWindowFlags_NoInputs);
            ImGui::Image((ImTextureID)g_eyeOverlayCapture.texture, sz);
            ImGui::End();
        }
    }

    // Dragging logic for custom capture preview rectangles
    if (g_editingCustomCaptureIndex>=0 && g_editingCustomCaptureIndex<(int)g_customCaptures.size()) {
        auto& cap = g_customCaptures[g_editingCustomCaptureIndex];
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImVec2 p1((float)cap.x, (float)cap.y);
        ImVec2 p2((float)(cap.x+cap.width), (float)(cap.y+cap.height));
        draw->AddRect(p1,p2, IM_COL32(0,255,0,128), 2.0f,0,3.0f);
        float dw = cap.displaySize.x>0 ? cap.displaySize.x : (float)cap.targetWidth;
        float dh = cap.displaySize.y>0 ? cap.displaySize.y : (float)cap.targetHeight;
        ImVec2 d1(cap.displayPos.x, cap.displayPos.y);
        ImVec2 d2(cap.displayPos.x+dw, cap.displayPos.y+dh);
        draw->AddRect(d1,d2, IM_COL32(255,255,0,128), 2.0f,0,2.0f);
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            ImVec2 mp = io.MousePos;
            bool down = io.MouseDown[0], released = io.MouseReleased[0];
            if (down && g_draggingCaptureIndex==-1) {
                if (mp.x>=cap.x && mp.x<=cap.x+cap.width && mp.y>=cap.y && mp.y<=cap.y+cap.height) {
                    g_draggingCaptureIndex = g_editingCustomCaptureIndex;
                    g_draggingCaptureRect = true;
                    g_lastMousePos = mp;
                } else if (mp.x>=cap.displayPos.x && mp.x<=cap.displayPos.x+dw &&
                           mp.y>=cap.displayPos.y && mp.y<=cap.displayPos.y+dh) {
                    g_draggingCaptureIndex = g_editingCustomCaptureIndex;
                    g_draggingDisplayRect = true;
                    g_lastMousePos = mp;
                }
            }
            if (g_draggingCaptureIndex == g_editingCustomCaptureIndex && down) {
                ImVec2 delta = ImVec2(mp.x-g_lastMousePos.x, mp.y-g_lastMousePos.y);
                if (delta.x!=0 || delta.y!=0) {
                    if (g_draggingCaptureRect) { cap.x += (int)delta.x; cap.y += (int)delta.y; }
                    else if (g_draggingDisplayRect) { cap.displayPos.x += delta.x; cap.displayPos.y += delta.y; }
                    g_lastMousePos = mp;
                }
            }
            if (released) {
                g_draggingCaptureIndex = -1;
                g_draggingCaptureRect = g_draggingDisplayRect = false;
            }
        }
    } else {
        g_draggingCaptureIndex = -1;
        g_draggingCaptureRect = g_draggingDisplayRect = false;
    }
}

bool IsForegroundAllowedForHotkeys() { return IsForegroundAllowed(); }

void KeyHandler() {
    while (g_gameInfo.isRunning) {
        if (g_capturingHotkey && g_capturingHotkeyFor) {
            for (int key=1; key<=255; ++key) {
                if (key==VK_LBUTTON) continue;
                if (GetAsyncKeyState(key) & 0x0001) {
                    *g_capturingHotkeyFor = key;
                    g_capturingHotkey = false;
                    g_capturingHotkeyFor = nullptr;
                    break;
                }
            }
        } else if (IsForegroundAllowedForHotkeys()) {
            if (GetAsyncKeyState(g_hotkeys.thinKey) & 0x0001) DoThinResize();
            if (GetAsyncKeyState(g_hotkeys.wideKey) & 0x0001) DoWideResize();
            if (GetAsyncKeyState(g_hotkeys.eyeKey)  & 0x0001) DoEyeResize();
            if (GetAsyncKeyState(g_hotkeys.deleteKey)&0x0001) DoNormalResize();
            if (GetAsyncKeyState(VK_UP) & 0x0001) {
                imguiSettings.togglegui = !imguiSettings.togglegui;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ----------------------------------------------
//  DirectX & WinMain
// ----------------------------------------------
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2; sd.BufferDesc.Width = 0; sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60; sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &level, &g_pd3dDeviceContext) != S_OK)
        return false;
    ID3D11Texture2D* back = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) { g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_pRenderTargetView); back->Release(); }
    return true;
}

void CleanupDeviceD3D() {
    if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView=nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain=nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext=nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice=nullptr; }
}

extern IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView=nullptr; }
                g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* back = nullptr;
                g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
                if (back) { g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_pRenderTargetView); back->Release(); }
            }
            return 0;
        case WM_DESTROY:
            g_gameInfo.isRunning = false;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    GdiplusStartupInput gdiSI;
    GdiplusStartup(&g_gdiplusToken, &gdiSI, nullptr);
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0,0, hInstance,
                      nullptr, nullptr, nullptr, nullptr, _T("GameWindow"), nullptr };
    if (!RegisterClassEx(&wc)) return 1;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hWnd = CreateWindowEx(WS_EX_LAYERED, wc.lpszClassName, _T("Game Window with ImGui"),
                            WS_POPUP, 0,0, sw,sh, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return 1;
    SetLayeredWindowAttributes(g_hWnd, RGB(0,255,0), 0, LWA_COLORKEY);
    ShowWindow(g_hWnd, SW_SHOWMAXIMIZED); UpdateWindow(g_hWnd);
    if (!CreateDeviceD3D(g_hWnd)) { CleanupDeviceD3D(); return 1; }
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark(); SetupImGuiStyle();
    LoadSettings();
    ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplWin32_Init(g_hWnd); ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    std::thread keyloop(KeyHandler);
    bool ret = LoadTextureFromMemory(CheatBg_png, CheatBg_png_len, &bg_texture, &bg_texture_width, &bg_texture_height);
    IM_ASSERT(ret);
    MSG msg = {}; auto lastTime = std::chrono::high_resolution_clock::now(); int frames=0;
    while (msg.message != WM_QUIT && g_gameInfo.isRunning) {
        if (PeekMessage(&msg, nullptr,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); continue; }
        frames++; auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now-lastTime).count();
        if (dt >= 1.0f) { g_gameInfo.frameRate = frames/dt; g_gameInfo.frameCount+=frames; frames=0; lastTime=now; }
        if (g_targetHwnd && !IsWindow(g_targetHwnd)) {
            g_targetHwnd = nullptr; SetOverlayOwner(nullptr);
            if (g_eyeOverlayCaptureActive) { g_eyeOverlayCapture.freeTexture(); g_eyeOverlayCaptureActive=false; }
        }
        if (g_eyeOverlayCaptureActive && g_targetHwnd && IsWindow(g_targetHwnd)) {
            EyeZoomConfig ez; RECT client; GetClientRect(g_targetHwnd, &client);
            RECT crop = EyeZoomCropRect(ez, client.right, client.bottom);
            Bitmap* bmp = CaptureAndBlend(g_targetHwnd, L"overlay.png",
                                          g_eyeOverlayCapture.width, g_eyeOverlayCapture.height, &crop);
            UpdateCustomCaptureTexture(g_eyeOverlayCapture, bmp);
            delete bmp;
        }

        bool allowed = IsForegroundAllowed();
        if (allowed && g_activeMacro>=0 && g_captureSettings[g_activeMacro].enabled && g_targetHwnd) {
            Bitmap* bmp = CaptureTransformed(g_targetHwnd, g_captureSettings[g_activeMacro]);
            UpdateCaptureTexture(bmp); delete bmp;
        } else { UpdateCaptureTexture(nullptr); }
        if (allowed) {
            for (auto& cap : g_customCaptures) {
                if (cap.enabled) {
                    Bitmap* bmp = CaptureWindowOrDesktop(cap);
                    UpdateCustomCaptureTexture(cap, bmp); delete bmp;
                } else cap.freeTexture();
            }
        } else {
            for (auto& cap : g_customCaptures) cap.freeTexture();
            if (g_eyeOverlayCaptureActive) { g_eyeOverlayCapture.freeTexture(); g_eyeOverlayCaptureActive=false; }
        }
        ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        DrawResizeBackground(); RenderGUI(allowed); ImGui::Render();
        float clear[4] = {0,1,0,0};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }
    g_gameInfo.isRunning = false; if (keyloop.joinable()) keyloop.join();
    SaveSettings(); DoNormalResize();
    if (bg_texture) bg_texture->Release();
    if (g_captureTexture) g_captureTexture->Release();
    for (auto& c : g_customCaptures) c.freeTexture();
    for (int i=0;i<4;++i) g_backgrounds[i].freeTexture();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); DestroyWindow(g_hWnd); UnregisterClass(wc.lpszClassName, hInstance);
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
