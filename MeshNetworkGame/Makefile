CC=g++
JSON=/usr/local/Cellar/jsoncpp/0.6.0-rc2/include/jsoncpp/
SFML=/usr/local/Cellar/sfml/HEAD/include/SFML/
FLAGS=-std=c++11 -ljsoncpp -lsfml-graphics -lsfml-network -lsfml-system -lsfml-window
all:
	$(CC) *.cpp -I$(JSON) -I$(SFML) $(FLAGS) -o game

clean:
	rm game

