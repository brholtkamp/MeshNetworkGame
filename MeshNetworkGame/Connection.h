#ifndef __CONNECTION_H__
#define __CONNECTION_H__
#include <SFML/Network.hpp>
#include <string>
#include <memory>

#include "Message.h"

class Connection {
public:
    Connection(std::shared_ptr<sf::TcpSocket> socket);
    std::weak_ptr<sf::TcpSocket> getSocket();
    sf::IpAddress getAddress();
    unsigned short getPort();
    bool isSocketOpen();
    std::string toString();

    bool sendMessage(Message message);
    
private:
    std::shared_ptr<sf::TcpSocket> socket;
    sf::IpAddress address;
    unsigned short port;
};

#endif // __CONNECTION_H__