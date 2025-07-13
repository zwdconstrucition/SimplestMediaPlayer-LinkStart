// Updated MediaPlayer.h with audio support
#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <string>
#include <memory>
#include <SDL.h>
#include "VideoDecoder.h"
#include "AudioDecoder.h"

class MediaPlayer {
public:
    MediaPlayer();
    ~MediaPlayer();

    bool initialize();
    void run();
    void cleanup();

    // Media control
    bool openFile(const std::string& filename);
    void play();
    void pause();
    void stop();
    bool isPlaying() const;

    // Seeking
    bool seekToTime(double seconds);
    double getCurrentTime() const;
    double getDuration() const;

    // Volume control
    void setVolume(float volume); // 0.0 to 1.0
    float getVolume() const;
    void mute();
    void unmute();
    bool isMuted() const;

private:
    static const int WINDOW_WIDTH = 1280;
    static const int WINDOW_HEIGHT = 720;

    // SDL components
    SDL_Window* window;
    SDL_Renderer* sdlRenderer;

    // Media decoders
    std::unique_ptr<VideoDecoder> videoDecoder;
    std::unique_ptr<AudioDecoder> audioDecoder;

    // Application state
    bool running;
    bool playing;
    bool muted;
    float volume;

    // Video components
    SDL_Texture* videoTexture;
    bool hasVideo;
    bool hasAudio;

    // Current media file
    std::string currentFile;

    // Private methods
    bool initializeSDL();
    bool initializeFFmpeg();
    void handleEvents();
    void render();
    void renderVideoFrame();
    void renderAudioVisualization();
    void renderControls();
    bool loadVideoFile(const std::string& filename);
    bool loadAudioFile(const std::string& filename);
    bool loadMediaFile(const std::string& filename);
    void syncAudioVideo();
    void updateTimeDisplay();

    // Helper methods
    std::string formatTime(double seconds) const;
    bool isVideoFile(const std::string& filename) const;
    bool isAudioFile(const std::string& filename) const;
};

#endif // MEDIAPLAYER_H