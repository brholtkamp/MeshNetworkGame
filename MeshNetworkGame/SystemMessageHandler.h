#ifndef __SYSTEM_MESSAGE_HANDLER_H__
#define __SYSTEM_MESSAGE_HANDLER_H__
#include "MessageHandler.h"
#include "Log.h"

class SystemMessageHandler : public MessageHandler {
public:
    SystemMessageHandler();

    void handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type);
};
#endif // __SYSTEM_MESSAGE_HANDLER_H__
