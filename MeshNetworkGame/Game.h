#ifndef __GAME_HANDLER_H__
#define __GAME_HANDLER_H__
#include <SFML/Graphics.hpp>

#include <iostream>
#include <fstream>

#include <math.h>
#include <string>
#include <map>

#include "MessageHandler.h"

#define debug 0
#define boardFilename "board.txt"
#define fontFilename "sansation.ttf"
#define boardDimension 10
#define windowSize 1000
#define tileSize windowSize / boardDimension
#define wall 49
#define space 48

const int kDistanceAmount = 4;

struct Player {
    unsigned int x;
    unsigned int y;
    sf::Color color;
};

class Game : public MessageHandler {
public:
    Game(std::string name);
    void startGame(bool intiatedStart);
    void readyUp();
    void playGame();
    void handleMessage(std::string sender, std::string type, Json::Value message);

private:
    // Game state mechanisms
    bool gameStarted;
    std::thread gameThread;

    // Ready mechanics
    bool imReady;
    bool allPlayersReady();
    std::map<std::string, bool> readyPlayers;

    // Game information
    void movePlayer(std::string sender, Json::Value message);
    void move();
    void checkForTag();
    bool collideCheck(std::string player, unsigned int x, unsigned int y);

    std::string playerName;
    std::map<std::string, Player> players;
    std::string taggedPlayer;
    std::string previouslyTaggedPlayer;
    bool madeDistance;

    // Game graphics
    sf::Font font;
    sf::RectangleShape boardRender[boardDimension][boardDimension];
    unsigned int boardData[boardDimension][boardDimension];
    void setupBoard();
};

#endif // __GAME_HANDLER_H___