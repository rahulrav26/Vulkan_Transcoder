#include "H264Demuxer.hpp"
#include <iostream>

// The FFmpeg headers must be wrapped in extern "C" because they are C libraries.
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// Constructor: Opens the input file and initializes the demuxer.
H264Demuxer::H264Demuxer(const std::string& filepath) {
    // Open the input file and read its header to fill the format context.
    if (avformat_open_input(&formatContext, filepath.c_str(), nullptr, nullptr) != 0) {
        throw std::runtime_error("FFmpeg: Could not open input file: " + filepath);
    }

    // Read packets from the media file to get stream information.
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        avformat_close_input(&formatContext); // Clean up on failure
        throw std::runtime_error("FFmpeg: Could not find stream information");
    }

    // Find the best video stream in the file.
    videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0) {
        avformat_close_input(&formatContext); // Clean up on failure
        throw std::runtime_error("FFmpeg: Could not find a video stream in the input file");
    }

    // Get a pointer to the codec parameters for the video stream.
    AVStream* videoStream = formatContext->streams[videoStreamIndex];
    codecParameters = videoStream->codecpar;

    // Verify that the video stream is encoded with H.264.
    if (codecParameters->codec_id != AV_CODEC_ID_H264) {
        avformat_close_input(&formatContext); // Clean up on failure
        throw std::runtime_error("FFmpeg: Video stream is not H.264");
    }

    // The 'extradata' field of the codec parameters for H.264 streams in MP4/MOV
    // containers typically holds the SPS and PPS NAL units. We copy this data
    // as it's needed to initialize the Vulkan video session.
    if (codecParameters->extradata_size > 0) {
        sps_pps_data.assign(codecParameters->extradata, codecParameters->extradata + codecParameters->extradata_size);
        std::cout << "Demuxer: Found " << sps_pps_data.size() << " bytes of SPS/PPS extradata." << std::endl;
    } else {
        // While not ideal, some streams might have SPS/PPS in-band. This implementation
        // relies on it being in the container header (extradata).
        std::cout << "Warning: No SPS/PPS extradata found in container header." << std::endl;
    }

    std::cout << "Demuxer initialized for file: " << filepath << std::endl;
    std::cout << "Video Resolution: " << getWidth() << "x" << getHeight() << std::endl;
}

// Destructor: Ensures the format context is properly closed to free resources.
H264Demuxer::~H264Demuxer() {
    if (formatContext) {
        avformat_close_input(&formatContext);
    }
}

// Reads one frame of data from the stream into the provided packet.
bool H264Demuxer::getNextPacket(AVPacket* packet) {
    // av_read_frame returns 0 on success, or a negative error code on failure/EOF.
    // We loop to skip packets from other streams (e.g., audio).
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            return true; // Found a video packet
        }
        // Free the packet that we're skipping.
        av_packet_unref(packet);
    }
    // End of file reached or an error occurred.
    return false;
}

// Accessor for video width.
int H264Demuxer::getWidth() const {
    return codecParameters ? codecParameters->width : 0;
}

// Accessor for video height.
int H264Demuxer::getHeight() const {
    return codecParameters ? codecParameters->height : 0;
}

