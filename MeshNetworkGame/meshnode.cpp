#include "MeshNode.h"

void MeshNode::listen() {
    std::cout << "Listening on " << listen_port << std::endl;
    while (listening) {
        if (selector.wait()) {
            if (selector.isReady(listener)) {
                std::shared_ptr<sf::TcpSocket> client(new sf::TcpSocket());
                if (listener.accept(*client) == sf::Socket::Done) {
                    std::cout << "Connection from " << client->getRemoteAddress().toString() << " on " << client->getRemotePort() << std::endl;
                    clients.push_back(client);
                    selector.add(*client);
                } else {
                    std::cerr << "Client failed to connect" << std::endl;
                }
            } else {
                for_each(clients.begin(), clients.end(), [&] (std::shared_ptr<sf::TcpSocket> client) { 
                    if (selector.isReady(*client)) {
                        sf::Packet packet;
                        if (client->receive(packet) == sf::Socket::Done) {
                            std::string buffer;
                            if (packet >> buffer) {
                                std::cout << buffer << std::endl;
                            }
                        }
                    }
                });
            }
        }
    }
}

MeshNode::MeshNode(unsigned short input_listen_port) {
    listen_port = input_listen_port;
    listener.listen(listen_port);

    selector.add(listener);
    listening = true;

    listening_thread = std::thread(&MeshNode::listen, this);
    listening_thread.detach();
}

MeshNode::~MeshNode() {
        
}

bool MeshNode::broadcast(std::string message) {
    sf::Packet packet;
    packet << message;
    bool success = true;

    for_each(clients.begin(), clients.end(), [&] (std::shared_ptr<sf::TcpSocket> client) { 
        if (client->send(packet) != sf::Socket::Done) {
            success = false;
            std::cerr << "Client " << client->getRemoteAddress().toString() << " failed to receive the message" << std::endl;
        }
    });

    return success;
}

bool MeshNode::connectTo(std::string address, unsigned short port) {
    std::shared_ptr<sf::TcpSocket> connection(new sf::TcpSocket);

    if (connection->connect(address, port) == sf::TcpSocket::Done) {
        selector.add(*connection);
        clients.push_back(connection);
        return true;
    } else {
        return false;
    }
}
