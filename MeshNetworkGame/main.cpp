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

    out << "Please enter a host to connect to: " << std::endl;
    in >> address;
     
    while (address != "quit") {
        out << "Please enter a port to use: "<< std::endl;
        in >> port;

        if (!node->connectTo(address, port)) {
                log << "Failed to connect to " << address << ":" << port << std::endl;
        }

        out << "Please enter a host to connect to (or quit to move into the menu): " << std::endl;
        in >> address;
    }

    std::string foo;

    while (foo != "quit") {
        out << "Type something to do " << std::endl;
        in >> foo;

        if (foo == "info") {
            node->listConnections();
        } else if (foo == "lag") {
            std::string user;
            unsigned int lag;

            out << "What user do you wish to add/remove lag? ";
            in >> user;
            out << "What amount of lag? ";
            in >> lag;

            node->setLag(user, lag);
        } else {
            Json::Value message;
            message["test"] = foo;
            node->broadcast("test", message);
        }
    }

    return 0;
}
