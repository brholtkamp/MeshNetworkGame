#ifndef __PING_MESSAGE_HANDLER_H__
#define __PING_MESSAGE_HANDLER_H__
#include "MessageHandler.h"
#include "Log.h"

class PingMessageHandler : public MessageHandler {
public:
    PingMessageHandler();

    void handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type);
};
#endif // __PING_MESSAGE_HANDLER_H__
