#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

using namespace std;

int main(int argc, char *argv[]) {
    std::unique_ptr<MeshNode> node(new MeshNode(1000));
    std::unique_ptr<MeshNode> node2(new MeshNode(1001));

    if (node->connectTo("127.0.0.1", 1001)) {
        int counter = 0;
        while (counter < 3) {
            node->broadcast("This is a test of the emergency broadcast system!");
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }    

    return 0;
}