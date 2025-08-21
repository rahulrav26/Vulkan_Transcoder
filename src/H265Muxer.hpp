#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint> // <--- FIX: Added this include for uint8_t

// Forward declarations for FFmpeg types to avoid including the C headers
// in a C++ header file.
struct AVFormatContext;
struct AVPacket;
struct AVStream;

// The H265Muxer class encapsulates the logic for writing a raw H.265
// bitstream into an MP4 container file using the FFmpeg libraries.
class H265Muxer {
public:
    // Constructor: Creates the output file and initializes the muxer.
    // It sets up the video stream with the specified parameters.
    // Throws a std::runtime_error on failure.
    H265Muxer(const std::string& filepath, int width, int height, int fps);

    // Destructor: Finalizes the MP4 file by writing the trailer and
    // closes all FFmpeg resources.
    ~H265Muxer();

    // Writes a single compressed video packet to the output file.
    // The data vector contains the raw H.265 NAL units for one frame.
    // The pts (Presentation Timestamp) is crucial for correct playback timing.
    void writePacket(const std::vector<uint8_t>& data, int64_t pts);

    // Writes the initial H.265 parameter sets (VPS, SPS, PPS) to the
    // stream's configuration. This is typically done once before writing any frames.
    void setCodecParameters(const std::vector<uint8_t>& vps, const std::vector<uint8_t>& sps, const std::vector<uint8_t>& pps);


private:
    // --- Private FFmpeg Handles ---
    AVFormatContext* formatContext = nullptr;
    AVStream* videoStream = nullptr;

    // --- Private Helper Methods ---
    // Writes the MP4 container header to the file. Must be called after
    // setting codec parameters and before writing any packets.
    void writeHeader();

    // Flag to ensure the header is only written once.
    bool headerWritten = false;
};

