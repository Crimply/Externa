// eyezoom.h
// Should work as a drop in eyezoom for eyeframe's CaptureAndBlend pipeline
//
// Mostly ported from tuxinjector (mode_system.rs, overlay_gen.rs)
//
// how to use this with your existing code:
//
//   Your CaptureAndBlend() already does 3 things:
//     1. Captures pixels from the game window (with an optional crop RECT)
//     2. Stretches them to fill the target size
//     3. Draws an overlay PNG on top
//
//   Eyezoom just provides the right inputs for that:
//     - EyeZoomCropRect()        -> the crop RECT (a thin strip from the center)
//     - GenerateEyeZoomOverlay() -> the overlay PNG (grid + numbers)
//
//   The "zoom" effect comes from CaptureAndBlend stretching a tiny 60px-wide
//   strip to fill your whole panel. thats literally it

#pragma once

#include <windows.h>
#include <string>

// All the knobs you can tweak. Loaded once, regenerate the overlay if you change any.
//
// The most important ones to play with:
//   cloneWidth  -> how many pixels to grab on each side of center (30 = 60px total)
//   cloneHeight -> how tall the captured strip is
//   overlayWidth -> how many grid cells get labels (can be less than cloneWidth)
//
struct EyeZoomConfig {

    // --- Source region (what to grab from the game) ---

    // How many pixels to capture on each side of the viewport center.


    // Total capture width = cloneWidth * 2, so 30 means 60px wide.
    // Each pixel column = 1 pixel of angular offset from the crosshair.
    // Making this bigger captures more of the screen but each cell gets smaller.
    int cloneWidth         = 30;

    // How tall the captured strip is in pixels.
    // 1300 works well for the 16384px tall viewport.
    int cloneHeight        = 975;

    // --- Grid overlay ---

    // How many cells get drawn on each side of center.
    // Set this equal to cloneWidth to label every single column,
    // or lower to only label the inner ones (less visual clutter).
    int overlayWidth       = 30;

    // Highlight every Nth cell in amber. 10 = highlight at 10, 20, 30...
    // Set to 0 to disable highlights entirely.
    int highlightInterval  = 10;

    // --- Game viewport size ---
    // This tells us where the "center" of the game is.
    // Eyezoom is almost always used with 384x16384 (Tall mode), so you
    // probably don't need to change these.
    int viewportWidth      = 384;
    int viewportHeight     = 16384;

    // --- Colors ---
    // All [r, g, b, a] with values 0.0 to 1.0.
    // The two grid colors alternate per cell. Highlight overrides both.

    float gridColor1[4]       = {1.0f, 0.714f, 0.757f, 1.0f};  // pink
    float gridColor2[4]       = {0.678f, 0.847f, 0.902f, 1.0f}; // blue
    float highlightColor[4]   = {1.0f, 0.835f, 0.310f, 1.0f};   // amber
    float centerLineColor[4]  = {1.0f, 1.0f, 1.0f, 1.0f};       // white crosshair
    float textColor[4]        = {0.0f, 0.0f, 0.0f, 1.0f};       // black labels

    // --- Label style ---
    // "slackow"    -> last digit only (1,2,3...9,10,1,2..9,20,...) - cleanest look imo, slackow the goat
    // "horizontal" -> full number in each cell (1,2,3...10,11,12...) - can get squished if font size is too large
    // "stacked"    -> digits stacked vertically for multi-digit numbers - alright, i dont really like it
    // "compact"    -> first digit smaller, rest normal size - the ugliest imo but i still keep it because some people might like it idk
    const char* numberStyle   = "slackow";
};

// Returns the crop RECT to pass into CaptureAndBlend's cropRect parameter.
// This is the thin vertical strip from the center of the game viewport.
//
// gameWidth/gameHeight = the game window's client area size.
// You get these from GetClientRect(gameHwnd, &rect).
//
// Example:
//   RECT client;
//   GetClientRect(g_targetHwnd, &client);
//   RECT crop = EyeZoomCropRect(cfg, client.right, client.bottom);
//   // crop is now something like {left=162, top=7550, right=222, bottom=8850}
//   // (a 60px wide, 1300px tall strip from the viewport center)
//
RECT EyeZoomCropRect(const EyeZoomConfig& cfg, int gameWidth, int gameHeight);

// Generates the grid overlay PNG and saves it to disk.
// Call this ONCE at startup, and again if the user (you) changes any config values.
// The output file is what you need to pass as CaptureAndBlend's overlayPath.
//
// imgWidth/imgHeight should match your panel size (g_eyeOverlayCapture.width/height)
// so the grid lines up pixel-perfect with the stretched capture.
//
// Example:
//   GenerateEyeZoomOverlay(cfg, 400, 900, L"overlay.png");
//   // now overlay.png has the pink/blue grid + crosshair + numbers
//
bool GenerateEyeZoomOverlay(const EyeZoomConfig& cfg,
                             int imgWidth, int imgHeight,
                             const std::wstring& outPath);