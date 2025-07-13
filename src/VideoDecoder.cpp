#include "VideoDecoder.h"
#include <iostream>
#include <cstring>

VideoDecoder::VideoDecoder()
	: formatContext(nullptr)
	, videoCodecContext(nullptr)
	, videoCodec(nullptr)
	, frame(nullptr)
	, frameRGB(nullptr)
	, packet(nullptr)
	, swsContext(nullptr)
	, videoStreamIndex(-1)
	, frameWidth(0)
	, frameHeight(0)
	, pixelFormat(AV_PIX_FMT_NONE)
	, buffer(nullptr)
	, bufferSize(0)
	, timeBase(0.0)
	, frameRate(0.0)
	, duration(0)
	, isOpen(false)
	, endOfStream(false) {
}

VideoDecoder::~VideoDecoder() {
	close();
}

bool VideoDecoder::OpenFile(const std::string& filename) {
	std::cout << "Opening video file: " << filename << std::endl;

	// Clean up any exsiting state
	close();

	// Allocate format context
	formatContext = avformat_alloc_context();
	if (!formatContext) {
		std::cerr << "Could not allocate format context" << std::endl;
		return false;
	}

	// Opening input file
	if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) < 0) {
		std::cerr << "Could not open input file: " << filename << std::endl;
		cleanup();
		return false;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(formatContext, nullptr) < 0) {
		std::cerr << "Could not find stream information" << std::endl;
		cleanup();
		return false;
	}

	// Find video stream
	if (!findVideoStream()) {
		std::cerr << "Could not find video stream" << std::endl;
		cleanup();
		return false;
	}

	// Setup decoder
	if (!setupDecoder()) {
		std::cerr << "Could not setup video decoder" << std::endl;
		cleanup();
		return false;
	}

	// Setup scaler
	if (!setupScaler()) {
		std::cerr << "Could not setup video scaler" << std::endl;
		cleanup();
		return false;
	}

	// Calculate timing information
	calculateTiming();

	// Allocate frames and packet
	frame = av_frame_alloc();
	frameRGB = av_frame_alloc();
	packet = av_packet_alloc();

	if (!frame || !frameRGB || !packet) {
		std::cerr << "Could not allocate frames/packet" << std::endl;
		cleanup();
		return false;
	}

	// Allocate RGB buffer
	bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frameWidth, frameHeight, 1);
	buffer = (uint8_t*)av_malloc(bufferSize);
	if (!buffer) {
		std::cerr << "Could not allocate RGB buffer" << std::endl;
		cleanup();
		return false;
	}

	// Setup RGB frame
	av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, frameWidth, frameHeight, 1);

	isOpen = true;
	endOfStream = false;

	std::cout << "Video file opened successfully!" << std::endl;
	printFileInfo();

	return true;
}

bool VideoDecoder::findVideoStream() {
	videoStreamIndex = -1;

	for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
		if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStreamIndex = i;
			break;
		}
	}

	if (videoStreamIndex == -1) {
		return false;
	}

	// Get stream parameters
	AVStream* videoStream = formatContext->streams[videoStreamIndex];
	frameWidth = videoStream->codecpar->width;
	frameHeight = videoStream->codecpar->height;
	pixelFormat = (AVPixelFormat)videoStream->codecpar->format;

	return true;
}

bool VideoDecoder::setupDecoder() {
	// Get codec parameters
	AVCodecParameters* codecParams = formatContext->streams[videoStreamIndex]->codecpar;

	// Find decoder
	videoCodec = avcodec_find_decoder(codecParams->codec_id);
	if (!videoCodec) {
		std::cerr << "Unsupported codec!" << std::endl;
		return false;
	}

	// Allocate codec context
	videoCodecContext = avcodec_alloc_context3(videoCodec);
	if (!videoCodecContext) {
		std::cerr << "Could not allocate codec context" << std::endl;
		return false;
	}

	// Copy codec parameters to context
	if (avcodec_parameters_to_context(videoCodecContext, codecParams) < 0) {
		std::cerr << "Could not copy codec parameters" << std::endl;
		return false;
	}

	// Opend codec 
	if (avcodec_open2(videoCodecContext, videoCodec, nullptr) < 0) {
		std::cerr << "Could not open codec" << std::endl;
		return false;
	}

	return true;
}

bool VideoDecoder::setupScaler() {
	// Create scaling context
	swsContext = sws_getContext(
		frameWidth, frameHeight, pixelFormat,
		frameWidth, frameHeight, AV_PIX_FMT_RGB24,
		SWS_BILINEAR, nullptr, nullptr, nullptr
	);

	if (!swsContext) {
		std::cerr << "Could not create scaling context" << std::endl;
		return false;
	}

	return true;
}

void VideoDecoder::calculateTiming() {
	AVStream* videoStream = formatContext->streams[videoStreamIndex];

	// Time base
	timeBase = av_q2d(videoStream->time_base);

	// Frame rate
	AVRational frameRateRational = videoStream->r_frame_rate;
	frameRate = av_q2d(frameRateRational);

	// Duration
	duration = videoStream->duration;
	if (duration == AV_NOPTS_VALUE && formatContext->duration != AV_NOPTS_VALUE) {
		duration = formatContext->duration * timeBase / AV_TIME_BASE;
	}
}

bool VideoDecoder::getNextFrame(uint8_t** rgbData, int& width, int& height) {
	if (!isOpen || endOfStream) {
		return false;
	}

	while (true) {
		// Read packet
		int ret = av_read_frame(formatContext, packet);
		if (ret < 0) {
			if (ret == AVERROR_EOF) {
				endOfStream = true;
				std::cout << "End of stream reached" << std::endl;
			}
			else {
				std::cerr << "Error reading frame: " << ret << std::endl;
			}
			return false;
		}

		// Check if packet belongs to video stream
		if (packet->stream_index != videoStreamIndex) {
			av_packet_unref(packet);
			continue;
		}

		// Send packet to decoder
		ret = avcodec_send_packet(videoCodecContext, packet);
		if (ret < 0) {
			std::cerr << "Error sending packet to decoder: " << ret << std::endl;
			av_packet_unref(packet);
			continue;
		}

		// Receive frame from decoder
		ret = avcodec_receive_frame(videoCodecContext, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_packet_unref(packet);
			continue;
		}

		// Convert frame to RGB
		sws_scale(swsContext,
				(const uint8_t* const*)frame->data, frame->linesize,
				0, frameHeight,
				frameRGB->data, frameRGB->linesize);

		// Set output parameters
		*rgbData = frameRGB->data[0];
		width = frameWidth;
		height = frameHeight;

		av_packet_unref(packet);
		return true;
	}
}

bool VideoDecoder::seekToTime(double seconds) {
	if (!isOpen) {
		return false;
	}

	int64_t timestamp = (int64_t)(seconds / timeBase);

	int ret = av_seek_frame(formatContext, videoStreamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		std::cerr << "Error seeking to time: " << seconds << std::endl;
		return false;
	}

	// Flush decoder buffers
	avcodec_flush_buffers(videoCodecContext);
	endOfStream = false;

	return true;
}

double VideoDecoder::getCurrentTime() const {
	if (!isOpen || !frame) {
		return 0.0;
	}

	return frame->pts * timeBase;
}

void VideoDecoder::printFileInfo() const {
	if (!isOpen) {
		return;
	}

	std::cout << "=== Video File Information ===" << std::endl;
	std::cout << "Codec: " << videoCodec->name << std::endl;
	std::cout << "Resolution" << frameWidth << "x" << frameHeight << std::endl;
	std::cout << "Frame Rate: " << frameRate << "fps" << std::endl;
	std::cout << "Duration: " << getDuration() << "seconds" << std::endl;
	std::cout << "Pixel Format" << av_get_pix_fmt_name(pixelFormat) << std::endl;
	std::cout << "================================" << std::endl;
}

void VideoDecoder::cleanup() {
	// Free buffers
	if (buffer) {
		av_free(buffer);
		buffer = nullptr;
	}

	// Free frames
	if (frame) {
		av_frame_free(&frame);
	}
	if (frameRGB) {
		av_frame_free(&frameRGB);
	}

	// Free packet
	if (packet) {
		av_packet_free(&packet);
	}

	// Free scaling context
	if (swsContext) {
		sws_freeContext(swsContext);
		swsContext = nullptr;
	}

	// Free codec context
	if (videoCodecContext) {
		avcodec_free_context(&videoCodecContext);
	}

	// Free format context
	if (formatContext) {
		avformat_close_input(&formatContext);
	}

	// Reset state
	videoStreamIndex = -1;
	frameWidth = frameHeight = 0;
	isOpen = false;
	endOfStream = false;
}

void VideoDecoder::close() {
	cleanup();
}

