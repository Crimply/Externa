// ResizeAndCentreWindowToggle.h
#pragma once

#include <windows.h>
#include <string>

// Helper functions (can be static or inside an anonymous namespace)
static RECT GetWindowRectHelper(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

static POINT GetRectCenter(const RECT& rect) {
    return { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
}

static RECT RectFromCenter(POINT center, int width, int height) {
    return {
        center.x - width / 2,
        center.y - height / 2,
        center.x + width / 2,
        center.y + height / 2
    };
}

static void SetWindowBorderless(HWND hwnd) {
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
}

static void RestoreWindowStyle(HWND hwnd, LONG_PTR style, LONG_PTR exStyle) {
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
}

static bool ResizeAndCentreWindowToggle(const std::string& windowTitle, int newWidth, int newHeight) {
    if (windowTitle.empty()) return false;

    HWND hwnd = FindWindowA(NULL, windowTitle.c_str());
    if (!hwnd) return false;

    static bool isResized = false;
    static RECT originalRect = {0};

    RECT currentRect = GetWindowRectHelper(hwnd);
    int currentWidth  = currentRect.right - currentRect.left;
    int currentHeight = currentRect.bottom - currentRect.top;

    // If already resized and dimensions match, revert.
    if (isResized && currentWidth == newWidth && currentHeight == newHeight) {
        SetWindowPos(hwnd, NULL,
                     originalRect.left, originalRect.top,
                     originalRect.right - originalRect.left,
                     originalRect.bottom - originalRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        isResized = false;
        return false;
    }

    // Perform resize.
    if (!isResized) {
        originalRect = currentRect;
    }

    POINT center = GetRectCenter(currentRect);
    RECT newRect = RectFromCenter(center, newWidth, newHeight);

    SetWindowPos(hwnd, NULL,
                 newRect.left, newRect.top,
                 newRect.right - newRect.left, newRect.bottom - newRect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    isResized = true;
    return true;
}