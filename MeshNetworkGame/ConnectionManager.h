#ifndef __CONNECTION_MANAGER_H__
#define __CONNECTION_MANAGER_H__
#include <SFML/Network.hpp>
#include <memory>
#include <vector>
#include <map>

#include "Connection.h"
#include "Log.h"


class ConnectionManager {
public:
    bool addConnection(std::shared_ptr<sf::TcpSocket> socket);
    std::shared_ptr<Connection> getConnection(sf::IpAddress address, unsigned short port);
    bool closeConnection(sf::IpAddress address, unsigned short port);

    std::vector<std::shared_ptr<Connection>>::iterator begin() { return connections.begin(); }
    std::vector<std::shared_ptr<Connection>>::const_iterator begin() const { return connections.begin(); }
    std::vector<std::shared_ptr<Connection>>::iterator end() { return connections.end(); }
    std::vector<std::shared_ptr<Connection>>::const_iterator end() const { return connections.end(); }
private:
    std::vector<std::shared_ptr<Connection>> connections;
};

#endif // __CONNECTION_MANAGER_H__