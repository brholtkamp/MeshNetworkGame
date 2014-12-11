#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

int main(int argc, char *argv[]) {
    std::string name;

    std::cout << "Please give your node a name" << std::endl;
    std::cin >> name;

    std::unique_ptr<MeshNode> node(new MeshNode(10010, name));

    std::string address;
    unsigned short port;

    std::cout << "Please enter a host to connect to: " << std::endl;
    std::cin >> address;
     
    while (address != "quit") {
        std::cout << "Please enter a port to use: "<< std::endl;
        std::cin >> port;

        if (!node->connectTo(address, port)) {
                log << "Failed to connect to " << address << ":" << port << std::endl;
        }

        std::cout << "Please enter a host to connect to: " << std::endl;
        std::cin >> address;
    }

    std::string foo;

    while (foo != "quit") {
        std::cout << "Type something to say" << std::endl;
        std::cin >> foo;

        if (foo == "info") {
            node->listConnections();
        } else {
            Json::Value message;
            message["test"] = foo;
            node->broadcast("test", message);
        }
    }

    return 0;
}
