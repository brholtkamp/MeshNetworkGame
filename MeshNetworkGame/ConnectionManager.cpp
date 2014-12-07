#include "ConnectionManager.h"

bool ConnectionManager::addConnection(std::shared_ptr<sf::TcpSocket> socket) {
    if (!connections.empty()) {
        for (auto connection : connections) {
            if (connection->getSocket().lock() == socket) {
                Log("Connection to " + socket->getRemoteAddress().toString() + ":" + std::to_string(socket->getRemotePort()) + " already exists");
                return false;
            }
        }
    }
    
    connections.push_back(std::make_shared<Connection>(socket));
    Log("Connection from " + connections.back().get()->toString() + " successful!");
    return true;
}

std::shared_ptr<Connection> ConnectionManager::getConnection(sf::IpAddress address, unsigned short port) {
    for (auto connection : connections) {
        if (connection->getAddress() == address && connection->getPort() == port) {
            return connection;
        }
    }

    Log ("Connection to " + address.toString() + ":" + std::to_string(port) + " does not exist");
    return nullptr;
}

bool ConnectionManager::closeConnection(sf::IpAddress address, unsigned short port) {
    for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
        if (connection->get()->getAddress() == address && connection->get()->getPort() == port) {
            connections.erase(connection);
            return true;
        }
    }

    Log ("Connection to " + address.toString() + ":" + std::to_string(port) + " does not exist");
    return false;
}