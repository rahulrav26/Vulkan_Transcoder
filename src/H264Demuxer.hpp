#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint> // <--- FIX: Added this include for uint8_t

// Forward declarations for FFmpeg types to avoid including FFmpeg headers
// in a public C++ header file. This is good practice to reduce compile times
// and hide implementation details.
struct AVFormatContext;
struct AVPacket;
struct AVCodecParameters;

// The H264Demuxer class encapsulates all interactions with the FFmpeg libraries
// for the purpose of reading an H.264 video file.
class H264Demuxer {
public:
    // Constructor: Opens the specified video file and prepares for demuxing.
    // Throws a std::runtime_error if the file cannot be opened or is invalid.
    H264Demuxer(const std::string& filepath);

    // Destructor: Cleans up all allocated FFmpeg resources.
    ~H264Demuxer();

    // Reads the next compressed video frame from the file into the provided AVPacket.
    // Returns true if a packet was successfully read, false if the end of the file is reached.
    bool getNextPacket(AVPacket* packet);

    // --- Accessors for Video Stream Information ---

    // Returns the index of the video stream within the container file.
    int getVideoStreamIndex() const { return videoStreamIndex; }

    // Returns a vector containing the raw SPS (Sequence Parameter Set) and
    // PPS (Picture Parameter Set) data, which is required by the Vulkan decoder.
    const std::vector<uint8_t>& getSpsPpsData() const { return sps_pps_data; }

    // Returns a pointer to the FFmpeg codec parameters structure, which contains
    // information like resolution, profile, level, etc.
    const AVCodecParameters* getCodecParameters() const { return codecParameters; }

    // Returns the width of the video.
    int getWidth() const;

    // Returns the height of the video.
    int getHeight() const;

private:
    // --- Private FFmpeg Handles ---
    AVFormatContext* formatContext = nullptr;
    int videoStreamIndex = -1;
    const AVCodecParameters* codecParameters = nullptr;

    // --- Private Data Storage ---
    // Stores the H.264 extradata, which typically contains the SPS and PPS NAL units.
    std::vector<uint8_t> sps_pps_data;
};

