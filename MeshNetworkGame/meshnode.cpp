#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort, std::string _name): listeningPort(_listeningPort), name(_name) {
    listener = std::unique_ptr<sf::TcpListener>(new sf::TcpListener);
    selector = std::unique_ptr<sf::SocketSelector>(new sf::SocketSelector);

    // Set the listener to be blocking so that the listen thread will wait properly
    listener->setBlocking(true);

    // Determine an open listening port and our address
    while (listener->listen(listeningPort) == sf::TcpListener::Error) {
        listeningPort++;
    }
    localAddress = sf::IpAddress::getPublicAddress();
    log << "Listening on " << localAddress << ":" << listeningPort << std::endl;

    selector->add(*listener);

    // Set the start time to keep our map valid
    connectionsInvalidated = std::chrono::system_clock::now();
    routingInvalidated = std::chrono::system_clock::now();

    listening = true;
    listenerThread = std::thread(&MeshNode::listen, this);
}

MeshNode::~MeshNode() {
    listening = false;
    listenerThread.join();

    // Iterate through our connections and clear all threads
    for (auto connection = connections.begin(); connection != connections.end(); ++connection) {
        connection->second->disconnected = true;
        connection->second->pingThread.join();
        connection->second->connectionRequestThread.join();
        connection->second->optimizationThread.join();
    }
}

void MeshNode::listen() {
    while (listening) {
        if (selector->wait(sf::milliseconds(kListenerWaitTime))) {
            // Check our listener for a new connection
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
                    // Keep a handle on the end so that we can properly reset iterators on delete
                    auto end = connections.end();
                    for (auto connection = connections.begin(); connection != end && !connections.empty(); ++connection) {
                        // Check the multiplexer for sockets with data
                        if (selector->isReady(*connection->second->socket)) {
                            sf::Packet packet;
                            sf::Socket::Status status = connection->second->socket->receive(packet);

                            if (status == sf::Socket::Done) {
                                // Attempt to parse the message for JSON
                                std::string buffer;
                                if (packet >> buffer) {
                                    Json::Reader reader;
                                    Json::Value message;
                                    reader.parse(buffer, message);

                                    // Craft a message object to be handled
                                    Message incomingMessage;
                                    incomingMessage.contents = message["contents"];
                                    incomingMessage.type = message["type"].asString();
                                    std::vector<std::string> route;
                                    for (auto node : message["route"]) {
                                        route.push_back(node.asString());
                                    }
                                    incomingMessage.route = route;

                                    handleMessage(incomingMessage);
                                } else {
                                    log << "Unable to parse message" << std::endl;
                                }
                            } else {
                                log << connection->first << " has disconnected" << std::endl;
                                // Invalidate the map and remove the connection so that any thread with an iterator will refresh it
                                removeConnection(connection->first); 
                                if (!connections.empty()) {
                                    // Reset the iterators so we don't get a crash
                                    connection = connections.begin();
                                    end = connections.end();
                                } else {
                                    break;
                                }
                            }
                        } else if (connection->second->disconnected) {
                            log << connection->first << " has timed out" << std::endl;
                            removeConnection(connection->first); 
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

void MeshNode::pingConnection(std::string user) {
    while (!connections[user]->disconnected) {
        ping(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kPingRate));
    }

    return;
}

void MeshNode::searchConnections(std::string user) {
    while (!connections[user]->disconnected) {
        sendConnections(user);
        std::this_thread::sleep_for(std::chrono::milliseconds(kUpdateNetworkRate));
    }

    return;
}

void MeshNode::optimize(std::string user) {
    while (!connections[user]->disconnected) {
        std::chrono::system_clock::time_point beganOptimizing = std::chrono::system_clock::now();
        // Iterate through all available connections that aren't to the user in question
        for (auto connection = connections.begin(); connection != connections.end(); connection++) {
            // Check the validity of the map so that we don't have a dereferenced iterator
            if (beganOptimizing > connectionsInvalidated && beganOptimizing > routingInvalidated && connection->first != user) {
                beginOptimization(user, connection->first);
            // The map is invalid, we gotta break out before increments
            } else if (beganOptimizing < connectionsInvalidated || beganOptimizing < routingInvalidated) { 
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kRouteOptimizationRate));
    }

    return;
}

void MeshNode::ping(std::string user) {
    Json::Value message;
    message["ping"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    Message outgoingMessage = craftMessage(user, "ping", message, true);
    sendMessage(user, outgoingMessage);
}

void MeshNode::pong(std::string user, Json::Value message) {
    Message outgoingMessage = craftMessage(user, "pong", message, true);
    sendMessage(user, outgoingMessage);
}

void MeshNode::updatePing(std::string user, Json::Value message) {
    message["pong"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    // Parse the contents of the ping/pong message and determine the time elapsed from this machine
    unsigned long long thisPing = std::stoll(message["pong"].asString()) - std::stoll(message["ping"].asString());
    connections[user]->ping.count++;
    connections[user]->ping.sum += thisPing;
    connections[user]->ping.lastPing = std::chrono::system_clock::now();

    if (connections[user]->ping.count % kPingUpdateRate == 0) {
        // Cast the long longs to doubles to remove integer division before storage
        unsigned long long newPing = static_cast<unsigned long long>(static_cast<double>(connections[user]->ping.sum) / static_cast<double>(connections[user]->ping.count));
        connections[user]->ping.currentPing = newPing;

        // Clear out the ping values so that we get a new history to work with (keeps variance low)
        connections[user]->ping.count = 0;
        connections[user]->ping.sum = 0;
        if (connections[user]->ping.currentPing < connections[user]->ping.optimumPing) {
            connections[user]->ping.optimumPing = connections[user]->ping.currentPing;
#if verbose
            log << "Direct route is more efficient with " << newPing << "ms" << std::endl;
#endif
            std::vector<std::string> newRoute;
            newRoute.push_back(name);
            newRoute.push_back(user);
            routingTable[user] = newRoute;
        }
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

    // Perform first part of the handshake
    if (user->send(request) == sf::Socket::Done) {
        sf::Packet response;

        // Perform second part of the handshake
        if (user->receive(response) == sf::Socket::Done) {
            std::string buffer;
            Json::Reader reader;
            Json::Value info;

            response >> buffer;

            reader.parse(buffer, info);

            // Handshake finished, add to our map
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
        // Add a new object to the map
        connections[clientName] = std::unique_ptr<Connection>(new Connection());

        // Copy over all of the relevant info from the message
        connections[clientName]->address = sf::IpAddress(info["address"].asString());
        connections[clientName]->listeningPort = static_cast<unsigned short>(info["listeningPort"].asUInt());
        connections[clientName]->personalPort = user->getLocalPort();
        connections[clientName]->socket = std::move(user);

        // Set the port to be nonblocking in order to allow all of the threads to send messages at their will
        connections[clientName]->socket->setBlocking(false);

        // Zero out all of the ping variables for the first run
        connections[clientName]->ping.count = 0;
        connections[clientName]->ping.sum = 0;
        connections[clientName]->ping.currentPing = 9999;
        connections[clientName]->ping.optimumPing = 9999;
        connections[clientName]->ping.lastPing = std::chrono::system_clock::now();
        connections[clientName]->lag = 0;
        connections[clientName]->disconnected = false;

        // Add to the multiplexer and make a direct routing table entry
        selector->add(*connections[clientName]->socket);
        routingTable[clientName].push_back(name);
        routingTable[clientName].push_back(clientName);

        // Spawn all relevant threads
        connections[clientName]->pingThread = std::move(std::thread(&MeshNode::pingConnection, this, clientName));
        connections[clientName]->connectionRequestThread = std::move(std::thread(&MeshNode::searchConnections, this, clientName));
        connections[clientName]->optimizationThread = std::move(std::thread(&MeshNode::optimize, this, clientName));

        log << "Connection to " << clientName << " on " << connections[clientName]->address << ":" << connections[clientName]->listeningPort << " established on " << connections[clientName]->personalPort << std::endl;
        return true;
    }

    return false;
}

void MeshNode::removeConnection(std::string user) {
    // Mark the map invalid for any other thread using an iterator can respond accordingly
    connectionsInvalidated = std::chrono::system_clock::now();
    routingInvalidated = std::chrono::system_clock::now();

    // Remove from the multiplexer
    selector->remove(*connections[user]->socket);

    // Kill all threads belonging to that connections[user]
    connections[user]->disconnected = true;
    connections[user]->pingThread.join();
    connections[user]->connectionRequestThread.join();
    connections[user]->optimizationThread.join();

    // Remove any references of this user from the routing table
    purgeFromRoutes(user);

    // Finally remove the entry
    connections.erase(user);
    routingTable.erase(user);
}

bool MeshNode::connectionExists(std::string user) {
    if (connections.find(user) != connections.end() && user != name) {
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
    if (user != name) {
        connections[user]->lag = lag;
        connections[user]->ping.optimumPing = 9999;
        connections[user]->ping.currentPing = 9999;
        connections[user]->ping.sum = 0;
        connections[user]->ping.count = 0;
        connections[user]->ping.lastPing = std::chrono::system_clock::now();
    }
}

Message MeshNode::craftMessage(std::string user, std::string type, Json::Value contents, bool directRoute) {
    Message outgoingMessage;
    // Add in the contents
    outgoingMessage.contents = contents;


    // Determine how to design the route
    if (directRoute) {
        // Just add in the sender and receiver
        outgoingMessage.route.push_back(name);
        outgoingMessage.route.push_back(user);
    } else {
        // Add in the sender and all the relevant route
        if (routingTable[user].empty()) {
            log << "Routing table for " << user << " is empty!!";
        }
        for (auto node : routingTable[user]) {
            outgoingMessage.route.push_back(node);
        }
    }

    // Add in the type
    outgoingMessage.type = type;
    
    return outgoingMessage;
}

void MeshNode::sendMessage(std::string user, Message message) {
    if (connectionExists(user)) {
        sf::Packet packet;

        Json::Value finalMessage;
        finalMessage["contents"] = message.contents;
        finalMessage["type"] = message.type;
        for (auto node : message.route) {
            finalMessage["route"].append(node);
        }

        packet << finalMessage.toStyledString();

        std::this_thread::sleep_for(std::chrono::milliseconds(connections[user]->lag));

        if (connections[user]->socket->send(packet) != sf::Socket::Done) {
            log << "Failed to send a message to " << user << ":" << std::endl << finalMessage.toStyledString() << std::endl;
            connections[user]->disconnected = true;
        }
        return;
    } else {
        log << "Connection to " << user << " does not exist to send message: " << std::endl << message.toString() << std::endl;
    }
}

void MeshNode::forwardMessage(Message message) {
    std::string nextUser;
    for (auto user = message.route.begin(); user != message.route.end(); user++) {
        // Find where this node is in the pathway and move onto the next one
        if (*user == name) {
            nextUser = *(++user);
        }
    }

#if verbose
    if (!isSystemMessage(message)) {
        log << "Forwarding message of type " << message.type << " to " << message.route.back() << std::endl;
    }
#endif

    sendMessage(nextUser, message);
}

void MeshNode::broadcast(std::string type) {
    Json::Value message;
    broadcast(type, message);
}

void MeshNode::broadcast(std::string type, Json::Value message) {
    // Iterate through all available connections and broadcast the same message
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        Message outgoingMessage = craftMessage(connection->first, type, message);
        log << "Broadcasting " << std::endl << outgoingMessage.toString();
        sendMessage(connection->first, outgoingMessage);
    }
}

void MeshNode::handleMessage(Message message) {
    // Check to see if we're the destination
    if (message.route.back() == name) {
        handleContent(message);
    } else {
#if verbose
        // If this isn't a system message, put it into the log for view
        if (!isSystemMessage(message)) {
            log << "Forwarding " << std::endl << message.toString() << std::endl;
        }
#endif
        forwardMessage(message);
    }
}

void MeshNode::handleContent(Message message) {
    if (message.type == "ping") {
        pong(message.route.front(), message.contents);
    } else if (message.type == "pong") {
        updatePing(message.route.front(), message.contents);
    } else if (message.type == "sendConnections") {
        receiveConnections(message.route.front(), message.contents);
    } else if (message.type == "requestConnections") {
        std::vector<std::string> users;
        for (auto user : message.contents["users"]) {
            users.push_back(user.asString());
        }
        sendRequestedConnections(message.route.front(), std::move(users));
    } else if (message.type == "responseConnections") {
        parseConnections(message.contents);
    } else if (message.type == "optimizeRoute") {
        forwardOptimization(message);
    } else if (message.type == "receiveOptimizedRoute"){
        returnOptimization(message);
    } else if (handlers.find(message.type) != handlers.end()) {
        handlers[message.type]->handleMessage(message.route.front(), message.type, message.contents);
    } else {
        log << message.toString() << std::endl;
    }
}

bool MeshNode::isSystemMessage(Message message) {
    if (message.type != "ping" && 
        message.type != "pong" &&
        message.type != "sendConnections" &&
        message.type != "requestConnections" &&
        message.type != "responseConnections" &&
        message.type != "optimizeRoute" &&
        message.type != "receiveOptimizedRoute") {
        return false;
    } else {
        return true;
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
        std::string newUser = unknownUser["name"].asString();
        if (!connectionExists(newUser) && newUser != name) {
            log << "Requesting connection to " << newUser << std::endl;
            users.push_back(newUser);
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

void MeshNode::parseConnections(Json::Value message) {
    std::vector<sf::IpAddress> addresses;
    std::vector<unsigned short> ports;

    for (auto user : message["users"]) {
        addresses.push_back(sf::IpAddress(user["address"].asString()));
        ports.push_back(static_cast<unsigned short>(user["port"].asUInt()));
    }

    while (!addresses.empty() && !ports.empty()) {
        if (connectTo(addresses.back(), ports.back())) {
            addresses.pop_back();
            ports.pop_back();
        } 
    }
}

void MeshNode::beginOptimization(std::string userToBeOptimized, std::string userToSendThrough) {
    Json::Value message, firstUser;
    message["destination"] = userToBeOptimized;
    firstUser["name"] = userToSendThrough;
    firstUser["ping"] = connections[userToSendThrough]->ping.currentPing;
    message["data"].append(firstUser);

    Message outgoingMessage = craftMessage(userToSendThrough, "optimizeRoute", message, true);
    sendMessage(userToSendThrough, outgoingMessage);
}

void MeshNode::forwardOptimization(Message message) {
    // See if we're the node being optimized for
    if (message.contents["destination"] == name) {
        // Compute the ping from this optimization message
        message.contents["finalPing"] = 0;
        for (auto user : message.contents["data"]) {
            message.contents["finalPing"] = message.contents["finalPing"].asUInt() + user["ping"].asUInt();
        }

        // Turn the vector around to send it back the way it came
        std::vector<std::string> reverseRoute;
        for (auto node = message.route.rbegin(); node != message.route.rend(); ++node) {
            reverseRoute.push_back(*node);
        }

        // Swap the route and the message type
        message.route = reverseRoute;
        message.type = "receiveOptimizedRoute";

        // Send the message along the way
        returnOptimization(message);
    } else {
        // Look through all of my existing connections
        for (auto& connection : connections) {
            // Determine if that node has already been visited
            bool alreadyInRoute = false;
            for (auto node : message.route) {
                if (connection.first == node) {
                    alreadyInRoute = true;
                }
            }
            
            // If it's not already in the path, append ourselves and send it on it's way 
            if (!alreadyInRoute) {
                // Make a copy of our existing message
                Message newMessage = message;

                // Add the ping of that user to this message
                Json::Value nextUser;
                nextUser["name"] = connection.first;
                nextUser["ping"] = connection.second->ping.currentPing;

                // Add the path to that user to this message
                message.contents["data"].append(nextUser);
                message.route.push_back(connection.first);

                // Send it off
                forwardMessage(message);
            } 
        }
    }
}

void MeshNode::returnOptimization(Message message) {
    if (message.route.back() == name) {
        std::string destination = message.contents["destination"].asString();
        // See if the final ping is < the current ping + lag && it's not the same as the old route
        if (message.contents["finalPing"].asUInt() < connections[destination]->ping.optimumPing) {
#if verbose
            log << "Updating route to " << message.contents["destination"].asString() << " with " << message.contents["finalPing"].asString() << "ms with route: " << std::endl;
#endif
            connections[destination]->ping.optimumPing = message.contents["finalPing"].asUInt();

            std::vector<std::string> newRoute;
            for (auto route = message.route.rbegin(); route != message.route.rend(); ++route) {
#if verbose
                if (*route != message.route.front()) {
                    log << *route << " -> ";
                } else {
                    log << *route << std::endl;
                }
#endif
                newRoute.push_back(*route);
            }
            routingTable[destination] = newRoute;
            return;
        }
    } else {
        forwardMessage(message);
    }
}

bool MeshNode::isInRoute(std::string newUser, std::string routeToCheck) {
    for (auto route : routingTable[routeToCheck]) {
        if (route == newUser) {
            return true;
        }
    }

    return false;
}

void MeshNode::purgeFromRoutes(std::string user) {
    for (auto& route : routingTable) {
        for (auto node : route.second) {
            if (node == user) {
                std::string beginning, end;
                beginning = route.second.front();
                end = route.second.back();

                route.second.clear();
                route.second.push_back(beginning);
                route.second.push_back(end);

                log << "Purged " << user << " from route to " << route.second.back() << std::endl;
                log << "Resetting route to " << end << std::endl;

                connections[end]->ping.currentPing = 9999;
                connections[end]->ping.optimumPing = 9999;
                    
                break;
            }
        }
    }
}

void MeshNode::listConnections() {
    for (auto connection = connections.begin(); connection != connections.end(); connection++) {
        log << connection->first << ": " << connection->second->address << ":" << connection->second->listeningPort << " on " << connection->second->personalPort << std::endl;
        if (connection->second->ping.currentPing == 9999 || connection->second->ping.optimumPing == 9999) {
            log << "Currently calculating ping: " << kPingUpdateRate - connection->second->ping.count << " more pings required" <<std::endl;
        } else {
            log << "Direct ping to " << connection->first << ": " << connection->second->ping.currentPing << "ms" << std::endl;
            log << "Current route's ping to " << connection->first << ": " << connection->second->ping.optimumPing << "ms " << std::endl; 
        }
        if (connection->second->lag > 0) {
            log << "Has " << connection->second->lag << "ms artificial lag" << std::endl;
        }
        if (routingTable[connection->first].size() > 1) {
            log << "Route to " << connection->first << ":" << std::endl;
            for (auto route = routingTable[connection->first].begin(); route != routingTable[connection->first].end(); route++) {
                if (*route != routingTable[connection->first].back()) {
                    log << *route << " -> ";
                } else {
                    log << *route << std::endl;
                }
            }
        }
        log << std::endl;
    }
}

unsigned int MeshNode::numberOfConnections() {
    return connections.size();
}

bool MeshNode::registerHandler(std::shared_ptr<MessageHandler> handler) {
    for (auto handle : handler->getMessageTypes()) {
        if (handlers.find(handle) != handlers.end()) {
            log << "Handle " << handle << " already exists, overwritten by a new handler" << std::endl;
        }
        handlers[handle] = handler;
    }
    handler->setMeshNode(std::unique_ptr<MeshNode>(this));
    return true;
}

void MeshNode::listHandlers() {
    log << "All registered handles: " << std::endl;
    for (auto& handle : handlers) {
        log << handle.first << std::endl;
    }
}
