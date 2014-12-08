#include "PingMessageHandler.h"

PingMessageHandler::PingMessageHandler() {
    messageTypes.push_back("ping");
    messageTypes.push_back("pong");

    averagePing = 0.0;
    currentSumOfPings = 0.0;
    currentNumberOfPings = 0;
}

void PingMessageHandler::handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type) {
    if (type == "ping") {
        message["contents"]["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        if(!node->sendMessage(fromAddress, fromPort, "pong", message)) {
            Log("Failed to send back ping");
        }
    } else if (type == "pong") {
        Log(std::to_string(node->pong(message)));
    }
}
