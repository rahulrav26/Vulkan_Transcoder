#pragma once

#include "VulkanBase.hpp"
#include "H264Demuxer.hpp"
#include "H265Muxer.hpp"

#include <vulkan/vulkan.h>
#include "/usr/include/vk_video/vulkan_video_codec_h264std_decode.h"
#include "/usr/include/vk_video/vulkan_video_codec_h265std_encode.h"

#include <string>
#include <vector>
#include <memory>


struct FrameResources {
    VkBuffer decodeBitstreamBuffer;
    VkDeviceMemory decodeBitstreamBufferMemory;
    void* pDecodeBitstreamBufferHost;
    VkImage decodedImage;
    VkDeviceMemory decodedImageMemory;
    VkImageView decodedImageView;
    VkBuffer encodeBitstreamBuffer;
    VkDeviceMemory encodeBitstreamBufferMemory;
    void* pEncodeBitstreamBufferHost;
    VkCommandBuffer decodeCommandBuffer;
    VkCommandBuffer encodeCommandBuffer;
    VkFence encodeCompleteFence;
    VkSemaphore decodeCompleteSemaphore;
};

class VideoTranscoder {
public:
    VideoTranscoder(VulkanBase* vulkanBase, const std::string& inPath, const std::string& outPath);
    ~VideoTranscoder();
    void run();

private:
    VulkanBase* vulkanBase = nullptr;
    std::unique_ptr<H264Demuxer> demuxer;
    std::unique_ptr<H265Muxer> muxer;

    VkVideoSessionKHR decodeSession = VK_NULL_HANDLE;
    VkVideoSessionParametersKHR decodeSessionParameters = VK_NULL_HANDLE;
    VkVideoSessionKHR encodeSession = VK_NULL_HANDLE;
    VkVideoSessionParametersKHR encodeSessionParameters = VK_NULL_HANDLE;

    // --- FIX: Add missing member variable declarations ---
    VkVideoProfileInfoKHR decodeProfile{};
    VkVideoProfileInfoKHR encodeProfile{};
    std::vector<VkDeviceMemory> decodeSessionMemory;
    std::vector<VkDeviceMemory> encodeSessionMemory;

    std::vector<FrameResources> frameResources;
    uint32_t currentFrame = 0;
    VkImage decodeDpbImage = VK_NULL_HANDLE;
    VkDeviceMemory decodeDpbImageMemory = VK_NULL_HANDLE;
    std::vector<VkImageView> decodeDpbImageViews;
    VkImage encodeDpbImage = VK_NULL_HANDLE;
    VkDeviceMemory encodeDpbImageMemory = VK_NULL_HANDLE;
    std::vector<VkImageView> encodeDpbImageViews;

    VkCommandPool decodeCommandPool = VK_NULL_HANDLE;
    VkCommandPool encodeCommandPool = VK_NULL_HANDLE;

    // --- FIX: Add missing function pointer declarations ---
    PFN_vkGetVideoSessionMemoryRequirementsKHR pfn_vkGetVideoSessionMemoryRequirementsKHR;
    PFN_vkBindVideoSessionMemoryKHR pfn_vkBindVideoSessionMemoryKHR;
    PFN_vkCreateVideoSessionKHR pfn_vkCreateVideoSessionKHR;
    PFN_vkDestroyVideoSessionKHR pfn_vkDestroyVideoSessionKHR;
    PFN_vkCreateVideoSessionParametersKHR pfn_vkCreateVideoSessionParametersKHR;
    PFN_vkDestroyVideoSessionParametersKHR pfn_vkDestroyVideoSessionParametersKHR;
    PFN_vkCmdBeginVideoCodingKHR pfn_vkCmdBeginVideoCodingKHR;
    PFN_vkCmdEndVideoCodingKHR pfn_vkCmdEndVideoCodingKHR;
    PFN_vkCmdDecodeVideoKHR pfn_vkCmdDecodeVideoKHR;
    PFN_vkCmdEncodeVideoKHR pfn_vkCmdEncodeVideoKHR;

    void loadVideoFunctionPointers();
    void init();
    void initDecode();
    void initEncode();
    // --- FIX: Add missing function declaration ---
    void bindVideoSessionMemory(VkVideoSessionKHR session, std::vector<VkDeviceMemory>& memory);
    void createFrameResources();
    void createDpbImages();
    void createCommandPools();
    void cleanup();

    void transcodeLoop();
    void recordDecodeCommandBuffer(uint32_t frameIndex, VkDeviceSize bitstreamSize);
    void recordEncodeCommandBuffer(uint32_t frameIndex);
    void submitWork(uint32_t frameIndex);
};

