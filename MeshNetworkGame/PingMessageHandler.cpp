#include "PingMessageHandler.h"

PingMessageHandler::PingMessageHandler() {
    messageTypes.push_back("ping");
    messageTypes.push_back("pong");
}

void PingMessageHandler::handleMessage(Message message) {
    Log(message.toString());
}
