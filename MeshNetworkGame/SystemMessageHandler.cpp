#include "SystemMessageHandler.h"

SystemMessageHandler::SystemMessageHandler() {
    messageTypes.push_back("connection");
    messageTypes.push_back("timeout");
    messageTypes.push_back("name");
}

void SystemMessageHandler::handleMessage(sf::IpAddress fromAddress, unsigned short fromPort, std::string type, Json::Value message) {
    Log(message.toStyledString());
}
