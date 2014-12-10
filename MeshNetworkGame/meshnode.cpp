#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort) {
    listenerInfo.address = sf::IpAddress::getPublicAddress();
    listenerInfo.port = _listeningPort;

    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener());
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector());

    setupHandlers();
    startHandlingMessages();
    startListening();
    startSendingHeartbeats();
}

MeshNode::~MeshNode() {
    if (isListening()) {
        stopListening();
    }

    if (isHandlingMessages()) {
        stopHandlingMessages();
    }

    if (isSendingHeartbeats()) {
        stopSendingHeartbeats();
    }
}

void MeshNode::setupHandlers() {
    pingHandler = std::unique_ptr<PingMessageHandler>(new PingMessageHandler());
    registerMessageHandler(std::move(pingHandler));

    systemHandler = std::unique_ptr<SystemMessageHandler>(new SystemMessageHandler());
    registerMessageHandler(std::move(systemHandler));
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
                    bool connectionClosed = false;
                    for (auto connection = connections.begin(); connection != connections.end() && !connectionClosed; connection++) {
                        if (selector->isReady(*connection->second->socket)) {
                            sf::Packet packet;
                            std::string buffer;
                            sf::Socket::Status status = connection->second->socket->receive(packet);
                            
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
                                        incomingMessage.destination.address = connection->second->user.address;
                                        incomingMessage.destination.port = connection->second->user.port;
                                        incomingMessage.contents = message["contents"];
                                        incomingMessage.type = message["type"].asString();
                                        if (message.isMember("pathway")) {
                                            for (auto path = message["pathway"].begin(); path != message["pathway"].end(); path++) {
                                                ConnectionInfo connection;
                                                connection.address = sf::IpAddress((*path)["address"].asString());
                                                connection.port = static_cast<unsigned short>((*path)["port"].asUInt());
                                                incomingMessage.pathway.push_back(connection);
                                            }
                                        }

                                        incomingMessages.push_back(incomingMessage);
                                    }
                                    break;
                                case sf::Socket::Disconnected:
                                case sf::Socket::Error:
                                    Log("Connection disconnection or error from " + connection->second->user.address.toString() + ":" + std::to_string(connection->second->user.port));
                                    closeConnection(connection->second->user.address, connection->second->user.port);
                                    connectionClosed = true;
                                    break;
                                default:
                                    Log("Connection failure from " + connection->second->user.address.toString() + ":" + std::to_string(connection->second->user.port));
                                    closeConnection(connection->second->user.address, connection->second->user.port);
                                    connectionClosed = true;
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

void MeshNode::handleIncomingMessages() {
    while (handlingMessages) {
        if (incomingMessages.size() != 0) {
            Message message = incomingMessages.front();

            //Log("Got " + message.type);

            std::string username = makeUsernameString(message.destination.address, message.destination.port);
            handlers[message.type]->get()->handleMessage(connections[username]->user.address, connections[username]->user.port, message.type, message.contents);

            incomingMessages.pop_front();
        }
    }

    return;
}

void MeshNode::handleOutgoingMessages() {
    while (handlingMessages) {
        if (outgoingMessages.size() != 0) {
            Message message = outgoingMessages.front();

            //Log("Sending " + message.type);

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
                    pathwayVector.append(pathway);
                }
                buffer["pathway"] = pathwayVector;
            }
            packet << buffer.toStyledString();
            
            std::string username = makeUsernameString(message.destination.address, message.destination.port);
            if (connections[username]->socket->send(packet) != sf::Socket::Done) {
                Log("Failed to send message: " + buffer.toStyledString());
            }

            outgoingMessages.pop_front();
        }
    }

    return;
}

void MeshNode::heartbeat(sf::IpAddress address, unsigned short port) {
    std::string username = makeUsernameString(address, port);
    while (sendingHeartbeats) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - connections[username]->lastHeartbeat).count() > kHeartbeatTimeout) {
            Log("Heartbeat timeout on " + username);
            return;
        } 
        ping(address, port);
        Log(std::to_string(connections[username]->currentPing));

        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatRate));
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
}

void MeshNode::stopListening() {
    listening = false;
    listeningThread.join();
}

bool MeshNode::isListening() {
    return listening;
}

void MeshNode::startHandlingMessages() {
    handlingMessages = true;
    incomingMessagesThread = std::thread(&MeshNode::handleIncomingMessages, this);
    outgoingMessagesThread = std::thread(&MeshNode::handleOutgoingMessages, this);
}

void MeshNode::stopHandlingMessages() {
    handlingMessages = false;
    incomingMessagesThread.join();
    outgoingMessagesThread.join();
}

bool MeshNode::isHandlingMessages() {
    return handlingMessages;
}

void MeshNode::startSendingHeartbeats() {
    sendingHeartbeats = true;
    for (auto& connection : connections) {
        startHeartbeat(connection.second->user.address, connection.second->user.port);
    }
}

void MeshNode::stopSendingHeartbeats() {
    sendingHeartbeats = false;
    for (auto& connection : connections) {
        connection.second->heartbeatThread.join();
    }
}

bool MeshNode::isSendingHeartbeats() {
    return sendingHeartbeats;
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

unsigned int MeshNode::pong(sf::IpAddress address, unsigned short port, Json::Value message) {
    message["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    message["result"] = std::stoll(message["pong"].asString()) - std::stoll(message["ping"].asString());
    if (!sendMessage(address, port, "pongresult", message)) {
        Log("Failed to send result to destination");
    }
    return message["result"].asUInt();
}

void MeshNode::updatePing(sf::IpAddress address, unsigned short port, unsigned long long ping) {
    std::string username = makeUsernameString(address, port);

    connections[username]->sumOfPings += ping;
    connections[username]->countOfPings++;
    connections[username]->lastHeartbeat = std::chrono::system_clock::now();
    if (connections[username]->countOfPings % kPingUpdateRate == 0) {
        connections[username]->currentPing = static_cast<unsigned long long>(static_cast<double>(connections[username]->sumOfPings) / static_cast<double>(connections[username]->countOfPings));
    }
}

void MeshNode::startHeartbeat(sf::IpAddress address, unsigned short port) {
    std::string username = makeUsernameString(address, port);
    if (connectionExists(address, port)) {
        connections[username]->heartbeatThread = std::thread(&MeshNode::heartbeat, this, connections[username]->user.address, connections[username]->user.port);
    }
}

bool MeshNode::sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message) {
    if (connectionExists(address, port)) {
        std::string username = makeUsernameString(address, port);

        Message outgoingMessage;
        if (message.isMember("contents")) {
            outgoingMessage.contents = message["contents"];
        } else {
            outgoingMessage.contents = message;
        }
        outgoingMessage.type = type;
        outgoingMessage.destination.address = connections[username]->user.address;
        outgoingMessage.destination.port = connections[username]->user.port;
        if (!connections[username]->pathway.empty()) {
            outgoingMessage.pathway = connections[username]->pathway;
        }

        outgoingMessages.push_back(outgoingMessage);

        return true;
    }

    Log("Cannot find connection to " + address.toString() + ":" + std::to_string(port) + " to send a message");
    return false;
}

void MeshNode::broadcast(std::string type, Json::Value message) {
    for (auto& connection : connections) {
        Message outgoingMessage;
        outgoingMessage.contents = message;
        outgoingMessage.type = type;
        outgoingMessage.destination.address = connection.second->user.address;
        outgoingMessage.destination.port = connection.second->user.port;
        if (!connection.second->pathway.empty()) {
            outgoingMessage.pathway = connection.second->pathway;
        }

        outgoingMessages.push_back(outgoingMessage);
    }
}

bool MeshNode::registerMessageHandler(std::unique_ptr<MessageHandler> handler) {
    for (auto& handle : handlerReferences) {
        if (handle->getMessageTypes() == handler->getMessageTypes()) {
            Log("Handler already exists");
            return false;
        }
    }

    std::vector<std::string> handles = handler->getMessageTypes(); 
    handler->setMeshNode(std::unique_ptr<MeshNode>(this));
    handlerReferences.push_back(std::move(handler));

    for (auto handleReference = handlerReferences.begin(); handleReference != handlerReferences.end(); handleReference++) {
        for (auto handle : handleReference->get()->getMessageTypes()) {
            std::vector<std::unique_ptr<MessageHandler>>::iterator newHandle = handleReference;
            handlers[handle] = std::move(newHandle);
        }
    }
    return true;
}

void MeshNode::listAllHandlers() {
    Log("Outputting all handles: ");
    for (auto handle : handlers) {
        Log(handle.first);
    }
}

bool MeshNode::addConnection(sf::IpAddress address, unsigned short port, std::unique_ptr<sf::TcpSocket> socket) {
    if (!connections.empty()) {
        if (connectionExists(address, port)) {
            Log("Connection to " + address.toString() + ":" + std::to_string(port) + " already exists");
            return false;
        }
    }
    
    socket->setBlocking(false);
    selector->add(*socket);
    std::unique_ptr<Connection> newConnection = std::unique_ptr<Connection>(new Connection());
    newConnection->user.address = std::move(address);
    newConnection->user.port = std::move(port);
    newConnection->socket = std::move(socket);
    newConnection->sumOfPings = 0;
    newConnection->countOfPings = 0;
    newConnection->currentPing = 0;
    newConnection->lastHeartbeat = std::chrono::system_clock::now();
    connections[makeUsernameString(address, port)] = std::move(newConnection);
    startHeartbeat(address, port);
    Log("Connection to " + address.toString() + ":" + std::to_string(port)+ " successful!");
    outputConnections();
    return true;
}

bool MeshNode::connectionExists(sf::IpAddress address, unsigned short port) {
    if (!connections.empty()) {
        std::string username = makeUsernameString(address, port);
        if (connections.find(username) != connections.end()) {
            if (connections[username]->socket->getRemoteAddress() != sf::IpAddress::None) {
                return true;
            } else {
                closeConnection(address, port);
            }
        }
    }
    return false;
}

bool MeshNode::closeConnection(sf::IpAddress address, unsigned short port) {
    if (!connections.empty()) {
        if (connectionExists(address, port)) {
            std::string username = makeUsernameString(address, port);
            connections[username]->heartbeatThread.join();
            connections[username] = nullptr;
            return true;
        } 
    }

    Log ("Connection to " + address.toString() + ":" + std::to_string(port) + " does not exist or timed out");
    return false;
}

void MeshNode::outputConnections() {
    Log("All connections: ");
    for (auto& connection : connections) {
        Log(connection.second->user.address.toString() + ":" + std::to_string(connection.second->user.port) + " = " + std::to_string(connection.second->currentPing) + "ms");
    }
}

unsigned int MeshNode::numberOfConnections() {
    return connections.size();
}

std::string MeshNode::makeUsernameString(sf::IpAddress address, unsigned short port) {
    return address.toString() + ":" + std::to_string(port);
}
