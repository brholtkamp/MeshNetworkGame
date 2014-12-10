#ifndef __MESHNODE_H__
#define __MESHNODE_H__
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <list>
#include <deque>
#include <map>
#include <ctime>
#include <sstream>
#include <thread>
#include <functional>
#include <memory>

#include "Log.h"
#include "PingMessageHandler.h"
#include "SystemMessageHandler.h"
#include "MessageQueue.h"

class MessageHandler;
class PingMessageHandler;
class SystemMessageHandler;

struct ConnectionInfo {
    sf::IpAddress address;
    unsigned short port;
};

struct Connection {
    std::unique_ptr<sf::TcpSocket> socket;
    ConnectionInfo user;
    std::vector<ConnectionInfo> pathway;
    unsigned long long sumOfPings;
    unsigned long long countOfPings;
    unsigned long long currentPing;
};

struct Message {
    ConnectionInfo destination;
    Json::Value contents;
    std::string type;
    std::vector<ConnectionInfo> pathway;
};

const int kListeningTimeout = 2500;
const int kListeningPort = 10010;
const int kPingUpdateRate = 300;

class MeshNode {
public:
    MeshNode(unsigned short _listening_port = kListeningPort);
    ~MeshNode();

    void startListening();
    void stopListening();
    bool isListening();
    unsigned short getListeningPort();

    void listAllHandlers();
    void startHandlingMessages();
    void stopHandlingMessages();
    bool isHandlingMessages();

    bool connectTo(sf::IpAddress address, unsigned short port);
    unsigned int numberOfConnections();

    void ping(sf::IpAddress address, unsigned short port);
    unsigned int pong(sf::IpAddress address, unsigned short port, Json::Value message);
    void updatePing(sf::IpAddress address, unsigned short port, unsigned long long newPing);

    bool sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message);
    bool optimizeFor(sf::IpAddress address, unsigned short port);
    void broadcast(std::string type, Json::Value message);
    void outputConnections();
private:
    void handleIncomingMessages();
    void handleOutgoingMessages();
    void listen();

    void setupHandlers();
    bool registerMessageHandler(std::unique_ptr<MessageHandler> handler);

    bool addConnection(sf::IpAddress address, unsigned short port, std::unique_ptr<sf::TcpSocket> socket);
    bool connectionExists(sf::IpAddress address, unsigned short port);
    bool closeConnection(sf::IpAddress address, unsigned short port);

    std::thread listeningThread;
    ConnectionInfo listenerInfo;
    std::unique_ptr<sf::SocketSelector> selector;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening;

    std::string makeUsernameString(sf::IpAddress address, unsigned short port);
    std::thread incomingMessagesThread;
    std::thread outgoingMessagesThread;
    std::deque<Message> outgoingMessages;
    std::deque<Message> incomingMessages;
    bool handlingMessages;

    std::map<std::string, std::vector<std::unique_ptr<Connection>>::iterator> connections;
    std::vector<std::unique_ptr<Connection>> connectionReferences;
    
    std::map<std::string, std::vector<std::unique_ptr<MessageHandler>>::iterator> handlers;
    std::vector<std::unique_ptr<MessageHandler>> handlerReferences;

    std::unique_ptr<PingMessageHandler> pingHandler;
    std::unique_ptr<SystemMessageHandler> systemHandler;
};
#endif // __MESHNODE_H__
