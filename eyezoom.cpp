// eyezoom.cpp
// This file mainly covers crop rect calculation and overlay PNG generation
//
// Two functions, that's all this file does:
//   1. EyeZoomCropRect()        - figure out what rectangle to grab from the game
//   2. GenerateEyeZoomOverlay() - draw the measuring grid PNG
//
// Ported from tuxinjector:
//   - Crop rect logic:  mode_system.rs:918-925
//   - Grid drawing:     overlay_gen.rs:44-296
//   I also reccomend to read that code too

#include "eyezoom.h"

#include "eyeframe.h"

#include <gdiplus.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;


// -- CROP RECT --
//
// This is pretty much the most important part to understand. The eyezoom works by
// grabbing a TINY strip from the dead center of the game viewport.
//
// Imagine the game viewport (384 x 16384). Right in the middle of it,
// we grab a strip that's only 60px wide (cloneWidth * 2) and 1300px
// tall (cloneHeight). That's it.
//
// Then CaptureAndBlend() stretches that tiny strip to fill your whole panel.
// A 60px strip stretched to 400px wide = ~6.7x (xd) horizontal zoom. That's the
// entire zoom mechanism - just stretching a small capture.

RECT EyeZoomCropRect(const EyeZoomConfig& cfg, int gameW, int gameH) {
    // Viewport is almost always 384x16384 for eyezoom (Tall mode)
    int vw = cfg.viewportWidth;
    int vh = cfg.viewportHeight;

    // Find the center of the viewport - this is where the crosshair is
    int cx = vw / 2;
    int cy = vh / 2;

    // Build the crop rect centered on that point
    //
    // With defaults (cloneWidth=30, cloneHeight=1300, viewport=384x16384):
    //   left   = 192 - 30  = 162
    //   top    = 8192 - 650 = 7542
    //   right  = 192 + 30  = 222
    //   bottom = 7542 + 1300 = 8842
    //
    // So we're grabbing a 60x1300 strip from the center of a 384x16384 viewport.
    //
    RECT r;
    r.left   = cx - cfg.cloneWidth;
    r.top    = cy - cfg.cloneHeight / 2;
    r.right  = cx + cfg.cloneWidth;
    r.bottom = r.top + cfg.cloneHeight;

    // Clamp so we don't try to capture outside the window
    r.left   = (std::max)(0L,         r.left);
    r.top    = (std::max)(0L,         r.top);
    r.right  = (std::min)((LONG)gameW, r.right);
    r.bottom = (std::min)((LONG)gameH, r.bottom);

    return r;
}


// -- OVERLAY PNG GENERATION --
//
// This code creates the PNG that gets composited over the zoomed
// game capture. It has three primary layers:
//
//   1. Alternating colored cells (pink/blue) in a horizontal band at the
//      vertical center. Each cell is one "clone width" unit wide.
//      Every Nth cell (highlightInterval) gets drawn in amber instead.
//
//   2. A white crosshair line running the full height of the image,
//      2px wide, dead center. This marks pixel 0 (the crosshair).
//
//   3. Distance labels in each cell showing how many pixels from center.
//      The "slackow" style only shows the last digit (cleaner to read),
//      with the full number at the tens marks (10, 20, 30...).
//

// Convert our float [r,g,b,a] colors to GDI+'s Color(a,r,g,b) format
static Color MakeColor(const float c[4]) {
    return Color(
        (BYTE)(c[3] * 255 + .5f),  // alpha
        (BYTE)(c[0] * 255 + .5f),  // red
        (BYTE)(c[1] * 255 + .5f),  // green
        (BYTE)(c[2] * 255 + .5f)   // blue
    );
}

// We need the PNG encoder CLSID to save the bitmap - GDI+ makes you look it up
static bool GetPngClsid(CLSID* clsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (!size) return false;

    std::vector<BYTE> buf(size);
    auto* enc = reinterpret_cast<ImageCodecInfo*>(buf.data());
    GetImageEncoders(num, size, enc);

    for (UINT i = 0; i < num; i++) {
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) {
            *clsid = enc[i].Clsid;
            return true;
        }
    }
    return false;
}

bool GenerateEyeZoomOverlay(const EyeZoomConfig& cfg,
                             int imgW, int imgH,
                             const std::wstring& outPath)
{
    if (imgW <= 0 || imgH <= 0) return false;

    // Total number of cells across the full width.
    // With cloneWidth=30, that's 60 cells total (30 left + 30 right of center).
    int totalCells = (std::max)(cfg.cloneWidth * 2, 1);

    // Each cell's width in pixels, within our output image.
    // e.g. 400px panel / 60 cells = ~6px per cell
    int cellW = (std::max)(imgW / totalCells, 1);

    int leftOffset = (imgW - totalCells * cellW) / 2;

    int perSide = cfg.cloneWidth;

    // How many cells actually get labels drawn.
    // Can be less than perSide if you want a cleaner look on the edges.
    int ovW = (std::min)(cfg.overlayWidth, perSide);

    // Height of the colored cell band (proportional to image height).
    // This is where the pink/blue boxes and numbers go.
    int boxH    = (std::max)(20, (int)(imgH * 0.04f));
    int cy      = imgH / 2;
    int gridTop = cy - boxH / 2;

    // Start with a fully transparent canvas
    Bitmap bmp(imgW, imgH, PixelFormat32bppARGB);
    Graphics gfx(&bmp);
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    gfx.SetTextRenderingHint(TextRenderingHintAntiAlias);
    gfx.Clear(Color(0, 0, 0, 0));

    // -- Draw the alternating colored cells --
    //
    // We go from -overlayWidth to +overlayWidth, skipping 0 (that's the center
    // crosshair, drawn separately). Negative = left of center, positive = right.
    //
    // The cell index "bi" maps the offset to a position in the image:
    //   offset -30 -> bi = 0  (leftmost cell)
    //   offset  -1 -> bi = 29 (just left of center)
    //   offset  +1 -> bi = 30 (just right of center)
    //   offset +30 -> bi = 59 (rightmost cell)
    //
    for (int xOff = -ovW; xOff <= ovW; xOff++) {
        if (xOff == 0) continue;  // center line, not a cell

        int num  = abs(xOff);     // distance from center (always positive)
        int bi   = xOff + perSide - (xOff > 0 ? 1 : 0);  // cell index
        int left = leftOffset + bi * cellW;    // pixel position in image

        // Pick color: amber for decade marks, otherwise alternate pink/blue
        const float* c;
        if (cfg.highlightInterval > 0 && num % cfg.highlightInterval == 0)
            c = cfg.highlightColor;  // amber highlight at 10, 20, 30...
        else
            c = (bi % 2 == 0) ? cfg.gridColor1 : cfg.gridColor2;  // pink/blue

        SolidBrush brush(MakeColor(c));
        gfx.FillRectangle(&brush, left, gridTop, cellW, boxH);
    }

    // -- Center crosshair --
    // A thin white vertical line running the full height of the image.
    // This marks the exact center (pixel 0, where the crosshair is in-game).
    {
        SolidBrush xhBrush(MakeColor(cfg.centerLineColor));
        gfx.FillRectangle(&xhBrush, imgW / 2 - 1, 0, 2, imgH);
    }

    // -- Distance labels --
    // Each cell gets a number showing how many pixels from center it is.
    //
    // The "slackow" style (default) keeps things readable by only showing
    // the last digit most of the time:
    //   cells: 1 2 3 4 5 6 7 8 9 [10] 1 2 3 4 5 6 7 8 9 [20] ...
    //   (decade marks show the full number, everything else is just the last digit)
    //
    // Change numberStyle to "horizontal" if you want full numbers everywhere.
    //

    // Try Consolas first (monospace, looks good for numbers), fall back to Arial
    // FontFamily family(L"Consolas");
    FontFamily consolasFamily(L"Consolas");
    FontFamily arialFamily(L"Arial");
    FontFamily* family = consolasFamily.IsAvailable() ? &consolasFamily : &arialFamily;

    // Font size = 70% of the box height so there's some padding
    Font font(family, boxH * 0.7f, FontStyleRegular, UnitPixel);
    SolidBrush txtBrush(MakeColor(cfg.textColor));

    // Center text both horizontally and vertically within each cell
    StringFormat fmt;
    fmt.SetAlignment(StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);

    for (int xOff = -ovW; xOff <= ovW; xOff++) {
        if (xOff == 0) continue;

        int num = abs(xOff);
        int bi  = xOff + perSide - (xOff > 0 ? 1 : 0);

        // Figure out what text to show based on the number style
        bool decade = cfg.highlightInterval > 0 &&
                      num % cfg.highlightInterval == 0;

        std::wstring label;
        if (strcmp(cfg.numberStyle, "slackow") == 0 && !decade && num >= 10)
            label = std::to_wstring(num % 10);  // just last digit
        else
            label = std::to_wstring(num);        // full number

        // Draw centered in the cell's box area
        RectF r(leftOffset + (bi + 0.5f) * cellW - cellW / 2.0f, (float)gridTop, (float)cellW, (float)boxH);
        gfx.DrawString(label.c_str(), -1, &font, r, &fmt, &txtBrush);
    }

    // -- Save to disk --
    CLSID pngClsid;
    if (!GetPngClsid(&pngClsid)) return false;
    return bmp.Save(outPath.c_str(), &pngClsid, nullptr) == Ok;
}