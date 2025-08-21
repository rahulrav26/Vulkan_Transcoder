#include "VulkanBase.hpp"
#include "VideoTranscoder.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

// The main entry point for the Vulkan Transcoder application.
int main(int argc, char* argv[]) {
    // --- Argument Parsing ---
    // The application expects two command-line arguments:
    // 1. The path to the input H.264 video file.
    // 2. The path for the output H.265 video file.
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file.mp4> <output_file.mp4>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string inputFilePath = argv[1];
    std::string outputFilePath = argv[2];

    // --- Application Logic ---
    // All core logic is wrapped in a try-catch block to handle exceptions
    // thrown by the Vulkan and FFmpeg components.
    try {
        // 1. Initialize the core Vulkan components (instance, device, queues).
        VulkanBase vulkanBase;
        vulkanBase.initVulkan();

        // 2. Initialize the main transcoder class, which sets up video sessions
        //    and all necessary resources.
        VideoTranscoder transcoder(&vulkanBase, inputFilePath, outputFilePath);

        // 3. Start the main transcoding loop.
        transcoder.run();

    } catch (const std::exception& e) {
        // If any part of the setup or execution fails, print the error and exit.
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // If the program completes without exceptions, it was successful.
    std::cout << "\nApplication finished successfully." << std::endl;
    return EXIT_SUCCESS;
}

