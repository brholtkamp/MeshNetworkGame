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
#include <chrono>

#include "Log.h"
#include "PingMessageHandler.h"
#include "SystemMessageHandler.h"
#include "MessageQueue.h"

#define Log std::cout

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
    std::thread heartbeatThread;
    std::chrono::system_clock::time_point lastHeartbeat;
};

struct Message {
    ConnectionInfo destination;
    Json::Value contents;
    std::string type;
    std::vector<ConnectionInfo> pathway;
};

const int kListeningTimeout = 2500; // Cancel a socket if it exceeds x ms
const int kHeartbeatTimeout = 2500; // Cancel a connection if it exceeds x ms
const int kHeartbeatRate = 100; // Send a heartbeat every x ms
const int kListeningPort = 10010;
const int kPingUpdateRate = 5; // Update the currentPing every x pings
const int kPingDumpRate = 100; // Every x pings, reset the sum

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

    void startSendingHeartbeats();
    void stopSendingHeartbeats();
    bool isSendingHeartbeats();

    bool connectTo(sf::IpAddress address, unsigned short port);
    unsigned int numberOfConnections();

    void ping(sf::IpAddress address, unsigned short port);
    std::string pong(sf::IpAddress address, unsigned short port, Json::Value message);
    void updatePing(sf::IpAddress address, unsigned short port, std::string newPing);

    bool sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message);
    bool optimizeFor(sf::IpAddress address, unsigned short port);
    void broadcast(std::string type, Json::Value message);
    void outputConnections();
private:
    // Listener
    void listen();

    std::thread listeningThread;
    ConnectionInfo listenerInfo;
    std::unique_ptr<sf::SocketSelector> selector;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening;

    // Handlers
    void handleIncomingMessages();
    void handleOutgoingMessages();

    std::thread incomingMessagesThread;
    std::thread outgoingMessagesThread;
    MessageQueue<Message> outgoingMessages;
    MessageQueue<Message> incomingMessages;
    bool handlingMessages;

    // Connections
    std::string makeUsernameString(sf::IpAddress address, unsigned short port);
    bool addConnection(sf::IpAddress address, unsigned short port, std::unique_ptr<sf::TcpSocket> socket);
    bool connectionExists(sf::IpAddress address, unsigned short port);
    bool closeConnection(sf::IpAddress address, unsigned short port);

    std::map<std::string, std::unique_ptr<Connection>> connections;

    // Heartbeat/ping tools
    void heartbeat(sf::IpAddress address, unsigned short port);
    void startHeartbeat(sf::IpAddress address, unsigned short port);

    std::vector<std::thread> heartbeatThreads;
    bool sendingHeartbeats;
    
    // Handlers
    void setupHandlers();
    bool registerMessageHandler(std::unique_ptr<MessageHandler> handler);

    std::map<std::string, std::vector<std::unique_ptr<MessageHandler>>::iterator> handlers;
    std::vector<std::unique_ptr<MessageHandler>> handlerReferences;

    std::unique_ptr<PingMessageHandler> pingHandler;
    std::unique_ptr<SystemMessageHandler> systemHandler;
};
#endif // __MESHNODE_H__
