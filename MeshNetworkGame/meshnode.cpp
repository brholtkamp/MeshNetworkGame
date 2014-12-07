#include "MeshNode.h"

MeshNode::MeshNode(unsigned short _listeningPort): listeningPort(_listeningPort) {
    connections = std::unique_ptr<ConnectionManager>(new ConnectionManager);

    startListening();
    startHandlingMessages();
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
        if (selector.wait(sf::milliseconds(kListeningTimeout))) {
            if (selector.isReady(listener)) {
                std::shared_ptr<sf::TcpSocket> newClient = std::make_shared<sf::TcpSocket>();
                if (listener.accept(*newClient) == sf::Socket::Done) {
                    // Connection established 
                    connections->addConnection(newClient);
                } else {
                    Log("Client failed to connect");
                }
            } else {
                for (auto connection : *connections) {
                    if (selector.isReady(*connection->getSocket().lock())) {
                        sf::Packet packet;
                        std::string buffer;
                        sf::Socket::Status status = connection->getSocket().lock()->receive(packet);
                        
                        switch (status) {
                            case sf::Socket::Done:
                                if (packet >> buffer) {
                                    Json::Reader reader;
                                    Json::Value message;
                                    reader.parse(buffer, message);
                                    Message finalMessage = fromJSON(message);

                                    incomingMessages.push_back(std::move(finalMessage));
                                }
                                break;
                            case sf::Socket::Disconnected:
                            case sf::Socket::Error:
                                Log("Socket disconnection or error from " + connection->toString());
                                connections->closeConnection(connection->getAddress(), connection->getPort());
                                break;
                            default:
                                Log("Socket failure from " + connection->toString());
                                break;
                        }
                    }
                }
            }
        }
    }

    listener.close();
    return;
}

void MeshNode::handleMessage() {
    while (handlingMessages) {
        if (incomingMessages.size() != 0) {
            Message message = incomingMessages.get_next();

            handlers[message.getType()]->handleMessage(message);
        }

        if (outgoingMessages.size() != 0) {
            Message message = outgoingMessages.get_next();

            handlers[message.getType()]->handleMessage(message);
        }
    }

    return;
}

void MeshNode::startListening() {
    while (listener.listen(listeningPort) == sf::Socket::Error) {
        listeningPort++;
    }

    listener.setBlocking(false);

    selector.add(listener);

    listening = true;
    listeningThread = std::thread(&MeshNode::listen, this);
    Log("Listening on " + std::to_string(listeningPort));
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
    std::shared_ptr<sf::TcpSocket> socket = std::make_shared<sf::TcpSocket>();

    if (socket->connect(address, port) == sf::TcpSocket::Done) {
        socket->setBlocking(false);
        selector.add(*socket);
        connections->addConnection(socket);
        return true;
    } else {
        Log("Failed to connect to " + address.toString() + ":" + std::to_string(port));
        return true;
    }
}

void MeshNode::ping(sf::IpAddress address, unsigned short port) {
    Json::Value message;
    message["ping"] = "ping";

    sendMessage(address, port, "ping", message);
}

bool MeshNode::sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message) {
    std::shared_ptr<Connection> connection = connections->getConnection(address, port);

    if (connection != nullptr) {
        Message message(address, port, type, message);

        outgoingMessages.push_back(message);
        return true;
    }

    Log("Cannot find connection to " + address.toString() + ":" + std::to_string(port) + " to send a message");
    return false;
}

void MeshNode::broadcast(std::string type, Json::Value message) {
    for (auto connection : *connections) {
        Message message(connection->getAddress(), connection->getPort(), type, message);
        outgoingMessages.push_back(message);
    }
}

bool MeshNode::registerMessageHandler(std::shared_ptr<MessageHandler> handler) {
    bool success = true;

    for (auto handle : handler->getMessageTypes()) {
        if (handlers.find(handle) == handlers.end()) {
            handlers[handle] = handler;
        } else {
            Log("Handle" + handle + " is already registered");
            success = false;
        }
    }

    return success;
}

void MeshNode::listAllHandlers() {
    for (auto handler : handlers) {
        std::cout << handler.first << std::endl;
    }
}