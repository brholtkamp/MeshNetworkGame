#ifndef __MESHNODE_H__
#define __MESHNODE_H__
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <json/json.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <thread>
#include <functional>
#include <memory>

const int listeningTimeout = 5000;

class MeshNode {
public:
    MeshNode(unsigned short input_listen_port = 1010);
    ~MeshNode();

    void listen();
    bool connectTo(std::string address, unsigned short port);
    bool broadcast(std::string message);
    sf::TcpSocket getSocketOf(std::string address);

private:
    std::thread listening_thread;
    std::unique_ptr<sf::SocketSelector> selector;
    std::unique_ptr<std::vector<std::shared_ptr<sf::TcpSocket>>> clients;
    std::unique_ptr<sf::TcpListener> listener;
    bool listening;
    unsigned short listen_port;
};

#endif // __MESHNODE_H__
