#include "GameServer.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
#include <cmath>
#include <chrono>

#include "GameMessage.h"
#include "Pathfinding.h"

// Map Dimensions, all rooms are the same size
constexpr uint16_t GameServer::MAP_WIDTH;
constexpr uint16_t GameServer::MAP_HEIGHT;

// Message Payload Sizes (excluding the initial type byte)
constexpr size_t GameServer::WORLD_STATE_HEADER;
constexpr size_t GameServer::MAP_DATA_SIZE;
constexpr size_t GameServer::ENTITY_DATA_SIZE;
constexpr size_t GameServer::MOVE_PAYLOAD_SIZE;
constexpr size_t GameServer::ATTACK_PAYLOAD_SIZE;

// Simulation and Broadcast Settings
constexpr float GameServer::DELTA_TIME;
constexpr float GameServer::TICK_RATE_MS;
constexpr int GameServer::BROADCAST_INTERVAL;
constexpr size_t GameServer::MAX_HISTORY_TICKS;

// Player Attack Variables
constexpr float GameServer::ATTACK_COOLDOWN;
constexpr float GameServer::ATTACK_RANGE;

// Constructor initialises the server with specified TCP and UDP ports and starts the simulation loop in a detached thread
GameServer::GameServer(unsigned short tcpPort, unsigned short udpPort) :
    m_tcpPort(tcpPort), m_udpPort(udpPort) 
{
	std::thread(&GameServer::simulationLoop, this).detach(); // Start the simulation loop in a detached thread
}

// Starts the TCP server, binds to the specified port, and listens for incoming client connections
// For each accepted client connection, a new thread is spawned to handle communication with that client - so that one client doesn't lag others
// It also assigns a unique ID to each client and initialises their player state in the authoritative server state
void GameServer::tcpStart()
{
	// Binds the listener to the specified TCP port and starts listening for incoming connections
    sf::TcpListener listener;
    sf::Socket::Status status = listener.listen(m_tcpPort);
	// If the listener fails to bind to the port, an error message is printed and the function returns
    if (status != sf::Socket::Status::Done)
    {
        std::cerr << "Error binding listener to port" << std::endl;
        return;
    }

    std::cout << "TCP Server is listening to port " << m_tcpPort << ", waiting for connections..." << std::endl;

	// Loop for incoming client connections
    while (m_running)
    {
        auto client = std::make_shared<sf::TcpSocket>();
        // A blocking call that waits until a client connects; done so that we don't consume CPU cycles when there are no incoming connections
		status = listener.accept(*client); 
		// If the connection is accepted successfully, 
        // the server assigns a unique ID to the client, initialises their player state, and starts a new thread to handle communication with that client
        if (status == sf::Socket::Status::Done)
        {
            int32_t assignedId;
			// Assigns a unique ID to the client and adds them to the list of connected clients
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                assignedId = m_nextId++;
                m_clients.push_back(client);
            }

			// Initialises the player's state in the authoritative server state
            {
                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                EntityState newState;
                newState.position = sf::Vector2f(0.0f, 0.0f);
                newState.type = EntityType::PLAYER;
                newState.speed = 5.0f;
                m_entityStates[assignedId] = newState;
            }

            std::cout << "New client connected: " << client->getRemoteAddress() << std::endl;
			status = client->send(&assignedId, sizeof(int32_t)); // Send the assigned ID to the client so they know their unique identifier in the game

			// If the server fails to send the assigned ID to the client, an error message is printed
            if (status != sf::Socket::Status::Done)
                std::cerr << "Could not send ID to client " << assignedId << std::endl;

            // Start a new thread to handle communication with the client, decoupled from the main server loop so that one slow client doesn't affect others
			std::thread(&GameServer::handleClient, this, client, assignedId).detach(); 
        }
    }
}

// Starts the UDP server, binds to the specified port, and enters a loop to receive messages from clients and send responses back
// This is used to echo client pings so they can discover the server's local IP address to connect to the TCP server
void GameServer::udpStart()
{
	// Binds the socket to the specified UDP port
    sf::UdpSocket socket;
    sf::Socket::Status status = socket.bind(m_udpPort);
	// If the socket fails to bind to the port, an error message is printed and the function returns
    if (status != sf::Socket::Status::Done) 
    {
        std::cerr << "Error binding socket to port " << m_udpPort << std::endl;
        return;
    }

    std::cout << "UDP Server started on port " << m_udpPort << std::endl;

	// UDP is connectionless, so we just loop around receiving messages from any client and sending responses back to the sender
    while (m_running) 
    {
		// Message Buffer and Sender Info
        char data[1024]; // Typical safe size for small UDP packets
		std::size_t received; // Actual size of the received data
        sf::IpAddress sender;
        unsigned short senderPort;

		// Attemps to receive a message from any client; this is a blocking call that waits until a message is received
        // Done so that we don't consume CPU cycles when there are no incoming messages
        status = socket.receive(data, sizeof(data), received, sender, senderPort);
		// If the server fails to receive data, an error message is printed and the loop continues to wait for the next message
        if (status != sf::Socket::Status::Done) 
        {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }

        std::cout << "Received: " << data << " from " << sender << ":" << senderPort << std::endl;

		// Echo the received message back to the sender; this allows clients to discover the server's local IP address
        status = socket.send(data, received, sender, senderPort);
		// If the server fails to send the response back to the client, an error message is printed
        if (status != sf::Socket::Status::Done) 
            std::cerr << "Error sending data" << std::endl;
    }

	socket.unbind(); // Unbind the socket when done, no longer accepting messages
    std::cout << "UDP Discovery: Thread terminated safely." << std::endl;
}

// Main loop that updates the game state at a fixed tick rate, processes player actions, and broadcasts world state to clients
void GameServer::simulationLoop()
{
    auto tickDuration = std::chrono::milliseconds(static_cast<long>(TICK_RATE_MS)); // Used to calculate how long to sleep thread for to maintain 60Hz
    int tickCount = 0; // Counter for broadcast interval

    // Simulation loop, runs at a fixed 60Hz tick rate
    // Handles authoritative entity movement, records historical snapshots for lag compensation, and broadcasts the world state to all connected clients with a 10Hz interval
    while (m_running) 
    {
        auto startTime = std::chrono::steady_clock::now(); // Records the exact moment the simulation begins its loop

        // Updates entity positions, uses these positions to build the 'WorldStateMessage', also pushes these finalised positions into a 'WorldSnapshot' for 'm_history' 
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);

            m_currentTick++; // Increments running total of server ticks

            // Processes the movement for all entities
            for (auto& pair : m_entityStates)
            {
                EntityState& state = pair.second;

                // Checks if the entity is moving, and currently has a path
                if (state.isMoving && !state.currentPath.empty())
                {
                    sf::Vector2f target = state.currentPath.front(); // Takes first node on the grid
                    sf::Vector2f direction = target - state.position;
                    float distSq = (direction.x * direction.x) + (direction.y * direction.y);
                    float moveStep = state.speed * DELTA_TIME; // Maximum allowed distance this tick
                    float moveStepSq = moveStep * moveStep; // Used to check distance to target

                    // Entity will overshoot movement this tick, just snap them
                    if (distSq <= moveStepSq)
                    {
                        state.position = target;
                        state.currentPath.erase(state.currentPath.begin());

                        if (state.currentPath.empty()) 
                            state.isMoving = false;
                    }
                    else // Standard movement 
                    {
                        float distance = std::sqrt(distSq);
                        state.position += (direction / distance) * moveStep; // Normalises the vector and moves them by the 'moveStep'
                    }
                }
            }

            // Increments the tick count to check if the elapsed broadcasting time has passed
            if (++tickCount >= BROADCAST_INTERVAL)
            {
                tickCount = 0; // Resets for following broadcasts

                WorldStateMessage worldMsg;
                worldMsg.tick = m_currentTick; // Gets the current tick to send with message for interpolation and lag compensation
                // Loops through all entities and adds their position to the 'WorldStateMessage' 
                for (auto const& pair : m_entityStates) 
                    worldMsg.entityPositions[pair.first] = pair.second.position;

                std::vector<uint8_t> bytes = worldMsg.serialise();

                broadcastMessage(bytes, nullptr); // Sends the message to everyone, hence nullptr sender ID
            }

            WorldSnapshot snap;
            snap.tick = m_currentTick; // Gets the current tick for the 'WorldSnapshot' for lag compensation

            // Loops through all entities adding their position to the 'WorldSnapshot'
            for (auto const& pair : m_entityStates)
            {
                int32_t id = pair.first;
                const EntityState& state = pair.second;
                snap.positions[id] = state.position;
            }

            m_history.push_back(snap); // Pushes the current tick's entity positions to the history

            // Checks if the max history is reached, if so removes the oldest
            if (m_history.size() > MAX_HISTORY_TICKS)
                m_history.pop_front();
        }

        // Calculates the elapsed time using the start time from the top of the loop
        auto endTime = std::chrono::steady_clock::now();
        auto elapsed = endTime - startTime;
        // If the tick processed faster than target duration; sleep for the remainder to make up for it
        if (elapsed < tickDuration) 
            std::this_thread::sleep_for(tickDuration - elapsed);
    }
}

// Runs in its own thread for each client, responsible for receiving messages from said client and processing them
void GameServer::handleClient(std::shared_ptr<sf::TcpSocket> client, int32_t clientId)
{
    // Loop waiting for messages from clients, blocks until type ID is received; then determines message type and size and processes it
    while (m_running)
    {
        uint8_t typeByte; // Message type ID
        size_t received; // Used to check if the whole message was received

        sf::Socket::Status status = client->receive(&typeByte, 1, received); // Blocking call waiting till the message type ID is received
        // Check to ensure the client is still properly connected
        if (status == sf::Socket::Disconnected || status == sf::Socket::Error) 
        {
            std::cerr << "Client " << clientId << " Disconnected!";
            break;
        }

        // Checks if the message type ID was received, so it can be processed
        if (received == 1) 
        {
            GameMessageType type = static_cast<GameMessageType>(typeByte); // Assigns a message type based on the ID
            size_t remainingSize = 0; // Pre-defines the exact byte length to expect for the rest of the payload, to avoid overflows

            // Assigns payload size based on type IDs
            if (type == GameMessageType::PLAYER_MOVE) 
                remainingSize = MOVE_PAYLOAD_SIZE;
            else if (type == GameMessageType::PLAYER_ATTACK) 
                remainingSize = ATTACK_PAYLOAD_SIZE;
            else if (type == GameMessageType::MAP_DATA)
                remainingSize = MAP_DATA_SIZE;
            else 
            {
                std::cerr << "Unknown message type received: " << (int)typeByte << std::endl;
                break;
            }

            // Allocate a buffer for the complete payload, plus one for the type ID that was already consumed
            std::vector<uint8_t> fullPacket(remainingSize + 1);
            fullPacket[0] = typeByte; // Puts the ID back to the front

            status = client->receive(&fullPacket[1], remainingSize, received); // Blocking call that reads the message after the header

            // Checks a message is received, and its the expected size
            if (status == sf::Socket::Done && received == remainingSize) 
            {
                auto msg = GameMessageFactory::create(fullPacket); // The factory deserialises the messages so they can be processed

                // If the message was successfully recognised
                if (msg) 
                {
                    // ------------------------------------------------
                    // Checks the message type and processes it
                    // Downcasts generic message pointer to access specific message variables
                    // ------------------------------------------------

                    if (msg->type == GameMessageType::PLAYER_MOVE) 
                    {
                        auto move = static_cast<PlayerMoveMessage*>(msg.get());
                        {
                            sf::Vector2f startPos;
                            MapGrid mapCopy; // Used to temporarily avoid players, so they're not marked as permanent obstacles
                            // Gathers a snapshot of data for pathfinding: start position and collision grid (avoiding other entities)
                            {
                                std::lock_guard<std::mutex> stateLock(m_stateMutex);

                                if (m_entityStates.find(move->id) == m_entityStates.end()) return; // Checks ID is valid

                                // If position is 0, 0; this is the first move message so set their position directly without pathfinding
                                if (m_entityStates[move->id].position.x == 0.0f && m_entityStates[move->id].position.y == 0.0f)
                                {
                                    m_entityStates[move->id].position = sf::Vector2f(move->posX, move->posY);
                                    std::cout << "Initial sync: Player " << move->id << " teleported to " << move->posX << ", " << move->posY << std::endl;

                                    broadcastMessage(fullPacket, client);
                                    continue;
                                }

                                // Checks whether they're currently moving and have a path, if so their start position becomes the node they're walking towards
                                // Done so the pathfinding grid is always aligned to the centre of a tile
                                // If not, they're stationary and should be in the centre of a tile
                                startPos = (m_entityStates[move->id].isMoving && !m_entityStates[move->id].currentPath.empty())
                                 ? m_entityStates[move->id].currentPath.front() : m_entityStates[move->id].position;

                                mapCopy = m_currentMap; // Takes a snapshot of the map's current state

                                // Loops through all other entities and turns them into obstacles, to be pathfound around
                                for (const auto& pair : m_entityStates)
                                {
                                    // Check to ensure client isn't marked as an obstacle for themself
                                    if (pair.first != move->id)
                                    {
                                        // Snaps to int coords
                                        int px = static_cast<int>(pair.second.position.x);
                                        int py = static_cast<int>(pair.second.position.y);

                                        // Safety check to ensure coords are within map boundaries
                                        if (px >= 0 && px < mapCopy.width && py >= 0 && py < mapCopy.height)
                                            mapCopy.collision[py * mapCopy.width + px] = true;
                                    }
                                }
                            }

                            // True authoritative server path
                            auto truePath = Pathfinding::findPath((int)startPos.x, (int)startPos.y, (int)move->posX, (int)move->posY, mapCopy.collision, mapCopy.width, mapCopy.height);
                            
                            // Checks whether a path was found
                            if (!truePath.empty())
                            {
                                std::lock_guard<std::mutex> stateLock(m_stateMutex);
                                // Checks ID is valid
                                if (m_entityStates.find(move->id) != m_entityStates.end())
                                {
                                    m_entityStates[move->id].currentPath = truePath; // Overrides current path with the true authoritative path
                                    m_entityStates[move->id].isMoving = true;
                                }
                            }
                            else
                                std::cout << "Invalid path requested by Player " << move->id << std::endl;
                        }
                        std::cout << "Relaying Move: Player " << move->id << " to " << move->posX << ", " << move->posY << std::endl;
                    }
                    else if (msg->type == GameMessageType::PLAYER_ATTACK) 
                    {
                        auto attack = static_cast<PlayerAttackMessage*>(msg.get());
                        processAttack(attack->id, attack->tick); // Processes attack, collision checking, with lag compensation
                    }
                    else if (msg->type == GameMessageType::MAP_DATA)
                    {
                        auto mapMsg = static_cast<MapDataMessage*>(msg.get());
                        // Updates the server's map data with that which the client sends
                        {
                            std::lock_guard<std::mutex> stateLock(m_stateMutex);
                            m_currentMap.width = mapMsg->width;
                            m_currentMap.height = mapMsg->height;
                            m_currentMap.collision.clear();
                            // Clears the old collision data, and updates it with the new just received one
                            for (uint8_t cell : mapMsg->grid) 
                                m_currentMap.collision.push_back(cell == 1); // Checks if cell is an obstacle
                        }
                        std::cout << "Server received Map Data: " << mapMsg->width << "x" << mapMsg->height << std::endl;
                    }

                    broadcastMessage(fullPacket, client); // Sends the message to all OTHER clients
                }
            }
        }
    }

    // Clears the processed entity from the game world, as the server is no longer running or the client has disconnected 
    {
        std::lock_guard<std::mutex> stateLock(m_stateMutex);
        m_entityStates.erase(clientId);
    }

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client), m_clients.end()); // Removes the client's socket from the broadcast list, cleanly
}

// Sends a message from the server to all connected clients except the sender (if specified)
void GameServer::broadcastMessage(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender)
{
    std::lock_guard<std::mutex> lock(m_clientsMutex); // Protects the client list 
    // Loops through all connected clients
    for (auto& client : m_clients)
    {
        // Only broadcasts to OTHER clients, not the sender
        if (client != sender)
        {
            sf::Socket::Status status = client->send(message.data(), message.size()); // Sends the broadcast message
            // If the server fails to send the broadcast, an error message is printed
            if (status != sf::Socket::Status::Done)
                std::cerr << "Error sending message to client" << std::endl;
        }
    }
}

// Processes an attack action from a player 
// Checking for cooldowns and determining if anything was hit
// Based on their positions in the historical snapshot of the world at the time of the attack
void GameServer::processAttack(int32_t attackerId, uint32_t historicalTick)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);

    auto it = m_entityStates.find(attackerId);
    if (it == m_entityStates.end()) return;

    if (it->second.attackTimer.getElapsedTime().asSeconds() < ATTACK_COOLDOWN) return;
    it->second.attackTimer.restart();

    WorldSnapshot targetSnap;
    if (!m_history.empty()) 
    {
        targetSnap = m_history.back();

        for (auto rit = m_history.rbegin(); rit != m_history.rend(); ++rit) 
        {
            if (rit->tick <= historicalTick) 
            {
                targetSnap = *rit;
                break;
            }
        }
    }
    else 
        return; // Server just booted, no history available

    if (targetSnap.positions.find(attackerId) == targetSnap.positions.end()) return;
    sf::Vector2f attackerPos = targetSnap.positions[attackerId];

    std::cout << "Player " << attackerId << " attacked at tick " << historicalTick << " from " << attackerPos.x << ", " << attackerPos.y << std::endl;

    for (const auto& pair : targetSnap.positions) 
    {
        int32_t targetId = pair.first;
        if (targetId == attackerId) continue;

        auto liveEntity = m_entityStates.find(targetId);
        if (liveEntity == m_entityStates.end() || liveEntity->second.type == EntityType::PLAYER)
            continue;

        sf::Vector2f targetPos = pair.second;

        float dx = attackerPos.x - targetPos.x;
        float dy = attackerPos.y - targetPos.y;
        float distanceSquared = (dx * dx) + (dy * dy);
        float rangeSquared = ATTACK_RANGE * ATTACK_RANGE;

        if (distanceSquared <= rangeSquared)
        {
            std::cout << "HIT! Player " << targetId << " was in range at tick " << historicalTick << std::endl;

            // TODO: Broadcast an ENTITY_DAMAGED message
        }
        else
            std::cout << "MISS: Player " << targetId << " was too far away at tick " << historicalTick << std::endl;
    }
}