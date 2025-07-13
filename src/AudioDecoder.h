// AudioDecoder.h
#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
}

#include <SDL.h>

struct AudioFrame {
    std::vector<uint8_t> data;
    int64_t pts;
    double timestamp;
};

class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();

    bool openFile(const std::string& filename);
    void close();
    bool isFileOpen() const;

    // Audio properties
    int getSampleRate() const;
    int getChannels() const;
    int64_t getDuration() const;
    double getCurrentTime() const;

    // Playback control
    bool startPlayback();
    void stopPlayback();
    void pausePlayback();
    void resumePlayback();
    bool isPlaying() const;

    // Seeking
    bool seekToTime(double seconds);

    // Audio callback for SDL
    static void audioCallback(void* userdata, uint8_t* stream, int len);

private:
    // FFmpeg components
    AVFormatContext* formatContext;
    AVCodecContext* codecContext;
    AVStream* audioStream;
    SwrContext* swrContext;

    // Audio properties
    int sampleRate;
    int channels;
    int64_t duration;
    int audioStreamIndex;

    // SDL Audio
    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;

    // Threading and synchronization
    std::thread decoderThread;
    std::atomic<bool> isDecoding;
    std::atomic<bool> playbackStarted;
    std::atomic<bool> playbackPaused;
    std::atomic<bool> shouldStop;

    // Audio buffer
    std::queue<AudioFrame> audioFrameQueue;
    std::mutex queueMutex;
    std::condition_variable queueCondition;

    // Current playback position
    std::atomic<double> currentTime;

    // Buffer for audio callback
    std::vector<uint8_t> audioBuffer;
    size_t bufferPosition;
    std::mutex bufferMutex;

    // Private methods
    bool initializeDecoder();
    void decodingLoop();
    bool decodeNextFrame();
    void fillAudioBuffer(uint8_t* stream, int len);
    void clearQueue();

    // Audio format conversion
    bool setupResampler();
    bool convertAudioFrame(AVFrame* frame, AudioFrame& audioFrame);
};

#endif // AUDIODECODER_H