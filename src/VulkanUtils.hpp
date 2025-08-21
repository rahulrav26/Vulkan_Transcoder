#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

// The VulkanUtils namespace provides a collection of static helper functions
// for common Vulkan resource creation and management tasks.
namespace VulkanUtils {

    // Finds a suitable memory type index on the physical device that matches
    // the given type filter and memory property flags.
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Creates a VkBuffer and allocates its backing memory.
    // FIX: Added pNext parameter to support chaining video profiles.
    void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& bufferMemory, const void* pNext = nullptr);

    // Creates a VkImage and allocates its backing memory.
    // FIX: Added pNext parameter to support chaining video profiles.
    void createImage(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t width, uint32_t height,
                     VkFormat format, VkImageUsageFlags usage,
                     VkImage& image, VkDeviceMemory& imageMemory, uint32_t arrayLayers = 1, const void* pNext = nullptr);

    // Creates a VkImageView for a given VkImage.
    VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, uint32_t arrayLayers = 1);

    // Records a command to transition the layout of an image.
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout);

} // namespace VulkanUtils

