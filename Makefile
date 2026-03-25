SFML_PATH=/usr/local/Cellar/sfml/2.6.1/
CXXFLAGS= -std=c++14 -Wall -Wpedantic -I${SFML_PATH}include/
LDFLAGS=-L${SFML_PATH}lib/
CFLAGS=-g -lsfml-graphics -lsfml-window -lsfml-system -lsfml-network -pthread
CPPFLAGS=
LDLIBS=
LIBS=
CPP=g++
OBJS=GameServer.o main.o

all: server

server: $(OBJS)
	$(CPP) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(CFLAGS)

%.o: %.cpp
		$(CPP) $(CXXFLAGS) -c $< -o $@
clean:
	\rm -f *.o server