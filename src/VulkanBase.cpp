#include "VulkanBase.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>
#include <set>

// --- VENDOR ID for NVIDIA ---
const uint32_t NVIDIA_VENDOR_ID = 0x10de;

// Constructor: Does nothing, initialization is handled by initVulkan().
VulkanBase::VulkanBase() {}

// Destructor: Cleans up all Vulkan resources in the reverse order of creation.
VulkanBase::~VulkanBase() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        vkDestroyDevice(device, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}

// Main initialization function that calls the setup steps in order.
void VulkanBase::initVulkan() {
    createInstance();
    pickPhysicalDevice();
    createLogicalDevice();
}

// Creates the Vulkan instance.
void VulkanBase::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Video Transcoder";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> requiredExtensions = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
    std::cout << "Vulkan instance created." << std::endl;
}

// --- MODIFIED: This function now prioritizes NVIDIA discrete GPUs ---
void VulkanBase::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout << "\n--- Finding a Suitable GPU ---" << std::endl;
    std::cout << "Found " << deviceCount << " device(s)." << std::endl;

    VkPhysicalDevice candidate = VK_NULL_HANDLE;

    // Prioritize finding a suitable NVIDIA discrete GPU first
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.vendorID == NVIDIA_VENDOR_ID && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }
    }

    // If no suitable NVIDIA dGPU is found, check all other devices
    if (physicalDevice == VK_NULL_HANDLE) {
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        std::cout << "\n--------------------------------" << std::endl;
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "\nSuccessfully selected Physical Device: " << props.deviceName << std::endl;
    std::cout << "--------------------------------\n" << std::endl;
}


// Creates the logical device and retrieves queue handles.
void VulkanBase::createLogicalDevice() {
    queueFamilyIndices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices.decodeFamily.value(),
        queueFamilyIndices.encodeFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceSynchronization2Features syncFeatures{};
    syncFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    syncFeatures.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pNext = &syncFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, queueFamilyIndices.decodeFamily.value(), 0, &decodeQueue);
    vkGetDeviceQueue(device, queueFamilyIndices.encodeFamily.value(), 0, &encodeQueue);
    std::cout << "Logical device and queues created." << std::endl;
}

bool VulkanBase::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    std::cout << "\n[INFO] Checking device: " << props.deviceName << std::endl;

    std::cout << "  - Checking for queue families..." << std::endl;
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool queuesFound = indices.isComplete();
    if (indices.decodeFamily.has_value()) {
        std::cout << "    [PASS] Video Decode Queue Family found at index " << indices.decodeFamily.value() << std::endl;
    } else {
        std::cout << "    [FAIL] Video Decode Queue Family NOT found." << std::endl;
    }
    if (indices.encodeFamily.has_value()) {
        std::cout << "    [PASS] Video Encode Queue Family found at index " << indices.encodeFamily.value() << std::endl;
    } else {
        std::cout << "    [FAIL] Video Encode Queue Family NOT found." << std::endl;
    }

    std::cout << "  - Checking for required device extensions..." << std::endl;
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    if (queuesFound && extensionsSupported) {
        std::cout << "[INFO] >>> This device IS suitable! <<<" << std::endl;
        return true;
    } else {
        std::cout << "[INFO] >>> This device is NOT suitable. <<<" << std::endl;
        return false;
    }
}

QueueFamilyIndices VulkanBase::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2});
    std::vector<VkQueueFamilyVideoPropertiesKHR> videoQueueFamilyProperties(queueFamilyCount, {VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR});

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        queueFamilyProperties[i].pNext = &videoQueueFamilyProperties[i];
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilyProperties.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) {
            if (!indices.decodeFamily.has_value()) {
                 indices.decodeFamily = i;
            }
        }
        if (queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) {
             if (!indices.encodeFamily.has_value()) {
                indices.encodeFamily = i;
            }
        }
    }
    return indices;
}

bool VulkanBase::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> available;
    for (const auto& extension : availableExtensions) {
        available.insert(extension.extensionName);
    }

    bool allFound = true;
    for (const char* requiredExt : deviceExtensions) {
        if (available.count(requiredExt)) {
            std::cout << "    [Found] " << requiredExt << std::endl;
        } else {
            std::cout << "    [Missing] " << requiredExt << std::endl;
            allFound = false;
        }
    }
    return allFound;
}

