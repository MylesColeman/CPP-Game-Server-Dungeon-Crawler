#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <SFML/Network.hpp>
#include <vector>
#include <mutex>
#include <map>
#include <memory>
#include <deque>
#include <cstdint>

// Snapshot of the world state at a given tick, used for historical state tracking and client reconciliation
struct WorldSnapshot 
{
    uint32_t tick;
    std::map<int32_t, sf::Vector2f> positions;
};

// Boolean grid representing the map's collidable obstacles. True means collidable, false means walkable
struct MapGrid 
{
    uint16_t width, height;
    std::vector<bool> collision;
};

// Enum class to help differentiate entity types
enum class EntityType 
{
    PLAYER, 
    ENEMY 
};

// All relevant data for entities; the authoritave server states that are sent to clients and used for updates
struct EntityState 
{
    EntityType type;
    sf::Vector2f position;
    std::vector<sf::Vector2f> currentPath;
    bool isMoving = false;
    float speed = 5.f;
    sf::Clock attackTimer;
};

class GameServer 
{
public:
    // Map Dimensions, all rooms are the same size
    static constexpr uint16_t MAP_WIDTH = 20;
    static constexpr uint16_t MAP_HEIGHT = 11;

	// Message Payload Sizes (excluding the initial type byte)
    static constexpr size_t WORLD_STATE_HEADER = 9; // Byte(1) + Int(4) + Int(4)
    static constexpr size_t MAP_DATA_SIZE = MAP_WIDTH * MAP_HEIGHT; // 20 * 11 grid
    static constexpr size_t ENTITY_DATA_SIZE = 12; // Int(4) + Float(4) + Float(4)
    static constexpr size_t MOVE_PAYLOAD_SIZE = 12; // Int(4) + Float(4) + Float(4)
    static constexpr size_t ATTACK_PAYLOAD_SIZE = 8; // Int(4) + Int(4)

	// Simulation and Broadcast Settings
    static constexpr float DELTA_TIME = 1.0f / 60.0f;
    static constexpr float TICK_RATE_MS = 16.0f;
    static constexpr int BROADCAST_INTERVAL = 6;
    static constexpr size_t MAX_HISTORY_TICKS = 30;
    
    // Player Attack Variables
    static constexpr float ATTACK_COOLDOWN = 0.5f;
    static constexpr float ATTACK_RANGE = 2.0f;

	GameServer(unsigned short tcp_port, unsigned short udp_port); // Constructor initialises the server with specified TCP and UDP ports and starts the simulation loop in a detached thread
    // Starts the TCP server, binds to the specified port, and listens for incoming client connections
	// For each accepted client connection, a new thread is spawned to handle communication with that client - so that one client doesn't lag others
	// It also assigns a unique ID to each client and initialises their player state in the authoritative server state
    void tcpStart(); 
	// Starts the UDP server, binds to the specified port, and enters a loop to receive messages from clients and send responses back
    // This is used to echo client pings so they can discover the server's local IP address to connect to the TCP server
    void udpStart();
private:
    // Networking
    unsigned short m_tcpPort;
    unsigned short m_udpPort;
	std::vector<std::shared_ptr<sf::TcpSocket>> m_clients; // List of connected clients
	std::mutex m_clientsMutex; // Mutex to protect access to the clients list

	// Simulation
    void simulationLoop(); // Main loop that updates the game state at a fixed tick rate, processes player actions, and broadcasts world state to clients
	bool m_running = true; // Flag to control the main simulation loop
	uint32_t m_currentTick = 0; // Current tick count, incremented each simulation loop iteration
	std::deque<WorldSnapshot> m_history; // History of world snapshots for the last MAX_HISTORY_TICKS ticks, used for client reconciliation and lag compensation
    
	MapGrid m_currentMap; // Current map data, including dimensions and collision grid, which is updated when the server receives new map data from clients

	int32_t m_nextId = 0; // Counter for assigning unique IDs to all entities
	std::map<int32_t, EntityState> m_entityStates; // Authoritative state of all entities in the game, indexed by their unique ID
	std::mutex m_stateMutex; // Mutex to protect access to the entity states and world history

    // Runs in its own thread for each client, responsible for receiving messages from said client and processing them
	void handleClient(std::shared_ptr<sf::TcpSocket> client, int32_t my_id);
    // Sends a message from the server to all connected clients except the sender (if specified)
	void broadcastMessage(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender); 

    // Processes an attack action from a player 
    // Checking for cooldowns and determining if anything was hit
    // Based on their positions in the historical snapshot of the world at the time of the attack
	void processAttack(int32_t attacker_id, uint32_t historical_tick); 
};

#endif