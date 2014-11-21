#include "MeshNode.h"

MeshNode::MeshNode(unsigned short input_listen_port) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener);
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector);

    listen_port = input_listen_port;
    listening = false;
}

MeshNode::~MeshNode() {
    // Make sure we free up the port we're listening on
    if (isListening()) {
        stopListening();
    }
}

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(kListeningTimeout))) {
            if (selector->isReady(*listener)) {
                std::shared_ptr<sf::TcpSocket> new_client = std::make_shared<sf::TcpSocket>(); 
                if (listener->accept(*new_client) == sf::Socket::Done) {
                    // Connection successful
                    connections.push_back(Connection(new_client->getRemoteAddress(), new_client->getRemotePort(), std::move(new_client)));
                    selector->add(*std::get<2>(connections[connections.size() - 1]));
#if DEBUG
                    Log("Connection from: " + std::get<0>(connections[connections.size() - 1]).toString() + ":" + std::to_string(std::get<1>(connections[connections.size() - 1])));
#endif
                } else {
                    Log("Client failed to connect");
                }
            } else {
                for (size_t i = 0; i < connections.size(); i++) { 
                    if (selector->isReady(*std::get<2>(connections[i]))) {
                        sf::Packet packet;
                        std::string buffer;
                        sf::Socket::Status status = std::get<2>(connections[i])->receive(packet);

                        switch(status) {
                        case sf::Socket::Done:
                            if (packet >> buffer) {
                                Json::Reader reader;
                                Json::Value message;
                                reader.parse(buffer, message);
#if DEBUG
                                if (message["message"] == "Ping!") {
                                    Log("Ping!");
                                    sendTo(std::get<0>(connections[i]), std::get<1>(connections[i]), "Pong!");
                                } else if (message["message"] == "Pong!") {
                                    Log("Pong!");
                                    sendTo(std::get<0>(connections[i]), std::get<1>(connections[i]), "Ping!");
                                } else {
                                    Log(message["message"].asString());
                                }
#endif
                            }
                            break;
                        case sf::Socket::Disconnected: 
                        case sf::Socket::Error:
                            closeConnection(connections[i]);
                            break;
                        default:
                            Log("Socket failure from " + std::get<0>(connections[i]).toString() + ":" + std::to_string(std::get<1>(connections[i])));
                            break;
                        }
                    }
                }
            }
        }
    }
    listener->close();
    return;
}

void MeshNode::startListening() {
    // Attempt to find a port to listen on
    while (listener->listen(listen_port) == sf::Socket::Error) {
        listen_port++;
    }
    
    listener->setBlocking(false);  // Nonblocking is necessary to let the thread run async

    // Add the listener to the multiplexer for the listen thread
    selector->add(*listener);

    listening = true;
    listening_thread = std::thread(&MeshNode::listen, this);
}

void MeshNode::stopListening() {
    listening = false;
    listening_thread.join();

    selector->remove(*listener);
}

bool MeshNode::isListening() {
    return listening;
}

unsigned short MeshNode::getListeningPort() {
    return listen_port;
}

bool MeshNode::broadcast(std::string message) {
    sf::Packet packet;
    Json::Value json_message;
    json_message["message"] = message;
    packet << json_message.toStyledString();
    bool success = true;

    for_each(connections.begin(), connections.end(), [&] (Connection connection) { 
        if (std::get<2>(connection)->send(packet) != sf::Socket::Done) {
            success = false;
            Log("Client " + std::get<0>(connection).toString() + " failed to receive the message");
        }
    });

    if (!success) {
        //Check all connections
    }

    return success;
}

bool MeshNode::connectTo(sf::IpAddress address, unsigned short port) {
    std::shared_ptr<sf::TcpSocket> socket = std::shared_ptr<sf::TcpSocket>(new sf::TcpSocket());

    if (socket->connect(address, port) == sf::TcpSocket::Done) {
        selector->add(*socket);
        connections.push_back(Connection(address, port, socket));
        return true;
    } else {
        Log("Failed to connect to " + address.toString() + ":" + std::to_string(port));
        return false;
    }
}

bool MeshNode::sendTo(sf::IpAddress address, unsigned short port, std::string message) {
    std::weak_ptr<sf::TcpSocket> socket;
    bool existingSocket = false;

    for (size_t i = 0; i < connections.size(); i++) {
        if (std::get<0>(connections[i]) == address && std::get<1>(connections[i]) == port) {
            socket = std::get<2>(connections[i]);
            existingSocket = true;
        }
    }

    if (existingSocket) {
        sf::Packet packet;
        Json::Value final_message;
        final_message["message"] = message;
        packet << final_message.toStyledString();

        if (socket.lock()->send(packet) == sf::Socket::Done) {
            return true;
        } else {
            Log("Failed to send message to " + address.toString() + ":" + std::to_string(port));
            return false;
        }
    } else {
        Log("There is no current socket connection to " + address.toString() + ":" + std::to_string(port));
        return false;
    }
}

void MeshNode::closeConnection(Connection connection) {
    Log("Client " + std::get<0>(connection).toString() + ":" + std::to_string(std::get<1>(connection)) + " has disconnected.");
    selector->remove(*std::get<2>(connection));
    std::swap(connection, connections.back());
    connections.pop_back();
}