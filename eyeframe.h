// eyeframe.h (optional, but for clarity)
#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <string>

// Initialize GDI+ (call once at program start)
void InitGdiPlus();
void ShutdownGdiPlus();

// Capture the source window, blend overlay.png on top, and return a GDI+ Bitmap.
// The returned Bitmap is newly allocated; caller must delete it.
// - sourceHwnd: the window to capture
// - overlayPath: path to the overlay image (PNG with alpha)
// - targetWidth, targetHeight: desired final image size (will stretch both game and overlay)
// - cropRect: rectangle in client coordinates of source window to capture (if NULL, uses entire client area)
Gdiplus::Bitmap* CaptureAndBlend(
    HWND sourceHwnd,
    const std::wstring& overlayPath,
    int targetWidth,
    int targetHeight,
    const RECT* cropRect = nullptr);