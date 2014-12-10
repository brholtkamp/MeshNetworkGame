#include "PingMessageHandler.h"

PingMessageHandler::PingMessageHandler() {
    messageTypes.push_back("ping");
    messageTypes.push_back("pong");
    messageTypes.push_back("pongresult");
}

void PingMessageHandler::handleMessage(sf::IpAddress fromAddress, unsigned short fromPort, std::string type, Json::Value message) {
    if (type == "ping") {
        message["pong"] = "pong";
        if(!node->sendMessage(fromAddress, fromPort, "pong", message)) {
            Log << "Failed to send back ping" << std::endl;
        }
    } else if (type == "pong") {
        node->updatePing(fromAddress, fromPort, node->pong(fromAddress, fromPort, message));
    } else if (type == "pongresult") {
        node->updatePing(fromAddress, fromPort, message["result"].asString());
    }
}
