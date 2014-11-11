#ifndef __MESHNODE_H__
#define __MESHNODE_H__
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <SFML/System/Time.hpp>
#include <json/json.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <functional>
#include <memory>

static const sf::Int32 timeoutAmount = 15;

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
    sf::SocketSelector selector;
    std::vector<std::shared_ptr<sf::TcpSocket>> clients;
    sf::TcpListener listener;
    bool listening;
    unsigned short listen_port;
};

#endif // __MESHNODE_H__
