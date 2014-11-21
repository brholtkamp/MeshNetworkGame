#include "Log.h"

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto current_time = std::chrono::system_clock::to_time_t(now);
    std::stringstream buffer;
    buffer << "[" << std::put_time(std::localtime(&current_time), "%c") << "]";
    return buffer.str();
}

void Log(const std::string message) {
    std::cout << getCurrentTime() << " " << message << std::endl;
}