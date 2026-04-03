//
// Created by Crimp on 3/26/2026.
//
#pragma once

#include <windows.h>
#include <string>

// Callback data structure to pass the partial title and store the result
struct EnumData {
    std::wstring partialTitle;
    HWND hWndResult;
};

// EnumWindows callback function
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    EnumData* pData = reinterpret_cast<EnumData*>(lParam);
    wchar_t windowTitle[256];

    // Get the window title
    if (GetWindowTextW(hWnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t))) {
        // Check if the title contains our partial string (case-insensitive example)
        std::wstring title(windowTitle);
        if (title.find(pData->partialTitle) != std::wstring::npos) {
            pData->hWndResult = hWnd;
            return FALSE; // Stop enumeration
        }
    }
    return TRUE; // Continue enumeration
}

// Find a window by partial title (case-sensitive)
HWND FindWindowByPartialTitle(const wchar_t* partialTitle) {
    EnumData data;
    data.partialTitle = partialTitle;
    data.hWndResult = NULL;

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.hWndResult;
}