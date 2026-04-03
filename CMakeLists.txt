cmake_minimum_required(VERSION 3.10)
project(rankedStats)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(CURL 8.19.0 REQUIRED)

# Define some handy Social SDK variables
add_executable(rankedStats #WIN32
        main.cpp
        imguitheming.h

        uishit.h
        json.hpp
        imgui/imgui.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_impl_dx11.cpp
        imgui/imgui_impl_win32.cpp
        imgui/imgui_internal.h
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imstb_rectpack.h
        imgui/imstb_textedit.h
        imgui/imstb_truetype.h
        pleasework.h
        eyeframe.h
        eyeframe.cpp
        eyeframe.h
        ResizeAndCentreWindowToggle.h
        resizeing.h
        fileread.h
        winfind.h
        eyezoom.cpp
        eyezoom.h
        bleh.hpp
)


target_link_libraries(rankedStats
        PRIVATE
        d3d11
        d3dcompiler
        dwmapi

)


target_link_libraries(rankedStats PRIVATE CURL::libcurl)

target_link_libraries(rankedStats PRIVATE gdiplus)

target_link_libraries(rankedStats PRIVATE msimg32)
#target_link_libraries(rankedStats PRIVATE w32api)


if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
endif()