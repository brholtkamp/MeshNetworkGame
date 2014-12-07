#include "Connection.h"

Connection::Connection(std::shared_ptr<sf::TcpSocket> _socket): socket(std::move(_socket)) {
    address = socket->getRemoteAddress();
    port = socket->getRemotePort();
}

std::weak_ptr<sf::TcpSocket> Connection::getSocket() {
    return std::weak_ptr<sf::TcpSocket>(socket);
}

sf::IpAddress Connection::getAddress() {
    return address;
}

unsigned short Connection::getPort() {
    return port;
}

bool Connection::isSocketOpen() {
    return socket->getRemoteAddress() == sf::IpAddress::None;
}

std::string Connection::toString() {
    return address.toString() + ":" + std::to_string(port);
}

bool Connection::sendMessage(Message message) {
    sf::Packet packet;
    Json::Value json;
    packet << toJSON(message).toStyledString();
    return socket->send(packet) == sf::Socket::Done;
}