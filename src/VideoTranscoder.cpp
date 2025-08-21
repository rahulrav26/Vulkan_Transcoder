#include "VideoTranscoder.hpp"
#include "VulkanUtils.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

constexpr VkDeviceSize BITSTREAM_BUFFER_SIZE = 2 * 1024 * 1024;
constexpr uint32_t NUM_FRAME_RESOURCES = 1;
constexpr uint32_t DPB_SIZE = 8;

VideoTranscoder::VideoTranscoder(VulkanBase* vulkanBase, const std::string& inPath, const std::string& outPath)
    : vulkanBase(vulkanBase) {
    if (!vulkanBase || !vulkanBase->getDevice()) {
        throw std::invalid_argument("VulkanBase pointer or device cannot be null.");
    }

    demuxer = std::make_unique<H264Demuxer>(inPath);
    muxer = std::make_unique<H265Muxer>(outPath, demuxer->getWidth(), demuxer->getHeight(), 30);

    init();
}

VideoTranscoder::~VideoTranscoder() {
    vkDeviceWaitIdle(vulkanBase->getDevice());
    cleanup();
}

void VideoTranscoder::run() {
    std::cout << "Starting transcoding process..." << std::endl;
    transcodeLoop();
    std::cout << "Transcoding finished successfully." << std::endl;
}

void VideoTranscoder::loadVideoFunctionPointers() {
    VkDevice device = vulkanBase->getDevice();
    // Load all required video function pointers
    pfn_vkGetVideoSessionMemoryRequirementsKHR = (PFN_vkGetVideoSessionMemoryRequirementsKHR)vkGetDeviceProcAddr(device, "vkGetVideoSessionMemoryRequirementsKHR");
    pfn_vkBindVideoSessionMemoryKHR = (PFN_vkBindVideoSessionMemoryKHR)vkGetDeviceProcAddr(device, "vkBindVideoSessionMemoryKHR");
    pfn_vkCreateVideoSessionKHR = (PFN_vkCreateVideoSessionKHR)vkGetDeviceProcAddr(device, "vkCreateVideoSessionKHR");
    pfn_vkDestroyVideoSessionKHR = (PFN_vkDestroyVideoSessionKHR)vkGetDeviceProcAddr(device, "vkDestroyVideoSessionKHR");
    pfn_vkCreateVideoSessionParametersKHR = (PFN_vkCreateVideoSessionParametersKHR)vkGetDeviceProcAddr(device, "vkCreateVideoSessionParametersKHR");
    pfn_vkDestroyVideoSessionParametersKHR = (PFN_vkDestroyVideoSessionParametersKHR)vkGetDeviceProcAddr(device, "vkDestroyVideoSessionParametersKHR");
    pfn_vkCmdBeginVideoCodingKHR = (PFN_vkCmdBeginVideoCodingKHR)vkGetDeviceProcAddr(device, "vkCmdBeginVideoCodingKHR");
    pfn_vkCmdEndVideoCodingKHR = (PFN_vkCmdEndVideoCodingKHR)vkGetDeviceProcAddr(device, "vkCmdEndVideoCodingKHR");
    pfn_vkCmdDecodeVideoKHR = (PFN_vkCmdDecodeVideoKHR)vkGetDeviceProcAddr(device, "vkCmdDecodeVideoKHR");
    pfn_vkCmdEncodeVideoKHR = (PFN_vkCmdEncodeVideoKHR)vkGetDeviceProcAddr(device, "vkCmdEncodeVideoKHR");

    if (!pfn_vkGetVideoSessionMemoryRequirementsKHR || !pfn_vkBindVideoSessionMemoryKHR || !pfn_vkCreateVideoSessionKHR ||
        !pfn_vkDestroyVideoSessionKHR || !pfn_vkCreateVideoSessionParametersKHR || !pfn_vkDestroyVideoSessionParametersKHR ||
        !pfn_vkCmdBeginVideoCodingKHR || !pfn_vkCmdEndVideoCodingKHR || !pfn_vkCmdDecodeVideoKHR || !pfn_vkCmdEncodeVideoKHR) {
        throw std::runtime_error("Failed to load one or more Vulkan video function pointers!");
    }
     std::cout << "Successfully loaded Vulkan video function pointers." << std::endl;
}

void VideoTranscoder::init() {
    loadVideoFunctionPointers();
    initDecode();
    initEncode();
    createCommandPools();
    createDpbImages();
    createFrameResources();
}

void VideoTranscoder::initDecode() {
    VkDevice device = vulkanBase->getDevice();
    VkFormat decodedImageFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    VkExtensionProperties h264StdVersion{};
    strncpy(h264StdVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME, VK_MAX_EXTENSION_NAME_SIZE);
    h264StdVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION;

    VkVideoDecodeH264ProfileInfoKHR h264Profile{};
    h264Profile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
    h264Profile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
    h264Profile.pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;

    decodeProfile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
    decodeProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    decodeProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    decodeProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    decodeProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    decodeProfile.pNext = &h264Profile;

    VkVideoSessionCreateInfoKHR sessionCreateInfo{};
    sessionCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
    sessionCreateInfo.queueFamilyIndex = vulkanBase->getQueueFamilyIndices().decodeFamily.value();
    sessionCreateInfo.pVideoProfile = &decodeProfile;
    sessionCreateInfo.pictureFormat = decodedImageFormat;
    sessionCreateInfo.maxCodedExtent = { (uint32_t)demuxer->getWidth(), (uint32_t)demuxer->getHeight() };
    sessionCreateInfo.referencePictureFormat = decodedImageFormat;
    sessionCreateInfo.maxDpbSlots = DPB_SIZE;
    sessionCreateInfo.maxActiveReferencePictures = DPB_SIZE;
    sessionCreateInfo.pStdHeaderVersion = &h264StdVersion;

    if (pfn_vkCreateVideoSessionKHR(device, &sessionCreateInfo, nullptr, &decodeSession) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create decode session!");
    }
    std::cout << "Decode session created." << std::endl;

    // --- FIX: Allocate and bind memory for the video session ---
    bindVideoSessionMemory(decodeSession, decodeSessionMemory);

    // --- FIX: Create video session parameters ---
    // A real implementation would parse SPS/PPS from the bitstream here.
    // For simplicity, we'll create a placeholder.
    VkVideoSessionParametersCreateInfoKHR paramsCreateInfo = {VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
    paramsCreateInfo.videoSession = decodeSession;
    if (pfn_vkCreateVideoSessionParametersKHR(device, &paramsCreateInfo, nullptr, &decodeSessionParameters) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create decode session parameters!");
    }
}

void VideoTranscoder::initEncode() {
    VkDevice device = vulkanBase->getDevice();
    VkFormat inputImageFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    VkExtensionProperties h265StdVersion{};
    strncpy(h265StdVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_EXTENSION_NAME, VK_MAX_EXTENSION_NAME_SIZE);
    h265StdVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H265_ENCODE_SPEC_VERSION;

    VkVideoEncodeH265ProfileInfoKHR h265Profile{};
    h265Profile.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR;
    h265Profile.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;

    encodeProfile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
    encodeProfile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
    encodeProfile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
    encodeProfile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    encodeProfile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
    encodeProfile.pNext = &h265Profile;

    VkVideoSessionCreateInfoKHR sessionCreateInfo{};
    sessionCreateInfo.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
    sessionCreateInfo.queueFamilyIndex = vulkanBase->getQueueFamilyIndices().encodeFamily.value();
    sessionCreateInfo.pVideoProfile = &encodeProfile;
    sessionCreateInfo.pictureFormat = inputImageFormat;
    sessionCreateInfo.maxCodedExtent = { (uint32_t)demuxer->getWidth(), (uint32_t)demuxer->getHeight() };
    sessionCreateInfo.referencePictureFormat = inputImageFormat;
    sessionCreateInfo.maxDpbSlots = DPB_SIZE;
    sessionCreateInfo.maxActiveReferencePictures = DPB_SIZE;
    sessionCreateInfo.pStdHeaderVersion = &h265StdVersion;

    if (pfn_vkCreateVideoSessionKHR(device, &sessionCreateInfo, nullptr, &encodeSession) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create encode session!");
    }
    std::cout << "Encode session created." << std::endl;

    // --- FIX: Allocate and bind memory for the video session ---
    bindVideoSessionMemory(encodeSession, encodeSessionMemory);

    // --- FIX: Create video session parameters ---
    VkVideoSessionParametersCreateInfoKHR paramsCreateInfo = {VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR};
    paramsCreateInfo.videoSession = encodeSession;
    if (pfn_vkCreateVideoSessionParametersKHR(device, &paramsCreateInfo, nullptr, &encodeSessionParameters) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create encode session parameters!");
    }
}

// --- FIX: New helper function to bind memory to a video session ---
void VideoTranscoder::bindVideoSessionMemory(VkVideoSessionKHR session, std::vector<VkDeviceMemory>& memory) {
    VkDevice device = vulkanBase->getDevice();
    VkPhysicalDevice physicalDevice = vulkanBase->getPhysicalDevice();

    uint32_t memReqCount = 0;
    pfn_vkGetVideoSessionMemoryRequirementsKHR(device, session, &memReqCount, nullptr);
    std::vector<VkVideoSessionMemoryRequirementsKHR> memReqs(memReqCount, {VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR});
    pfn_vkGetVideoSessionMemoryRequirementsKHR(device, session, &memReqCount, memReqs.data());

    memory.resize(memReqCount);
    std::vector<VkBindVideoSessionMemoryInfoKHR> bindInfos(memReqCount, {VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR});

    for (uint32_t i = 0; i < memReqCount; ++i) {
        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReqs[i].memoryRequirements.size;
        allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(physicalDevice, memReqs[i].memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate video session memory");
        }

        bindInfos[i].memoryBindIndex = memReqs[i].memoryBindIndex;
        bindInfos[i].memory = memory[i];
        bindInfos[i].memoryOffset = 0;
        bindInfos[i].memorySize = allocInfo.allocationSize;
    }

    if (pfn_vkBindVideoSessionMemoryKHR(device, session, memReqCount, bindInfos.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind video session memory");
    }
}


void VideoTranscoder::createCommandPools() {
    VkDevice device = vulkanBase->getDevice();
    const auto& qfIndices = vulkanBase->getQueueFamilyIndices();
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = qfIndices.decodeFamily.value();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &decodeCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create decode command pool!");
    }
    poolInfo.queueFamilyIndex = qfIndices.encodeFamily.value();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &encodeCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create encode command pool!");
    }
}

void VideoTranscoder::createDpbImages() {
    VkDevice device = vulkanBase->getDevice();
    VkPhysicalDevice pDevice = vulkanBase->getPhysicalDevice();
    VkFormat format = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    uint32_t width = demuxer->getWidth();
    uint32_t height = demuxer->getHeight();

    // --- FIX: Provide video profile info when creating video-related images ---
    VkVideoProfileListInfoKHR decodeProfileList{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    decodeProfileList.profileCount = 1;
    decodeProfileList.pProfiles = &decodeProfile;

    VkVideoProfileListInfoKHR encodeProfileList{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    encodeProfileList.profileCount = 1;
    encodeProfileList.pProfiles = &encodeProfile;

    VkImageUsageFlags decodeDpbUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
    VulkanUtils::createImage(pDevice, device, width, height, format, decodeDpbUsage, decodeDpbImage, decodeDpbImageMemory, DPB_SIZE, &decodeProfileList);

    VkImageUsageFlags encodeDpbUsage = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
    VulkanUtils::createImage(pDevice, device, width, height, format, encodeDpbUsage, encodeDpbImage, encodeDpbImageMemory, DPB_SIZE, &encodeProfileList);
}

void VideoTranscoder::createFrameResources() {
    frameResources.resize(NUM_FRAME_RESOURCES);
    VkDevice device = vulkanBase->getDevice();
    VkPhysicalDevice pDevice = vulkanBase->getPhysicalDevice();
    VkFormat imageFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    uint32_t width = demuxer->getWidth();
    uint32_t height = demuxer->getHeight();

    // --- FIX: Provide video profile info when creating video-related resources ---
    VkVideoProfileListInfoKHR decodeProfileList{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    decodeProfileList.profileCount = 1;
    decodeProfileList.pProfiles = &decodeProfile;

    VkVideoProfileListInfoKHR encodeProfileList{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    encodeProfileList.profileCount = 1;
    encodeProfileList.pProfiles = &encodeProfile;

    VkVideoProfileListInfoKHR combinedProfileList{VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR};
    VkVideoProfileInfoKHR profiles[] = {decodeProfile, encodeProfile};
    combinedProfileList.profileCount = 2;
    combinedProfileList.pProfiles = profiles;

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (uint32_t i = 0; i < NUM_FRAME_RESOURCES; ++i) {
        auto& res = frameResources[i];
        VulkanUtils::createBuffer(pDevice, device, BITSTREAM_BUFFER_SIZE, VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            res.decodeBitstreamBuffer, res.decodeBitstreamBufferMemory, &decodeProfileList);
        vkMapMemory(device, res.decodeBitstreamBufferMemory, 0, BITSTREAM_BUFFER_SIZE, 0, &res.pDecodeBitstreamBufferHost);

        VulkanUtils::createBuffer(pDevice, device, BITSTREAM_BUFFER_SIZE, VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            res.encodeBitstreamBuffer, res.encodeBitstreamBufferMemory, &encodeProfileList);
        vkMapMemory(device, res.encodeBitstreamBufferMemory, 0, BITSTREAM_BUFFER_SIZE, 0, &res.pEncodeBitstreamBufferHost);

        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
        VulkanUtils::createImage(pDevice, device, width, height, imageFormat, imageUsage, res.decodedImage, res.decodedImageMemory, 1, &combinedProfileList);
        res.decodedImageView = VulkanUtils::createImageView(device, res.decodedImage, imageFormat);

        allocInfo.commandPool = decodeCommandPool;
        if (vkAllocateCommandBuffers(device, &allocInfo, &res.decodeCommandBuffer) != VK_SUCCESS) throw std::runtime_error("Failed to allocate decode command buffer!");
        allocInfo.commandPool = encodeCommandPool;
        if (vkAllocateCommandBuffers(device, &allocInfo, &res.encodeCommandBuffer) != VK_SUCCESS) throw std::runtime_error("Failed to allocate encode command buffer!");
        if (vkCreateFence(device, &fenceInfo, nullptr, &res.encodeCompleteFence) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &res.decodeCompleteSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

void VideoTranscoder::transcodeLoop() {
    AVPacket* packet = av_packet_alloc();
    if (!packet) throw std::runtime_error("Failed to allocate AVPacket");
    int frameCount = 0;

    while (demuxer->getNextPacket(packet)) {
        if (packet->stream_index != demuxer->getVideoStreamIndex()) {
            av_packet_unref(packet);
            continue;
        }

        FrameResources& res = frameResources[currentFrame];
        vkWaitForFences(vulkanBase->getDevice(), 1, &res.encodeCompleteFence, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanBase->getDevice(), 1, &res.encodeCompleteFence);

        memcpy(res.pDecodeBitstreamBufferHost, packet->data, packet->size);
        recordDecodeCommandBuffer(currentFrame, packet->size);
        recordEncodeCommandBuffer(currentFrame);
        submitWork(currentFrame);

        std::vector<uint8_t> encodedData(1024, 0);
        memcpy(encodedData.data(), res.pEncodeBitstreamBufferHost, 1024);
        muxer->writePacket(encodedData, frameCount);

        av_packet_unref(packet);
        currentFrame = (currentFrame + 1) % NUM_FRAME_RESOURCES;
        frameCount++;
        std::cout << "\rTranscoded frame " << frameCount << std::flush;
    }
    av_packet_free(&packet);
    std::cout << std::endl;
}

void VideoTranscoder::recordDecodeCommandBuffer(uint32_t frameIndex, VkDeviceSize bitstreamSize) {
    FrameResources& res = frameResources[frameIndex];
    vkResetCommandBuffer(res.decodeCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(res.decodeCommandBuffer, &beginInfo);

    // --- FIX: Bind session parameters and manage DPB ---
    VkVideoBeginCodingInfoKHR beginCodingInfo{VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR};
    beginCodingInfo.videoSession = decodeSession;
    beginCodingInfo.videoSessionParameters = decodeSessionParameters;
    // A real implementation would manage reference slots here.
    pfn_vkCmdBeginVideoCodingKHR(res.decodeCommandBuffer, &beginCodingInfo);

    VkVideoPictureResourceInfoKHR dstPictureResource{VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR};
    dstPictureResource.imageViewBinding = res.decodedImageView;
    dstPictureResource.codedExtent = { (uint32_t)demuxer->getWidth(), (uint32_t)demuxer->getHeight() };

    // --- FIX: Provide required codec-specific picture info ---
    VkVideoDecodeH264PictureInfoKHR h264PicInfo{VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR};
    // A real implementation would parse this from the bitstream.
    // h264PicInfo.pStdPictureInfo = &some_StdVideoDecodeH264PictureInfo;

    VkVideoDecodeInfoKHR decodeInfo{VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR};
    decodeInfo.pNext = &h264PicInfo; // <-- Chain the picture info
    decodeInfo.srcBuffer = res.decodeBitstreamBuffer;
    decodeInfo.srcBufferOffset = 0;
    decodeInfo.srcBufferRange = bitstreamSize;
    decodeInfo.dstPictureResource = dstPictureResource;
    // A real implementation needs to set up pSetupReferenceSlot and pReferenceSlots.

    pfn_vkCmdDecodeVideoKHR(res.decodeCommandBuffer, &decodeInfo);

    VkVideoEndCodingInfoKHR endCodingInfo{VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};
    pfn_vkCmdEndVideoCodingKHR(res.decodeCommandBuffer, &endCodingInfo);

    vkEndCommandBuffer(res.decodeCommandBuffer);
}

void VideoTranscoder::recordEncodeCommandBuffer(uint32_t frameIndex) {
    FrameResources& res = frameResources[frameIndex];
    vkResetCommandBuffer(res.encodeCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(res.encodeCommandBuffer, &beginInfo);

    VulkanUtils::transitionImageLayout(res.encodeCommandBuffer, res.decodedImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR);

    VkVideoBeginCodingInfoKHR beginCodingInfo{VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR};
    beginCodingInfo.videoSession = encodeSession;
    beginCodingInfo.videoSessionParameters = encodeSessionParameters;
    pfn_vkCmdBeginVideoCodingKHR(res.encodeCommandBuffer, &beginCodingInfo);

    VkVideoPictureResourceInfoKHR srcPictureResource{VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR};
    srcPictureResource.imageViewBinding = res.decodedImageView;
    srcPictureResource.codedExtent = { (uint32_t)demuxer->getWidth(), (uint32_t)demuxer->getHeight() };

    VkVideoEncodeH265PictureInfoKHR h265PicInfo{VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PICTURE_INFO_KHR};
    // A real implementation would configure NALU type, etc.

    VkVideoEncodeInfoKHR encodeInfo{VK_STRUCTURE_TYPE_VIDEO_ENCODE_INFO_KHR};
    encodeInfo.pNext = &h265PicInfo;
    encodeInfo.dstBuffer = res.encodeBitstreamBuffer;
    encodeInfo.dstBufferRange = BITSTREAM_BUFFER_SIZE;
    encodeInfo.srcPictureResource = srcPictureResource;

    pfn_vkCmdEncodeVideoKHR(res.encodeCommandBuffer, &encodeInfo);

    VkVideoEndCodingInfoKHR endCodingInfo{VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};
    pfn_vkCmdEndVideoCodingKHR(res.encodeCommandBuffer, &endCodingInfo);

    vkEndCommandBuffer(res.encodeCommandBuffer);
}

void VideoTranscoder::submitWork(uint32_t frameIndex) {
    FrameResources& res = frameResources[frameIndex];
    VkSubmitInfo decodeSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    decodeSubmitInfo.commandBufferCount = 1;
    decodeSubmitInfo.pCommandBuffers = &res.decodeCommandBuffer;
    decodeSubmitInfo.signalSemaphoreCount = 1;
    decodeSubmitInfo.pSignalSemaphores = &res.decodeCompleteSemaphore;
    vkQueueSubmit(vulkanBase->getDecodeQueue(), 1, &decodeSubmitInfo, VK_NULL_HANDLE);

    VkSubmitInfo encodeSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR };
    encodeSubmitInfo.waitSemaphoreCount = 1;
    encodeSubmitInfo.pWaitSemaphores = &res.decodeCompleteSemaphore;
    encodeSubmitInfo.pWaitDstStageMask = waitStages;
    encodeSubmitInfo.commandBufferCount = 1;
    encodeSubmitInfo.pCommandBuffers = &res.encodeCommandBuffer;
    vkQueueSubmit(vulkanBase->getEncodeQueue(), 1, &encodeSubmitInfo, res.encodeCompleteFence);
}

void VideoTranscoder::cleanup() {
    VkDevice device = vulkanBase->getDevice();
    for (auto& res : frameResources) {
        vkDestroyFence(device, res.encodeCompleteFence, nullptr);
        vkDestroySemaphore(device, res.decodeCompleteSemaphore, nullptr);
        vkUnmapMemory(device, res.decodeBitstreamBufferMemory);
        vkDestroyBuffer(device, res.decodeBitstreamBuffer, nullptr);
        vkFreeMemory(device, res.decodeBitstreamBufferMemory, nullptr);
        vkUnmapMemory(device, res.encodeBitstreamBufferMemory);
        vkDestroyBuffer(device, res.encodeBitstreamBuffer, nullptr);
        vkFreeMemory(device, res.encodeBitstreamBufferMemory, nullptr);
        vkDestroyImageView(device, res.decodedImageView, nullptr);
        vkDestroyImage(device, res.decodedImage, nullptr);
        vkFreeMemory(device, res.decodedImageMemory, nullptr);
    }

    vkDestroyImage(device, decodeDpbImage, nullptr);
    vkFreeMemory(device, decodeDpbImageMemory, nullptr);
    vkDestroyImage(device, encodeDpbImage, nullptr);
    vkFreeMemory(device, encodeDpbImageMemory, nullptr);
    vkDestroyCommandPool(device, decodeCommandPool, nullptr);
    vkDestroyCommandPool(device, encodeCommandPool, nullptr);

    if (pfn_vkDestroyVideoSessionParametersKHR) {
        if (decodeSessionParameters) pfn_vkDestroyVideoSessionParametersKHR(device, decodeSessionParameters, nullptr);
        if (encodeSessionParameters) pfn_vkDestroyVideoSessionParametersKHR(device, encodeSessionParameters, nullptr);
    }

    if (pfn_vkDestroyVideoSessionKHR) {
        if (decodeSession) pfn_vkDestroyVideoSessionKHR(device, decodeSession, nullptr);
        if (encodeSession) pfn_vkDestroyVideoSessionKHR(device, encodeSession, nullptr);
    }

    for(auto& mem : decodeSessionMemory) vkFreeMemory(device, mem, nullptr);
    for(auto& mem : encodeSessionMemory) vkFreeMemory(device, mem, nullptr);
}

