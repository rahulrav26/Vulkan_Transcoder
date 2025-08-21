#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <string>

// A structure to hold the indices of the queue families we need.
// We need separate families for video decoding and encoding.
struct QueueFamilyIndices {
    std::optional<uint32_t> decodeFamily;
    std::optional<uint32_t> encodeFamily;

    // Helper function to check if we have found all required families.
    bool isComplete() const {
        return decodeFamily.has_value() && encodeFamily.has_value();
    }
};

// This class handles the boilerplate setup for a Vulkan application.
// It initializes the instance, selects a physical device, creates a logical device,
// and retrieves the necessary queue handles.
class VulkanBase {
public:
    // Constructor and Destructor
    VulkanBase();
    ~VulkanBase();

    // Initializes the entire Vulkan stack.
    void initVulkan();

    // Accessors for the created Vulkan objects.
    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }
    VkQueue getDecodeQueue() const { return decodeQueue; }
    VkQueue getEncodeQueue() const { return encodeQueue; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return queueFamilyIndices; }

private:
    // --- Core Vulkan Handles ---
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue decodeQueue = VK_NULL_HANDLE;
    VkQueue encodeQueue = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;

    // --- Private Helper Methods for Initialization ---

    // Creates the Vulkan instance and enables validation layers.
    void createInstance();

    // Selects a suitable physical GPU that supports video operations.
    void pickPhysicalDevice();

    // Creates the logical device and the queues for video processing.
    void createLogicalDevice();

    // --- Helper Methods for Device Selection ---

    // Checks if a given physical device is suitable for our needs.
    bool isDeviceSuitable(VkPhysicalDevice device);

    // Finds the necessary queue families (decode, encode) on a given device.
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    // Checks if a device supports all the required extensions.
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    // The list of device extensions required for video transcoding.
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
        VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,
        VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME
    };

    // The list of validation layers to enable for debugging.
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

// Enable validation layers only in debug builds.
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif
};

