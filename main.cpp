#include "GameServer.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <string>

int main() 
{
	GameServer server(4300, 4301); // Instatiates the server with the TCP and UDP ports to listen on
    // Starts both networking modules in separate background threads
	std::thread udpThread(&GameServer::udpStart, &server); 
	std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Delay for the UDP server to start before the TCP server starts accepting clients, helps with clarity
    std::thread tcpThread(&GameServer::tcpStart, &server);

    std::string input;
    while (std::cin >> input)
    {
        if (input == "stop")
        {
            std::cout << "Server Shutting Down!" << std::endl;
            server.stopServer();
            break;
        }
    }
    
    // Waits for both threads to finish before exiting the process
    if (udpThread.joinable()) 
        udpThread.join();

    if (tcpThread.joinable()) 
        tcpThread.join();
}