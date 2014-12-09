#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"
#include "PingMessageHandler.h"
#include "SystemMessageHandler.h"

int main(int argc, char *argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::string username = "brian";
    std::unique_ptr<PingMessageHandler> pingHandler(new PingMessageHandler());
    std::unique_ptr<SystemMessageHandler> systemHandler(new SystemMessageHandler());
    std::unique_ptr<MeshNode> node(new MeshNode(10010, username));
    node->registerMessageHandler(std::move(pingHandler));
    node->registerMessageHandler(std::move(systemHandler));

    char mode;
    std::cout << "Listen mode? (y/n)" << std::endl;
    std::cin >> mode;
    
    if (mode == 'n') {
        std::string address;
        unsigned short port;
 
        std::cout << "Please enter a host to connect to: " << std::endl;
        std::cin >> address;
        std::cout << "Please enter a port to use: "<< std::endl;
        std::cin >> port;
        
        if (node->connectTo(address, port)) {
            while (node->numberOfConnections() > 0) {
                node->ping(address, port);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        } 
    } else {
        while(node->isListening()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    return 0;
}
