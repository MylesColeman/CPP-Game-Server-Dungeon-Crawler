#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <SFML/Network.hpp>
#include <vector>
#include <mutex>
#include <string>
#include <memory>

class GameServer {
public:
    GameServer(unsigned short tcp_port, unsigned short udp_port);
    void tcp_start();
    void udp_start();
private:
    unsigned short m_tcp_port;
    unsigned short m_udp_port;
    std::vector<std::shared_ptr<sf::TcpSocket>> m_clients;
    std::mutex m_clients_mutex;

    void handle_client(std::shared_ptr<sf::TcpSocket> client);
    void broadcast_message(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender);
};

#endif