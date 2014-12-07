CC = clang 
JSON=/usr/local/Cellar/jsoncpp/0.6.0-rc2/include/jsoncpp/
SFML=/usr/local/Cellar/sfml/HEAD/include/SFML/
FLAGS=-std=c++11

all:
	$(CC) $(FLAGS) *.cpp -I$(JSON) -I$(SFML) -ljsoncpp -lsfml -o game

clean:
	rm game

