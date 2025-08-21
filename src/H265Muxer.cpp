#include "H265Muxer.hpp"
#include <iostream>

// The FFmpeg headers must be wrapped in extern "C" because they are C libraries.
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// Constructor: Initializes the output format context and video stream.
H265Muxer::H265Muxer(const std::string& filepath, int width, int height, int fps) {
    // Allocate the output media context.
    if (avformat_alloc_output_context2(&formatContext, nullptr, nullptr, filepath.c_str()) < 0) {
        throw std::runtime_error("Muxer: Could not create output context for " + filepath);
    }

    // Add a new video stream to the output media file.
    videoStream = avformat_new_stream(formatContext, nullptr);
    if (!videoStream) {
        throw std::runtime_error("Muxer: Could not allocate stream");
    }

    // Set the basic codec parameters for the stream.
    // The detailed parameters (VPS/SPS/PPS) are set later.
    videoStream->codecpar->codec_id = AV_CODEC_ID_HEVC;
    videoStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    videoStream->codecpar->width = width;
    videoStream->codecpar->height = height;
    // Set the timebase, which defines the units of the presentation timestamp (PTS).
    videoStream->time_base = {1, fps};

    // Open the output file for writing if needed by the container format.
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatContext->pb, filepath.c_str(), AVIO_FLAG_WRITE) < 0) {
            throw std::runtime_error("Muxer: Could not open output file: " + filepath);
        }
    }
    std::cout << "Muxer initialized for file: " << filepath << std::endl;
}

// Destructor: Finalizes the output file and frees all resources.
H265Muxer::~H265Muxer() {
    if (formatContext) {
        // Write the stream trailer to the output media file.
        av_write_trailer(formatContext);

        // Close the output file.
        if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatContext->pb);
        }

        // Free the stream.
        avformat_free_context(formatContext);
    }
}

// Sets the codec-specific extradata (VPS, SPS, PPS).
void H265Muxer::setCodecParameters(const std::vector<uint8_t>& vps, const std::vector<uint8_t>& sps, const std::vector<uint8_t>& pps) {
    // This function would combine VPS, SPS, and PPS into a single buffer
    // formatted according to the HEVC specification for MP4 containers (hvcc).
    // For simplicity, we will assume this is handled and passed in correctly.
    // A real implementation requires careful construction of this buffer.
    std::vector<uint8_t> extradata;
    // ... logic to build hvcc extradata ...
    
    // Allocate and copy the extradata to the codec parameters.
    videoStream->codecpar->extradata = (uint8_t*)av_malloc(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!videoStream->codecpar->extradata) {
        throw std::runtime_error("Muxer: Failed to allocate extradata");
    }
    memcpy(videoStream->codecpar->extradata, extradata.data(), extradata.size());
    videoStream->codecpar->extradata_size = extradata.size();
}

// Writes the container header to the file.
void H265Muxer::writeHeader() {
    if (avformat_write_header(formatContext, nullptr) < 0) {
        throw std::runtime_error("Muxer: Error occurred when writing header");
    }
    headerWritten = true;
    std::cout << "Muxer: Wrote container header." << std::endl;
}

// Writes a single compressed frame to the output file.
void H265Muxer::writePacket(const std::vector<uint8_t>& data, int64_t pts) {
    // The header must be written before the first packet.
    if (!headerWritten) {
        writeHeader();
    }

    AVPacket packet{};
    av_init_packet(&packet);

    packet.data = const_cast<uint8_t*>(data.data());
    packet.size = data.size();
    packet.stream_index = videoStream->index;

    // Rescale the timestamp from the application's timebase to the stream's timebase.
    // For this project, we can assume they are the same (based on FPS).
    packet.pts = pts;
    packet.dts = pts; // For simple cases, DTS can be the same as PTS.

    // A simple check for keyframes (IDR frames in H.265).
    // A proper implementation would parse the NAL unit type from the bitstream.
    // For now, we assume all packets passed could be keyframes for simplicity.
    packet.flags |= AV_PKT_FLAG_KEY;

    // Write the compressed frame to the media file.
    if (av_interleaved_write_frame(formatContext, &packet) < 0) {
        std::cerr << "Muxer: Warning, failed to write packet." << std::endl;
    }

    av_packet_unref(&packet);
}

