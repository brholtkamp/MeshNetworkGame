#include "Game.h"

Game::Game(std::string name): playerName(name) {
    messageTypes.push_back("move");
    messageTypes.push_back("start");
    messageTypes.push_back("ready");
    messageTypes.push_back("tag");

    gameStarted = false;
    imReady = false;

    if (!font.loadFromFile("sansation.ttf")) {
        log << "Failed to load text" << std::endl;
    }
}

void Game::handleMessage(std::string sender, std::string type, Json::Value message) {
    if (type == "move") {
        log << "Got move" << std::endl;
        movePlayer(sender, message);
    } else if (type == "start") {
        startGame(false);
    } else if (type == "ready") {
        readyPlayers[sender] = true;
        players[sender].x = message["x"].asUInt();
        players[sender].y = message["y"].asUInt();

        if (message["color"].asString() == "green") {
            players[sender].color = sf::Color::Green;
            taggedPlayer = sender;
        } else if (message["color"].asString() == "blue") {
            players[sender].color = sf::Color::Blue;
        } else if (message["color"].asString() == "yellow") {
            players[sender].color = sf::Color::Yellow;
        } else if (message["color"].asString() == "black") {
            players[sender].color = sf::Color::Black;
        } else if (message["color"].asString() == "magenta") {
            players[sender].color = sf::Color::Magenta;
        }

        log << sender << " is ready to play!" << std::endl;
    } else if (type == "tag") {

    } else {
        log << "Unknown game message: " << type << std::endl << message.toStyledString() << std::endl;
    }
}

void Game::startGame(bool initiatedStart) {
    if (allPlayersReady()) {
        if (initiatedStart) {
            Json::Value message;
            message["starter"] = playerName;
            node->broadcast("start", message);
        }
        gameStarted = true;
        std::thread(&Game::playGame, this).detach();
    } else {
        log << "Not all players are ready" << std::endl;
    }
}

void Game::readyUp() {
    unsigned int input = 10;
    Json::Value message;

    while (input > 5) {
        out << "Please choose a color: " << std::endl   
            << "\t1: Green" << std::endl
            << "\t2: Blue" << std::endl
            << "\t3: Yellow" << std::endl
            << "\t4: Black" << std::endl
            << "\t5: Magenta" << std::endl;
        in >> input;
    }

    switch (input) {
    case 1:
        players[playerName].color = sf::Color::Green;
        players[playerName].x = 0;
        players[playerName].y = 0;
        out << "You will be IT first" << std::endl;
        message["color"] = "green";
        taggedPlayer = playerName;
        break;
    case 2:
        players[playerName].color = sf::Color::Blue;
        players[playerName].x = 9;
        players[playerName].y = 9;
        message["color"] = "blue";
        break;
    case 3:
        players[playerName].color = sf::Color::Yellow;
        players[playerName].x = 2;
        players[playerName].y = 2;
        message["color"] = "yellow";
        break;
    case 4:
        players[playerName].color = sf::Color::Black;
        players[playerName].x = 6;
        players[playerName].y = 4;
        message["color"] = "black";
        break;
    case 5:
        players[playerName].color = sf::Color::Magenta;
        players[playerName].x = 0;
        players[playerName].y = 9;
        message["color"] = "magenta";
        break;
    }
    message["x"] = players[playerName].x;
    message["y"] = players[playerName].y;

    imReady = true;
    node->broadcast("ready", message);
}

void Game::playGame() {
    sf::RenderWindow window(sf::VideoMode(windowSize, windowSize), "MeshNetworkGame");
    setupBoard();

    while(gameStarted && window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::KeyPressed) {
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
                    if (players[playerName].y > 0 && boardData[players[playerName].y-1][players[playerName].x] != wall && collideCheck(playerName, players[playerName].x, players[playerName].y-1)) {
                        players[playerName].y -= 1;
                    }
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
                    if (players[playerName].x > 0 && boardData[players[playerName].y][players[playerName].x-1] != wall && collideCheck(playerName, players[playerName].x-1, players[playerName].y)) {
                        players[playerName].x -= 1;
                    } 
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
                    if (players[playerName].y < boardDimension - 1 && boardData[players[playerName].y+1][players[playerName].x] != wall && collideCheck(playerName, players[playerName].x, players[playerName].y+1)) {
                        players[playerName].y += 1;
                    }
                } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
                    if (players[playerName].x < boardDimension - 1 && boardData[players[playerName].y][players[playerName].x+1] != wall && collideCheck(playerName, players[playerName].x+1, players[playerName].y)) {
                        players[playerName].x += 1;
                    }
                }
                move();
                checkForTag();

#if debug
                log << "Player position: " << players[playerName].x << ":" << players[playerName].y << std::endl;
#endif
            }
        }

        for (int i = 0; i < boardDimension; i++) {
            for (int j = 0; j < boardDimension; j++) {
                window.draw(boardRender[i][j]);
            }
        }

        for (auto player : players) {
            sf::CircleShape circle;
            if (player.first != taggedPlayer) {
                circle.setFillColor(player.second.color);
            } else {
                circle.setFillColor(sf::Color::Red);
            }
            circle.setRadius(50);
            circle.setPosition(static_cast<float>(tileSize * player.second.x), static_cast<float>(tileSize * player.second.y));
            window.draw(circle);
        }


#if debug
        for (int i = 0; i < boardDimension; i++) {
            for (int j = 0; j < boardDimension; j++) {
                sf::Text text;
                text.setFont(font);
                text.setString(std::to_string(i) + ":" + std::to_string(j));
                text.setCharacterSize(50);
                text.setColor(sf::Color::Black);
                text.setPosition(boardRender[i][j].getPosition().x, boardRender[i][j].getPosition().y);
                window.draw(text);
            }
        }
#endif


        window.display();
    }

    return;
}

void Game::move() {
#if debug
    log << "Moved to " << players[playerName].x << ":" << players[playerName].y << std::endl;
#endif
    Json::Value message;
    message["x"] = players[playerName].x;
    message["y"] = players[playerName].y;
    node->broadcast("move", message);
}

void Game::movePlayer(std::string sender, Json::Value message) {
#if debug
    log << "Player " << sender << " moved to " << players[sender].x << ":" << players[sender].y << std::endl;
#endif
    players[sender].x = message["x"].asUInt();
    players[sender].y = message["y"].asUInt();
}

void Game::checkForTag() {
    for (auto player : players) {
        for (auto otherPlayer : players) {
            if (player.first != otherPlayer.first) {
                // If they're within 1 tile of each other
                if ((player.second.x - otherPlayer.second.x == -1 && player.second.y - otherPlayer.second.y == -1) ||   // Up-Left
                    (player.second.x - otherPlayer.second.x == -1 && player.second.y - otherPlayer.second.y == 0) ||    // Up
                    (player.second.x - otherPlayer.second.x == -1 && player.second.y - otherPlayer.second.y == 1) ||    // Up-Right
                    (player.second.x - otherPlayer.second.x == 0 && player.second.y - otherPlayer.second.y == -1) ||    // Left
                    (player.second.x - otherPlayer.second.x == 1 && player.second.y - otherPlayer.second.y == -1) ||    // Down-Left
                    (player.second.x - otherPlayer.second.x == 1 && player.second.y - otherPlayer.second.y == 0) ||     // Down
                    (player.second.x - otherPlayer.second.x == 1 && player.second.y - otherPlayer.second.y == 1) ||     // Down-Right
                    (player.second.x - otherPlayer.second.x == 0 && player.second.y - otherPlayer.second.y == 1)        // Right
                    ){

                    // If one of them is tagged and they've made distance, tag them
                    if (player.first == taggedPlayer && madeDistance) {
                        taggedPlayer = otherPlayer.first;
                        previouslyTaggedPlayer = player.first;
                        madeDistance = false;
                    } else if (otherPlayer.first == taggedPlayer && madeDistance) {
                        taggedPlayer = player.first;
                        previouslyTaggedPlayer = otherPlayer.first;
                        madeDistance = false;
                    }
                }
            }
        }
    }

    if (!madeDistance) {
        double distance;
        if ((distance = std::sqrt(std::pow(static_cast<double>(players[taggedPlayer].x) - static_cast<double>(players[previouslyTaggedPlayer].x), 2.0) + std::pow(static_cast<double>(players[taggedPlayer].y) - static_cast<double>(players[previouslyTaggedPlayer].y), 2.0))) >= kDistanceAmount) {
#if debug
            log << "Distance achieved!" << distance << std::endl;
            log << "Sqrt( (" << players[taggedPlayer].x << " - " << players[previouslyTaggedPlayer].x << ")^2 + (" << players[taggedPlayer].y << " - "  << players[previouslyTaggedPlayer].y << ")^2)" << std::endl;
#endif
            madeDistance = true;
        }
    }
}

bool Game::collideCheck(std::string player, unsigned int x, unsigned int y) {
    for (auto otherPlayer : players) {
        if (otherPlayer.second.x == x && otherPlayer.second.y == y) {
            return false;
        }
    }

    return true;
}

void Game::setupBoard() {
    std::ifstream boardFile;
    madeDistance = true;
    boardFile.open(boardFilename);

    if (boardFile.is_open()) {
        for (int i = 0; i < boardDimension; i++) {
            for (int j = 0; j < boardDimension; j++) {
                boardData[i][j] = boardFile.get();
                boardRender[i][j].setSize(sf::Vector2f(tileSize, tileSize));
                boardRender[i][j].setPosition(static_cast<float>(tileSize * j), static_cast<float>(tileSize * i));
                if (boardData[i][j] == wall) {
                    boardRender[i][j].setFillColor(sf::Color::Black);
                } else if (boardData[i][j] == space) {
                    boardRender[i][j].setFillColor(sf::Color::White);
                }
            }
            boardFile.get();
        }
    } else {
        log << "Could not open board file: " << boardFilename << std::endl;
    }

#if debug
    for (int i = 0; i < boardDimension; i++) {
        for (int j = 0; j < boardDimension; j++) {
            log << boardData[i][j] - 48 << " ";
        }
        log << std::endl;
    }
#endif

    boardFile.close();
}

bool Game::allPlayersReady() {
    for (auto& player : readyPlayers) {
        if (!player.second) {
            return false;
        }
    }

    if (readyPlayers.size() == node->numberOfConnections()) {
        return true;
    } else {
        return false;
    }
}