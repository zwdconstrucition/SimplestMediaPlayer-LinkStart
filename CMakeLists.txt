cmake_minimum_required(VERSION 3.15)
project(MediaPlayer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library paths
set(FFMPEG_DIR "${CMAKE_SOURCE_DIR}/libs/ffmpeg")
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/libs/SDL2")

# FFmpeg setup
set(FFMPEG_INCLUDE_DIR "${FFMPEG_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_DIR}/lib")
file(GLOB FFMPEG_LIBS "${FFMPEG_LIB_DIR}/*.lib")

# SDL2 setup
set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")
set(SDL2_LIB_DIR "${SDL2_DIR}/lib/x64")  # Change to x86 for 32-bit builds
set(SDL2_LIBS "${SDL2_LIB_DIR}/SDL2.lib" "${SDL2_LIB_DIR}/SDL2main.lib")

# Add source subdirectory
add_subdirectory(src)