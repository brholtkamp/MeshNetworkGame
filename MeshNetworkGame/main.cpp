#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

int main(int argc, char *argv[]) {
    char mode;
    std::cout << "Listen mode? (y/n/t)" << std::endl;
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
            int counter = 0;
            while (counter < 10000) {
                if (node->ping(address, port)) {
                    counter++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } else {
                    break;
                }
            }
        } 
    } else if (mode == 't') {
        std::unique_ptr<MeshNode> node(new MeshNode());

        std::string address;
        address = "127.0.0.1";
        unsigned short port;
        port = 10010;

        if (node->connectTo(address, port)) {
            int counter = 0;
            while (counter < 10000) {
                if (node->ping(address, port)) {
                    counter++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                } else if (node->numberOfConnections() == 0) {
                    break;
                }
            }
        }
    } else {
        std::unique_ptr<MeshNode> node(new MeshNode());
        bool started = false;
        while(true) {
            //Listening
            if (!started && node->numberOfConnections() != 0) {
                started = true;
            } else if (started && node->numberOfConnections() == 0) {
                break;
            } else {

            }
        }
    }

    std::cin.ignore();
    return 0;
}