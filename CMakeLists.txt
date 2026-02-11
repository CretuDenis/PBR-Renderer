cmake_minimum_required(VERSION 3.10)
project(CCraft)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include/core)

add_subdirectory(extern/GLFW)
include_directories(extern/GLFW/include)

add_subdirectory(extern/cglm)
include_directories(extern/cglm/include)

add_library(glad extern/glad/src/glad.c)
target_include_directories(glad PUBLIC extern/glad/include)

file(GLOB_RECURSE SOURCES src/*.c)
add_executable(CCraft ${SOURCES})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} cglm glad glfw)
else()
    target_link_libraries(${PROJECT_NAME} cglm glad glfw ${CMAKE_DL_LIBS})
endif()
