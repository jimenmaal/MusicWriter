#define SDL_MAIN_HANDLED
#include "MainWindow.h"
#include <SDL.h>
#include <fstream>
#include <string>
#include <windows.h>

static void logMessage(const char* message) {
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(nullptr, buffer, MAX_PATH) == 0) {
        return;
    }
    std::string path(buffer);
    auto pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        path = path.substr(0, pos + 1);
    } else {
        path = "";
    }
    path += "MusicWriter.log";

    std::ofstream out(path, std::ios::app);
    if (out.is_open()) {
        out << message << "\n";
    }
}

int main(int argc, char* argv[]) {
    logMessage("Starting MusicWriter");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        logMessage("SDL_Init failed");
        return 1;
    }

    logMessage("SDL initialized");
    MainWindow window;
    if (!window.init()) {
        logMessage("Window init failed");
        SDL_Quit();
        return 1;
    }

    logMessage("Window initialized");
    window.run();
    logMessage("Window loop exited");
    SDL_Quit();
    return 0;
}
