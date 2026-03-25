#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
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
            int num_clients = 0;
            {
                std::lock_guard<std::mutex> lock(m_clients_mutex);
                num_clients = m_clients.size();
                m_clients.push_back(client);
            }
            std::cout << "New client connected: "
                << client->getRemoteAddress()
                << std::endl;
            {
                status = client->send(&num_clients, sizeof(int));
                if (status != sf::Socket::Status::Done)
                    std::cerr << "Could not send ID to client" << num_clients << std::endl;
            }
            std::thread(&GameServer::handle_client, this, client).detach();
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
void GameServer::handle_client(std::shared_ptr<sf::TcpSocket> client)
{
    // RECEIVING
    uint8_t payload[1024];
    size_t received;
    while (true)
        {
            memset(payload, 0, 1024);
            
            sf::Socket::Status status = client->receive(payload, sizeof(payload), received);
            if (status == sf::Socket::Disconnected || status == sf::Socket::Error)
            {
                std::cerr << "Client Disconnected!" << std::endl;
                break;
            } 
            
            if (received > 0) 
            {
                std::vector<uint8_t> message(payload, payload + received);

                auto msg = GameMessageFactory::create(message);
                if (msg && msg->type == GameMessageType::PLAYER_MOVE)
                {
                    auto move = static_cast<PlayerMoveMessage*>(msg.get());
                    std::cout << "Relaying Move: Player " << move->id
                                << " to (" << move->posX << ", " << move->posY << ")" << std::endl;
                }

                broadcast_message(message, client);
            }
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