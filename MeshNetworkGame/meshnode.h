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
#include <ctime>
#include <sstream>
#include <thread>
#include <functional>
#include <memory>

#include "Log.h"

typedef std::tuple<sf::IpAddress, unsigned short, std::shared_ptr<sf::TcpSocket>> Connection;

const int kListeningTimeout = 5000;
const int kListeningPort = 10010;

enum MessageTypes {
    PING,
    PONG
};

struct Message {
    Json::Value message;
    Connection connection;
    unsigned int id;
};

struct OptimalConnection {
    sf::IpAddress remote_address;
    std::vector<sf::IpAddress> address_chain;
};

class MeshNode {
public:
    MeshNode(unsigned short input_listen_port = kListeningPort);
    ~MeshNode();

    void listen(); // Main function for handling responses
    void startListening();
    void stopListening();
    bool isListening();
    unsigned short getListeningPort();

    void handleMessages();
    void startHandlingMessages();
    void stopHandlingMessages();
    bool isHandlingMessages();

    bool connectTo(sf::IpAddress address, unsigned short port);
    bool sendTo(sf::IpAddress address, unsigned short port, std::string message);
    bool sendTo(sf::IpAddress address, unsigned short port, Json::Value message);
    bool optimizeFor(sf::IpAddress address, unsigned short port);
    void closeConnection(Connection connection);
    bool checkConnection(Connection connection);
    bool optimizeConnection(Connection connection);
    int numberOfConnections();

    bool broadcast(std::string message);
    bool broadcast(Json::Value message);
    bool ping(sf::IpAddress address, unsigned short port);
    void pong(Message ping);
private:
    // Listening
    std::thread listening_thread;
    std::unique_ptr<sf::SocketSelector> selector;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening;
    unsigned short listen_port;

    // Current connections
    std::vector<Connection> connections; 
    std::vector<OptimalConnection> optimal_connections;

    //Messaging Queues
    std::thread message_handler_thread;
    std::deque<Message> outgoing_messages;
    std::deque<Message> incoming_messages;
    unsigned int current_id;
    bool handling_messages;

};

#endif // __MESHNODE_H__