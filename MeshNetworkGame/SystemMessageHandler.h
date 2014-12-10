#ifndef __SYSTEM_MESSAGE_HANDLER_H__
#define __SYSTEM_MESSAGE_HANDLER_H__
#include "MessageHandler.h"
#include "MeshNode.h"

class MessageHandler;

class SystemMessageHandler : public MessageHandler {
public:
    SystemMessageHandler();

    void handleMessage(sf::IpAddress fromAddress, unsigned short fromPort, std::string type, Json::Value message);
};
#endif // __SYSTEM_MESSAGE_HANDLER_H__
