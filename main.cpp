#include "GameServer.h"

#include <thread>
#include <chrono>

int main() 
{
	GameServer server(4300, 4301); // Instatiates the server with the TCP and UDP ports to listen on
	std::thread udp_thread(&GameServer::udp_start, &server); // Start the UDP server in a separate thread

	std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Delay for the UDP server to start before the TCP server starts accepting clients, helps with clarity

    server.tcp_start(); // Start the TCP server in the main thread. This will block until the server is stopped
    
    // Waits for the UDP thread to finish before exiting the process
    if (udp_thread.joinable()) 
        udp_thread.join();
}