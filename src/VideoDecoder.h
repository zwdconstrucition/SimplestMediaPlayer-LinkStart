#ifndef VIDEODECODER_H
#define VIDEODECODER_H

#include <string>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class VideoDecoder {
private:
	// FFmpeg components
	AVFormatContext* formatContext;
	AVCodecContext* videoCodecContext;
	const AVCodec* videoCodec;
	AVFrame* frame;
	AVFrame* frameRGB;
	AVPacket* packet;
	struct SwsContext* swsContext;

	// Video stream info
	int videoStreamIndex;
	int frameWidth;
	int frameHeight;
	AVPixelFormat pixelFormat;

	// Buffer for RGB conversion
	uint8_t* buffer;
	int bufferSize;

	// Timing info
	double timeBase;
	double frameRate;
	int64_t duration;

	// State
	bool isOpen;
	bool endOfStream;

	// Private methods
	bool findVideoStream();
	bool setupDecoder();
	bool setupScaler();
	void calculateTiming();
	void cleanup();

public:
	VideoDecoder();
	~VideoDecoder();

	// Main interface
	bool OpenFile(const std::string& filename);
	bool getNextFrame(uint8_t** rgbData, int& width, int& height);
	bool seekToTime(double seconds);
	void close();

	// Getters
	bool isFileOpen() const { return isOpen; }
	bool hasEnded() const { return endOfStream; }
	int getWidth() const { return frameWidth; }
	int getHeight() const { return frameHeight; }
	double getDuration() const{ return duration * timeBase; }
	double getFrameRate() const { return frameRate; }
	double getCurrentTime() const;

	// Utility
	void printFileInfo() const;
};

#endif
