#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort, std::string _name): listeningPort(_listeningPort), name(_name) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener());
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector());

    while (listener->listen(listeningPort) == sf::TcpListener::Error) {
        listeningPort++;
    }
    localAddress = sf::IpAddress::getPublicAddress();
    listener->setBlocking(false);

    log << "Listening on " << localAddress << ":" << listeningPort << std::endl;

    selector->add(*listener);

    listening = true;
    listenThread = std::thread(&MeshNode::listen, this);
}

MeshNode::~MeshNode() {
    listening = false;
    listenThread.join();

    for (auto connection = connections.begin(); connection != connections.end() && connection->second != nullptr; connection++) {
        /* Threads */
        connection->second->sendingHeartbeats = false;
        connection->second->requestingConnections = false;
        connection->second->heartbeatThread.join();
        connection->second->requestingThread.join();
    }
}

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(kConnectionTimeout))) {
            if (selector->isReady(*listener)) {
                std::unique_ptr<sf::TcpSocket> user = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket()); 
                if (listener->accept(*user) == sf::Socket::Done) {
                    sf::IpAddress address = user->getRemoteAddress();
                    unsigned short port = user->getRemotePort();

                    log << "Connection attempt from " << address << ":" << port << std::endl;

                    if (!submitInfo(std::move(user))) {
                        log << "Connection attempt failed from " << address << ":" << port << std::endl;
                    } 
                } else {
                    log << "User failed to connect" << std::endl;
                }
            } else {
                if (!connections.empty()) {
                    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
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
                                            log << "Failed to parse JSON message" << std::endl;
                                            break;
                                        }

                                        Message incomingMessage;
                                        incomingMessage.contents = message["contents"];
                                        incomingMessage.type = message["type"].asString();
                                        incomingMessage.broadcast = message.isMember("broadcast");
                                        std::vector<std::string> pathway;
                                        for (auto path : message["pathway"]) {
                                            pathway.push_back(path.asString());
                                        }
                                        incomingMessage.pathway = pathway;

                                        handleMessage(incomingMessage);
                                    }
                                    break;
                                case sf::Socket::Disconnected:
                                    log << "Connection to " << connection->first << " was disconnected" << std::endl;
                                    removeConnection(connection->first);
                                    connection = connections.end();
                                    break;
                                case sf::Socket::Error:
                                default:
                                    log << "Connection to " << connection->first << " had an error and timed out" << std::endl;
                                    removeConnection(connection->first);
                                    connection = connections.end();
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

void MeshNode::heartbeat(std::string user) {
    while (connections[user]->sendingHeartbeats) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - pingInfo[user]->lastPing).count() % kConnectionTimeout == 0 && pingInfo[user]->currentPing != 0) {
            log << user << " timed out on heartbeats" << std::endl;
            removeConnection(user);
            return;
        }

        ping(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatRate));
    }

    return;
}

void MeshNode::searchConnections(std::string user) {
    while (connections[user]->requestingConnections) {
        sendConnections(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kUpdateNetworkRate));
    }

    return;
}

void MeshNode::ping(std::string user) {
    Json::Value message;
    message["ping"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    sendMessage(user, "ping", message);
}

void MeshNode::pong(std::string user, Json::Value message) {
    sendMessage(user, "pong", message);
}

void MeshNode::updatePing(std::string user, Json::Value message) {
     message["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
     unsigned long long newPing = std::stoll(message["pong"].asString()) - std::stoll(message["ping"].asString());
     pingInfo[user]->count++;
     pingInfo[user]->sum += newPing;
     pingInfo[user]->lastPing = std::chrono::system_clock::now();

    if (pingInfo[user]->count % kPingUpdateRate == 0) {
        pingInfo[user]->currentPing = static_cast<unsigned long long>(static_cast<double>(pingInfo[user]->sum) / static_cast<double>(pingInfo[user]->count));
    }

     if (pingInfo[user]->count % kPingDumpRate == 0) {
        pingInfo[user]->count = 0;
        pingInfo[user]->sum = 0;
     }
}

bool MeshNode::submitInfo(std::unique_ptr<sf::TcpSocket> user) {
    user->setBlocking(false);
    sf::Packet response;

    Json::Value message;
    message["type"] = "info";
    message["address"] = localAddress.toString();
    message["listeningPort"] = listeningPort;
    message["name"] = name;

    response << message.toStyledString();

    if (user->send(response) == sf::Socket::Done) {
        sf::Packet response;
        
        if (user->receive(response) == sf::Socket::Done) {
            std::string buffer;
            Json::Reader reader;
            Json::Value info;

            response >> buffer;

            reader.parse(buffer, info);

            if (info["type"] == "info") {
                addConnection(std::move(user), info);
            }

            return true;
        }
    }

    log << "Failed to send info to a connecting user" << std::endl;
    return false;
}

bool MeshNode::addConnection(std::unique_ptr<sf::TcpSocket> user, Json::Value userInfo) {
    std::string name = userInfo["name"].asString();
    if (connections.find(name) != connections.end() || connections[name] == nullptr) {

        connections[name] = std::unique_ptr<Connection>(new Connection());
        connections[name]->address = sf::IpAddress(userInfo["address"].asString());
        connections[name]->listeningPort = static_cast<unsigned short>(userInfo["listeningPort"].asUInt());
        connections[name]->personalPort = user->getLocalPort();
        connections[name]->socket = std::move(user);
        connections[name]->socket->setBlocking(false);
        selector->add(*connections[name]->socket);
        pingInfo[name] = std::unique_ptr<PingInfo>(new PingInfo());
        pingInfo[name]->sum = 0;
        pingInfo[name]->count = 0;
        pingInfo[name]->currentPing = 0;
        pingInfo[name]->lastPing = std::chrono::system_clock::now();
        routingTable[name].push_back(name);

        /* Threads */
        connections[name]->sendingHeartbeats = true;
        connections[name]->requestingConnections = true;
        connections[name]->heartbeatThread = std::move(std::thread(&MeshNode::heartbeat, this, name));
        connections[name]->requestingThread = std::move(std::thread(&MeshNode::searchConnections, this, name));

        log << name << " is now connected to this node from " << connections[name]->address << ":" << connections[name]->listeningPort << " on " << connections[name]->personalPort << std::endl;

        return true;
    }

    log << name << " is already connected to this node" << std::endl;
    return false;
}

bool MeshNode::connectionExists(std::string user) {
     if (connections.find(user) != connections.end()) {
         return true;
     }

     return false;
 }

void MeshNode::removeConnection(std::string user) {
     if (connections.find(user) != connections.end()) {
        /* Threads */
        connections[user]->sendingHeartbeats = false;
        connections[user]->heartbeatThread.join();
        connections[user]->requestingConnections = false;
        connections[user]->requestingThread.join();
        connections.erase(user);
        pingInfo.erase(user);
        return;
     }

     log << user << " was not in the list of connections to be removed" << std::endl;
 }

bool MeshNode::connectTo(sf::IpAddress address, unsigned short port) {
     std::unique_ptr<sf::TcpSocket> user = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket());
     if (address != localAddress || port != listeningPort) {
         if (user->connect(address, port, sf::milliseconds(kConnectionTimeout)) == sf::Socket::Done) {
            if (submitInfo(std::move(user))) {
                return true;
            }
         }
     }

     return false;
 }

void MeshNode::sendMessage(std::string user, std::string type, Json::Value message) {
    if (connectionExists(user)) {
        Message outgoingMessage;
        outgoingMessage.contents = message;
        outgoingMessage.pathway.push_back(name);
        for (auto path : routingTable[user]) {
            outgoingMessage.pathway.push_back(path);
        }
        outgoingMessage.type = type;
        outgoingMessage.broadcast = message.isMember("broadcast");
        
        Json::Value finalMessage;
        finalMessage["contents"] = outgoingMessage.contents;
        finalMessage["type"] = outgoingMessage.type;
        if (outgoingMessage.broadcast) {
            finalMessage["broadcast"] = "broadcast";
        }
        for (auto path : outgoingMessage.pathway) {
            finalMessage["pathway"].append(path);
        }
        
        sf::Packet packet;
        packet << finalMessage.toStyledString();

        if (connectionExists(user)) {
            if (connections[user]->socket->send(packet) != sf::Socket::Done) {
                log << "Failed to send a message to " << user << ":" << std::endl << finalMessage.toStyledString() << std::endl;
            }
        }
        return;
    }

    log << "Connection to " << user << " does not exist to send " << type << " message" << std::endl;
 }

void MeshNode::broadcast(std::string type, Json::Value message) {
     message["broadcast"] = "broadcast";
     for (auto connection = connections.begin(); connection != connections.end(); connection++) {
         sendMessage(connection->first, type, message);
     }
 }

void MeshNode::handleMessage(Message message) {
    // First to receive this message or it's only to me
    handleContent(message);
    /*
    if (message.pathway.size() == 1 || message.history.size() == 0) {
        // Handle message
    } else if (message.pathway.size() == (message.history.size() - 1)) {
        // Handle message
    } else if (message.broadcast) {
        // Handle message
        // Broadcast to all not in history
    } else {
        // Forward message
    }*/
}

void MeshNode::handleContent(Message message) {
    if (message.type == "ping") {
        pong(message.pathway[0], message.contents);
    } else if (message.type == "pong") {
        updatePing(message.pathway[0], message.contents);
    } else if (message.type == "sendConnections") {
        receiveConnections(message.pathway[0], message.contents);
    } else if (message.type == "requestConnection") {
        std::vector<std::string> users;
        for (auto user : message.contents["users"]) {
            users.push_back(user.asString());
        }
        sendConnections(message.pathway[0], std::move(users));
    } else if (message.type == "responseConnections") {
        std::vector<sf::IpAddress> addresses;
        std::vector<unsigned short> ports;

        for (auto user : message.contents["users"]) {
            addresses.push_back(sf::IpAddress(user["address"].asString()));
            ports.push_back(static_cast<unsigned short>(user["port"].asUInt()));
        }

        while (!addresses.empty() && !ports.empty()) {
            if (connectTo(addresses.back(), ports.back())) {
                addresses.pop_back();
                ports.pop_back();
            } 
        }
    } else {
        log << message.toString() << std::endl;
    }
} 

void MeshNode::sendConnections(std::string user) {
    Json::Value message;
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        message["users"].append(connection->first);
    }
    
    sendMessage(user, "sendConnections", message);
}

void MeshNode::receiveConnections(std::string user, Json::Value message) {
    std::vector<std::string> users;
    for (auto unknownUser : message["users"]) {
        if (!connectionExists(unknownUser.asString()) && unknownUser.asString() != name) {
            users.push_back(unknownUser.asString());
        }
    }

    if (!users.empty()) {
        requestConnections(user, std::move(users));
    }
}

void MeshNode::requestConnections(std::string user, std::vector<std::string> requestedUsers) {
    Json::Value message;
    for (auto requestedUser : requestedUsers) {
        message["users"].append(requestedUser);
    }

    sendMessage(user, "requestConnection", message);
}

void MeshNode::sendConnections(std::string user, std::vector<std::string> requestedUsers) {
    Json::Value message;
    for (auto requestedUser : requestedUsers) {
        Json::Value newUser;
        newUser["address"] = connections[requestedUser]->address.toString();
        newUser["port"] = connections[requestedUser]->listeningPort;
        message["users"].append(newUser);
    }

    sendMessage(user, "responseConnections", message);
}

void MeshNode::listConnections() {
     for (auto connection = connections.begin(); connection != connections.end(); connection++) {
         log << connection->first << ": " << connection->second->address << ":" << connection->second->listeningPort << std::endl;
     }
 }