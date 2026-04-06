#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
#include <cmath>
#include "GameServer.h"
#include "GameMessage.h"

// Note: This is compiled with SFML 2.6.2 in mind.
// It would work similarly with slightly older versions of SFML.
// A thourough rework is necessary for SFML 3.0.

GameServer::GameServer(unsigned short tcp_port, unsigned short udp_port) :
    m_tcp_port(tcp_port), m_udp_port(udp_port) {}

// Binds to a port and then loops around.  For every client that connects,
// we start a new thread receiving their messages.
void GameServer::tcp_start()
{
    // BINDING
    sf::TcpListener listener;
    sf::Socket::Status status = listener.listen(m_tcp_port);
    if (status != sf::Socket::Status::Done)
    {
        std::cerr << "Error binding listener to port" << std::endl;
        return;
    }

    std::cout << "TCP Server is listening to port "
        << m_tcp_port
        << ", waiting for connections..."
        << std::endl;

    while (true)
    {
        // ACCEPTING
        auto client = std::make_shared<sf::TcpSocket>();
        status = listener.accept(*client);
        if (status == sf::Socket::Status::Done)
        {
            int32_t assigned_id;
            {
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                assigned_id = m_next_id++;
                m_clients.push_back(client);
            }

            {
                std::lock_guard<std::mutex> state_lock(m_state_mutex);
                PlayerState newState;
                newState.position = sf::Vector2f(0.0f, 0.0f);
                newState.type = EntityType::PLAYER;
                m_entity_states[assigned_id] = newState;
            }

            std::cout << "New client connected: " << client->getRemoteAddress() << std::endl;
            status = client->send(&assigned_id, sizeof(int32_t));

            if (status != sf::Socket::Status::Done)
                std::cerr << "Could not send ID to client" << assigned_id << std::endl;

            std::thread(&GameServer::handle_client, this, client, assigned_id).detach();
        }
    }
    // No need to call close of the listener.
    // The connection is closed automatically when the listener object is out of scope.
}

// UDP echo server. Used to let the clients know our IP address in case
// they send a UDP broadcast message.
void GameServer::udp_start()
{
    // BINDING
    sf::UdpSocket socket;
    sf::Socket::Status status = socket.bind(m_udp_port);
    if (status != sf::Socket::Status::Done) {
        std::cerr << "Error binding socket to port " << m_udp_port << std::endl;
        return;
    }
    std::cout << "UDP Server started on port " << m_udp_port << std::endl;

    while (true) {
        // RECEIVING
        char data[1024];
        std::size_t received;
        sf::IpAddress sender;
        unsigned short senderPort;

        status = socket.receive(data, sizeof(data), received, sender, senderPort);
        if (status != sf::Socket::Status::Done) {
            std::cerr << "Error receiving data" << std::endl;
            continue;
        }

        std::cout << "Received: " << data << " from " << sender << ":" <<
            senderPort << std::endl;

        // SENDING
        status = socket.send(data, received, sender, senderPort);
        if (status != sf::Socket::Status::Done) {
            std::cerr << "Error sending data" << std::endl;
        }
    }

    // Everything that follows only makes sense if we have a graceful way to exiting the loop.
    socket.unbind();
    std::cout << "Server stopped" << std::endl;
}

// Loop around, receive messages from client and send them to all
// the other connected clients.
void GameServer::handle_client(std::shared_ptr<sf::TcpSocket> client, int32_t my_id)
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
                remaining_size = 12; // 4 (id) + 4 (x) + 4 (y)
            else if (type == GameMessageType::PLAYER_ATTACK) 
                remaining_size = 4;  // 4 (id)
            else 
            {
                std::cerr << "Unknown message type received: " << (int)type_byte << std::endl;
                continue;
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
                            std::lock_guard<std::mutex> state_lock(m_state_mutex);
                            m_entity_states[move->id].position = sf::Vector2f(move->posX, move->posY);
                        }
                    }
                    else if (msg->type == GameMessageType::PLAYER_ATTACK) 
                    {
                        auto attack = static_cast<PlayerAttackMessage*>(msg.get());
                        process_attack(attack->id);
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
void GameServer::broadcast_message(const std::vector<uint8_t>& message, std::shared_ptr<sf::TcpSocket> sender)
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

void GameServer::process_attack(int32_t attacker_id)
{
    std::lock_guard<std::mutex> lock(m_state_mutex);

    auto it = m_entity_states.find(attacker_id);
    if (it == m_entity_states.end()) return;

    sf::Vector2f attacker_pos = it->second.position;
    float attack_range = 2.0f;

    std::cout << "Player " << attacker_id << " attacked at " << attacker_pos.x << ", " << attacker_pos.y << std::endl;

    for (auto& pair : m_entity_states) 
    {
        if (pair.second.type == EntityType::PLAYER) continue;

        int32_t target_id = pair.first;
        if (target_id == attacker_id) continue;

        sf::Vector2f target_pos = pair.second.position;

        float dx = attacker_pos.x - target_pos.x;
        float dy = attacker_pos.y - target_pos.y;
        float distanceSquared = (dx * dx) + (dy * dy);
        float rangeSquared = attack_range * attack_range;

        if (distanceSquared <= rangeSquared)
        {
            std::cout << "HIT! Player " << target_id << " was in range." << std::endl;

            // TODO: Broadcast an ENTITY_DAMAGED message
        }
        else
            std::cout << "MISS: Player " << target_id << " was too far away." << std::endl;
    }
}