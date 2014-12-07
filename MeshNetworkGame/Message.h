#ifndef __MESSAGE_H__
#define __MESSAGE_H__
#include <json/json.h>
#include <SFML/Network.hpp>
#include <string>
#include <sstream>
#include <vector>

#include "Log.h"

struct Path {
    sf::IpAddress address;
    unsigned short port;
};

class Message {
public:
    Message();
    Message(sf::IpAddress address, unsigned short port, std::string type, Json::Value message);
    ~Message();

    void setAddress(sf::IpAddress address);
    sf::IpAddress getAddress();
    void setPort(unsigned short port);
    unsigned short getPort();
    void setType(std::string type);
    std::string getType();
    void setMessage(Json::Value message);
    Json::Value getMessage();

    bool addToPathway(sf::IpAddress address, unsigned short port);
    bool removeFromPathway(sf::IpAddress address, unsigned short port);
    std::vector<Path> getPathway();
    Path getFrontOfPathway();

    std::string toString();

private:
    Path destination;
    std::vector<Path> pathway;
    std::string type;
    Json::Value message;
};

Json::Value toJSON(Message message);
Message fromJSON(Json::Value message);

#endif  // __MESSAGE_H__
