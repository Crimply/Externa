#pragma once

#include <windows.h>
#include <cstdint>

class Resizing {
public:
    static bool currentlyResized;
    static int previousWidth;
    static int previousHeight;
    static LONG_PTR previousStyle;
    static LONG_PTR previousExStyle;

    // Helper functions
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
        style |= WS_POPUP;  // ensure it becomes borderless
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
    }

    static void RestoreWindowStyle(HWND hwnd, LONG_PTR style, LONG_PTR exStyle) {
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    }

    static bool toggleResizeInternal(HWND hwnd, int width, int height, bool borderless) {
        RECT currentRect = GetWindowRectHelper(hwnd);
        int currentWidth = currentRect.right - currentRect.left;
        int currentHeight = currentRect.bottom - currentRect.top;

        // Determine if we need to resize (either not resized or dimensions changed)
        bool resizing = !currentlyResized || currentWidth != width || currentHeight != height;

        if (resizing) {
            // Store original state if not already resized
            if (!currentlyResized) {
                previousWidth = currentWidth;
                previousHeight = currentHeight;
                previousStyle = GetWindowLongPtr(hwnd, GWL_STYLE);
                previousExStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            }

            POINT center = GetRectCenter(currentRect);
            RECT newRect = RectFromCenter(center, width, height);

            // Apply or remove border based on the flag
            if (borderless) {
                SetWindowBorderless(hwnd);
            } else {
                // For normal mode, ensure original style is restored (borders)
                RestoreWindowStyle(hwnd, previousStyle, previousExStyle);
            }

            SetWindowPos(hwnd, nullptr,
                         newRect.left, newRect.top,
                         newRect.right - newRect.left, newRect.bottom - newRect.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            currentlyResized = true;
            return true;
        } else {
            undoResize(hwnd);
            return false;
        }
    }

public:
    // Public interface: toggle resize.
    // borderless = true  -> remove borders (thin, wide, eye)
    // borderless = false -> keep original borders (normal)
    static bool toggleResize(HWND hwnd, int width, int height, bool borderless) {
        return toggleResizeInternal(hwnd, width, height, borderless);
    }

    static void undoResize(HWND hwnd) {
        if (!currentlyResized) return;

        RECT currentRect = GetWindowRectHelper(hwnd);
        POINT center = GetRectCenter(currentRect);
        RECT newRect = RectFromCenter(center, previousWidth, previousHeight);

        RestoreWindowStyle(hwnd, previousStyle, previousExStyle);
        SetWindowPos(hwnd, nullptr,
                     newRect.left, newRect.top,
                     newRect.right - newRect.left, newRect.bottom - newRect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        currentlyResized = false;
    }
};

// Static member definitions
bool Resizing::currentlyResized = false;
int Resizing::previousWidth = 0;
int Resizing::previousHeight = 0;
LONG_PTR Resizing::previousStyle = 0;
LONG_PTR Resizing::previousExStyle = 0;