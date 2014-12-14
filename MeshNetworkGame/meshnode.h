#ifndef __MESHNODE_H__
#define __MESHNODE_H__
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <json/json.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

#include <vector>
#include <deque>
#include <map>

#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>

#include "MessageHandler.h"

class MessageHandler;

#define verbose 0
#define out std::cout 
#define in  std::cin
#define log std::cerr

struct PingInfo {
    unsigned long long sum;
    unsigned long long count;
    unsigned long long currentPing;
    unsigned long long optimumPing;
    std::chrono::system_clock::time_point lastPing;
};

struct Connection {
    std::unique_ptr<sf::TcpSocket> socket;
    sf::IpAddress address;
    unsigned short personalPort;
    unsigned short listeningPort;
    PingInfo ping;
    unsigned int lag;

    bool disconnected;
    std::thread pingThread;
    std::thread connectionRequestThread;
    std::thread optimizationThread;
};

struct Message {
    Json::Value contents;
    std::string type;
    std::vector<std::string> route;

    std::string toString() {
        std::stringstream buffer;
        if (!route.empty()) { 
            buffer << "From: " << route.front() << std::endl;
            buffer << "To: " << route.back() << std::endl;
            buffer << "Route: " << std::endl;
            for (auto node = route.begin(); node != route.end(); ++node) {
                if (*node != route.back()) {
                    buffer << *node << " -> ";
                } else {
                    buffer << *node << std::endl;
                }
            }
        }
        buffer << "Contents: " << std::endl;
        buffer << contents.toStyledString() << std::endl;
        return buffer.str();
    }

    Message& operator=(const Message& other) {
        contents = other.contents;
        type = other.type;
        route = other.route;
    }
};

const int kListeningPort = 10010;  // Default listening port
const int kListenerWaitTime = 10; // Wait x ms between messages
const std::string kDefaultName = "test"; // Default name
const int kConnectionTimeout = 5000; // Cancel a connection if it exceeds x ms
const int kPingRate = 100; // Send a ping every x ms
const int kPingReportRate = 500; // Print out the ping for a connection every x ms
const int kPingUpdateRate = 100; // Compute the average ping every x ms
const int kPingDumpRate = 1000; // Every x pings, reset the sum
const int kUpdateNetworkRate = 1000; // Every x pings, ask other nodes for more nodes
const int kRouteOptimizationRate = 1500; // Attempt to optimize the route after x ms

class MeshNode {
public:
    MeshNode(unsigned short _listening_port = kListeningPort, std::string name = kDefaultName);
    ~MeshNode();

    bool connectTo(sf::IpAddress address, unsigned short port);
    bool registerHandler(std::shared_ptr<MessageHandler> handler);
    void setLag(std::string user, unsigned int lag);
    void broadcast(std::string type);
    void broadcast(std::string type, Json::Value message);
    unsigned int numberOfConnections();
    void listConnections();
    void listHandlers();
private:
    // Local info
    sf::IpAddress localAddress;
    unsigned short listeningPort;
    std::string name;
    std::map<std::string, std::unique_ptr<Connection>> connections;
    std::chrono::system_clock::time_point connectionsInvalidated;

    // Listening and handling new clients
    void listen();
    bool addConnection(std::unique_ptr<sf::TcpSocket> user);
    bool craftConnection(std::unique_ptr<sf::TcpSocket> user, Json::Value info);
    void removeConnection(std::string user);
    bool connectionExists(std::string user);

    std::thread listenerThread;
    std::unique_ptr<sf::TcpListener> listener; // Socket listener
    std::unique_ptr<sf::SocketSelector> selector;
    bool listening;

    // Ping measuring 
    void pingConnection(std::string);
    void ping(std::string name);
    void pong(std::string name, Json::Value message);
    void updatePing(std::string name, Json::Value message);

    // Message handling
    void handleMessage(Message message);
    void handleContent(Message message);
    void sendMessage(std::string userToSendTo, Message message);
    void forwardMessage(Message message);
    bool isSystemMessage(Message message);
    Message craftMessage(std::string userToSendTo, std::string type, Json::Value contents, bool directRoute = false);

    std::thread heartbeatThread;
    bool sendingHeartbeats;

    // Connection exploring
    void searchConnections(std::string user);
    void sendConnections(std::string user);
    void receiveConnections(std::string user, Json::Value message);
    void requestConnections(std::string user, std::vector<std::string> requestedUsers);
    void parseConnections(Json::Value message);
    void sendRequestedConnections(std::string user, std::vector<std::string> requestedUsers);

    // Route handling
    void optimize(std::string user);
    bool isInRoute(std::string newUser, std::string routeToCheck);
    void purgeFromRoutes(std::string user);
    void beginOptimization(std::string userToBeOptimized, std::string userToSendThrough);
    void forwardOptimization(Message message);
    void returnOptimization(Message message);
    std::map<std::string, std::vector<std::string>> routingTable;
    std::chrono::system_clock::time_point routingInvalidated;

    // Message handlers
    std::map<std::string, std::shared_ptr<MessageHandler>> handlers;
};
#endif // __MESHNODE_H__
