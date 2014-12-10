#include "Log.h"

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto currentTime = std::chrono::system_clock::to_time_t(now);
    std::stringstream buffer;
    buffer << std::put_time(std::localtime(const_cast<const std::time_t *>(&currentTime)), "%c");
    return buffer.str();
}

void Log(const std::string message) {
    std::cout << "[" << getCurrentTime() << "] " << message << std::endl;
}
