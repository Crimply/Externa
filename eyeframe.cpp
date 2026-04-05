// eyeframe.cpp
#include "eyeframe.h"
#include <gdiplus.h>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static ULONG_PTR g_gdiplusToken = 0;

void InitGdiPlus() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
}

void ShutdownGdiPlus() {
    if (g_gdiplusToken)
        GdiplusShutdown(g_gdiplusToken);
}

// Helper: load a file into memory
static std::vector<BYTE> ReadFileToBuffer(const std::wstring& path) {
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return {};
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size))
        return buffer;
    return {};
}

// Helper: load overlay image from file (cached internally to avoid reloading every frame)
static Bitmap* LoadOverlayImage(const std::wstring& path) {
    static std::wstring lastPath;
    static Bitmap* cached = nullptr;

    if (path == lastPath && cached)
        return cached;

    // Clear previous cache
    if (cached) {
        delete cached;
        cached = nullptr;
    }

    std::vector<BYTE> imgData = ReadFileToBuffer(path);
    if (imgData.empty())
        return nullptr;

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &pStream) != S_OK)
        return nullptr;

    pStream->Write(imgData.data(), (ULONG)imgData.size(), nullptr);
    LARGE_INTEGER li = { 0 };
    pStream->Seek(li, STREAM_SEEK_SET, nullptr);

    Bitmap* bmp = Bitmap::FromStream(pStream);
    if (!bmp || bmp->GetLastStatus() != Ok) {
        delete bmp;
        bmp = nullptr;
    }

    pStream->Release();

    if (bmp) {
        lastPath = path;
        cached = bmp;
    }

    return bmp;
}

// Main function: capture source window, blend overlay, return new Bitmap
Bitmap* CaptureAndBlend(
    HWND sourceHwnd,
    const std::wstring& overlayPath,
    int targetWidth,
    int targetHeight,
    const RECT* cropRect)
{
    if (!sourceHwnd || !IsWindow(sourceHwnd))
        return nullptr;

    // Get source client rect (or use provided crop rect)
    RECT srcRect;
    if (cropRect) {
        srcRect = *cropRect;
    } else {
        GetClientRect(sourceHwnd, &srcRect);
    }

    int srcWidth = srcRect.right - srcRect.left;
    int srcHeight = srcRect.bottom - srcRect.top;
    if (srcWidth <= 0 || srcHeight <= 0)
        return nullptr;

    // Create a 32-bit ARGB bitmap for the final image
    Bitmap* finalBitmap = new Bitmap(targetWidth, targetHeight, PixelFormat32bppARGB);
    Graphics graphics(finalBitmap);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.Clear(Color(0, 0, 0, 0));

    // 1. Capture the source window and draw it stretched to the target size
    HDC srcDC = GetDC(sourceHwnd);
    HDC memDC = CreateCompatibleDC(srcDC);
    HBITMAP captureBitmap = CreateCompatibleBitmap(srcDC, srcWidth, srcHeight);
    if (!captureBitmap) {
        ReleaseDC(sourceHwnd, srcDC);
        DeleteDC(memDC);
        delete finalBitmap;
        return nullptr;
    }

    SelectObject(memDC, captureBitmap);
    // Copy the desired region from the source window
    BitBlt(memDC, 0, 0, srcWidth, srcHeight,
           srcDC, srcRect.left, srcRect.top, SRCCOPY);

    // Convert captured HBITMAP to GDI+ Bitmap and draw stretched
    Bitmap captureImage(captureBitmap, nullptr);
    graphics.DrawImage(&captureImage, 0, 0, targetWidth, targetHeight);

    // Cleanup temporary capture resources
    DeleteDC(memDC);
    ReleaseDC(sourceHwnd, srcDC);
    DeleteObject(captureBitmap);

    // 2. Load overlay image (cached) and draw it stretched over the game

    Bitmap* overlay = LoadOverlayImage(overlayPath);
    if (overlay) {
        // Graphics graphics(finalBitmap);
        // graphics.SetInterpolationMode(InterpolationModeHighQualityBilinear);
        int offset = (targetWidth - 50 * floor(targetWidth/50.0f)) / 2.0f;
        graphics.DrawImage(overlay, -offset, 0, targetWidth + offset, targetHeight);
        // graphics.DrawImage(overlay, 0, 0, targetWidth, targetHeight);
    }

    // Bitmap* overlay = LoadOverlayImage(overlayPath);
    // if (overlay) {
    //     graphics.DrawImage(overlay, 0, 0, targetWidth, targetHeight);
    // }

    return finalBitmap;
}