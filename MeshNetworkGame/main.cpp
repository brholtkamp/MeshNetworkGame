#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <chrono>

#include "MeshNode.h"

using namespace std;

int main() {
    MeshNode node(1000);
    MeshNode node2(1001);
    if (node.connectTo("127.0.0.1", 1001)) {
        node.broadcast("This is a test of the emergency broadcast system!");
    }

    cin.get();
    
    return 0;
}