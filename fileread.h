//
// Created by Crimp on 3/26/2026.
//
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>
#include <psapi.h>
#include <shlobj.h>
#include <vector>

// Get the path to the user's AppData\Roaming folder
std::string GetAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        wchar_t* p = path;
        int size = WideCharToMultiByte(CP_UTF8, 0, p, -1, NULL, 0, NULL, NULL);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, p, -1, &result[0], size, NULL, NULL);
        result.pop_back(); // remove null terminator
        return result;
    }
    return "";
}

// Find the process ID of a window by its title (partial match)
DWORD GetProcessIdByWindowTitle(const std::string& windowTitle) {
    HWND hWnd = FindWindowA(NULL, windowTitle.c_str());
    if (!hWnd) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hWnd, &pid);
    return pid;
}

// Get the full path of the executable of a running process
std::string GetProcessExePath(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return "";
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH)) {
        CloseHandle(hProcess);
        int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, NULL, NULL);
        result.pop_back();
        return result;
    }
    CloseHandle(hProcess);
    return "";
}

/**
 * Reads the content of a file from the given Minecraft instance folder.
 * @param instancePath Path to the instance's .minecraft folder (e.g., "C:\\Users\\Name\\AppData\\Roaming\\.minecraft")
 * @param filename      Name of the file to read (e.g., "options.txt")
 * @return String containing the file content, or an error message if the file cannot be opened.
 */
std::string ReadFileFromInstance(const std::string& instancePath, const std::string& filename) {
    if (instancePath.empty()) {
        return "Error: Instance path is not set.";
    }

    // Construct the full path
    std::string fullPath = instancePath + "\\" + filename;

    // Open the file
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return "Error: Could not open file: " + fullPath;
    }

    // Read the entire file into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Tries to locate the .minecraft folder for the given target window title.
// Returns empty string if not found.
std::string FindMinecraftFolderFromTargetWindow(const std::string& windowTitle) {
    DWORD pid = GetProcessIdByWindowTitle(windowTitle);
    if (pid == 0) return "";

    std::string exePath = GetProcessExePath(pid);
    if (exePath.empty()) return "";

    // Convert to lowercase for case‑insensitive checks
    std::string exeLower = exePath;
    std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);

    // 1. If it's the vanilla launcher (Minecraft.exe or MinecraftLauncher.exe),
    //    the game directory is %APPDATA%\.minecraft
    if (exeLower.find("minecraftlauncher") != std::string::npos ||
        exeLower.find("minecraft.exe") != std::string::npos) {
        std::string appdata = GetAppDataPath();
        if (!appdata.empty()) return appdata + "\\.minecraft";
    }

    // 2. If it's a Java process (javaw.exe), we need to look for the .minecraft folder
    if (exeLower.find("javaw.exe") != std::string::npos) {
        // a) Try the working directory of the process (often the instance folder in MultiMC/Prism)
        //    Unfortunately, we can't easily get the working directory of another process without
        //    reading the PEB. As a simpler fallback, we can check common instance folders.
        // b) Try %APPDATA%\.minecraft (vanilla)
        std::string appdata = GetAppDataPath();
        if (!appdata.empty()) {
            std::string vanilla = appdata + "\\.minecraft";
            if (GetFileAttributesA(vanilla.c_str()) != INVALID_FILE_ATTRIBUTES)
                return vanilla;
        }
        // c) Try MultiMC / Prism typical location: look for "instances" folder relative to the javaw.exe
        //    but that's unreliable. Instead, we could prompt the user.
    }

    // 3. Fallback: try to see if there is a .minecraft folder in the same directory as the executable
    size_t lastSlash = exePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        std::string folder = exePath.substr(0, lastSlash) + "\\.minecraft";
        if (GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES)
            return folder;
    }

    // 4. Nothing found
    return "";
}