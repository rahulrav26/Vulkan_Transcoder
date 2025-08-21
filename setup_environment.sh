#!/bin/bash

# This script prepares an Ubuntu 22.04 or 24.04 system for building
# and running the Vulkan Video Transcoder project.
# It automatically detects the GPU (AMD or NVIDIA) and installs the
# appropriate drivers and all necessary dependencies.

# --- Script Configuration ---
set -e # Exit immediately if a command exits with a non-zero status.
export DEBIAN_FRONTEND=noninteractive # Prevents interactive prompts from apt.

# --- Helper Functions ---
print_info() {
    echo -e "\033[34m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[32m[SUCCESS]\033[0m $1"
}

print_warning() {
    echo -e "\033[33m[WARNING]\033[0m $1"
}

# --- 1. System Detection ---
print_info "Detecting system configuration..."

# Detect Ubuntu Version
OS_VERSION=$(lsb_release -rs)
OS_CODENAME=$(lsb_release -cs)

if [[ "$OS_VERSION" == "22.04" || "$OS_VERSION" == "24.04" ]]; then
    print_info "Detected Ubuntu $OS_VERSION ($OS_CODENAME)."
else
    print_warning "This script is designed for Ubuntu 22.04 or 24.04. Your version ($OS_VERSION) is not officially supported."
    exit 1
fi

# Detect GPU Vendor
GPU_VENDOR=""
if lspci | grep -i 'vga.*amd' > /dev/null; then
    GPU_VENDOR="AMD"
elif lspci | grep -i 'vga.*nvidia' > /dev/null; then
    GPU_VENDOR="NVIDIA"
else
    print_warning "Could not detect a supported AMD or NVIDIA GPU."
    # We can still proceed to install common dependencies.
fi

print_info "Detected GPU Vendor: ${GPU_VENDOR:-'Unknown'}"

# --- 2. Install Core Dependencies (Common to all systems) ---
print_info "Installing core development packages (build-essential, cmake, etc.)..."
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config wget

# --- 3. Install GPU-Specific Drivers ---
if [[ "$GPU_VENDOR" == "NVIDIA" ]]; then
    print_info "NVIDIA GPU detected. Installing recommended proprietary drivers..."
    # The ubuntu-drivers tool is the safest way to install NVIDIA drivers.
    # It handles dependencies, kernel module signing, and secure boot correctly.
    sudo ubuntu-drivers autoinstall
    print_success "NVIDIA drivers installed. A reboot will be required."

elif [[ "$GPU_VENDOR" == "AMD" ]]; then
    print_info "AMD GPU detected. Installing Mesa Vulkan drivers..."
    # The standard Mesa drivers are usually sufficient. We ensure the Vulkan parts are installed.
    sudo apt-get install -y mesa-vulkan-drivers libvulkan1
    print_success "AMD Mesa Vulkan drivers are installed/verified."
fi

# --- 4. Install Vulkan SDK ---
print_info "Installing the LunarG Vulkan SDK..."
# Add the LunarG repository key
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
# Add the repository corresponding to the detected Ubuntu version
if [[ "$OS_CODENAME" == "jammy" ]]; then # Ubuntu 22.04
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list http://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
elif [[ "$OS_CODENAME" == "noble" ]]; then # Ubuntu 24.04
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-noble.list http://packages.lunarg.com/vulkan/lunarg-vulkan-noble.list
fi

sudo apt-get update
sudo apt-get install -y vulkan-sdk
print_success "Vulkan SDK installed successfully."

# --- 5. Install FFmpeg Development Libraries ---
print_info "Installing FFmpeg development libraries..."
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev
print_success "FFmpeg libraries installed successfully."

# --- 6. Setup Project Headers ---
print_info "Setting up Vulkan Video headers..."
mkdir -p include

VK_VIDEO_HEADER_SRC_DIR="/usr/include/vk_video"

# Check if the local SDK headers exist and copy them.
if [ -d "$VK_VIDEO_HEADER_SRC_DIR" ] && [ -f "$VK_VIDEO_HEADER_SRC_DIR/vulkan_video_codec_h264std.h" ]; then
    print_info "Found local Vulkan SDK video headers. Copying..."
    cp "$VK_VIDEO_HEADER_SRC_DIR/vulkan_video_codec_h264std.h" ./include/
    cp "$VK_VIDEO_HEADER_SRC_DIR/vulkan_video_codec_h264std_decode.h" ./include/
    cp "$VK_VIDEO_HEADER_SRC_DIR/vulkan_video_codec_h265std.h" ./include/
    cp "$VK_VIDEO_HEADER_SRC_DIR/vulkan_video_codec_h265std_encode.h" ./include/
    print_success "Successfully copied video headers."
else
    # Fallback to downloading from the web if local headers are not found.
    print_warning "Local Vulkan Video headers not found at '$VK_VIDEO_HEADER_SRC_DIR'."
    print_info "Falling back to downloading headers from the web..."
    wget -O include/vulkan_video_codec_h264std.h https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/include/vulkan/vulkan_video_codec_h264std.h
    wget -O include/vulkan_video_codec_h264std_decode.h https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/include/vulkan/vulkan_video_codec_h264std_decode.h
    wget -O include/vulkan_video_codec_h265std.h https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/include/vulkan/vulkan_video_codec_h265std.h
    wget -O include/vulkan_video_codec_h265std_encode.h https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/include/vulkan/vulkan_video_codec_h265std_encode.h
    print_success "Successfully downloaded video headers."
fi


# --- 7. Final Instructions ---
echo
print_success "Environment setup is complete!"
echo
print_info "Next Steps:"
echo "1. If you were on an NVIDIA system, please REBOOT NOW to load the new drivers:"
echo "   sudo reboot"
echo
echo "2. Compile the project:"
echo "   cmake -S . -B build"
echo "   cmake --build build"
echo
echo "3. Run the application:"
echo "   ./build/transcoder <input_file.mp4> <output_file.mp4>"
echo

