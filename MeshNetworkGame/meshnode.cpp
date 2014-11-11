#include "MeshNode.h"

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(listeningTimeout))) {
            if (selector->isReady(*listener)) {
                std::shared_ptr<sf::TcpSocket> new_client = std::make_shared<sf::TcpSocket>();
                if (listener->accept(*new_client) == sf::Socket::Done) {
                    std::cout << "Connection from " << new_client->getRemoteAddress().toString() << " on " << new_client->getRemotePort() << std::endl;
                    selector->add(*new_client);
                    clients->push_back(std::move(new_client));
                } else {
                    std::cerr << "Client failed to connect" << std::endl;
                }
            } else {
                for_each(clients->begin(), clients->end(), [&] (std::shared_ptr<sf::TcpSocket> client) { 
                    if (selector->isReady(*client)) {
                        sf::Packet packet;
                        if (client->receive(packet) == sf::Socket::Done) {
                            std::string buffer;
                            if (packet >> buffer) {
                                auto now = std::chrono::system_clock::now();
                                auto current_time = std::chrono::system_clock::to_time_t(now);
                                std::cout << "[" << std::put_time(std::localtime(&current_time), "%c") << "] " << buffer << std::endl;
                            }
                        }
                    }
                });
            }
        }
    }
    listener->close();
    return;
}

MeshNode::MeshNode(unsigned short input_listen_port) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener);
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector);
    clients = std::unique_ptr<std::vector<std::shared_ptr<sf::TcpSocket>>>(new std::vector<std::shared_ptr<sf::TcpSocket>>);

    listen_port = input_listen_port;

    listener->listen(listen_port);
    listener->setBlocking(false);

    selector->add(*listener);
    listening = true;

    listening_thread = std::thread(&MeshNode::listen, this);
}

MeshNode::~MeshNode() {
    listening = false;
    listening_thread.join();
}

bool MeshNode::broadcast(std::string message) {
    sf::Packet packet;
    packet << message;
    bool success = true;

    for_each(clients->begin(), clients->end(), [&] (std::shared_ptr<sf::TcpSocket> client) { 
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
        selector->add(*connection);
        clients->push_back(std::move(connection));
        return true;
    } else {
        return false;
    }
}
