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
#include "Message.h"
#include "MessageHandler.h"
#include "ConnectionManager.h"
#include "MessageQueue.h"

class MessageHandler;
class ConnectionManager;
class Connection;

const int kListeningTimeout = 5000;
const int kHeartBeatTimeout = 2500;
const int kListeningPort = 10010;

class MeshNode {
public:
    MeshNode(unsigned short _listening_port = kListeningPort);
    ~MeshNode();

    void listen();
    void startListening();
    void stopListening();
    bool isListening();
    unsigned short getListeningPort();

    void handleMessage();
    bool registerMessageHandler(std::shared_ptr<MessageHandler> handler);
    void listAllHandlers();
    void startHandlingMessages();
    void stopHandlingMessages();
    bool isHandlingMessages();

    bool connectTo(sf::IpAddress address, unsigned short port);
    unsigned int numberOfConnections();

    void ping(sf::IpAddress address, unsigned short port);
    bool sendMessage(sf::IpAddress address, unsigned short port, std::string type, Json::Value message);
    void broadcast(std::string type, Json::Value message);

private:
    std::thread listeningThread;
    unsigned short listeningPort;
    sf::SocketSelector selector;
    sf::TcpListener listener;
    bool listening;

    std::unique_ptr<ConnectionManager> connections;

    std::thread messageHandlingThread;
    std::map<std::string, std::shared_ptr<MessageHandler>> handlers;
    bool handlingMessages;

    MessageQueue<Message> outgoingMessages;
    MessageQueue<Message> incomingMessages;
};
#endif // __MESHNODE_H__
