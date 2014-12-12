#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort, std::string _name): listeningPort(_listeningPort), name(_name) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener);
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector);

    listener->setBlocking(true);

    while (listener->listen(listeningPort) == sf::TcpListener::Error) {
        listeningPort++;
    }
    localAddress = sf::IpAddress::getPublicAddress();
    log << "Listening on " << localAddress << ":" << listeningPort << std::endl;

    selector->add(*listener);

    listening = true;
    listenerThread = std::thread(&MeshNode::listen, this);
}

MeshNode::~MeshNode() {
    listening = false;
    listenerThread.join();

    for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
        connection->second->timedOut = true;
        connection->second->heartbeatThread.join();
        connection->second->connectionRequestThread.join();
    }
}

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(10))) {
            if (selector->isReady(*listener)) {
                std::unique_ptr<sf::TcpSocket> client = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket());
                if (listener->accept(*client) == sf::Socket::Done) {
                    log << "Got new connection attempt from " << client->getRemoteAddress() << ":" << client->getRemotePort() << std::endl;
                    if (!addConnection(std::move(client))) {
                        log << "Failed to get new connection" << std::endl;    
                    }
                }
            } else {
                if (!connections.empty()) {
                    auto end = connections.end();
                    for (auto connection = connections.begin(); connection != end && !connections.empty(); ++connection) {
                        if (selector->isReady(*connection->second->socket)) {
                            sf::Packet packet;
                            sf::Socket::Status status = connection->second->socket->receive(packet);

                            if (status == sf::Socket::Done) {
                                std::string buffer;
                                if (packet >> buffer) {
                                    Json::Reader reader;
                                    Json::Value message;
                                    reader.parse(buffer, message);

                                    Message incomingMessage;
                                    incomingMessage.contents = message["contents"];
                                    incomingMessage.type = message["type"].asString();
                                    incomingMessage.broadcast = message.isMember("broadcast");
                                    std::vector<std::string> pathway;
                                    for (auto path : message["pathway"]) {
                                        pathway.push_back(path.asString());
                                    }
                                    incomingMessage.pathway = pathway;

                                    handleMessage(incomingMessage, connection->first);
                                } else {
                                    log << "Unable to parse message" << std::endl;
                                }
                            } else {
                                log << connection->first << " has disconnected" << std::endl;
                                selector->remove(*connection->second->socket);
                                connection->second->timedOut = true;
                                connection->second->heartbeatThread.join();
                                connection->second->connectionRequestThread.join();
                                connections.erase(connection);
                                if (!connections.empty()) {
                                    connection = connections.begin();
                                    end = connections.end();
                                } else {
                                    break;
                                }
                            }
                        } else if (connection->second->timedOut) {
                            log << connection->first << " has timed out" << std::endl;
                            selector->remove(*connection->second->socket);
                            connection->second->heartbeatThread.join();
                            connection->second->connectionRequestThread.join();
                            connections.erase(connection);
                            if (!connections.empty()) {
                                connection = connections.begin();
                                end = connections.end();
                            } else {
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
    while (!connections[user]->timedOut) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - connections[user]->ping.lastPing).count() % kConnectionTimeout == 0 && connections[user]->ping.currentPing != 0) {
            log << user << " timed out on heartbeats" << std::endl;
            connections[user]->timedOut = true;
            return;
        }

        ping(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatRate));
    }

    return;
}

void MeshNode::searchConnections(std::string user) {
    while (!connections[user]->timedOut) {
        sendConnections(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kUpdateNetworkRate));
    }

    return;
}

void MeshNode::ping(std::string user) {
    Json::Value message;
    message["ping"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    Message outgoingMessage = craftMessage(user, "ping", message);
    sendMessage(user, outgoingMessage);
}

void MeshNode::pong(std::string user, Json::Value message) {
    Message outgoingMessage = craftMessage(user, "pong", message);
    sendMessage(user, outgoingMessage);
}

void MeshNode::updatePing(std::string user, Json::Value message) {
    message["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    unsigned long long newPing = std::stoll(message["pong"].asString()) - std::stoll(message["ping"].asString());
    connections[user]->ping.count++;
    connections[user]->ping.sum += newPing;
    connections[user]->ping.lastPing = std::chrono::system_clock::now();

    if (connections[user]->ping.count % kPingUpdateRate == 0) {
        connections[user]->ping.currentPing = static_cast<unsigned long long>(static_cast<double>(connections[user]->ping.sum) / static_cast<double>(connections[user]->ping.count)) + connections[user]->lag;
    }

    if (connections[user]->ping.count % kPingDumpRate == 0) {
        connections[user]->ping.count = 0;
        connections[user]->ping.sum = 0;
    }
}

bool MeshNode::addConnection(std::unique_ptr<sf::TcpSocket> user) {
    sf::Packet request;

    Json::Value message;
    message["type"] = "info";
    message["address"] = localAddress.toString();
    message["listeningPort"] = listeningPort;
    message["name"] = name;

    request << message.toStyledString();

    if (user->send(request) == sf::Socket::Done) {
        sf::Packet response;

        if (user->receive(response) == sf::Socket::Done) {
            std::string buffer;
            Json::Reader reader;
            Json::Value info;

            response >> buffer;

            reader.parse(buffer, info);

            if (craftConnection(std::move(user), info)) {
                return true;
            }
        }
    }

    return false;
}

bool MeshNode::craftConnection(std::unique_ptr<sf::TcpSocket> user, Json::Value info) {
    std::string clientName = info["name"].asString();
    if (info["type"].asString() == "info" && !connectionExists(clientName)) {
        connections[clientName] = std::unique_ptr<Connection>(new Connection());
        connections[clientName]->address = sf::IpAddress(info["address"].asString());
        connections[clientName]->listeningPort = static_cast<unsigned short>(info["listeningPort"].asUInt());
        connections[clientName]->personalPort = user->getLocalPort();
        connections[clientName]->socket = std::move(user);
        connections[clientName]->ping.count = 0;
        connections[clientName]->ping.sum = 0;
        connections[clientName]->ping.currentPing = 0;
        connections[clientName]->ping.lastPing = std::chrono::system_clock::now();
        connections[clientName]->lag = 0;
        connections[clientName]->timedOut = false;
        selector->add(*connections[clientName]->socket);
        routingTable[clientName].push_back(clientName);

        connections[clientName]->heartbeatThread = std::move(std::thread(&MeshNode::heartbeat, this, clientName));
        connections[clientName]->connectionRequestThread = std::move(std::thread(&MeshNode::searchConnections, this, clientName));

        log << "Connection to " << clientName << " on " << connections[clientName]->address << ":" << connections[clientName]->listeningPort << " established on " << connections[clientName]->personalPort << std::endl;
        return true;
    }

    return false;
}

bool MeshNode::connectionExists(std::string user) {
    if (connections.find(user) != connections.end()) {
        return true;
    }

    return false;
}

bool MeshNode::connectTo(sf::IpAddress address, unsigned short port) {
    std::unique_ptr<sf::TcpSocket> user = std::unique_ptr<sf::TcpSocket>(new sf::TcpSocket()); 
    if (address != localAddress || port != listeningPort) {
        if (user->connect(address, port, sf::milliseconds(kConnectionTimeout)) == sf::Socket::Done) {
            if (!addConnection(std::move(user))) {
                log << "Failed to connect to user from " << address << ":" << port << std::endl;
            }
            return true;
        }
    }

    return false;
}

void MeshNode::setLag(std::string user, unsigned int lag) {
    connections[user]->lag = lag;
}

Message MeshNode::craftMessage(std::string user, std::string type, Json::Value contents) {
    Message outgoingMessage;
    outgoingMessage.contents = contents;
    outgoingMessage.pathway.push_back(name);
    for (auto path : routingTable[user]) {
        outgoingMessage.pathway.push_back(path);
    }
    outgoingMessage.type = type;
    outgoingMessage.broadcast = contents.isMember("broadcast");
    
    return outgoingMessage;
}

void MeshNode::sendMessage(std::string user, Message message) {
    if (connectionExists(user)) {
        sf::Packet packet;

        Json::Value finalMessage;
        finalMessage["contents"] = message.contents;
        finalMessage["type"] = message.type;
        if (message.broadcast) {
            finalMessage["broadcast"] = "broadcast";
        }
        for (auto path : message.pathway) {
            finalMessage["pathway"].append(path);
        }

        packet << finalMessage.toStyledString();

        if (connectionExists(user)) {
            if (connections[user]->socket->send(packet) != sf::Socket::Done) {
                log << "Failed to send a message to " << user << ":" << std::endl << finalMessage.toStyledString() << std::endl;
                connections[user]->timedOut = true;
            }
        }
        return;
    } else {
        log << "Connection to " << user << " does not exist to send message: " << std::endl << message.toString() << std::endl;
    }
}

void MeshNode::broadcast(std::string type, Json::Value message) {
     message["broadcast"] = "broadcast";
     for (auto connection = connections.begin(); connection != connections.end(); connection++) {
         Message outgoingMessage = craftMessage(connection->first, type, message);
         sendMessage(connection->first, outgoingMessage);
     }
 }

void MeshNode::handleMessage(Message message, std::string sender) {
    if (message.pathway[message.pathway.size() - 1] == name) {
        handleContent(message);
    } else {
        for (auto user = message.pathway.begin(); user != message.pathway.end(); user++) {
            if (*user == sender) {
                user++;
                sendMessage(*user, message);
            }
        }
    }
}

void MeshNode::handleContent(Message message) {
    if (message.type == "ping") {
        pong(message.pathway[0], message.contents);
    } else if (message.type == "pong") {
        updatePing(message.pathway[0], message.contents);
    } else if (message.type == "sendConnections") {
        receiveConnections(message.pathway[0], message.contents);
    } else if (message.type == "requestConnections") {
        std::vector<std::string> users;
        for (auto user : message.contents["users"]) {
            users.push_back(user.asString());
        }
        sendRequestedConnections(message.pathway[0], std::move(users));
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
        Json::Value user;
        user["name"] = connection->first;
        user["ping"] = connection->second->ping.currentPing;
        message["users"].append(user);
    }

    Message outgoingMessage = craftMessage(user, "sendConnections", message);
    sendMessage(user, outgoingMessage);
}

void MeshNode::receiveConnections(std::string user, Json::Value message) {
    std::vector<std::string> users;

    for (auto unknownUser : message["users"]) {
        log << message.toStyledString() << std::endl;
        std::string newUser = unknownUser["name"].asString();
        if (!connectionExists(newUser) && newUser != name) {
            log << "Requesting connection to " << newUser << std::endl;
            users.push_back(newUser);
        }
        
        if (connectionExists(newUser) && isInRoute(newUser, user)) {
            if (connections[newUser]->ping.currentPing > unknownUser["ping"].asUInt()) {
                log << "Optimizing route to " << newUser << " via " << user << " with their " << unknownUser["ping"].asUInt() << "ms connection!" << std::endl;
                routingTable[user].push_back(newUser);
                log << "New route to " << newUser << std::endl;
                for (auto route : routingTable[newUser]) {
                    log << route << std::endl;
                }
            }
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

    Message outgoingMessage = craftMessage(user, "requestConnections", message);
    sendMessage(user, outgoingMessage);
}

void MeshNode::sendRequestedConnections(std::string user, std::vector<std::string> requestedUsers) {
    Json::Value message;
    for (auto requestedUser : requestedUsers) {
        if (connectionExists(requestedUser)) {
            Json::Value newUser;
            newUser["address"] = connections[requestedUser]->address.toString();
            newUser["port"] = connections[requestedUser]->listeningPort;
            message["users"].append(newUser);
        }
    }

    Message outgoingMessage = craftMessage(user, "responseConnections", message);
    sendMessage(user, outgoingMessage);
}

bool MeshNode::isInRoute(std::string newUser, std::string routeToCheck) {
    for (auto route : routingTable[routeToCheck]) {
        if (route == newUser) {
            return true;
        }
    }

    return false;
}

void MeshNode::listConnections() {
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        log << connection->first << ": " << connection->second->address << ":" << connection->second->listeningPort << " on " << connection->second->personalPort << " with " << connection->second->ping.currentPing << "ms" << std::endl;
    }
}
