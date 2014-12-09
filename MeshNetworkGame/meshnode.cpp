#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort, std::string _username) {
    listenerInfo.address = sf::IpAddress::getPublicAddress();
    listenerInfo.port = _listeningPort;

    if (_username == kDefaultName) {
        listenerInfo.name = sf::IpAddress::getPublicAddress().toString() + std::to_string(_listeningPort);
    } else {
        listenerInfo.name = _username;
    }


    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener());
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector());

    startHandlingMessages();
    startListening();
}

MeshNode::~MeshNode() {
    if (isListening()) {
        stopListening();
    }

    if (isHandlingMessages()) {
        stopHandlingMessages();
    }
}

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(kListeningTimeout))) {
            if (selector->isReady(*listener)) {
                std::unique_ptr<sf::TcpSocket> newClient = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket()); 
                if (listener->accept(*newClient) == sf::Socket::Done) {
                    // Connection established 
                    Log("Connection attempt from " + newClient->getRemoteAddress().toString() + ":" + std::to_string(newClient->getRemotePort()));
                    sf::IpAddress address = newClient->getRemoteAddress();
                    unsigned short port = newClient->getRemotePort();
                    if (!addConnection(address, port, std::move(newClient))) {
                        Log("Failed to store connection");
                    }
                } else {
                    Log("Client failed to connect");
                }
            } else {
                if (!connections.empty()) {
                    for (unsigned int i = 0; i < connections.size(); i++) {
                        if (selector->isReady(*connections[i]->socket)) {
                            sf::Packet packet;
                            std::string buffer;
                            sf::Socket::Status status = connections[i]->socket->receive(packet);
                            
                            switch (status) {
                                case sf::Socket::Done:
                                    if (packet >> buffer) {
                                        Json::Reader reader;
                                        Json::Value message;
                                        if (!reader.parse(buffer, message)) {
                                            Log("Failed to parse JSON message");
                                            break;
                                        } 

                                        Message incomingMessage;
                                        incomingMessage.destination.address = connections[i]->user.address;
                                        incomingMessage.destination.port = connections[i]->user.port;
                                        incomingMessage.contents = message["contents"];
                                        incomingMessage.type = message["type"].asString();
                                        if (message.isMember("pathway")) {
                                            for (auto path = message["pathway"].begin(); path != message["pathway"].end(); path++) {
                                                ConnectionInfo connection;
                                                connection.address = sf::IpAddress((*path)["address"].asString());
                                                connection.port = static_cast<unsigned short>((*path)["port"].asUInt());
                                                connection.name = (*path)["name"].asString();
                                                incomingMessage.pathway.push_back(connection);
                                            }
                                        }

                                        incomingMessages.push_back(incomingMessage);
                                    }
                                    break;
                                case sf::Socket::Disconnected:
                                case sf::Socket::Error:
                                    Log("Connection disconnection or error from " + connections[i]->user.address.toString() + ":" + std::to_string(connections[i]->user.port));
                                    closeConnection(connections[i]->user.address, connections[i]->user.port);
                                    break;
                                default:
                                    Log("Connection failure from " + connections[i]->user.address.toString() + ":" + std::to_string(connections[i]->user.port));
                                    closeConnection(connections[i]->user.address, connections[i]->user.port);
                                    break;
                            }
                        }
                    }
                }
            }
        }
    }

    listener->close();
    return;
}

void MeshNode::handleMessage() {
    while (handlingMessages) {
        if (incomingMessages.size() != 0) {
            Message message = incomingMessages.get_next();
 
            bool handleFound = false;
            for (auto handler = handlers.begin(); handler != handlers.end(); handler++) {
                for (auto handle : handler->get()->getMessageTypes()) {
                    if (message.type == handle) {
                        for (auto connection = connections.begin(); connection != connections.end(); connection++) {
                            handler->get()->handleMessage(message.contents, connection->get()->user.address, connection->get()->user.port, message.type);
                            handleFound = true;
                        }
                     }
                }
            }

            if (!handleFound) {
                Log("Unknown message of type " + message.type + ":" + message.contents.toStyledString());
            }
        }

        if (outgoingMessages.size() != 0) {
            Message message = outgoingMessages.get_next();

            sf::Packet packet;
            Json::Value buffer;
            buffer["type"] = message.type;
            buffer["contents"] = message.contents;
            if (!message.pathway.empty()) {
                Json::Value pathwayVector;
                for (auto path = message.pathway.begin(); path != message.pathway.end(); path++) {
                    Json::Value pathway;
                    pathway["address"] = path->address.toString();
                    pathway["port"] = std::to_string(path->port);
                    pathway["name"] = path->name;
                    pathwayVector.append(pathway);
                }
                buffer["pathway"] = pathwayVector;
            }
            packet << buffer.toStyledString();
            
            if (connectionExists(message.destination.address, message.destination.port)) {
                for (auto connection = connections.begin(); connection != connections.end(); connection++) {
                    if (connection->get()->user.address == message.destination.address && connection->get()->user.port == message.destination.port) {
                        if (connection->get()->socket->send(packet) != sf::Socket::Done) {
                            Log("Failed to send message: " + buffer.toStyledString());
                        }
                    }
                }   
            }
        }
    }

    return;
}

void MeshNode::startListening() {
    while (listener->listen(listenerInfo.port) == sf::Socket::Error) {
        listenerInfo.port++;
    }
    listener->setBlocking(false);

    selector->add(*listener);

    listening = true;
    listeningThread = std::thread(&MeshNode::listen, this);
    Log("Listening on " + listenerInfo.address.toString() + ":" + std::to_string(listenerInfo.port));
}

void MeshNode::stopListening() {
    listening = false;
    listeningThread.join();
    Log("Stopped listening");
}

bool MeshNode::isListening() {
    return listening;
}

void MeshNode::startHandlingMessages() {
    handlingMessages = true;

    messageHandlingThread = std::thread(&MeshNode::handleMessage, this);
    Log("Started handling messages");
}

void MeshNode::stopHandlingMessages() {
    handlingMessages = false;
    messageHandlingThread.join();
    Log("Stopped handling messages");
}

bool MeshNode::isHandlingMessages() {
    return handlingMessages;
}

bool MeshNode::connectTo(sf::IpAddress address, unsigned short port) {
    std::unique_ptr<sf::TcpSocket> socket = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket);

    if (socket->connect(address, port) == sf::TcpSocket::Done) {
        if (addConnection(address, port, std::move(socket))) {
            return true;
        } else {
            Log("Failed to store connection to " + address.toString() + ":" + std::to_string(port));
            return false;
        }
    } else {
        Log("Failed to connect to " + address.toString() + ":" + std::to_string(port));
        return true;
    }
}

void MeshNode::ping(sf::IpAddress address, unsigned short port) {
    Json::Value message;
    message["ping"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    if (!sendMessage(address, port, "ping", message)) {
        Log("Failed to ping" + address.toString() + ":" + std::to_string(port));
    }
}

long long MeshNode::pong(Json::Value response) {
    return std::stoll(response["pong"].asString()) - std::stoll(response["ping"].asString());
}

bool MeshNode::sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message) {
    if (connectionExists(address, port)) {
        for (auto connection = connections.begin(); connection != connections.end(); connection++) {
            if (connection->get()->user.address == address && connection->get()->user.port == port) {
                Message outgoingMessage;
                if (message.isMember("contents")) {
                    outgoingMessage.contents = message["contents"];
                } else {
                    outgoingMessage.contents = message;
                }
                outgoingMessage.type = type;
                outgoingMessage.destination.address = connection->get()->user.address;
                outgoingMessage.destination.port = connection->get()->user.port;
                if (!connection->get()->pathway.empty()) {
                    outgoingMessage.pathway = connection->get()->pathway;
                }

                outgoingMessages.push_back(outgoingMessage);

                return true;
            }
        }
    }

    Log("Cannot find connection to " + address.toString() + ":" + std::to_string(port) + " to send a message");
    return false;
}

void MeshNode::broadcast(std::string type, Json::Value message) {
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        Message outgoingMessage;
        outgoingMessage.contents = message;
        outgoingMessage.type = type;
        outgoingMessage.destination.address = connection->get()->user.address;
        outgoingMessage.destination.port = connection->get()->user.port;
        if (!connection->get()->pathway.empty()) {
            outgoingMessage.pathway = connection->get()->pathway;
        }

        outgoingMessages.push_back(outgoingMessage);
    }
}

bool MeshNode::registerMessageHandler(std::unique_ptr<MessageHandler> handler) {
    for (auto handle = handlers.begin(); handle != handlers.end(); handle++) {
        if (handle->get()->getMessageTypes() == handler->getMessageTypes()) {
            return false;
        }
    }

    handler->setMeshNode(std::unique_ptr<MeshNode>(this));
    handlers.push_back(std::move(handler));
    return true;
}

void MeshNode::listAllHandlers() {
    Log("Outputting all handles: ");
    for (auto handler = handlers.begin(); handler != handlers.end(); handler++) {
        for (auto handle : handler->get()->getMessageTypes()) {
            Log(handle);
        }
    }
}

bool MeshNode::addConnection(sf::IpAddress address, unsigned short port, std::unique_ptr<sf::TcpSocket> _socket) {
    if (!connections.empty()) {
        for (auto connection = connections.begin(); connection != connections.end(); connection++) {
            if (connection->get()->socket == _socket) {
                Log("Connection to " + connection->get()->user.address.toString() + ":" + std::to_string(connection->get()->user.port) + " already exists");
                return false;
            }
        }
    }
    
    _socket->setBlocking(false);
    selector->add(*_socket);
    std::unique_ptr<Connection> newConnection = std::unique_ptr<Connection>(new Connection());
    newConnection->user.address = std::move(address);
    newConnection->user.port = std::move(port);
    newConnection->socket = std::move(_socket);
    connections.push_back(std::move(newConnection));
    Log("Connection to " + connections.back()->user.address.toString() + ":" + std::to_string(connections.back()->user.port)+ " successful!");
    return true;
}

bool MeshNode::connectionExists(sf::IpAddress address, unsigned short port) {
    if (!connections.empty()) {
        for (auto connection = connections.begin(); connection != connections.end(); connection++) {
            if (connection->get()->user.address == address && connection->get()->user.port == port) {
                if (connection->get()->socket->getRemoteAddress() != sf::IpAddress::None) {
                    return true;
                } else {
                    closeConnection(address, port);
                }
            }
        }
    } 
    return false;
}

bool MeshNode::closeConnection(sf::IpAddress address, unsigned short port) {
    for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
        if (connection->get()->user.address == address && connection->get()->user.port == port) {
            selector->remove(*connection->get()->socket);
            connections.erase(connection);
            return true;
        }
    }

    Log ("Connection to " + address.toString() + ":" + std::to_string(port) + " does not exist");
    return false;
}

void MeshNode::outputConnections() {
    Log("All connections: ");
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        Log(connection->get()->user.address.toString() + ":" + std::to_string(connection->get()->user.port));
    }
}

unsigned int MeshNode::numberOfConnections() {
    return connections.size();
}
