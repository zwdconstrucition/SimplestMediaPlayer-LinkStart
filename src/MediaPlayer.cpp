// Updated MediaPlayer.cpp with audio support
#include "MediaPlayer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

MediaPlayer::MediaPlayer()
    : window(nullptr)
    , sdlRenderer(nullptr)
    , running(false)
    , playing(false)
    , muted(false)
    , volume(1.0f)
    , videoTexture(nullptr)
    , hasVideo(false)
    , hasAudio(false) {

    // Initialize decoders
    videoDecoder = std::make_unique<VideoDecoder>();
    audioDecoder = std::make_unique<AudioDecoder>();
}

MediaPlayer::~MediaPlayer() {
    cleanup();
}

bool MediaPlayer::initialize() {
    std::cout << "Initializing Media Player..." << std::endl;

    if (!initializeSDL()) {
        std::cerr << "Failed to initialize SDL" << std::endl;
        return false;
    }

    if (!initializeFFmpeg()) {
        std::cerr << "Failed to initialize FFmpeg" << std::endl;
        return false;
    }

    std::cout << "Media Player initialized successfully!" << std::endl;
    return true;
}

bool MediaPlayer::initializeSDL() {
    // Initialize SDL with audio support
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    window = SDL_CreateWindow(
        "FFmpeg Media Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer
    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdlRenderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set renderer color (black background)
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);

    std::cout << "SDL initialized successfully" << std::endl;
    return true;
}

bool MediaPlayer::initializeFFmpeg() {
    // Initialize FFmpeg
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    return true;
}

void MediaPlayer::run() {
    running = true;

    std::cout << "Starting main loop..." << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  O - Open file" << std::endl;
    std::cout << "  SPACE - Play/Pause" << std::endl;
    std::cout << "  S - Stop" << std::endl;
    std::cout << "  M - Mute/Unmute" << std::endl;
    std::cout << "  +/- - Volume Up/Down" << std::endl;
    std::cout << "  LEFT/RIGHT - Seek -/+ 10 seconds" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    while (running) {
        handleEvents();
        render();

        if (playing) {
            syncAudioVideo();
        }

        // Control frame rate
        SDL_Delay(16); // ~60 FPS
    }
}

void MediaPlayer::handleEvents() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                running = false;
                break;

            case SDLK_SPACE:
                // Toggle play/pause
                if (hasVideo || hasAudio) {
                    if (playing) {
                        pause();
                    }
                    else {
                        play();
                    }
                }
                break;

            case SDLK_o:
                // Open file dialog (simple test for now)
                std::cout << "O pressed - Open file" << std::endl;
                // For testing, try to load a sample media file
                loadMediaFile("test.ogg");
                break;

            case SDLK_s:
                // Stop playback
                if (hasVideo || hasAudio) {
                    stop();
                }
                break;

            case SDLK_m:
                // Toggle mute
                if (isMuted()) {
                    unmute();
                }
                else {
                    mute();
                }
                break;

            case SDLK_PLUS:
            case SDLK_EQUALS:
                // Volume up
                setVolume(std::min(1.0f, volume + 0.1f));
                break;

            case SDLK_MINUS:
                // Volume down
                setVolume(std::max(0.0f, volume - 0.1f));
                break;

            case SDLK_LEFT:
                // Seek backward 10 seconds
                if (hasVideo || hasAudio) {
                    double currentTime = getCurrentTime();
                    seekToTime(std::max(0.0, currentTime - 10.0));
                }
                break;

            case SDLK_RIGHT:
                // Seek forward 10 seconds
                if (hasVideo || hasAudio) {
                    double currentTime = getCurrentTime();
                    double duration = getDuration();
                    seekToTime(std::min(duration, currentTime + 10.0));
                }
                break;
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                std::cout << "Window resized to " << event.window.data1 << "x" << event.window.data2 << std::endl;
            }
            break;
        }
    }
}

void MediaPlayer::render() {
    // Clear screen
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(sdlRenderer);

    if (hasVideo) {
        renderVideoFrame();
    }
    else if (hasAudio) {
        renderAudioVisualization();
    }
    else {
        // Draw placeholder rectangle when no media
        SDL_SetRenderDrawColor(sdlRenderer, 64, 64, 64, 255);
        SDL_Rect rect = { WINDOW_WIDTH / 4, WINDOW_HEIGHT / 4, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 };
        SDL_RenderFillRect(sdlRenderer, &rect);

        // Draw text indication
        SDL_SetRenderDrawColor(sdlRenderer, 128, 128, 128, 255);
        SDL_Rect textRect = { WINDOW_WIDTH / 2 - 150, WINDOW_HEIGHT / 2 - 30, 300, 60 };
        SDL_RenderFillRect(sdlRenderer, &textRect);
    }

    // Render controls and time display
    renderControls();

    // Present the back buffer
    SDL_RenderPresent(sdlRenderer);
}

void MediaPlayer::renderVideoFrame() {
    if (!hasVideo || !playing) {
        return;
    }

    uint8_t* rgbData;
    int width, height;

    if (videoDecoder->getNextFrame(&rgbData, width, height)) {
        // Update texture with new frame data
        SDL_UpdateTexture(videoTexture, nullptr, rgbData, width * 3);

        // Calculate display rectangle (maintain aspect ratio)
        int windowWidth, windowHeight;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);

        float videoAspect = (float)width / height;
        float windowAspect = (float)windowWidth / windowHeight;

        SDL_Rect displayRect;
        if (videoAspect > windowAspect) {
            // Video is wider than window
            displayRect.w = windowWidth;
            displayRect.h = (int)(windowWidth / videoAspect);
            displayRect.x = 0;
            displayRect.y = (windowHeight - displayRect.h) / 2;
        }
        else {
            // Video is taller than window
            displayRect.w = (int)(windowHeight * videoAspect);
            displayRect.h = windowHeight;
            displayRect.x = (windowWidth - displayRect.w) / 2;
            displayRect.y = 0;
        }

        // Render video frame
        SDL_RenderCopy(sdlRenderer, videoTexture, nullptr, &displayRect);
    }
}

void MediaPlayer::renderAudioVisualization() {
    if (!hasAudio) {
        return;
    }

    // Simple audio visualization - draw bars based on playback status
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    // Draw background
    SDL_SetRenderDrawColor(sdlRenderer, 32, 32, 64, 255);
    SDL_Rect bgRect = { windowWidth / 4, windowHeight / 4, windowWidth / 2, windowHeight / 2 };
    SDL_RenderFillRect(sdlRenderer, &bgRect);

    // Draw simple audio visualization bars
    if (playing) {
        SDL_SetRenderDrawColor(sdlRenderer, 0, 255, 0, 255);
        int barWidth = 20;
        int barSpacing = 5;
        int numBars = 10;
        int totalWidth = numBars * barWidth + (numBars - 1) * barSpacing;
        int startX = (windowWidth - totalWidth) / 2;
        int baseY = windowHeight / 2 + 50;

        // Create animated bars based on time
        double currentTime = getCurrentTime();
        for (int i = 0; i < numBars; i++) {
            int barHeight = (int)(50 + 30 * sin(currentTime * 2 + i * 0.5));
            SDL_Rect barRect = {
                startX + i * (barWidth + barSpacing),
                baseY - barHeight,
                barWidth,
                barHeight
            };
            SDL_RenderFillRect(sdlRenderer, &barRect);
        }
    }

    // Draw audio file indicator
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
    SDL_Rect audioIconRect = { windowWidth / 2 - 30, windowHeight / 2 - 80, 60, 40 };
    SDL_RenderFillRect(sdlRenderer, &audioIconRect);
}

void MediaPlayer::renderControls() {
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    // Draw control bar at bottom
    SDL_SetRenderDrawColor(sdlRenderer, 40, 40, 40, 200);
    SDL_Rect controlBar = { 0, windowHeight - 60, windowWidth, 60 };
    SDL_RenderFillRect(sdlRenderer, &controlBar);

    // Draw play/pause indicator
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
    if (playing) {
        // Draw pause symbol (two bars)
        SDL_Rect bar1 = { 20, windowHeight - 45, 8, 30 };
        SDL_Rect bar2 = { 32, windowHeight - 45, 8, 30 };
        SDL_RenderFillRect(sdlRenderer, &bar1);
        SDL_RenderFillRect(sdlRenderer, &bar2);
    }
    else {
        // Draw play symbol (triangle)
        SDL_Point points[4] = {
            {20, windowHeight - 45},
            {20, windowHeight - 15},
            {40, windowHeight - 30},
            {20, windowHeight - 45}
        };
        SDL_RenderDrawLines(sdlRenderer, points, 4);
    }

    // Draw volume indicator
    SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
    SDL_Rect volumeRect = { windowWidth - 120, windowHeight - 40, 80, 20 };
    SDL_RenderDrawRect(sdlRenderer, &volumeRect);

    // Fill volume bar
    if (!muted) {
        SDL_SetRenderDrawColor(sdlRenderer, 0, 255, 0, 255);
        SDL_Rect volumeFill = { windowWidth - 118, windowHeight - 38, (int)(76 * volume), 16 };
        SDL_RenderFillRect(sdlRenderer, &volumeFill);
    }

    // Draw mute indicator if muted
    if (muted) {
        SDL_SetRenderDrawColor(sdlRenderer, 255, 0, 0, 255);
        SDL_RenderDrawLine(sdlRenderer, windowWidth - 140, windowHeight - 45, windowWidth - 125, windowHeight - 15);
        SDL_RenderDrawLine(sdlRenderer, windowWidth - 125, windowHeight - 45, windowWidth - 140, windowHeight - 15);
    }

    // Draw progress bar if media is loaded
    if (hasVideo || hasAudio) {
        double currentTime = getCurrentTime();
        double duration = getDuration();

        if (duration > 0) {
            SDL_SetRenderDrawColor(sdlRenderer, 100, 100, 100, 255);
            SDL_Rect progressBg = { 60, windowHeight - 35, windowWidth - 200, 10 };
            SDL_RenderFillRect(sdlRenderer, &progressBg);

            SDL_SetRenderDrawColor(sdlRenderer, 0, 150, 255, 255);
            int progressWidth = (int)((windowWidth - 200) * (currentTime / duration));
            SDL_Rect progressFill = { 60, windowHeight - 35, progressWidth, 10 };
            SDL_RenderFillRect(sdlRenderer, &progressFill);
        }
    }
}

bool MediaPlayer::loadMediaFile(const std::string& filename) {
    std::cout << "Loading media file: " << filename << std::endl;

    // Stop current playback
    stop();

    // Reset state
    hasVideo = false;
    hasAudio = false;

    // Clean up existing textures
    if (videoTexture) {
        SDL_DestroyTexture(videoTexture);
        videoTexture = nullptr;
    }

    // Try to load as video file first
    if (videoDecoder->OpenFile(filename)) {
        hasVideo = true;

        // Create texture for video rendering
        videoTexture = SDL_CreateTexture(
            sdlRenderer,
            SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,
            videoDecoder->getWidth(),
            videoDecoder->getHeight()
        );

        if (!videoTexture) {
            std::cerr << "Failed to create video texture: " << SDL_GetError() << std::endl;
            hasVideo = false;
        }
    }

    // Try to load audio (might be separate or part of video file)
    if (audioDecoder->openFile(filename)) {
        hasAudio = true;
    }

    if (!hasVideo && !hasAudio) {
        std::cerr << "Failed to load media file: " << filename << std::endl;
        return false;
    }

    currentFile = filename;

    std::cout << "Media file loaded successfully!" << std::endl;
    if (hasVideo) std::cout << "  - Video stream found" << std::endl;
    if (hasAudio) std::cout << "  - Audio stream found" << std::endl;
    std::cout << "Press SPACE to play" << std::endl;

    return true;
}

void MediaPlayer::syncAudioVideo() {
    if (!playing) return;

    // Simple sync - just check if both are playing
    if (hasVideo && hasAudio) {
        double videoTime = videoDecoder->getCurrentTime();
        double audioTime = audioDecoder->getCurrentTime();

        // Basic sync check (could be improved)
        double timeDiff = std::abs(videoTime - audioTime);
        if (timeDiff > 0.1) { // 100ms threshold
            // For now, just log the difference
            static int syncLogCount = 0;
            if (++syncLogCount % 300 == 0) { // Log every 5 seconds at 60fps
                std::cout << "A/V sync difference: " << timeDiff << "s" << std::endl;
            }
        }
    }
}

void MediaPlayer::cleanup() {
    std::cout << "Cleaning up..." << std::endl;

    // Stop playback
    stop();

    // Clean up video
    if (videoTexture) {
        SDL_DestroyTexture(videoTexture);
        videoTexture = nullptr;
    }

    if (videoDecoder) {
        videoDecoder->close();
    }

    if (audioDecoder) {
        audioDecoder->close();
    }

    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
    std::cout << "Cleanup complete" << std::endl;
}

// Media control methods
bool MediaPlayer::openFile(const std::string& filename) {
    return loadMediaFile(filename);
}

void MediaPlayer::play() {
    if ((hasVideo || hasAudio) && !playing) {
        std::cout << "Starting playback..." << std::endl;

        if (hasAudio) {
            audioDecoder->startPlayback();
        }

        playing = true;
        std::cout << "Playback started" << std::endl;
    }
}

void MediaPlayer::pause() {
    if ((hasVideo || hasAudio) && playing) {
        std::cout << "Pausing playback..." << std::endl;

        if (hasAudio) {
            audioDecoder->pausePlayback();
        }

        playing = false;
        std::cout << "Playback paused" << std::endl;
    }
}

void MediaPlayer::stop() {
    if (hasVideo || hasAudio) {
        std::cout << "Stopping playback..." << std::endl;

        if (hasAudio) {
            audioDecoder->stopPlayback();
        }

        playing = false;

        // Seek back to beginning
        if (hasVideo && videoDecoder->isFileOpen()) {
            videoDecoder->seekToTime(0.0);
        }
        if (hasAudio && audioDecoder->isFileOpen()) {
            audioDecoder->seekToTime(0.0);
        }

        std::cout << "Playback stopped" << std::endl;
    }
}

bool MediaPlayer::isPlaying() const {
    return playing;
}

bool MediaPlayer::seekToTime(double seconds) {
    if (!hasVideo && !hasAudio) {
        return false;
    }

    std::cout << "Seeking to: " << formatTime(seconds) << std::endl;

    bool success = true;

    if (hasVideo) {
        success &= videoDecoder->seekToTime(seconds);
    }

    if (hasAudio) {
        success &= audioDecoder->seekToTime(seconds);
    }

    return success;
}

double MediaPlayer::getCurrentTime() const {
    if (hasVideo) {
        return videoDecoder->getCurrentTime();
    }
    else if (hasAudio) {
        return audioDecoder->getCurrentTime();
    }
    return 0.0;
}

double MediaPlayer::getDuration() const {
    if (hasVideo) {
        return videoDecoder->getDuration();
    }
    else if (hasAudio) {
        return audioDecoder->getDuration() / (double)AV_TIME_BASE;
    }
    return 0.0;
}

// Volume control methods
void MediaPlayer::setVolume(float newVolume) {
    volume = std::clamp(newVolume, 0.0f, 1.0f);
    std::cout << "Volume set to: " << (int)(volume * 100) << "%" << std::endl;

    // TODO: Implement actual volume control in audio decoder
    // For now, just store the value
}

float MediaPlayer::getVolume() const {
    return volume;
}

void MediaPlayer::mute() {
    if (!muted) {
        muted = true;
        std::cout << "Audio muted" << std::endl;

        // TODO: Implement actual muting in audio decoder
    }
}

void MediaPlayer::unmute() {
    if (muted) {
        muted = false;
        std::cout << "Audio unmuted" << std::endl;

        // TODO: Implement actual unmuting in audio decoder
    }
}

bool MediaPlayer::isMuted() const {
    return muted;
}

// Helper methods
std::string MediaPlayer::formatTime(double seconds) const {
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);

    char buffer[32];
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
    }
    else {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
    }

    return std::string(buffer);
}

bool MediaPlayer::isVideoFile(const std::string& filename) const {
    std::string ext = filename.substr(filename.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov" ||
        ext == "wmv" || ext == "flv" || ext == "webm" || ext == "m4v";
}

bool MediaPlayer::isAudioFile(const std::string& filename) const {
    std::string ext = filename.substr(filename.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg" ||
        ext == "aac" || ext == "m4a" || ext == "wma" || ext == "opus";
}