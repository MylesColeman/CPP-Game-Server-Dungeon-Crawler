#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <SFML/Network.hpp>
#include <vector>
#include <mutex>
#include <map>
#include <memory>

enum class EntityType 
{
    PLAYER, 
    ENEMY 
};

struct PlayerState 
{
    sf::Vector2f position;
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

    int32_t m_next_id = 0;
    std::map<int32_t, PlayerState> m_entity_states;
    std::mutex m_state_mutex;

    void handle_client(std::shared_ptr<sf::TcpSocket> client, int32_t my_id);
    void broadcast_message(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender);

    void process_attack(int32_t attacker_id);
};

#endif