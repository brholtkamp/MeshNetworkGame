#include "SystemMessageHandler.h"

SystemMessageHandler::SystemMessageHandler() {
    messageTypes.push_back("connection");
    messageTypes.push_back("name");
    messageTypes.push_back("heartbeat");
}

void SystemMessageHandler::handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type) {
    Log(message.toStyledString());
}
