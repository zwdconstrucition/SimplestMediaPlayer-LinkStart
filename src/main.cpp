// Aplication entry point
#include <iostream>
#include <memory>
#include <SDL.h>
#include "MediaPlayer.h"

#ifdef _WIN32
#undef main // Fix SDL2 main redefinition issue on Windows
#endif

int main(int argc, char* argv[]) {
	try {
		MediaPlayer player;

		if (!player.initialize()) {
			std::cerr << "failed to initialize media player" << std::endl;
			return -1;
		}

		// main application loop
		player.run();

		player.cleanup();
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << std::endl;
		return -1;
	}
}