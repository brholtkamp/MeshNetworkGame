#ifndef __PING_MESSAGE_HANDLER_H__
#define __PING_MESSAGE_HANDLER_H__
#include "MessageHandler.h"
#include "Log.h"

const int kPingUpdateRate = 10;

class PingMessageHandler : public MessageHandler {
public:
    PingMessageHandler();

    void handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type);

    long long sumPings;
    long long numPings;
};
#endif // __PING_MESSAGE_HANDLER_H__
