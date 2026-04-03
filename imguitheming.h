//
// Created by Crimp on 2/15/2026.
//


#pragma once
#include "imgui/imgui.h"


inline void SetupImGuiStyle()
{
    // Visual Studio 2022 style by janekb04, modified for better readability
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 1.0f;

    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.CellPadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(20.0f, 3.0f);
    style.LogSliderDeadzone = 4.0f;

    // Colors (ImGuiCol_ arrays)
   ImVec4* colors = style.Colors;
   colors[ImGuiCol_Text]                   =   ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
   colors[ImGuiCol_TextDisabled]           =   ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
   colors[ImGuiCol_WindowBg]                =  ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
   colors[ImGuiCol_ChildBg]                 =  ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   colors[ImGuiCol_PopupBg]                 =  ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
   colors[ImGuiCol_Border]                  =  ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
   colors[ImGuiCol_BorderShadow]            =  ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
   colors[ImGuiCol_FrameBg]                 =  ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
   colors[ImGuiCol_FrameBgHovered]          =  ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
   colors[ImGuiCol_FrameBgActive]           =  ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
   colors[ImGuiCol_TitleBg]                 =  ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
   colors[ImGuiCol_TitleBgActive]           =  ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TitleBgCollapsed]        =  ImVec4(1.00f, 0.56f, 0.98f, 0.51f);
   colors[ImGuiCol_MenuBarBg]                = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ScrollbarBg]              = ImVec4(1.00f, 0.56f, 0.98f, 0.53f);
   colors[ImGuiCol_ScrollbarGrab]            = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabHovered]     = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ScrollbarGrabActive]      = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_CheckMark]                = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_SliderGrab]               = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_SliderGrabActive]         = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_Button]                   = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ButtonHovered]            = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ButtonActive]             = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_Header]                   = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_HeaderHovered]            = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_HeaderActive]             = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_Separator]                = ImVec4(1.00f, 0.56f, 0.98f, 0.50f);
   colors[ImGuiCol_SeparatorHovered]         = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_SeparatorActive]          = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_ResizeGrip]               = ImVec4(1.00f, 0.56f, 0.98f, 0.20f);
   colors[ImGuiCol_ResizeGripHovered]        = ImVec4(1.00f, 0.56f, 0.98f, 0.67f);
   colors[ImGuiCol_ResizeGripActive]         = ImVec4(1.00f, 0.56f, 0.98f, 0.95f);
   colors[ImGuiCol_Tab]                      = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TabHovered]               = ImVec4(1.00f, 0.56f, 0.98f, 0.80f);
   colors[ImGuiCol_TabActive]                = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TabUnfocused]             = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TabUnfocusedActive]       = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_PlotLines]                = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_PlotLinesHovered]         = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_PlotHistogram]            = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_PlotHistogramHovered]     = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TableHeaderBg]            = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TableBorderStrong]        = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TableBorderLight]         = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_TableRowBg]               = ImVec4(1.00f, 0.56f, 0.98f, 0.00f);
   colors[ImGuiCol_TableRowBgAlt]            = ImVec4(1.00f, 0.56f, 0.98f, 0.06f);
   colors[ImGuiCol_TextSelectedBg]           = ImVec4(1.00f, 0.56f, 0.98f, 0.35f);
   colors[ImGuiCol_DragDropTarget]           = ImVec4(1.00f, 0.56f, 0.98f, 0.90f);
   colors[ImGuiCol_NavHighlight]             = ImVec4(1.00f, 0.56f, 0.98f, 1.00f);
   colors[ImGuiCol_NavWindowingHighlight]    = ImVec4(1.00f, 0.56f, 0.98f, 0.70f);
   colors[ImGuiCol_NavWindowingDimBg]        = ImVec4(1.00f, 0.56f, 0.98f, 0.20f);
   colors[ImGuiCol_ModalWindowDimBg]         = ImVec4(1.00f, 0.56f, 0.98f, 0.35f);
}

