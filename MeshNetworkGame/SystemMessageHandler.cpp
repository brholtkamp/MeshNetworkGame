#include "SystemMessageHandler.h"

SystemMessageHandler::SystemMessageHandler() {
    messageTypes.push_back("connection");
    messageTypes.push_back("name");
    messageTypes.push_back("heartbeat");
}

void SystemMessageHandler::handleMessage(Message message) {
    Log(message.toString());
}
