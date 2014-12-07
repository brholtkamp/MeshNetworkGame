#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"
#include "PingMessageHandler.h"
#include "SystemMessageHandler.h"

int main(int argc, char *argv[]) {
    std::shared_ptr<PingMessageHandler> pingHandler(new PingMessageHandler());
    std::shared_ptr<SystemMessageHandler> systemHandler(new SystemMessageHandler());
    std::shared_ptr<MeshNode> node(new MeshNode(10010));
    node->registerMessageHandler(pingHandler);
    node->registerMessageHandler(systemHandler);

   char mode;
    std::cout << "Listen mode? (y/n/t)" << std::endl;
    std::cin >> mode;
    
    if (mode == 'n') {
        std::string address;
        unsigned short port;
 
        std::cout << "Please enter a host to connect to: " << std::endl;
        std::cin >> address;
        std::cout << "Please enter a port to use: "<< std::endl;
        std::cin >> port;
        
        if (node->connectTo(address, port)) {
            int counter = 0;
            while (counter < 10000) {
                node->ping(address, port);
                counter++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } 
    } else if (mode == 't') {
        std::string address;
        address = "127.0.0.1";
        unsigned short port;
        port = 10010;

        if (node->connectTo(address, port)) {
            int counter = 0;
            while (counter < 10000) {
                node->ping(address, port);
                counter++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    } else {
        while(node->isListening()) {

        }
    }

    std::cin.ignore();
    return 0;
}
