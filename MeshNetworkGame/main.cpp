#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

int main(int argc, char *argv[]) {
    char mode;
    std::cout << "Listen mode? (y/n)" << std::endl;
    std::cin >> mode;
    
    if (mode == 'n') {
        std::unique_ptr<MeshNode> node(new MeshNode());

        std::string address;
        unsigned short port;
 
        std::cout << "Please enter a host to connect to: " << std::endl;
        std::cin >> address;
        std::cout << "Please enter a port to use: "<< std::endl;
        std::cin >> port;
        
        if (node->connectTo(address, port)) {
#if DEBUG
            int counter = 0;
            Json::Value value;
            value["type"] = PING;
            while (counter < 10000) {
                node->sendTo(address, port, value);
                counter++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
#endif
        }    
    } else {
        std::unique_ptr<MeshNode> node(new MeshNode());

        while(true) {
            //Listening
        }
    }

    std::cin.ignore();
    return 0;
}