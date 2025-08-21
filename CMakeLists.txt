# Minimum CMake version required
cmake_minimum_required(VERSION 3.20)

# Define the project name and specify C++ as the language
project(VulkanTranscoder CXX)

# Set the C++ standard to C++17 and enforce it
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Required Packages ---

# Find the Vulkan SDK libraries and headers
# This command locates the necessary components for Vulkan development.
find_package(Vulkan REQUIRED)

# Find the FFmpeg libraries using pkg-config
# pkg-config is a helper tool to find compiler and linker flags for libraries.
# This is the standard way to find FFmpeg on Linux systems.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil)

# --- Define Executable and Link Libraries ---

# Define the final executable name and list all source files.
# Using a variable for source files makes the list cleaner.
set(SOURCES
    src/main.cpp
    src/VulkanBase.cpp
    src/H264Demuxer.cpp
    src/H265Muxer.cpp
    src/VideoTranscoder.cpp
    src/VulkanUtils.cpp
)
add_executable(transcoder ${SOURCES})

# Specify include directories.
# This tells the compiler where to find header files (#include <...>)
target_include_directories(transcoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

# Link the required libraries to the executable.
# This tells the linker which libraries our executable depends on.
target_link_libraries(transcoder PRIVATE
    Vulkan::Vulkan      # The official Vulkan target from find_package(Vulkan)
    PkgConfig::FFMPEG   # The imported target from pkg_check_modules for FFmpeg
)

# --- Compiler Flags (Optional but Recommended) ---

# Enable more warnings for cleaner code
if(CMAKE_COMPILER_IS_GNUXX OR CMAKE_COMPILER_IS_CLANGXX)
    target_compile_options(transcoder PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Installation (Optional) ---
# Defines where the executable will be installed when running 'make install'
install(TARGETS transcoder
    RUNTIME DESTINATION bin
)

