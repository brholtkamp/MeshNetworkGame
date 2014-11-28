#include "MeshNode.h"

MeshNode::MeshNode(unsigned short input_listen_port) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener);
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector);

    listen_port = input_listen_port;
    startListening();
    startHandlingMessages();
}

MeshNode::~MeshNode() {
    // Make sure we free up the port we're listening on
    if (isListening()) {
        stopListening();
    }
    if (isHandlingMessages()) {
        stopHandlingMessages();
    }
}

void MeshNode::handleMessages() {
    while (handling_messages) {
        // Consume all messages
        if (incoming_messages.size() != 0) { 
            Message message = incoming_messages.front();

            switch(message.message["type"].asInt()) {
            case PING:
                pong(message);
                break;
            case PONG:
                break;
            default:
                Log("Incoming: " + message.message.toStyledString());
                break;
            }
            
            incoming_messages.pop_front();
        }
        
        if (outgoing_messages.size() != 0) {
            Message message = outgoing_messages.front();
            std::weak_ptr<sf::TcpSocket> socket;
            socket = std::get<2>(message.connection);
            sf::Packet packet;
            packet << message.message.toStyledString();

            if (socket.lock()->send(packet) == sf::Socket::Done) {
                switch(message.message["type"].asInt()) {
                case PING:
                    break;
                case PONG:
                    break;
                default:
                    Log("Outgoing: " + message.message.toStyledString());
                    break;
                }
            } else {
                Log("Failed to send message to " + std::get<0>(message.connection).toString() + ":" + std::to_string(std::get<1>(message.connection)));
                closeConnection(message.connection);
            }

            outgoing_messages.pop_front();
        }
    }

    return;
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
                    Log("Connection from: " + std::get<0>(connections[connections.size() - 1]).toString() + ":" + std::to_string(std::get<1>(connections[connections.size() - 1])));
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

                                Message new_message;
                                new_message.message = message;
                                new_message.id = current_id++;
                                new_message.connection = connections[i];
                                incoming_messages.push_back(new_message);
                           }
                           break;
                        case sf::Socket::Disconnected: 
                        case sf::Socket::Error:
                            Log("Socket disconnection or error from " + std::get<0>(connections[i]).toString() + ":" + std::to_string(std::get<1>(connections[i])));
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
    Log("Listening on " + std::to_string(listen_port));
}

void MeshNode::stopListening() {
    listening = false;
    listening_thread.join();

    selector->remove(*listener);
    Log("Stopped listening");
}

bool MeshNode::isListening() {
    return listening;
}

unsigned short MeshNode::getListeningPort() {
    return listen_port;
}

void MeshNode::startHandlingMessages() {
    current_id = 0;
    handling_messages = true;

    message_handler_thread = std::thread(&MeshNode::handleMessages, this);
    Log("Ready to handle messages");
}

void MeshNode::stopHandlingMessages() {
    handling_messages = false;
    message_handler_thread.join();

    Log("Stopped handling messages");
}

bool MeshNode::isHandlingMessages() {
    return handling_messages;
}

bool MeshNode::broadcast(std::string message) {
    bool success = true;

    for_each(connections.begin(), connections.end(), [&] (Connection connection) { 
        if (checkConnection(connection)) {
            Message new_message;
            Json::Value value;
            value["message"] = message;
            value["timestamp"] = getCurrentTime();
            new_message.connection = connection;
            new_message.id = current_id++;
            new_message.message = value;
            outgoing_messages.push_back(new_message);
        } else {
            Log("Broadcast failed for " + std::get<0>(connection).toString() + ":" + std::to_string(std::get<1>(connection))); 
            success = false;
        }
    });

    return success;
}

bool MeshNode::broadcast(Json::Value message) {
    bool success = true;

    for_each(connections.begin(), connections.end(), [&] (Connection connection) { 
        if (checkConnection(connection)) {
            Message new_message;
            new_message.connection = connection;
            new_message.id = current_id++;
            message["timestamp"] = getCurrentTime();
            new_message.message = message;
            outgoing_messages.push_back(new_message);
        } else {
            Log("Broadcast failed for " + std::get<0>(connection).toString() + ":" + std::to_string(std::get<1>(connection))); 
            success = false;
        }
    });

    return success;
}

bool MeshNode::connectTo(sf::IpAddress address, unsigned short port) {
    std::shared_ptr<sf::TcpSocket> socket = std::shared_ptr<sf::TcpSocket>(new sf::TcpSocket());

    if (socket->connect(address, port) == sf::TcpSocket::Done) {
        socket->setBlocking(false);
        selector->add(*socket);
        connections.push_back(Connection(address, port, socket));
        return true;
    } else {
        Log("Failed to connect to " + address.toString() + ":" + std::to_string(port));
        return false;
    }
}

bool MeshNode::sendTo(sf::IpAddress address, unsigned short port, std::string message) {
    Connection connection;
    bool existingSocket = false;

    for (size_t i = 0; i < connections.size(); i++) {
        if (std::get<0>(connections[i]) == address && std::get<1>(connections[i]) == port) {
            connection = connections[i];
            existingSocket = true;
        }
    }

    if (existingSocket) {
        Message new_message;
        Json::Value value;
        value["message"] = message;
        value["timestamp"] = getCurrentTime();
        new_message.message = value;
        new_message.id = current_id++;
        new_message.connection = connection;
        outgoing_messages.push_back(new_message);
        return true;
    } else {
        Log("There is no current socket connection to " + address.toString() + ":" + std::to_string(port));
        return false;
    }
}

bool MeshNode::sendTo(sf::IpAddress address, unsigned short port, Json::Value message) {
    Connection connection;
    bool existingSocket = false;

    for (size_t i = 0; i < connections.size(); i++) {
        if (std::get<0>(connections[i]) == address && std::get<1>(connections[i]) == port) {
            connection = connections[i];
            existingSocket = true;
        }
    }

    if (existingSocket) {
        Message new_message;
        new_message.message = message;
        new_message.id = current_id++;
        new_message.connection = connection;
        outgoing_messages.push_back(new_message);
        return true;
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

bool MeshNode::checkConnection(Connection connection) {
    if (std::get<2>(connection)->getRemoteAddress() == sf::IpAddress::None) {
        return false;
    } else {
        return true;
    }
}

bool MeshNode::ping(sf::IpAddress address, unsigned short port) {
    Connection connection;
    bool existingSocket = false;

    for (size_t i = 0; i < connections.size(); i++) {
        if (std::get<0>(connections[i]) == address && std::get<1>(connections[i]) == port) {
            connection = connections[i];
            existingSocket = true;
        }
    }

    if (existingSocket) {
        Json::Value value;
        Message ping;
        value["type"] = PING;
        value["ping"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        ping.message = value;
        ping.connection = connection;
        ping.id = current_id++;
        outgoing_messages.push_back(ping);
        
        return true;
    } else {
        Log("There is no current socket connection to " + address.toString() + ":" + std::to_string(port));
        return false;
    }
}

void MeshNode::pong(Message ping) {
    Json::Value value = ping.message;
    value["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    Log(std::to_string(ping.id) + ": from " + std::get<0>(ping.connection).toString() + ":" + std::to_string(std::get<1>(ping.connection)) + " is " + std::to_string(std::stoll(value["pong"].asString()) - std::stoll(value["ping"].asString())) + "ms"); 
}

int MeshNode::numberOfConnections() {
    return connections.size();
}