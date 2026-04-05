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
#include <fstream>

#include "eyeframe.h"
#include "eyezoom.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")

using json = nlohmann::json;
using namespace Gdiplus;


//todo list
//  make compatable with more than 1080p
//  add png overlay stuff with image path for ninjabrainbot etc
//  split into more classes for readablity
//  genral cleanup ig
//  beg somone to fix it
//  more begging
//  fix windows boarderless being a needy bitch
//  fix eyezoom being a pretentious bitch
//  make defualt settings for pichart
//  add a setting so that a capture only works if there is a specific colour in the capture ie the f3 background so it doesnt capture when f3 not open (is this legal?)



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

struct HotkeyConfig {
    int thinKey = VK_F1;
    int wideKey = VK_F2;
    int eyeKey = VK_F3;
    int normalKey = VK_F4;
};

inline std::wstring toWide(const char* str) {
    return std::wstring(str, str + strlen(str));
}

HotkeyConfig g_hotkeys;

struct ResizeDimensions {
    int thin_w = 340;
    int thin_h = 1000;
    int wide_w = 1920;
    int wide_h = 300;
    int eye_w = 384;
    int eye_h = 16384;
    int normal_w = 1920;
    int normal_h = 1080;
} g_resizeDims;

struct imguiSet {
    bool togglegui = true;
    bool overlay = false;
} imguiSettings;

struct waterMarkInfo {
    std::vector<std::string> stringinfo = {""};
} waterMarkInfos;


struct CaptureSettings {
    bool enabled = false;
    RECT cropRect = {0, 0, 100, 100};
    int targetWidth = 100;
    int targetHeight = 100;
    float rotation = 0.0f;
    bool preserveAspect = true;
    bool colorKeyEnabled = false;
    COLORREF colorKey = RGB(0, 255, 0);
    int tolerance = 30;
    std::vector<COLORREF> multiColors;
    ImVec2 displayPos = ImVec2(0, 0);
    ImVec2 displaySize = ImVec2(0, 0);
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

int bg_texture_width = 16;
int bg_texture_height = 16;
ID3D11ShaderResourceView* bg_texture = nullptr;

struct GameInfo {
    int frameCount = 0;
    float frameRate = 0.0f;
    std::atomic<bool> isRunning = true;
} g_gameInfo;

bool g_capturingHotkey = false;
int* g_capturingHotkeyFor = nullptr;


struct CustomCapture {
    std::string name;

    int x = 0, y = 0, width = 800, height = 420;
    bool enabled = true;
    int visibilityModes = (1 << Rezise_Thin) | (1 << Rezise_Wide) | (1 << Rezise_Eye) | (1 << Rezise_Normal);

    int cropX = 0, cropY = 0, cropW = 0, cropH = 0;
    int targetWidth = 100;
    int targetHeight = 100;
    float rotation = 0.0f;
    bool preserveAspect = true;
    bool colorKeyEnabled = false;
    COLORREF colorKey = RGB(0, 255, 0);
    int tolerance = 30;
    std::vector<COLORREF> multiColors;
    ImVec2 displayPos = ImVec2(0, 0);
    ImVec2 displaySize = ImVec2(0, 0);
    bool colorPassMode = false;

    char targetWindowTitle[256] = "";

    ID3D11ShaderResourceView* texture = nullptr;
    int texWidth = 0, texHeight = 0;

    void freeTexture() {
        if (texture) {
            texture->Release();
            texture = nullptr;
        }
        texWidth = texHeight = 0;
    }
};

std::vector<CustomCapture> g_customCaptures;
int g_editingCustomCaptureIndex = -1;
int g_draggingCaptureIndex = -1;
bool g_draggingCaptureRect = false;
bool g_draggingDisplayRect = false;
ImVec2 g_lastMousePos;


Gdiplus::Bitmap* g_eyeOverlayImage = nullptr;      // loaded overlay.png
CustomCapture g_eyeOverlayCapture;                 // temporary capture for eye mode
bool g_eyeOverlayCaptureActive = false;            // true when in Eye mode with overlay


bool g_restrictToAllowedWindows = false;
std::vector<std::string> g_allowedWindows;


struct ResizeBackground {
    bool enabled = false;
    bool useImage = false;
    char imagePath[256] = "";
    float color[4] = {0.5f, 0.0f, 0.5f, 1.0f};
    ID3D11ShaderResourceView* texture = nullptr;
    int texWidth = 0, texHeight = 0;

    void freeTexture() {
        if (texture) {
            texture->Release();
            texture = nullptr;
        }
        texWidth = texHeight = 0;
    }
};
ResizeBackground g_backgrounds[4];


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
    if (GetKeyNameTextA(scanCode << 16, name, sizeof(name))) {
        return std::string(name);
    }
    char fallback[16];
    sprintf_s(fallback, "Key %d", vk);
    return fallback;
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

bool canresize() {
    return true;
}


void LoadEyeOverlayImage()
{
    if (g_eyeOverlayImage) return;
    EyeZoomConfig ezCfg;
    // ezCfg.numberStyle = "stacked";
    // ezCfg.gridColor1[0] = 0.82f; ezCfg.gridColor1[1] = 0.62f; ezCfg.gridColor1[2] = 0.88f; ezCfg.gridColor1[3] = 1.0f;
    // ezCfg.gridColor2[0] = 1.0f; ezCfg.gridColor2[1] = 1.0f; ezCfg.gridColor2[2] = 1.0f; ezCfg.gridColor2[3] = 1.0f;

    if (!g_eyeOverlayCustom) {
        GenerateEyeZoomOverlay(ezCfg, g_eyeOverlayCapture.width, g_eyeOverlayCapture.height, L"overlay.png");
    }

    std::wstring path = L"overlay.png"; // adjust path if needed
    Gdiplus::Bitmap* temp = Gdiplus::Bitmap::FromFile(path.c_str());
    // GenerateEyeZoomOverlay(ezCfg, g_eyeOverlayCapture.width, g_eyeOverlayCapture.height, L"overlay.png");
    if (temp && temp->GetLastStatus() == Gdiplus::Ok) {
        // Convert to 32bpp ARGB to ensure proper stretching and alpha
        int w = temp->GetWidth();
        int h = temp->GetHeight();
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
    if (!bmp) {
        cap.freeTexture();
        return;
    }
    int width = bmp->GetWidth();
    int height = bmp->GetHeight();
    if (width <= 0 || height <= 0) return;

    BitmapData bmpData;
    Rect rect(0, 0, width, height);
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok)
        return;

    if (!cap.texture || cap.texWidth != width || cap.texHeight != height) {
        cap.freeTexture();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* tex = nullptr;
        if (g_pd3dDevice->CreateTexture2D(&desc, nullptr, &tex) == S_OK) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &cap.texture);
            tex->Release();
        }
        cap.texWidth = width;
        cap.texHeight = height;
    }

    if (cap.texture) {
        ID3D11Resource* resource;
        cap.texture->GetResource(&resource);
        ID3D11Texture2D* tex = static_cast<ID3D11Texture2D*>(resource);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (g_pd3dDeviceContext->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK) {
            BYTE* dst = (BYTE*)mapped.pData;
            BYTE* src = (BYTE*)bmpData.Scan0;
            int srcPitch = bmpData.Stride;
            for (int y = 0; y < height; ++y) {
                memcpy(dst + y * mapped.RowPitch, src + y * srcPitch, width * 4);
            }
            g_pd3dDeviceContext->Unmap(tex, 0);
        }
        resource->Release();
    }

    bmp->UnlockBits(&bmpData);
}


void SetOverlayOwner(HWND target) {
    if (target && IsWindow(target)) {
        SetWindowLongPtr(g_hWnd, GWLP_HWNDPARENT, (LONG_PTR)target);
        SetWindowPos(g_hWnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        SetWindowLongPtr(g_hWnd, GWLP_HWNDPARENT, 0);
    }
}

void UpdateCustomCapturesVisibility() {
    for (auto& cap : g_customCaptures) {
        if (!cap.enabled) {
            cap.freeTexture();
        } else if (!(cap.visibilityModes & (1 << activeReszie))) {
            cap.freeTexture();
        }
    }
}

void SaveSettings();

void DoNormalResize() {
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
    if (activeReszie == Rezise_Thin) {
        DoNormalResize();
        return;
    }
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
        if (activeReszie == Rezise_Wide)
        {
            DoNormalResize();
            return;
        }

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
    if (activeReszie == Rezise_Eye)
    {
        DoNormalResize();
        return;
    }
    HWND hWnd = FindWindowByPartialTitle(toWide(g_targetWindowTitle).c_str());
    if (!hWnd) return;

    Resizing::toggleResize(hWnd, g_resizeDims.eye_w, g_resizeDims.eye_h, true);
    activeReszie = Rezise_Eye;
    g_targetHwnd = hWnd;
    UpdateCutoutRect();

    // Setup eye overlay capture if enabled
    if (g_eyeOverlayEnabled) {
        LoadEyeOverlayImage();
        if (g_eyeOverlayImage) {
            const int EYE_WIDTH = 768;
            const int EYE_HEIGHT = 432;

            g_eyeOverlayCapture.name = "EyeOverlay";
            g_eyeOverlayCapture.width = EYE_WIDTH;
            g_eyeOverlayCapture.height = EYE_HEIGHT;
            g_eyeOverlayCapture.targetWidth = EYE_WIDTH;
            g_eyeOverlayCapture.targetHeight = EYE_HEIGHT;
            g_eyeOverlayCapture.displaySize = ImVec2((float)EYE_WIDTH, (float)EYE_HEIGHT);
            g_eyeOverlayCapture.enabled = true;
            g_eyeOverlayCaptureActive = true;

            // Position: left of game window, vertically centered
            RECT gameRect;
            GetWindowRect(hWnd, &gameRect);
            int gameCenterY = (gameRect.top + gameRect.bottom) / 2;
            int overlayTop = gameCenterY - EYE_HEIGHT / 2;
            g_eyeOverlayCapture.displayPos = ImVec2((float)gameRect.left - EYE_WIDTH, (float)overlayTop);
        } else {
            g_eyeOverlayCaptureActive = false;
        }
    }

    UpdateCustomCapturesVisibility();
    SaveSettings();
}


void SaveSettings() {
    json j;
    j["target_window_title"] = g_targetWindowTitle;
    j["resize_dims"]["thin_w"] = g_resizeDims.thin_w;
    j["resize_dims"]["thin_h"] = g_resizeDims.thin_h;
    j["resize_dims"]["wide_w"] = g_resizeDims.wide_w;
    j["resize_dims"]["wide_h"] = g_resizeDims.wide_h;
    j["resize_dims"]["eye_w"] = g_resizeDims.eye_w;
    j["resize_dims"]["eye_h"] = g_resizeDims.eye_h;
    j["resize_dims"]["normal_w"] = g_resizeDims.normal_w;
    j["resize_dims"]["normal_h"] = g_resizeDims.normal_h;
    j["hotkeys"]["thin"] = g_hotkeys.thinKey;
    j["hotkeys"]["wide"] = g_hotkeys.wideKey;
    j["hotkeys"]["eye"] = g_hotkeys.eyeKey;
    j["hotkeys"]["normal"] = g_hotkeys.normalKey;
    j["overlay"] = imguiSettings.overlay;
    j["eye_overlay"] = g_eyeOverlayEnabled;
    j["eye_overlay_custom"] = g_eyeOverlayCustom;
    j["restrict_to_allowed_windows"] = g_restrictToAllowedWindows;
    j["allowed_windows"] = g_allowedWindows;

    json captureArray = json::array();
    for (int i = 0; i < 4; ++i) {
        json cap;
        cap["enabled"] = g_captureSettings[i].enabled;
        cap["cropRect"] = { g_captureSettings[i].cropRect.left,
                            g_captureSettings[i].cropRect.top,
                            g_captureSettings[i].cropRect.right,
                            g_captureSettings[i].cropRect.bottom };
        cap["targetWidth"] = g_captureSettings[i].targetWidth;
        cap["targetHeight"] = g_captureSettings[i].targetHeight;
        cap["rotation"] = g_captureSettings[i].rotation;
        cap["preserveAspect"] = g_captureSettings[i].preserveAspect;
        cap["colorKeyEnabled"] = g_captureSettings[i].colorKeyEnabled;
        cap["colorKey"] = (int)g_captureSettings[i].colorKey;
        cap["tolerance"] = g_captureSettings[i].tolerance;
        cap["multiColors"] = json::array();
        for (auto c : g_captureSettings[i].multiColors) cap["multiColors"].push_back((int)c);
        cap["displayPos"] = { g_captureSettings[i].displayPos.x, g_captureSettings[i].displayPos.y };
        cap["displaySize"] = { g_captureSettings[i].displaySize.x, g_captureSettings[i].displaySize.y };
        cap["colorPassMode"] = g_captureSettings[i].colorPassMode;
        captureArray.push_back(cap);
    }
    j["capture"] = captureArray;

    json customArray = json::array();
    for (const auto& cap : g_customCaptures) {
        json jcap;
        jcap["name"] = cap.name;
        jcap["x"] = cap.x;
        jcap["y"] = cap.y;
        jcap["width"] = cap.width;
        jcap["height"] = cap.height;
        jcap["enabled"] = cap.enabled;
        jcap["visibilityModes"] = cap.visibilityModes;
        jcap["cropX"] = cap.cropX;
        jcap["cropY"] = cap.cropY;
        jcap["cropW"] = cap.cropW;
        jcap["cropH"] = cap.cropH;
        jcap["targetWidth"] = cap.targetWidth;
        jcap["targetHeight"] = cap.targetHeight;
        jcap["rotation"] = cap.rotation;
        jcap["preserveAspect"] = cap.preserveAspect;
        jcap["colorKeyEnabled"] = cap.colorKeyEnabled;
        jcap["colorKey"] = (int)cap.colorKey;
        jcap["tolerance"] = cap.tolerance;
        jcap["multiColors"] = json::array();
        for (auto c : cap.multiColors) jcap["multiColors"].push_back((int)c);
        jcap["displayPos"] = { cap.displayPos.x, cap.displayPos.y };
        jcap["displaySize"] = { cap.displaySize.x, cap.displaySize.y };
        jcap["colorPassMode"] = cap.colorPassMode;
        jcap["targetWindowTitle"] = std::string(cap.targetWindowTitle);
        customArray.push_back(jcap);
    }
    j["custom_captures"] = customArray;

    json backgroundsArray = json::array();
    for (int i = 0; i < 4; ++i) {
        json bg;
        bg["enabled"] = g_backgrounds[i].enabled;
        bg["useImage"] = g_backgrounds[i].useImage;
        bg["imagePath"] = std::string(g_backgrounds[i].imagePath);
        bg["color"] = { g_backgrounds[i].color[0], g_backgrounds[i].color[1],
                        g_backgrounds[i].color[2], g_backgrounds[i].color[3] };
        backgroundsArray.emplace_back(bg);
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
        if (j.contains("target_window_title"))
            strcpy_s(g_targetWindowTitle, j["target_window_title"].get<std::string>().c_str());
        if (j.contains("resize_dims")) {
            auto& dims = j["resize_dims"];
            if (dims.contains("thin_w")) g_resizeDims.thin_w = dims["thin_w"];
            if (dims.contains("thin_h")) g_resizeDims.thin_h = dims["thin_h"];
            if (dims.contains("wide_w")) g_resizeDims.wide_w = dims["wide_w"];
            if (dims.contains("wide_h")) g_resizeDims.wide_h = dims["wide_h"];
            if (dims.contains("eye_w"))  g_resizeDims.eye_w  = dims["eye_w"];
            if (dims.contains("eye_h"))  g_resizeDims.eye_h  = dims["eye_h"];
            if (dims.contains("normal_w"))g_resizeDims.normal_w = dims["normal_w"];
            if (dims.contains("normal_h"))g_resizeDims.normal_h = dims["normal_h"];
        }
        if (j.contains("hotkeys")) {
            auto& hk = j["hotkeys"];
            if (hk.contains("thin")) g_hotkeys.thinKey = hk["thin"];
            if (hk.contains("wide")) g_hotkeys.wideKey = hk["wide"];
            if (hk.contains("eye"))  g_hotkeys.eyeKey  = hk["eye"];
            if (hk.contains("normal"))g_hotkeys.normalKey = hk["normal"];
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
            for (size_t i = 0; i < caps.size() && i < 4; ++i) {
                auto& cap = caps[i];
                g_captureSettings[i].enabled = cap.value("enabled", false);
                if (cap.contains("cropRect")) {
                    auto& cr = cap["cropRect"];
                    if (cr.size() >= 4) {
                        g_captureSettings[i].cropRect.left = cr[0];
                        g_captureSettings[i].cropRect.top = cr[1];
                        g_captureSettings[i].cropRect.right = cr[2];
                        g_captureSettings[i].cropRect.bottom = cr[3];
                    }
                }
                g_captureSettings[i].targetWidth = cap.value("targetWidth", 100);
                g_captureSettings[i].targetHeight = cap.value("targetHeight", 100);
                g_captureSettings[i].rotation = cap.value("rotation", 0.0f);
                g_captureSettings[i].preserveAspect = cap.value("preserveAspect", true);
                g_captureSettings[i].colorKeyEnabled = cap.value("colorKeyEnabled", false);
                g_captureSettings[i].colorKey = cap.value("colorKey", (int)RGB(0,255,0));
                g_captureSettings[i].tolerance = cap.value("tolerance", 30);
                if (cap.contains("multiColors")) {
                    g_captureSettings[i].multiColors.clear();
                    for (auto& col : cap["multiColors"]) {
                        g_captureSettings[i].multiColors.push_back((COLORREF)col.get<int>());
                    }
                }
                if (cap.contains("displayPos") && cap["displayPos"].size() >= 2) {
                    g_captureSettings[i].displayPos.x = cap["displayPos"][0];
                    g_captureSettings[i].displayPos.y = cap["displayPos"][1];
                }
                if (cap.contains("displaySize") && cap["displaySize"].size() >= 2) {
                    g_captureSettings[i].displaySize.x = cap["displaySize"][0];
                    g_captureSettings[i].displaySize.y = cap["displaySize"][1];
                }
                g_captureSettings[i].colorPassMode = cap.value("colorPassMode", false);
            }
        }

        if (j.contains("custom_captures")) {
            for (const auto& jcap : j["custom_captures"]) {
                CustomCapture cap;
                cap.name = jcap.value("name", "");
                cap.x = jcap.value("x", 0);
                cap.y = jcap.value("y", 0);
                cap.width = jcap.value("width", 100);
                cap.height = jcap.value("height", 100);
                cap.enabled = jcap.value("enabled", true);
                cap.visibilityModes = jcap.value("visibilityModes",
                    (1 << Rezise_Thin) | (1 << Rezise_Wide) | (1 << Rezise_Eye) | (1 << Rezise_Normal));
                cap.cropX = jcap.value("cropX", 0);
                cap.cropY = jcap.value("cropY", 0);
                cap.cropW = jcap.value("cropW", 0);
                cap.cropH = jcap.value("cropH", 0);
                cap.targetWidth = jcap.value("targetWidth", 100);
                cap.targetHeight = jcap.value("targetHeight", 100);
                cap.rotation = jcap.value("rotation", 0.0f);

                cap.preserveAspect = jcap.value("preserveAspect", true);
                cap.colorKeyEnabled = jcap.value("colorKeyEnabled", false);
                cap.colorKey = jcap.value("colorKey", (int)RGB(0,255,0));
                cap.tolerance = jcap.value("tolerance", 30);
                if (jcap.contains("multiColors")) {
                    for (auto& col : jcap["multiColors"]) {
                        cap.multiColors.push_back((COLORREF)col.get<int>());
                    }
                }
                if (jcap.contains("displayPos") && jcap["displayPos"].size() >= 2) {
                    cap.displayPos.x = jcap["displayPos"][0];
                    cap.displayPos.y = jcap["displayPos"][1];
                }
                if (jcap.contains("displaySize") && jcap["displaySize"].size() >= 2) {
                    cap.displaySize.x = jcap["displaySize"][0];
                    cap.displaySize.y = jcap["displaySize"][1];
                }
                cap.colorPassMode = jcap.value("colorPassMode", false);
                std::string title = jcap.value("targetWindowTitle", "");
                strcpy_s(cap.targetWindowTitle, title.c_str());
                g_customCaptures.push_back(cap);
            }
        }

        if (j.contains("backgrounds")) {
            auto& bgs = j["backgrounds"];
            for (size_t i = 0; i < bgs.size() && i < 4; ++i) {
                auto& bg = bgs[i];
                g_backgrounds[i].freeTexture();
                g_backgrounds[i].enabled = bg.value("enabled", false);
                g_backgrounds[i].useImage = bg.value("useImage", false);
                std::string path = bg.value("imagePath", "");
                strncpy_s(g_backgrounds[i].imagePath, path.c_str(), sizeof(g_backgrounds[i].imagePath) - 1);
                g_backgrounds[i].imagePath[sizeof(g_backgrounds[i].imagePath) - 1] = '\0';
                if (bg.contains("color") && bg["color"].size() >= 4) {
                    g_backgrounds[i].color[0] = bg["color"][0];
                    g_backgrounds[i].color[1] = bg["color"][1];
                    g_backgrounds[i].color[2] = bg["color"][2];
                    g_backgrounds[i].color[3] = bg["color"][3];
                }
            }
        }
    } catch (...) {}
}

enum eMenuPage {
    Page_Settings,
    Page_Resizing,
    Page_CustomCaptures,
};
static int currentPage = Page_Settings;

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);
    if (!pImageCodecInfo) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

void CaptureScreenArea(int x, int y, int width, int height, const std::string& filename) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return;
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDC, width, height);
    if (!bitmap) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);
    BitBlt(memDC, 0, 0, width, height, screenDC, x, y, SRCCOPY);
    Bitmap* gdiBitmap = Bitmap::FromHBITMAP(bitmap, nullptr);
    if (gdiBitmap) {
        CLSID pngClsid;
        if (GetEncoderClsid(L"image/png", &pngClsid) >= 0) {
            std::wstring wfilename(filename.begin(), filename.end());
            gdiBitmap->Save(wfilename.c_str(), &pngClsid, nullptr);
        }
        delete gdiBitmap;
    }
    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

bool IsForegroundAllowed() {
    if (!g_restrictToAllowedWindows) return true;
    HWND hwndForeground = GetForegroundWindow();
    if (!hwndForeground) return false;
    if (hwndForeground == g_hWnd) return true;
    wchar_t title[256];
    GetWindowTextW(hwndForeground, title, 256);
    std::wstring wtitle(title);
    std::string titleStr(wtitle.begin(), wtitle.end());
    for (const auto& allowed : g_allowedWindows) {
        if (titleStr.find(allowed) != std::string::npos) return true;
    }
    return false;
}

bool LoadTextureFromFile(const std::string& filename, ID3D11ShaderResourceView** outTexture, int* outWidth, int* outHeight) {
    Bitmap* bitmap = Bitmap::FromFile(std::wstring(filename.begin(), filename.end()).c_str());
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        delete bitmap;
        return false;
    }

    int width = bitmap->GetWidth();
    int height = bitmap->GetHeight();

    BitmapData bmpData;
    Rect rect(0, 0, width, height);
    if (bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok) {
        delete bitmap;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = bmpData.Scan0;
    initData.SysMemPitch = bmpData.Stride;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &initData, &tex);
    bitmap->UnlockBits(&bmpData);
    delete bitmap;

    if (FAILED(hr) || !tex) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    if (FAILED(g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, outTexture))) {
        tex->Release();
        return false;
    }

    tex->Release();
    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;
    return true;
}

void EnsureBackgroundTexture(ResizeBackground& bg) {
    if (!bg.enabled || !bg.useImage || bg.imagePath[0] == '\0') {
        bg.freeTexture();
        return;
    }
    if (bg.texture) return;
    LoadTextureFromFile(bg.imagePath, &bg.texture, &bg.texWidth, &bg.texHeight);
}

void DrawResizeBackground() {
    ResizeBackground& bg = g_backgrounds[activeReszie];
    if (!bg.enabled) return;

    EnsureBackgroundTexture(bg);

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;

    if (bg.useImage && bg.texture) {
        drawList->AddImage((ImTextureID)bg.texture, ImVec2(0,0), screenSize);
    } else {
        drawList->AddRectFilled(ImVec2(0,0), screenSize,
                                IM_COL32((int)(bg.color[0]*255), (int)(bg.color[1]*255),
                                         (int)(bg.color[2]*255), (int)(bg.color[3]*255)));
    }
}


void RenderGUI(bool isAllowed)
{
    if (activeReszie != Rezise_Normal && g_targetHwnd && IsWindow(g_targetHwnd)) {
        RECT clientRect;
        if (GetClientRect(g_targetHwnd, &clientRect)) {
            POINT topLeft = {clientRect.left, clientRect.top};
            POINT bottomRight = {clientRect.right, clientRect.bottom};
            ClientToScreen(g_targetHwnd, &topLeft);
            ClientToScreen(g_targetHwnd, &bottomRight);
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            drawList->AddRectFilled(ImVec2((float)topLeft.x, (float)topLeft.y),
                                    ImVec2((float)bottomRight.x, (float)bottomRight.y),
                                    IM_COL32(0, 0, 0, 255));
        }
    }

    SetWindowLongPtr(g_hWnd, GWL_EXSTYLE,
        GetWindowLongPtr(g_hWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
    if (!imguiSettings.togglegui) {
        SetWindowLongPtr(g_hWnd, GWL_EXSTYLE,
            GetWindowLongPtr(g_hWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    }

    if (imguiSettings.overlay) {
        ImGui::SetNextWindowBgAlpha(0.1f);
        ImGui::Begin("Watermark", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        for (auto& text : waterMarkInfos.stringinfo) {
            ImGui::Text("%s", text.c_str());
        }
        ImGui::End();
    }

    if (imguiSettings.togglegui) {
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

        std::string name = "Externa";
        ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar);
        if (bg_texture) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 windowSize = ImGui::GetWindowSize();
            drawList->AddImageRounded(
                (ImTextureID)(intptr_t)bg_texture,
                windowPos,
                ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255,255,255,255),
                6.0f, ImDrawFlags_RoundCornersAll);
        }

        ImGui::BeginChild("Sidebar", ImVec2(100, 0), true);
        {
            if (ImGui::Button("Settings", ImVec2(80, 30))) currentPage = Page_Settings;
            if (currentPage == Page_Settings) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("Resizing", ImVec2(80, 30))) currentPage = Page_Resizing;
            if (currentPage == Page_Resizing) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("Custom Captures", ImVec2(80, 30))) currentPage = Page_CustomCaptures;
            if (currentPage == Page_CustomCaptures) { ImGui::SameLine(); ImGui::Text("  <"); }
            if (ImGui::Button("GoBorderless", ImVec2(80, 30))) DoNormalResize();

            float windowHeight = ImGui::GetWindowHeight();
            ImGui::SetCursorPosY(windowHeight - 30 - ImGui::GetStyle().WindowPadding.y);
            if (ImGui::Button("Close", ImVec2(80, 30))) {
                g_gameInfo.isRunning = false;
                PostQuitMessage(0);
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("Content", ImVec2(0, 0), true);
        {
            switch (currentPage) {
            case Page_Settings: {
                    ImGui::Text("Settings");

                    ImGui::Separator();
                    ImGui::Text("Hotkeys");

                    static auto hotkeyButton = [](const char* label, int& key) {
                        ImGui::Text("%s", label);
                        ImGui::SameLine();
                        std::string btnLabel = GetKeyName(key);
                        if (ImGui::Button(btnLabel.c_str())) {
                            g_capturingHotkey = true;
                            g_capturingHotkeyFor = &key;
                        }
                    };

                    hotkeyButton("Thin",  g_hotkeys.thinKey);
                    hotkeyButton("Wide",  g_hotkeys.wideKey);
                    hotkeyButton("Eye",   g_hotkeys.eyeKey);
                    hotkeyButton("Normal",g_hotkeys.normalKey);

                    ImGui::Separator();

                    ImGui::Checkbox("Eye Overlay", &g_eyeOverlayEnabled);
                    ImGui::Checkbox("Custom Overlay", &g_eyeOverlayCustom);

                    ImGui::Separator();
                    ImGui::Text("Window Restrictions");
                    ImGui::Checkbox("Restrict to allowed windows only", &g_restrictToAllowedWindows);
                    if (g_restrictToAllowedWindows) {
                        static char newWindow[256] = "";
                        ImGui::InputText("Add window title (partial)", newWindow, sizeof(newWindow));
                        ImGui::SameLine();
                        if (ImGui::Button("Add")) {
                            if (strlen(newWindow) > 0) {
                                g_allowedWindows.push_back(newWindow);
                                newWindow[0] = 0;
                            }
                        }
                        for (int i = 0; i < (int)g_allowedWindows.size(); ++i) {
                            ImGui::Text("%s", g_allowedWindows[i].c_str());
                            ImGui::SameLine();
                            if (ImGui::Button(("Remove##"+std::to_string(i)).c_str())) {
                                g_allowedWindows.erase(g_allowedWindows.begin() + i);
                                --i;
                            }
                        }
                    }


                    ImGui::Separator();
                    ImGui::Text("Resize Backgrounds");

                    static int bgMode = 0;
                    const char* modeNames[] = { "Thin", "Wide", "Eye", "Normal" };
                    ImGui::Combo("Mode", &bgMode, modeNames, 4);

                    ResizeBackground& bg = g_backgrounds[bgMode];
                    ImGui::Checkbox("Enable Background", &bg.enabled);
                    if (bg.enabled) {
                        ImGui::Checkbox("Use Image", &bg.useImage);
                        if (bg.useImage) {
                            ImGui::InputText("Image Path", bg.imagePath, sizeof(bg.imagePath));
                            ImGui::SameLine();
                            if (ImGui::Button("Browse...")) {
                                OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                                char szFile[260] = "";
                                ofn.lpstrFile = szFile;
                                ofn.nMaxFile = sizeof(szFile);
                                ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
                                ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                                if (GetOpenFileNameA(&ofn)) {
                                    strncpy_s(bg.imagePath, szFile, sizeof(bg.imagePath) - 1);
                                    bg.imagePath[sizeof(bg.imagePath) - 1] = '\0';
                                    bg.freeTexture();
                                }
                            }
                            if (bg.texture) {
                                ImGui::Text("Preview:");
                                ImGui::Image((ImTextureID)bg.texture, ImVec2(100,100));
                            }
                        } else {
                            ImGui::ColorEdit4("Color", bg.color);
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Save Settings")) SaveSettings();
                    break;
            }

            case Page_Resizing: {
                    ImGui::Text("Resizing");

                    static bool resizeSuccess = false;
                    static std::string message = "";

                    ImGui::Separator();
                    ImGui::Text("Resize a window by its title");

                    ImGui::InputText("Win Title", g_targetWindowTitle, sizeof(g_targetWindowTitle));

                    ImGui::InputInt("Thin Width", &g_resizeDims.thin_w);
                    ImGui::InputInt("Thin Height", &g_resizeDims.thin_h);
                    ImGui::InputInt("Wide Width", &g_resizeDims.wide_w);
                    ImGui::InputInt("Wide Height", &g_resizeDims.wide_h);
                    ImGui::InputInt("Eye Width", &g_resizeDims.eye_w);
                    ImGui::InputInt("Eye Height", &g_resizeDims.eye_h);
                    ImGui::InputInt("Normal Width", &g_resizeDims.normal_w);
                    ImGui::InputInt("Normal Height", &g_resizeDims.normal_h);

                    if (ImGui::Button("Thin")) {
                        DoThinResize();
                        message = "Thin mode applied";
                        resizeSuccess = true;
                    }
                    if (ImGui::Button("Wide")) {
                        DoWideResize();
                        message = "Wide mode applied";
                        resizeSuccess = true;
                    }
                    if (ImGui::Button("Eye")) {
                        DoEyeResize();
                        message = "Eye mode applied";
                        resizeSuccess = true;
                    }
                    if (ImGui::Button("Normal")) {
                        DoNormalResize();
                        message = "Normal mode applied";
                        resizeSuccess = true;
                    }

                    if (!message.empty()) {
                        ImGui::SameLine();
                        ImGui::TextColored(resizeSuccess ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1), "%s", message.c_str());
                    }
                    break;
            }

            case Page_CustomCaptures: {
                    ImGui::Text("Custom Captures");

                    ImGui::Separator();
                    ImGui::Text("Add new capture:");

                    static char newName[256] = "";
                    static int newX = 0, newY = 0, newW = 100, newH = 100;
                    static char* newWindowTitle = g_targetWindowTitle;

                    ImGui::InputText("Name", newName, sizeof(newName));
                    ImGui::InputInt("X", &newX);
                    ImGui::InputInt("Y", &newY);
                    ImGui::InputInt("Width", &newW);
                    ImGui::InputInt("Height", &newH);
                    ImGui::InputText("Window Title (optional)", newWindowTitle, sizeof(newWindowTitle));

                    if (ImGui::Button("Add Capture")) {
                        if (strlen(newName) > 0 && newW > 0 && newH > 0) {
                            CustomCapture cap;
                            cap.name = newName;
                            cap.x = newX;
                            cap.y = newY;
                            cap.width = newW;
                            cap.height = newH;
                            cap.targetWidth = newW;
                            cap.targetHeight = newH;
                            cap.displayPos = ImVec2((float)newX, (float)newY);
                            strcpy_s(cap.targetWindowTitle, newWindowTitle);
                            g_customCaptures.push_back(cap);
                            strcpy_s(newName, "");
                            newX = newY = 0;
                            newW = newH = 100;
                            newWindowTitle[0] = 0;
                        }
                    }

                    ImGui::Separator();
                    ImGui::Text("Captures:");

                    int editingIndex = -1;
                    for (size_t i = 0; i < g_customCaptures.size(); ++i) {
                        auto& cap = g_customCaptures[i];
                        ImGui::PushID(i);
                        ImGui::Checkbox("##enabled", &cap.enabled);
                        ImGui::SameLine();
                        ImGui::Text("%s: (%d,%d) %dx%d", cap.name.c_str(), cap.x, cap.y, cap.width, cap.height);
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            cap.freeTexture();
                            g_customCaptures.erase(g_customCaptures.begin() + i);
                            --i;
                            ImGui::PopID();
                            continue;
                        }

                        if (ImGui::TreeNode(("Settings##" + cap.name).c_str())) {
                            editingIndex = (int)i;

                            ImGui::InputText("Target Window Title", cap.targetWindowTitle, sizeof(cap.targetWindowTitle));

                            ImGui::Text("Visible on:");
                            ImGui::CheckboxFlags("Thin", &cap.visibilityModes, 1 << Rezise_Thin);
                            ImGui::SameLine();
                            ImGui::CheckboxFlags("Wide", &cap.visibilityModes, 1 << Rezise_Wide);
                            ImGui::SameLine();
                            ImGui::CheckboxFlags("Eye", &cap.visibilityModes, 1 << Rezise_Eye);
                            ImGui::SameLine();
                            ImGui::CheckboxFlags("Normal", &cap.visibilityModes, 1 << Rezise_Normal);

                            ImGui::Text("Crop (relative to capture area)");
                            ImGui::InputInt("Crop X", &cap.cropX);
                            ImGui::InputInt("Crop Y", &cap.cropY);
                            ImGui::InputInt("Crop Width", &cap.cropW);
                            ImGui::InputInt("Crop Height", &cap.cropH);



                            ImGui::InputInt("Capture Width", &cap.width);
                            ImGui::InputInt("Capture Height", &cap.height);

                            int targetSize[2] = { cap.targetWidth, cap.targetHeight };
                            if (ImGui::InputInt2("Target Size", targetSize)) {
                                cap.targetWidth = targetSize[0];
                                cap.targetHeight = targetSize[1];
                            }
                            ImGui::SliderFloat("Rotation", &cap.rotation, -180.0f, 180.0f);
                            ImGui::Checkbox("Preserve Aspect", &cap.preserveAspect);
                            ImGui::Checkbox("Color Key", &cap.colorKeyEnabled);
                            if (cap.colorKeyEnabled) {
                                ImGui::ColorEdit3("Key Colour", (float*)&cap.colorKey);
                                ImGui::SliderInt("Tolerance", &cap.tolerance, 0, 255);
                                ImGui::Checkbox("Color Pass Mode", &cap.colorPassMode);

                                ImGui::Text("Additional Colors:");
                                for (int j = 0; j < (int)cap.multiColors.size(); ++j) {
                                    ImGui::PushID(j);
                                    float col[3] = { GetRValue(cap.multiColors[j]) / 255.0f,
                                                     GetGValue(cap.multiColors[j]) / 255.0f,
                                                     GetBValue(cap.multiColors[j]) / 255.0f };
                                    if (ImGui::ColorEdit3("##col", col)) {
                                        cap.multiColors[j] = RGB((int)(col[0]*255), (int)(col[1]*255), (int)(col[2]*255));
                                    }
                                    ImGui::SameLine();
                                    if (ImGui::Button("Remove")) {
                                        cap.multiColors.erase(cap.multiColors.begin() + j);
                                        --j;
                                    }
                                    ImGui::PopID();
                                }

                                static COLORREF newColor = RGB(255,0,0);
                                if (ImGui::ColorEdit3("New Color", (float*)&newColor)) {}
                                ImGui::SameLine();
                                if (ImGui::Button("Add Color")) {
                                    cap.multiColors.push_back(newColor);
                                }
                            }
                            ImGui::InputFloat2("Display Position", &cap.displayPos.x);
                            ImGui::InputFloat2("Display Size", &cap.displaySize.x);
                            if (cap.displaySize.x == 0 && cap.displaySize.y == 0) {
                                ImGui::Text("Display size will be target size");
                            }

                            if (ImGui::Button("Set Display Pos from Capture")) {
                                cap.displayPos = ImVec2((float)cap.x, (float)cap.y);
                            }

                            if (cap.texture) {
                                ImGui::Text("Preview:");
                                ImVec2 previewSize(200, 200);
                                float aspect = (float)cap.texHeight / cap.texWidth;
                                previewSize.y = previewSize.x * aspect;
                                ImGui::Image((ImTextureID)cap.texture, previewSize);
                            } else {
                                ImGui::Text("Preview not available");
                            }

                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    g_editingCustomCaptureIndex = editingIndex;

                    ImGui::Separator();
                    if (ImGui::Button("Capture All Enabled to Files")) {
                        for (const auto& cap : g_customCaptures) {
                            if (cap.enabled) {
                                std::string filename = cap.name + ".png";
                                if (cap.targetWindowTitle[0] != 0) {
                                    HWND hwnd = FindWindowByPartialTitle(std::wstring(cap.targetWindowTitle, cap.targetWindowTitle+strlen(cap.targetWindowTitle)).c_str());
                                    if (hwnd) {
                                        RECT client;
                                        GetClientRect(hwnd, &client);
                                        POINT topLeft = {client.left, client.top};
                                        ClientToScreen(hwnd, &topLeft);
                                        CaptureScreenArea(topLeft.x + cap.x, topLeft.y + cap.y, cap.width, cap.height, filename);
                                    } else {
                                        CaptureScreenArea(cap.x, cap.y, cap.width, cap.height, filename);
                                    }
                                } else {
                                    CaptureScreenArea(cap.x, cap.y, cap.width, cap.height, filename);
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

    // Draw macro capture overlay (if allowed and enabled)
    if (isAllowed && g_activeMacro >= 0 && g_captureSettings[g_activeMacro].enabled && g_captureTexture) {
        CaptureSettings& s = g_captureSettings[g_activeMacro];
        ImVec2 size = (s.displaySize.x > 0 && s.displaySize.y > 0) ?
                      s.displaySize : ImVec2((float)s.targetWidth, (float)s.targetHeight);
        ImGui::SetNextWindowPos(s.displayPos);
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("CaptureOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
        ImGui::Image((ImTextureID)g_captureTexture, size);
        ImGui::End();
    }

    // Draw custom capture overlays only if allowed and visible on current mode
    if (isAllowed) {
        for (auto& cap : g_customCaptures) {
            if (cap.enabled && (cap.visibilityModes & (1 << activeReszie)) && cap.texture) {
                ImVec2 size = (cap.displaySize.x > 0 && cap.displaySize.y > 0) ?
                              cap.displaySize : ImVec2((float)cap.targetWidth, (float)cap.targetHeight);
                ImGui::SetNextWindowPos(cap.displayPos);
                ImGui::SetNextWindowSize(size);
                ImGui::Begin(("CustomOverlay_" + cap.name).c_str(), nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoInputs);
                ImGui::Image((ImTextureID)cap.texture, size);
                ImGui::End();
            }
        }

        // Draw the eye overlay if active
        if (g_eyeOverlayCaptureActive && g_eyeOverlayCapture.texture) {
            ImVec2 size = (g_eyeOverlayCapture.displaySize.x > 0 && g_eyeOverlayCapture.displaySize.y > 0) ?
                          g_eyeOverlayCapture.displaySize :
                          ImVec2((float)g_eyeOverlayCapture.targetWidth, (float)g_eyeOverlayCapture.targetHeight);
            ImGui::SetNextWindowPos(g_eyeOverlayCapture.displayPos);
            ImGui::SetNextWindowSize(size);
            ImGui::Begin("EyeOverlay", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
            ImGui::Image((ImTextureID)g_eyeOverlayCapture.texture, size);
            ImGui::End();
        }
    }

    // Dragging logic for custom capture preview rectangles
    if (g_editingCustomCaptureIndex >= 0 && g_editingCustomCaptureIndex < (int)g_customCaptures.size()) {
        auto& cap = g_customCaptures[g_editingCustomCaptureIndex];
        ImDrawList* draw = ImGui::GetBackgroundDrawList();

        // Capture rectangle (green)
        ImVec2 p1((float)cap.x, (float)cap.y);
        ImVec2 p2((float)(cap.x + cap.width), (float)(cap.y + cap.height));
        draw->AddRect(p1, p2, IM_COL32(0, 255, 0, 128), 2.0f, 0, 3.0f);

        // Display rectangle (yellow)
        float dispW = cap.displaySize.x > 0 ? cap.displaySize.x : (float)cap.targetWidth;
        float dispH = cap.displaySize.y > 0 ? cap.displaySize.y : (float)cap.targetHeight;
        ImVec2 d1(cap.displayPos.x, cap.displayPos.y);
        ImVec2 d2(cap.displayPos.x + dispW, cap.displayPos.y + dispH);
        draw->AddRect(d1, d2, IM_COL32(255, 255, 0, 128), 2.0f, 0, 2.0f);

        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            ImVec2 mousePos = io.MousePos;
            bool leftDown = io.MouseDown[0];
            bool leftReleased = io.MouseReleased[0];

            if (leftDown && g_draggingCaptureIndex == -1) {
                if (mousePos.x >= cap.x && mousePos.x <= cap.x + cap.width &&
                    mousePos.y >= cap.y && mousePos.y <= cap.y + cap.height) {
                    g_draggingCaptureIndex = g_editingCustomCaptureIndex;
                    g_draggingCaptureRect = true;
                    g_lastMousePos = mousePos;
                    }
                else if (mousePos.x >= cap.displayPos.x && mousePos.x <= cap.displayPos.x + dispW &&
                         mousePos.y >= cap.displayPos.y && mousePos.y <= cap.displayPos.y + dispH) {
                    g_draggingCaptureIndex = g_editingCustomCaptureIndex;
                    g_draggingDisplayRect = true;
                    g_lastMousePos = mousePos;
                         }
            }

            if (g_draggingCaptureIndex == g_editingCustomCaptureIndex && leftDown) {
                ImVec2 delta = ImVec2(mousePos.x - g_lastMousePos.x, mousePos.y - g_lastMousePos.y);
                if (delta.x != 0 || delta.y != 0) {
                    if (g_draggingCaptureRect) {
                        cap.x += (int)delta.x;
                        cap.y += (int)delta.y;
                    } else if (g_draggingDisplayRect) {
                        cap.displayPos.x += delta.x;
                        cap.displayPos.y += delta.y;
                    }
                    g_lastMousePos = mousePos;
                }
            }

            if (leftReleased) {
                g_draggingCaptureIndex = -1;
                g_draggingCaptureRect = false;
                g_draggingDisplayRect = false;
            }
        }
    } else {
        g_draggingCaptureIndex = -1;
        g_draggingCaptureRect = false;
        g_draggingDisplayRect = false;
    }
}

bool IsForegroundAllowedForHotkeys() {
    return IsForegroundAllowed();
}

void KeyHandler() {
    while (g_gameInfo.isRunning) {
        if (g_capturingHotkey && g_capturingHotkeyFor) {
            for (int key = 1; key <= 255; ++key) {
                if (GetAsyncKeyState(key) & 0x0001) {
                    *g_capturingHotkeyFor = key;
                    g_capturingHotkey = false;
                    g_capturingHotkeyFor = nullptr;
                    break;
                }
            }
        } else if (IsForegroundAllowedForHotkeys()) {
            if (GetAsyncKeyState(g_hotkeys.thinKey) & 0x0001)   DoThinResize();
            if (GetAsyncKeyState(g_hotkeys.wideKey) & 0x0001)   DoWideResize();
            if (GetAsyncKeyState(g_hotkeys.eyeKey) & 0x0001)    DoEyeResize();
            // if (GetAsyncKeyState(g_hotkeys.normalKey) & 0x0001) DoNormalResize();
            if (GetAsyncKeyState(VK_UP) & 0x0001) {
                imguiSettings.togglegui = !imguiSettings.togglegui;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    if (D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext) != S_OK)
        return false;

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
        pBackBuffer->Release();
    }
    return true;
}

void CleanupDeviceD3D() {
    if (g_pRenderTargetView) { g_pRenderTargetView->Release(); g_pRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}


extern IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                if (g_pRenderTargetView) {
                    g_pRenderTargetView->Release();
                    g_pRenderTargetView = nullptr;
                }
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* pBackBuffer;
                g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
                if (pBackBuffer) {
                    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
                    pBackBuffer->Release();
                }
            }
            return 0;
        case WM_DESTROY:
            g_gameInfo.isRunning = false;
            PostQuitMessage(0);
            return 0;
        default: ;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}


Bitmap* CaptureTransformed(HWND targetHwnd, const CaptureSettings& settings) {
    if (!targetHwnd) return nullptr;

    RECT clientRect;
    GetClientRect(targetHwnd, &clientRect);
    RECT crop = settings.cropRect;
    int srcWidth = crop.right - crop.left;
    int srcHeight = crop.bottom - crop.top;
    if (srcWidth <= 0 || srcHeight <= 0) return nullptr;

    HDC srcDC = GetDC(targetHwnd);
    HDC memDC = CreateCompatibleDC(srcDC);
    HBITMAP captureBitmap = CreateCompatibleBitmap(srcDC, srcWidth, srcHeight);
    if (!captureBitmap) {
        ReleaseDC(targetHwnd, srcDC);
        DeleteDC(memDC);
        return nullptr;
    }
    SelectObject(memDC, captureBitmap);
    BitBlt(memDC, 0, 0, srcWidth, srcHeight,
           srcDC, crop.left, crop.top, SRCCOPY);

    Bitmap* srcBitmap = Bitmap::FromHBITMAP(captureBitmap, nullptr);
    if (!srcBitmap) {
        DeleteObject(captureBitmap);
        DeleteDC(memDC);
        ReleaseDC(targetHwnd, srcDC);
        return nullptr;
    }

    DeleteObject(captureBitmap);
    DeleteDC(memDC);
    ReleaseDC(targetHwnd, srcDC);

    int destWidth = settings.targetWidth;
    int destHeight = settings.targetHeight;
    if (destWidth <= 0 || destHeight <= 0) return srcBitmap;

    Bitmap* destBitmap = new Bitmap(destWidth, destHeight, PixelFormat32bppARGB);
    Graphics graphics(destBitmap);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.Clear(Color(0, 0, 0, 0));

    float scaleX = (float)destWidth / srcWidth;
    float scaleY = (float)destHeight / srcHeight;
    if (settings.preserveAspect) {
        float scale = std::min(scaleX, scaleY);
        scaleX = scaleY = scale;
    }
    float destCenterX = destWidth / 2.0f;
    float destCenterY = destHeight / 2.0f;

    graphics.TranslateTransform(destCenterX, destCenterY);
    graphics.RotateTransform(settings.rotation);
    graphics.ScaleTransform(scaleX, scaleY);
    graphics.TranslateTransform(-srcWidth / 2.0f, -srcHeight / 2.0f);
    graphics.DrawImage(srcBitmap, 0, 0, srcWidth, srcHeight);
    delete srcBitmap;

    if (settings.colorKeyEnabled) {
        BitmapData bmpData;
        Rect rect(0, 0, destWidth, destHeight);
        if (destBitmap->LockBits(&rect, ImageLockModeRead | ImageLockModeWrite,
                                 PixelFormat32bppARGB, &bmpData) == Ok) {
            BYTE* pixels = (BYTE*)bmpData.Scan0;
            int stride = bmpData.Stride;

            auto within = [](BYTE a, BYTE b, int tol) -> bool {
                return abs(a - b) <= tol;
            };
            auto matchesAnyKey = [&](BYTE r, BYTE g, BYTE b) -> bool {
                int tol = settings.tolerance;
                BYTE pkR = GetRValue(settings.colorKey), pkG = GetGValue(settings.colorKey), pkB = GetBValue(settings.colorKey);
                if (within(r, pkR, tol) && within(g, pkG, tol) && within(b, pkB, tol))
                    return true;
                for (COLORREF col : settings.multiColors) {
                    BYTE cr = GetRValue(col), cg = GetGValue(col), cb = GetBValue(col);
                    if (within(r, cr, tol) && within(g, cg, tol) && within(b, cb, tol))
                        return true;
                }
                return false;
            };

            for (int y = 0; y < destHeight; ++y) {
                BYTE* row = pixels + y * stride;
                for (int x = 0; x < destWidth; ++x) {
                    BYTE* pixel = row + x * 4;
                    BYTE r = pixel[2];
                    BYTE g = pixel[1];
                    BYTE b = pixel[0];
                    bool match = matchesAnyKey(r, g, b);
                    if (settings.colorPassMode) {
                        if (!match) pixel[3] = 0;
                    } else {
                        if (match) pixel[3] = 0;
                    }
                }
            }
            destBitmap->UnlockBits(&bmpData);
        }
    }

    return destBitmap;
}

void UpdateCaptureTexture(Bitmap* bmp) {
    if (!bmp) {
        if (g_captureTexture) {
            g_captureTexture->Release();
            g_captureTexture = nullptr;
        }
        return;
    }

    int width = bmp->GetWidth();
    int height = bmp->GetHeight();
    if (width <= 0 || height <= 0) return;

    BitmapData bmpData;
    Rect rect(0, 0, width, height);
    if (bmp->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Ok)
        return;

    if (!g_captureTexture || g_captureTexWidth != width || g_captureTexHeight != height) {
        if (g_captureTexture) g_captureTexture->Release();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D* tex = nullptr;
        if (g_pd3dDevice->CreateTexture2D(&desc, nullptr, &tex) == S_OK) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            g_pd3dDevice->CreateShaderResourceView(tex, &srvDesc, &g_captureTexture);
            tex->Release();
        }
        g_captureTexWidth = width;
        g_captureTexHeight = height;
    }

    if (g_captureTexture) {
        ID3D11Resource* resource;
        g_captureTexture->GetResource(&resource);
        ID3D11Texture2D* tex = static_cast<ID3D11Texture2D*>(resource);
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (g_pd3dDeviceContext->Map(tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped) == S_OK) {
            BYTE* dst = (BYTE*)mapped.pData;
            BYTE* src = (BYTE*)bmpData.Scan0;
            int srcPitch = bmpData.Stride;
            for (int y = 0; y < height; ++y) {
                memcpy(dst + y * mapped.RowPitch, src + y * srcPitch, width * 4);
            }
            g_pd3dDeviceContext->Unmap(tex, 0);
        }
        resource->Release();
    }

    bmp->UnlockBits(&bmpData);
}


Bitmap* CaptureWindowOrDesktop(const CustomCapture& cap) {
    HWND targetHwnd = nullptr;
    RECT clientRect = {0,0,0,0};

    if (cap.targetWindowTitle[0] != 0) {
        std::wstring wtitle(cap.targetWindowTitle, cap.targetWindowTitle + strlen(cap.targetWindowTitle));
        targetHwnd = FindWindowByPartialTitle(wtitle.c_str());
        if (targetHwnd && IsWindow(targetHwnd)) {
            GetClientRect(targetHwnd, &clientRect);
        } else {
            targetHwnd = nullptr;
        }
    }

    if (!targetHwnd) {
        return nullptr;
    }

    if (clientRect.right <= clientRect.left || clientRect.bottom <= clientRect.top) {
        return nullptr;
    }

    POINT topLeft = {clientRect.left, clientRect.top};
    ClientToScreen(targetHwnd, &topLeft);
    RECT clientScreenRect = {
        topLeft.x,
        topLeft.y,
        topLeft.x + (clientRect.right - clientRect.left),
        topLeft.y + (clientRect.bottom - clientRect.top)
    };

    int desiredX = clientScreenRect.left + cap.x;
    int desiredY = clientScreenRect.top  + cap.y;
    int desiredW = cap.width;
    int desiredH = cap.height;

    int captureX = std::max(static_cast<int>(clientScreenRect.left), desiredX);
    int captureY = std::max(static_cast<int>(clientScreenRect.top),  desiredY);
    int captureR = std::min(static_cast<int>(clientScreenRect.right),  desiredX + desiredW);
    int captureB = std::min(static_cast<int>(clientScreenRect.bottom), desiredY + desiredH);
    int captureW = captureR - captureX;
    int captureH = captureB - captureY;

    if (captureW <= 0 || captureH <= 0) {
        return nullptr;
    }

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP captureBitmap = CreateCompatibleBitmap(screenDC, captureW, captureH);
    if (!captureBitmap) {
        ReleaseDC(nullptr, screenDC);
        DeleteDC(memDC);
        return nullptr;
    }
    SelectObject(memDC, captureBitmap);
    BitBlt(memDC, 0, 0, captureW, captureH, screenDC, captureX, captureY, SRCCOPY);

    Bitmap* srcBitmap = Bitmap::FromHBITMAP(captureBitmap, nullptr);
    DeleteObject(captureBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    if (!srcBitmap) return nullptr;

    int cropX = cap.cropX, cropY = cap.cropY, cropW = cap.cropW, cropH = cap.cropH;
    if (cropW <= 0) cropW = captureW;
    if (cropH <= 0) cropH = captureH;
    cropX = std::max(0, std::min(cropX, captureW));
    cropY = std::max(0, std::min(cropY, captureH));
    cropW = std::min(cropW, captureW - cropX);
    cropH = std::min(cropH, captureH - cropY);
    if (cropW <= 0 || cropH <= 0) {
        delete srcBitmap;
        return nullptr;
    }

    Bitmap* croppedBitmap = srcBitmap->Clone(cropX, cropY, cropW, cropH, PixelFormat32bppARGB);
    delete srcBitmap;
    if (!croppedBitmap) return nullptr;

    int destWidth = cap.targetWidth;
    int destHeight = cap.targetHeight;
    if (destWidth <= 0 || destHeight <= 0) return croppedBitmap;

    Bitmap* destBitmap = new Bitmap(destWidth, destHeight, PixelFormat32bppARGB);
    Graphics graphics(destBitmap);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.Clear(Color(0, 0, 0, 0));

    float scaleX = (float)destWidth / cropW;
    float scaleY = (float)destHeight / cropH;
    if (cap.preserveAspect) {
        float scale = std::min(scaleX, scaleY);
        scaleX = scaleY = scale;
    }
    float destCenterX = destWidth / 2.0f;
    float destCenterY = destHeight / 2.0f;

    graphics.TranslateTransform(destCenterX, destCenterY);
    graphics.RotateTransform(cap.rotation);
    graphics.ScaleTransform(scaleX, scaleY);
    graphics.TranslateTransform(-cropW / 2.0f, -cropH / 2.0f);
    graphics.DrawImage(croppedBitmap, 0, 0, cropW, cropH);
    delete croppedBitmap;

    if (cap.colorKeyEnabled) {
        BitmapData bmpData;
        Rect rect(0, 0, destWidth, destHeight);
        if (destBitmap->LockBits(&rect, ImageLockModeRead | ImageLockModeWrite,
                                 PixelFormat32bppARGB, &bmpData) == Ok) {
            BYTE* pixels = (BYTE*)bmpData.Scan0;
            int stride = bmpData.Stride;

            auto within = [](BYTE a, BYTE b, int tol) { return abs(a - b) <= tol; };
            auto matchesAnyKey = [&](BYTE r, BYTE g, BYTE b) -> bool {
                int tol = cap.tolerance;
                BYTE pkR = GetRValue(cap.colorKey), pkG = GetGValue(cap.colorKey), pkB = GetBValue(cap.colorKey);
                if (within(r, pkR, tol) && within(g, pkG, tol) && within(b, pkB, tol))
                    return true;
                for (COLORREF col : cap.multiColors) {
                    BYTE cr = GetRValue(col), cg = GetGValue(col), cb = GetBValue(col);
                    if (within(r, cr, tol) && within(g, cg, tol) && within(b, cb, tol))
                        return true;
                }
                return false;
            };

            for (int y = 0; y < destHeight; ++y) {
                BYTE* row = pixels + y * stride;
                for (int x = 0; x < destWidth; ++x) {
                    BYTE* pixel = row + x * 4;
                    BYTE r = pixel[2];
                    BYTE g = pixel[1];
                    BYTE b = pixel[0];
                    bool match = matchesAnyKey(r, g, b);
                    if (cap.colorPassMode) {
                        if (!match) pixel[3] = 0;
                    } else {
                        if (match) pixel[3] = 0;
                    }
                }
            }
            destBitmap->UnlockBits(&bmpData);
        }
    }

    return destBitmap;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L, 0L,
        hInstance,
        nullptr, nullptr, nullptr, nullptr,
        _T("GameWindow"),
        nullptr
    };
    if (!RegisterClassEx(&wc)) return 1;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowEx(
        WS_EX_LAYERED,
        wc.lpszClassName,
        _T("Game Window with ImGui"),
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return 1;

    SetLayeredWindowAttributes(g_hWnd, RGB(0,0,0), 0, LWA_COLORKEY);
    ShowWindow(g_hWnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hWnd);

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupDeviceD3D();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    SetupImGuiStyle();
    LoadSettings();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    std::thread keyloop(KeyHandler);

    bool ret = LoadTextureFromMemory(CheatBg_png, CheatBg_png_len, &bg_texture, &bg_texture_width, &bg_texture_height);
    IM_ASSERT(ret);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    auto lastTime = std::chrono::high_resolution_clock::now();
    int frames = 0;


    while (msg.message != WM_QUIT && g_gameInfo.isRunning) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        frames++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        if (deltaTime >= 1.0f) {
            g_gameInfo.frameRate = frames / deltaTime;
            g_gameInfo.frameCount += frames;
            frames = 0;
            lastTime = currentTime;
        }

        if (g_targetHwnd && !IsWindow(g_targetHwnd)) {
            g_targetHwnd = nullptr;
            SetOverlayOwner(nullptr);
            if (g_eyeOverlayCaptureActive) {
                g_eyeOverlayCapture.freeTexture();
                g_eyeOverlayCaptureActive = false;
            }
        }

        // Update eye overlay position (size never changes!)
        if (g_eyeOverlayCaptureActive && g_targetHwnd && IsWindow(g_targetHwnd)) {
            EyeZoomConfig ezCfg;
            RECT client;
            GetClientRect(g_targetHwnd, &client);
            RECT crop = EyeZoomCropRect(ezCfg, client.right, client.bottom);
            Bitmap* bmp = CaptureAndBlend(g_targetHwnd,L"overlay.png",g_eyeOverlayCapture.width,g_eyeOverlayCapture.height,&crop);  // use whole client area, or provide a centered crop RECT if desired

            UpdateCustomCaptureTexture(g_eyeOverlayCapture, bmp);
            delete bmp;
        }

        bool isAllowed = IsForegroundAllowed();

        // Macro capture
        if (isAllowed && g_activeMacro >= 0 && g_captureSettings[g_activeMacro].enabled && g_targetHwnd) {
            Bitmap* bmp = CaptureTransformed(g_targetHwnd, g_captureSettings[g_activeMacro]);
            UpdateCaptureTexture(bmp);
            delete bmp;
        } else {
            UpdateCaptureTexture(nullptr);
        }

        if (isAllowed) {
            for (auto& cap : g_customCaptures) {
                if (cap.enabled) {
                    Bitmap* bmp = CaptureWindowOrDesktop(cap);
                    UpdateCustomCaptureTexture(cap, bmp);
                    delete bmp;
                } else {
                    cap.freeTexture();
                }
            }
        } else {
            for (auto& cap : g_customCaptures) {
                cap.freeTexture();
            }
            if (g_eyeOverlayCaptureActive) {
                g_eyeOverlayCapture.freeTexture();
                g_eyeOverlayCaptureActive = false;
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawResizeBackground();
        RenderGUI(isAllowed);

        ImGui::Render();

        float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    g_gameInfo.isRunning = false;
    if (keyloop.joinable()) keyloop.join();

    SaveSettings();
    DoNormalResize();

    if (bg_texture) { bg_texture->Release(); bg_texture = nullptr; }
    if (g_captureTexture) { g_captureTexture->Release(); g_captureTexture = nullptr; }
    for (auto& cap : g_customCaptures) {
        cap.freeTexture();
    }
    for (int i = 0; i < 4; ++i) {
        g_backgrounds[i].freeTexture();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClass(wc.lpszClassName, hInstance);

    GdiplusShutdown(g_gdiplusToken);
    return 0;
}