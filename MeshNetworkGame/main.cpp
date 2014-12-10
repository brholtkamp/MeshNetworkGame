#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

int main(int argc, char *argv[]) {
    std::unique_ptr<PingMessageHandler> pingHandler(new PingMessageHandler());
    std::unique_ptr<SystemMessageHandler> systemHandler(new SystemMessageHandler());
    std::unique_ptr<MeshNode> node(new MeshNode(10010));

    while (true) {
        std::string address;
        unsigned short port;
     
        std::cout << "Please enter a host to connect to: " << std::endl;
        std::cin >> address;
        std::cout << "Please enter a port to use: "<< std::endl;
        std::cin >> port;
            
        if (!node->connectTo(address, port)) {
            Log << "Failed to connect to " << address << ":" << port << std::endl;
        }
    }

    return 0;
}
