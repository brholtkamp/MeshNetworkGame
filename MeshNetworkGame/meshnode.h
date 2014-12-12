#ifndef __MESHNODE_H__
#define __MESHNODE_H__
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <json/json.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <vector>
#include <deque>
#include <map>

#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <chrono>

#include "Log.h"

#define out std::cout 
#define in  std::cin
#define log std::cerr

class MessageHandler;

struct PingInfo {
    unsigned long long sum;
    unsigned long long count;
    unsigned long long currentPing;
    std::chrono::system_clock::time_point lastPing;
};

struct Connection {
    std::unique_ptr<sf::TcpSocket> socket;
    sf::IpAddress address;
    unsigned short personalPort;
    unsigned short listeningPort;
    PingInfo ping;
    unsigned int lag;

    bool timedOut;
    std::thread heartbeatThread;
    std::thread connectionRequestThread;
};

struct Message {
    Json::Value contents;
    std::string type;
    std::vector<std::string> pathway;

    std::string toString() {
        std::stringstream buffer;
        buffer << "From: " << pathway[0] << std::endl;
        buffer << "To: " << pathway[pathway.size() - 1] << std::endl;
        buffer << "Pathway: " << std::endl;
        for (auto path : pathway) {
            buffer << "\t" << path << std::endl;
        }
        buffer << "Contents: " << std::endl;
        buffer << contents.toStyledString() << std::endl;
        return buffer.str();
    }
};

const int kListeningPort = 10010;  // Default listening port
const std::string kDefaultName = "test"; // Default name
const int kConnectionTimeout = 2000; // Cancel a connection if it exceeds x ms
const int kHeartbeatRate = 100; // Send a heartbeat every x ms
const int kPingReportRate = 250; // Print out the ping for a connection every x ms
const int kPingUpdateRate = 10; // Compute the average ping every x ms
const int kPingDumpRate = 100; // Every x pings, reset the sum
const int kUpdateNetworkRate = 10000; // Every x pings, ask other nodes for more nodes

class MeshNode {
public:
    MeshNode(unsigned short _listening_port = kListeningPort, std::string name = kDefaultName);
    ~MeshNode();

    bool connectTo(sf::IpAddress address, unsigned short port);
    void setLag(std::string user, unsigned int lag);
    void broadcast(std::string type, Json::Value message);
    void listConnections();
private:
    // Local info
    sf::IpAddress localAddress;
    unsigned short listeningPort;
    std::string name;
    std::map<std::string, std::unique_ptr<Connection>> connections;

    // Listening and handling new clients
    void listen();
    bool addConnection(std::unique_ptr<sf::TcpSocket> user);
    bool craftConnection(std::unique_ptr<sf::TcpSocket> user, Json::Value info);
    bool connectionExists(std::string user);

    std::thread listenerThread;
    std::unique_ptr<sf::TcpListener> listener; // Socket listener
    std::unique_ptr<sf::SocketSelector> selector;
    bool listening;

    // Heart beat
    void heartbeat(std::string);
    void ping(std::string name);
    void pong(std::string name, Json::Value message);
    void updatePing(std::string name, Json::Value message);

    // Message handling
    void handleMessage(Message message);
    void handleContent(Message message);
    void sendMessage(std::string user, Message message);
    void forwardMessage(Message message);
    bool isSystemMessage(Message message);
    Message craftMessage(std::string user, std::string type, Json::Value contents, bool directRoute = false);

    std::thread heartbeatThread;
    bool sendingHeartbeats;

    // Connection exploring
    void searchConnections(std::string user);
    void sendConnections(std::string user);
    void receiveConnections(std::string user, Json::Value message);
    void requestConnections(std::string user, std::vector<std::string> requestedUsers);
    void sendRequestedConnections(std::string user, std::vector<std::string> requestedUsers);

    // Route handling
    bool isInRoute(std::string newUser, std::string routeToCheck);
    void requestRoute(std::string user, std::string route);
    void sendRoute(std::string user, std::string route);
    void receiveRoute(std::string user, Json::Value message);
    std::map<std::string, std::vector<std::string>> routingTable;
};
#endif // __MESHNODE_H__
