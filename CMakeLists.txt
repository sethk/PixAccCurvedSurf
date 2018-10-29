cmake_minimum_required(VERSION 3.12)
project(PixAccCurvedSurf)

set(USE_GL3W 1)
set(USE_GLEW 0)
set(GLFW_STATIC 0)

set(CMAKE_VERBOSE_MAKEFILE 1)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-braces")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

add_executable(${PROJECT_NAME} Sources/Main.cc)

foreach(SHADER
        Debug.vert
        UniformColor.frag
        Debug.frag
        BicubicBezier.glsl
        iPASS.vert
        iPASS.tesc
        iPASS.tese
        BlinnPhong.frag)
    configure_file(Shaders/${SHADER} ${SHADER} COPYONLY)
endforeach()

set(IMGUI_DIR ThirdParty/imgui)
target_sources(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/examples/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR} ${IMGUI_DIR}/examples ${IMGUI_DIR}/misc/cpp ${IMGUI_DIR}/misc/stl)
file(COPY ${IMGUI_DIR}/misc/fonts/DroidSans.ttf DESTINATION .)

set(SUBLIME_DIR ThirdParty/SubLiME)
target_include_directories(${PROJECT_NAME} PRIVATE ${SUBLIME_DIR})
target_sources(${PROJECT_NAME} PRIVATE ${SUBLIME_DIR}/bspSlefe.c ${SUBLIME_DIR}/tpSlefe.c ${SUBLIME_DIR}/uniSlefe.c)
foreach(dim 2 3 4 5 6 7 8 9)
    file(COPY ${SUBLIME_DIR}/range/unirange-3_${dim}.asc DESTINATION range)
endforeach()

set(QUICKHULL_DIR ThirdParty/quickhull)
target_include_directories(${PROJECT_NAME} PRIVATE ${QUICKHULL_DIR})
target_sources(${PROJECT_NAME} PRIVATE ${QUICKHULL_DIR}/QuickHull.cpp)

find_package(glfw3 3 CONFIG REQUIRED)
if(${GLFW_STATIC})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_STATIC_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${GLFW_STATIC_LIBRARIES})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${OPENGL_LIBRARIES})
    target_link_libraries(${PROJECT_NAME} glfw)
endif()

if(${USE_GL3W})
    set(GL3W_DIR ${IMGUI_DIR}/examples/libs/gl3w)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GL3W_DIR})
    target_sources(${PROJECT_NAME} PRIVATE ${GL3W_DIR}/GL/gl3w.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GL3W IMGUI_IMPL_OPENGL_LOADER_GL3W)
    find_library(DL_LIBRARY dl)
    target_link_libraries(${PROJECT_NAME} ${DL_LIBRARY})
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
    if(EXISTS ${CORE_FOUNDATION_FRAMEWORK})
	target_link_libraries(${PROJECT_NAME} ${CORE_FOUNDATION_FRAMEWORK})
    endif()
elseif(${USE_GLEW})
    find_package(GLEW REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLEW_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLEW IMGUI_IMPL_OPENGL_LOADER_GLEW)
    target_link_libraries(${PROJECT_NAME} ${GLEW_LIBRARIES})
else()
    message(FATAL_ERROR "Missing GL extensaion loader library")
endif()

#find_package(glm 0.9.9 CONFIG REQUIRED)
#include(FindPkgConfig)
#pkg_check_modules(GLM REQUIRED glm)
find_library(GLM glm/glm.hpp REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${GLM_INCLUDE_DIRS})
