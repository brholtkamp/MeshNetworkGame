#ifndef __PING_MESSAGE_HANDLER_H__
#define __PING_MESSAGE_HANDLER_H__
#include "MessageHandler.h"
#include "MeshNode.h"

class MessageHandler;

class PingMessageHandler : public MessageHandler {
public:
    PingMessageHandler();

    void handleMessage(sf::IpAddress fromAddress, unsigned short fromPort, std::string type, Json::Value message);
};
#endif // __PING_MESSAGE_HANDLER_H__
