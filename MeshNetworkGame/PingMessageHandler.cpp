#include "PingMessageHandler.h"

PingMessageHandler::PingMessageHandler() {
    messageTypes.push_back("ping");
    messageTypes.push_back("pong");
    messageTypes.push_back("pongresult");

    sumPings = 0;
    numPings = 0;
}

void PingMessageHandler::handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type) {
    if (type == "ping") {
        message["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        if(!node->sendMessage(fromAddress, fromPort, "pong", message)) {
            Log("Failed to send back ping");
        }
    } else if (type == "pong") {
        sumPings += node->pong(fromAddress, fromPort, message);
        numPings++;
        if (numPings % kPingUpdateRate == 0) {
            node->updatePing(fromAddress, fromPort, static_cast<int>(static_cast<double>(sumPings) / static_cast<double>(numPings)));
        }
    } else if (type == "pongresult") {
        sumPings += message["result"].asUInt64();
        numPings++;
        if (numPings % kPingUpdateRate == 0) {
            node->updatePing(fromAddress, fromPort, static_cast<int>(static_cast<double>(sumPings) / static_cast<double>(numPings)));
        }
    }
}
