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
#include <mutex>

#include "Log.h"
#include "MessageHandler.h"
#include "MessageQueue.h"

class MessageHandler;

struct ConnectionInfo {
    std::string name;
    sf::IpAddress address;
    unsigned short port;
};

struct Connection {
    std::unique_ptr<sf::TcpSocket> socket;
    ConnectionInfo user;
    std::vector<ConnectionInfo> pathway;
    int currentPing;
};

struct Message {
    ConnectionInfo destination;
    Json::Value contents;
    std::string type;
    std::vector<ConnectionInfo> pathway;
};

const int kListeningTimeout = 2500;
const int kHeartBeatTimeout = 2500;
const int kListeningPort = 10010;
const std::string kDefaultName = "user";

class MeshNode {
public:
    MeshNode(unsigned short _listening_port = kListeningPort, std::string username = kDefaultName);
    ~MeshNode();

    void listen();
    void startListening();
    void stopListening();
    bool isListening();
    unsigned short getListeningPort();

    void handleMessage();
    bool registerMessageHandler(std::unique_ptr<MessageHandler> handler);
    void listAllHandlers();
    void startHandlingMessages();
    void stopHandlingMessages();
    bool isHandlingMessages();

    bool connectTo(sf::IpAddress address, unsigned short port);
    unsigned int numberOfConnections();

    void ping(sf::IpAddress address, unsigned short port);
    unsigned int pong(sf::IpAddress address, unsigned short port, Json::Value message);
    void updatePing(sf::IpAddress address, unsigned short port, int ping);

    bool sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message);
    bool optimizeFor(sf::IpAddress address, unsigned short port);
    void broadcast(std::string type, Json::Value message);
    void outputConnections();
private:
    bool addConnection(sf::IpAddress address, unsigned short port, std::unique_ptr<sf::TcpSocket> socket);
    bool connectionExists(sf::IpAddress address, unsigned short port);
    bool closeConnection(sf::IpAddress address, unsigned short port);

    std::thread listeningThread;
    ConnectionInfo listenerInfo;
    std::unique_ptr<sf::SocketSelector> selector;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening;

    std::vector<std::unique_ptr<Connection>> connections;

    std::thread messageHandlingThread;
    std::vector<std::unique_ptr<MessageHandler>> handlers;
    bool handlingMessages;

    MessageQueue<Message> outgoingMessages;
    MessageQueue<Message> incomingMessages;
};
#endif // __MESHNODE_H__
