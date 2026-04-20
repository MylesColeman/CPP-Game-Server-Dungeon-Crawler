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
	std::thread(&GameServer::simulation_loop, this).detach(); // Start the simulation loop in a detached thread
}

// Starts the TCP server, binds to the specified port, and listens for incoming client connections
// For each accepted client connection, a new thread is spawned to handle communication with that client - so that one client doesn't lag others
// It also assigns a unique ID to each client and initialises their player state in the authoritative server state
void GameServer::tcpStart()
{
	// Binds the listener to the specified TCP port and starts listening for incoming connections
    sf::TcpListener listener;
    sf::Socket::Status status = listener.listen(m_tcp_port);
	// If the listener fails to bind to the port, an error message is printed and the function returns
    if (status != sf::Socket::Status::Done)
    {
        std::cerr << "Error binding listener to port" << std::endl;
        return;
    }

    std::cout << "TCP Server is listening to port " << m_tcp_port << ", waiting for connections..." << std::endl;

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
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                assignedId = m_next_id++;
                m_clients.push_back(client);
            }

			// Initialises the player's state in the authoritative server state
            {
                std::lock_guard<std::mutex> state_lock(m_state_mutex);
                PlayerState newState;
                newState.position = sf::Vector2f(0.0f, 0.0f);
                newState.type = EntityType::PLAYER;
                newState.speed = 5.0f;
                m_entity_states[assignedId] = newState;
            }

            std::cout << "New client connected: " << client->getRemoteAddress() << std::endl;
			status = client->send(&assignedId, sizeof(int32_t)); // Send the assigned ID to the client so they know their unique identifier in the game

			// If the server fails to send the assigned ID to the client, an error message is printed
            if (status != sf::Socket::Status::Done)
                std::cerr << "Could not send ID to client " << assignedId << std::endl;

            // Start a new thread to handle communication with the client, decoupled from the main server loop so that one slow client doesn't affect others
			std::thread(&GameServer::handle_client, this, client, assignedId).detach(); 
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

void GameServer::simulationLoop()
{
    auto tick_duration = std::chrono::milliseconds(static_cast<long>(TICK_RATE_MS));
    int tick_count = 0;

    while (m_running) {
        auto start_time = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(m_state_mutex);

            m_current_tick++;

            for (auto& pair : m_entity_states)
            {
                PlayerState& state = pair.second;

                if (state.isMoving && !state.currentPath.empty())
                {
                    sf::Vector2f target = state.currentPath.front();
                    sf::Vector2f direction = target - state.position;
                    float distSq = (direction.x * direction.x) + (direction.y * direction.y);
                    float moveStep = state.speed * DELTA_TIME;
                    float moveStepSq = moveStep * moveStep;

                    if (distSq <= moveStepSq)
                    {
                        state.position = target;
                        state.currentPath.erase(state.currentPath.begin());

                        if (state.currentPath.empty()) 
                            state.isMoving = false;
                    }
                    else
                    {
                        float distance = std::sqrt(distSq);
                        state.position += (direction / distance) * moveStep;
                    }
                }
            }

            if (++tick_count >= BROADCAST_INTERVAL)
            {
                tick_count = 0;

                WorldStateMessage worldMsg;
                worldMsg.tick = m_current_tick;
                for (auto const& pair : m_entity_states) 
                    worldMsg.entityPositions[pair.first] = pair.second.position;

                std::vector<uint8_t> bytes = worldMsg.serialise();

                broadcast_message(bytes, nullptr);
            }

            WorldSnapshot snap;
            snap.tick = m_current_tick;

            for (auto const& pair : m_entity_states)
            {
                int32_t id = pair.first;
                const PlayerState& state = pair.second;
                snap.positions[id] = state.position;
            }

            m_history.push_back(snap);

            if (m_history.size() > MAX_HISTORY_TICKS)
                m_history.pop_front();
        }

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed = end_time - start_time;
        if (elapsed < tick_duration) 
            std::this_thread::sleep_for(tick_duration - elapsed);
    }
}

// Loop around, receive messages from client and send them to all
// the other connected clients.
void GameServer::handleClient(std::shared_ptr<sf::TcpSocket> client, int32_t my_id)
{
    // RECEIVING
    while (true)
    {
        uint8_t type_byte;
        size_t received;

        sf::Socket::Status status = client->receive(&type_byte, 1, received);
        if (status == sf::Socket::Disconnected || status == sf::Socket::Error) 
        {
            std::cerr << "Client Disconnected!" << std::endl;
            break;
        }

        if (received == 1) 
        {
            GameMessageType type = static_cast<GameMessageType>(type_byte);
            size_t remaining_size = 0;

            if (type == GameMessageType::PLAYER_MOVE) 
                remaining_size = MOVE_PAYLOAD_SIZE;
            else if (type == GameMessageType::PLAYER_ATTACK) 
                remaining_size = ATTACK_PAYLOAD_SIZE;
            else if (type == GameMessageType::MAP_DATA)
                remaining_size = MAP_DATA_SIZE;
            else 
            {
                std::cerr << "Unknown message type received: " << (int)type_byte << std::endl;
                break;
            }

            std::vector<uint8_t> full_packet(remaining_size + 1);
            full_packet[0] = type_byte;

            status = client->receive(&full_packet[1], remaining_size, received);

            if (status == sf::Socket::Done) 
            {
                auto msg = GameMessageFactory::create(full_packet);

                if (msg) 
                {
                    if (msg->type == GameMessageType::PLAYER_MOVE) 
                    {
                        auto move = static_cast<PlayerMoveMessage*>(msg.get());
                        {
                            sf::Vector2f startPos;
                            MapGrid mapCopy;
                            {
                                std::lock_guard<std::mutex> state_lock(m_state_mutex);

                                if (m_entity_states.find(move->id) == m_entity_states.end()) return;

                                // If position is 0, 0; this is the first move message so set their position directly without pathfinding
                                if (m_entity_states[move->id].position.x == 0.0f && m_entity_states[move->id].position.y == 0.0f)
                                {
                                    m_entity_states[move->id].position = sf::Vector2f(move->posX, move->posY);
                                    std::cout << "Initial sync: Player " << move->id << " teleported to " << move->posX << ", " << move->posY << std::endl;

                                    broadcast_message(full_packet, client);
                                    continue;
                                }

                                startPos = (m_entity_states[move->id].isMoving && !m_entity_states[move->id].currentPath.empty())
                                    ? m_entity_states[move->id].currentPath.front() : m_entity_states[move->id].position;

                                mapCopy = m_current_map;

                                for (const auto& pair : m_entity_states)
                                {
                                    if (pair.first != move->id && pair.second.type == EntityType::PLAYER)
                                    {
                                        int px = static_cast<int>(pair.second.position.x);
                                        int py = static_cast<int>(pair.second.position.y);

                                        if (px >= 0 && px < mapCopy.width && py >= 0 && py < mapCopy.height)
                                            mapCopy.collision[py * mapCopy.width + px] = true;
                                    }
                                }
                            }

                            auto truePath = Pathfinding::findPath((int)startPos.x, (int)startPos.y, (int)move->posX, (int)move->posY, 
                                mapCopy.collision, mapCopy.width, mapCopy.height);
                            
                            if (!truePath.empty())
                            {
                                std::lock_guard<std::mutex> state_lock(m_state_mutex);
                                if (m_entity_states.find(move->id) != m_entity_states.end())
                                {
                                    m_entity_states[move->id].currentPath = truePath;
                                    m_entity_states[move->id].isMoving = true;
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
                        process_attack(attack->id, attack->tick);
                    }
                    else if (msg->type == GameMessageType::MAP_DATA)
                    {
                        auto mapMsg = static_cast<MapDataMessage*>(msg.get());
                        {
                            std::lock_guard<std::mutex> state_lock(m_state_mutex);
                            m_current_map.width = mapMsg->width;
                            m_current_map.height = mapMsg->height;
                            m_current_map.collision.clear();
                            for (uint8_t b : mapMsg->grid) {
                                m_current_map.collision.push_back(b == 1);
                            }
                        }
                        std::cout << "Server received Map Data: " << mapMsg->width << "x" << mapMsg->height << std::endl;
                    }

                    broadcast_message(full_packet, client);
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        m_entity_states.erase(my_id);
    }

    std::lock_guard<std::mutex> lock(m_clients_mutex);
    m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client), m_clients.end());
}

// Sends `message` from `sender` to all the other connected clients
void GameServer::broadcastMessage(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender)
{
    // You might want to validate the message before you send it.
    // A few reasons for that:
    // 1. Make sure the message makes sense in the game.
    // 2. Make sure the sender is not cheating.
    // 3. First need to synchronise the players inputs (usually done in Lockstep).
    // 4. Compensate for latency and perform rollbacks (usually done in Ded Reckoning).
    // 5. Delay the sending of messages to make the game fairer wrt high ping players.
    // This is where you can write the authoritative part of the server.
    std::lock_guard<std::mutex> lock(m_clients_mutex);
    for (auto& client : m_clients)
    {
        if (client != sender)
        {
            // SENDING
            sf::Socket::Status status = client->send(message.data(), message.size());
            if (status != sf::Socket::Status::Done)
            {
                std::cerr << "Error sending message to client" << std::endl;
            }
        }
    }
}

void GameServer::processAttack(int32_t attacker_id, uint32_t historical_tick)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    auto it = m_entity_states.find(attacker_id);
    if (it == m_entity_states.end()) return;

    if (it->second.attackTimer.getElapsedTime().asSeconds() < ATTACK_COOLDOWN) return;
    it->second.attackTimer.restart();

    WorldSnapshot target_snap;
    if (!m_history.empty()) {
        target_snap = m_history.back();

        for (auto rit = m_history.rbegin(); rit != m_history.rend(); ++rit) 
        {
            if (rit->tick <= historical_tick) 
            {
                target_snap = *rit;
                break;
            }
        }
    }
    else 
        return; // Server just booted, no history available

    if (target_snap.positions.find(attacker_id) == target_snap.positions.end()) return;
    sf::Vector2f attacker_pos = target_snap.positions[attacker_id];

    std::cout << "Player " << attacker_id << " attacked at tick " << historical_tick << " from " << attacker_pos.x << ", " << attacker_pos.y << std::endl;

    for (const auto& pair : target_snap.positions) 
    {
        int32_t target_id = pair.first;
        if (target_id == attacker_id) continue;

        auto live_entity = m_entity_states.find(target_id);
        if (live_entity == m_entity_states.end() || live_entity->second.type == EntityType::PLAYER)
            continue;

        sf::Vector2f target_pos = pair.second;

        float dx = attacker_pos.x - target_pos.x;
        float dy = attacker_pos.y - target_pos.y;
        float distanceSquared = (dx * dx) + (dy * dy);
        float rangeSquared = ATTACK_RANGE * ATTACK_RANGE;

        if (distanceSquared <= rangeSquared)
        {
            std::cout << "HIT! Player " << target_id << " was in range at tick " << historical_tick << std::endl;

            // TODO: Broadcast an ENTITY_DAMAGED message
        }
        else
            std::cout << "MISS: Player " << target_id << " was too far away at tick " << historical_tick << std::endl;
    }
}