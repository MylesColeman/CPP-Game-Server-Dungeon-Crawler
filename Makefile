# Path to SFML installation
SFML_PATH=/usr/local/Cellar/sfml/2.6.1/

# Compiler to use
CXX = g++
# Tells the compiler where to look for SFML headers
CPPFLAGS = -I${SFML_PATH}include/
# Flags passed to compiler, forces C++14 standard, enables all warnings
CXXFLAGS = -std=c++14 -Wall -Wpedantic -g
# Flags passed to linker, tells the linker where to look for the SFML libraries
LDFLAGS = -L${SFML_PATH}lib/
# Flags passed to linker, tells which SFML libraries to use and generates debug information
LDLIBS = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-network -pthread

# Object files to be generated
OBJS=GameServer.o main.o Pathfinding.o

# Default target, builds the server executable
all: server

# Rule to link object files into the final ./server executable
server: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

# Specifies dependencies for each object file
GameServer.o: GameServer.cpp GameServer.h GameMessage.h Pathfinding.h
main.o: main.cpp GameServer.h
Pathfinding.o: Pathfinding.cpp Pathfinding.h
# Fallback rule to compile any .cpp file into a .o file, if no specific rule is defined for that file
%.o: %.cpp
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
# Rule to clean up the build directory by removing all .o files and the server executable
clean:
	\rm -f *.o server