// AudioDecoder.cpp
#include "AudioDecoder.h"
#include <iostream>
#include <algorithm>
#include <cstring>

AudioDecoder::AudioDecoder()
    : formatContext(nullptr)
    , codecContext(nullptr)
    , audioStream(nullptr)
    , swrContext(nullptr)
    , sampleRate(0)
    , channels(0)
    , duration(0)
    , audioStreamIndex(-1)
    , audioDevice(0)
    , isDecoding(false)
    , playbackStarted(false)
    , playbackPaused(false)
    , shouldStop(false)
    , currentTime(0.0)
    , bufferPosition(0) {
}

AudioDecoder::~AudioDecoder() {
    close();
}

bool AudioDecoder::openFile(const std::string& filename) {
    std::cout << "Opening audio file: " << filename << std::endl;

    // Close any existing file
    close();

    // Open input file
    if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Could not open audio file: " << filename << std::endl;
        return false;
    }

    // Find stream information
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // Find audio stream
    audioStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex < 0) {
        std::cerr << "Could not find audio stream" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    audioStream = formatContext->streams[audioStreamIndex];

    // Get audio properties
    sampleRate = audioStream->codecpar->sample_rate;
    channels = audioStream->codecpar->channels;
    duration = formatContext->duration;

    std::cout << "Audio format: " << sampleRate << "Hz, " << channels << " channels" << std::endl;

    if (!initializeDecoder()) {
        close();
        return false;
    }

    if (!setupResampler()) {
        close();
        return false;
    }

    std::cout << "Audio decoder initialized successfully" << std::endl;
    return true;
}

bool AudioDecoder::initializeDecoder() {
    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "Could not find audio decoder" << std::endl;
        return false;
    }

    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return false;
    }

    // Copy codec parameters
    if (avcodec_parameters_to_context(codecContext, audioStream->codecpar) < 0) {
        std::cerr << "Could not copy audio codec parameters" << std::endl;
        return false;
    }

    // Open codec
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open audio codec" << std::endl;
        return false;
    }

    return true;
}

bool AudioDecoder::setupResampler() {
    // Create resampler context
    swrContext = swr_alloc_set_opts(
        nullptr,
        AV_CH_LAYOUT_STEREO,              // Output channel layout
        AV_SAMPLE_FMT_S16,                // Output sample format
        sampleRate,                       // Output sample rate
        codecContext->channel_layout,     // Input channel layout
        codecContext->sample_fmt,         // Input sample format
        codecContext->sample_rate,        // Input sample rate
        0,
        nullptr
    );

    if (!swrContext) {
        std::cerr << "Could not create resampler context" << std::endl;
        return false;
    }

    if (swr_init(swrContext) < 0) {
        std::cerr << "Could not initialize resampler" << std::endl;
        return false;
    }

    return true;
}

bool AudioDecoder::startPlayback() {
    if (playbackStarted) {
        return true;
    }

    // Setup SDL Audio
    SDL_AudioSpec desired;
    desired.freq = sampleRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2; // Force stereo output
    desired.samples = 4096;
    desired.callback = audioCallback;
    desired.userdata = this;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpec, 0);
    if (audioDevice == 0) {
        std::cerr << "Could not open audio device: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "Audio device opened: " << audioSpec.freq << "Hz, "
        << (int)audioSpec.channels << " channels" << std::endl;

    // Clear any existing audio data
    clearQueue();
    audioBuffer.clear();
    bufferPosition = 0;

    // Start decoding thread
    isDecoding = true;
    shouldStop = false;
    playbackPaused = false;
    decoderThread = std::thread(&AudioDecoder::decodingLoop, this);

    // Start SDL audio playback
    SDL_PauseAudioDevice(audioDevice, 0);

    playbackStarted = true;
    std::cout << "Audio playback started" << std::endl;
    return true;
}

void AudioDecoder::stopPlayback() {
    if (!playbackStarted) {
        return;
    }

    std::cout << "Stopping audio playback..." << std::endl;

    // Stop decoding
    shouldStop = true;
    isDecoding = false;
    queueCondition.notify_all();

    // Wait for decoder thread to finish
    if (decoderThread.joinable()) {
        decoderThread.join();
    }

    // Stop SDL audio
    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }

    // Clear buffers
    clearQueue();
    audioBuffer.clear();
    bufferPosition = 0;

    playbackStarted = false;
    playbackPaused = false;
    currentTime = 0.0;

    std::cout << "Audio playback stopped" << std::endl;
}

void AudioDecoder::pausePlayback() {
    if (playbackStarted && !playbackPaused) {
        playbackPaused = true;
        SDL_PauseAudioDevice(audioDevice, 1);
        std::cout << "Audio playback paused" << std::endl;
    }
}

void AudioDecoder::resumePlayback() {
    if (playbackStarted && playbackPaused) {
        playbackPaused = false;
        SDL_PauseAudioDevice(audioDevice, 0);
        std::cout << "Audio playback resumed" << std::endl;
    }
}

void AudioDecoder::decodingLoop() {
    std::cout << "Audio decoding thread started" << std::endl;

    while (isDecoding && !shouldStop) {
        // Check if we need to decode more frames
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (audioFrameQueue.size() > 10) { // Keep buffer from getting too large
                queueCondition.wait(lock, [this] {
                    return audioFrameQueue.size() <= 5 || shouldStop;
                    });
            }
        }

        if (shouldStop) break;

        if (!decodeNextFrame()) {
            // End of file or error
            std::cout << "Audio decoding finished" << std::endl;
            break;
        }
    }

    std::cout << "Audio decoding thread ended" << std::endl;
}

bool AudioDecoder::decodeNextFrame() {
    AVPacket packet;
    av_init_packet(&packet);

    // Read packet from file
    if (av_read_frame(formatContext, &packet) < 0) {
        return false; // End of file
    }

    // Skip non-audio packets
    if (packet.stream_index != audioStreamIndex) {
        av_packet_unref(&packet);
        return true;
    }

    // Send packet to decoder
    if (avcodec_send_packet(codecContext, &packet) < 0) {
        av_packet_unref(&packet);
        return false;
    }

    // Receive decoded frames
    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(codecContext, frame) == 0) {
        // Convert and add to queue
        AudioFrame audioFrame;
        if (convertAudioFrame(frame, audioFrame)) {
            std::lock_guard<std::mutex> lock(queueMutex);
            audioFrameQueue.push(audioFrame);
            queueCondition.notify_one();
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    av_packet_unref(&packet);
    return true;
}

bool AudioDecoder::convertAudioFrame(AVFrame* frame, AudioFrame& audioFrame) {
    // Calculate output sample count
    int outputSamples = swr_get_out_samples(swrContext, frame->nb_samples);
    if (outputSamples <= 0) {
        return false;
    }

    // Allocate output buffer
    int outputBufferSize = av_samples_get_buffer_size(nullptr, 2, outputSamples, AV_SAMPLE_FMT_S16, 0);
    audioFrame.data.resize(outputBufferSize);

    uint8_t* outputBuffer = audioFrame.data.data();

    // Convert audio
    int convertedSamples = swr_convert(
        swrContext,
        &outputBuffer,
        outputSamples,
        (const uint8_t**)frame->data,
        frame->nb_samples
    );

    if (convertedSamples < 0) {
        return false;
    }

    // Resize buffer to actual converted size
    int actualSize = av_samples_get_buffer_size(nullptr, 2, convertedSamples, AV_SAMPLE_FMT_S16, 0);
    audioFrame.data.resize(actualSize);

    // Set timestamp
    audioFrame.pts = frame->pts;
    audioFrame.timestamp = frame->pts * av_q2d(audioStream->time_base);

    return true;
}

void AudioDecoder::audioCallback(void* userdata, uint8_t* stream, int len) {
    AudioDecoder* decoder = static_cast<AudioDecoder*>(userdata);
    decoder->fillAudioBuffer(stream, len);
}

void AudioDecoder::fillAudioBuffer(uint8_t* stream, int len) {
    std::lock_guard<std::mutex> lock(bufferMutex);

    // Clear the stream first
    memset(stream, 0, len);

    if (playbackPaused) {
        return;
    }

    int bytesNeeded = len;
    int streamPos = 0;

    while (bytesNeeded > 0) {
        // If current buffer is empty, get next frame
        if (bufferPosition >= audioBuffer.size()) {
            std::unique_lock<std::mutex> queueLock(queueMutex);
            if (audioFrameQueue.empty()) {
                break; // No more audio data
            }

            AudioFrame frame = audioFrameQueue.front();
            audioFrameQueue.pop();
            queueLock.unlock();

            audioBuffer = std::move(frame.data);
            bufferPosition = 0;
            currentTime = frame.timestamp;

            queueCondition.notify_one();
        }

        // Copy from current buffer
        int availableBytes = audioBuffer.size() - bufferPosition;
        int bytesToCopy = std::min(bytesNeeded, availableBytes);

        memcpy(stream + streamPos, audioBuffer.data() + bufferPosition, bytesToCopy);

        bufferPosition += bytesToCopy;
        streamPos += bytesToCopy;
        bytesNeeded -= bytesToCopy;
    }
}

void AudioDecoder::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!audioFrameQueue.empty()) {
        audioFrameQueue.pop();
    }
}

bool AudioDecoder::seekToTime(double seconds) {
    if (!formatContext) {
        return false;
    }

    int64_t timestamp = seconds * AV_TIME_BASE;

    // Stop current playback temporarily
    bool wasPlaying = isPlaying();
    if (wasPlaying) {
        pausePlayback();
    }

    // Seek in file
    if (av_seek_frame(formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "Failed to seek to time: " << seconds << std::endl;
        return false;
    }

    // Flush decoder
    avcodec_flush_buffers(codecContext);

    // Clear buffers
    clearQueue();
    audioBuffer.clear();
    bufferPosition = 0;
    currentTime = seconds;

    // Resume playback if it was playing
    if (wasPlaying) {
        resumePlayback();
    }

    std::cout << "Seeked to time: " << seconds << "s" << std::endl;
    return true;
}

void AudioDecoder::close() {
    stopPlayback();

    if (swrContext) {
        swr_free(&swrContext);
    }

    if (codecContext) {
        avcodec_free_context(&codecContext);
    }

    if (formatContext) {
        avformat_close_input(&formatContext);
    }

    audioStreamIndex = -1;
    sampleRate = 0;
    channels = 0;
    duration = 0;
    currentTime = 0.0;
}

// Getter methods
bool AudioDecoder::isFileOpen() const {
    return formatContext != nullptr;
}

int AudioDecoder::getSampleRate() const {
    return sampleRate;
}

int AudioDecoder::getChannels() const {
    return channels;
}

int64_t AudioDecoder::getDuration() const {
    return duration;
}

double AudioDecoder::getCurrentTime() const {
    return currentTime.load();
}

bool AudioDecoder::isPlaying() const {
    return playbackStarted && !playbackPaused;
}