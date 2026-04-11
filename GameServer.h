#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <SFML/Network.hpp>
#include <vector>
#include <mutex>
#include <map>
#include <memory>
#include <deque>
#include <cstdint>

struct WorldSnapshot 
{
    uint32_t tick;
    std::map<int32_t, sf::Vector2f> positions;
};

struct MapGrid 
{
    uint16_t width, height;
    std::vector<bool> collision;
};

enum class EntityType 
{
    PLAYER, 
    ENEMY 
};

struct PlayerState 
{
    sf::Vector2f position;
    std::vector<sf::Vector2f> currentPath;
    bool isMoving = false;
    float speed = 5.f;
    EntityType type;
    sf::Clock attackTimer;
};

class GameServer 
{
public:
    GameServer(unsigned short tcp_port, unsigned short udp_port);
    void tcp_start();
    void udp_start();
private:
    unsigned short m_tcp_port;
    unsigned short m_udp_port;
    std::vector<std::shared_ptr<sf::TcpSocket>> m_clients;
    std::mutex m_clients_mutex;

    bool m_running = true;
    void simulation_loop();
    uint32_t m_current_tick = 0;
    std::deque<WorldSnapshot> m_history;
    static constexpr size_t MAX_HISTORY_TICKS = 30;
    static constexpr float TICK_RATE_MS = 16.0f;
    static constexpr float DELTA_TIME = 1.0f / 60.0f;
    static constexpr int BROADCAST_INTERVAL = 6;

    static constexpr float ATTACK_COOLDOWN = 0.5f;
    static constexpr float ATTACK_RANGE = 2.0f;
    
    MapGrid m_current_map;

    int32_t m_next_id = 0;
    std::map<int32_t, PlayerState> m_entity_states;
    std::mutex m_state_mutex;

    void handle_client(std::shared_ptr<sf::TcpSocket> client, int32_t my_id);
    void broadcast_message(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender);

    void process_attack(int32_t attacker_id, uint32_t historical_tick);
};

#endif