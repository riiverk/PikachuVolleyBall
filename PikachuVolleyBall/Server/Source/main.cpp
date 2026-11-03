#include "Server.h"
#include <iostream>
#include <string_view>
#include <thread>
#include <chrono>

int main(int argc, char* argv[])
{
    const bool check = argc == 2 && std::string_view(argv[1]) == "--check";
    if (argc > 1 && !check)
    {
        std::cerr << "Usage: Server.exe [--check]\n";
        return 1;
    }
    Server server;
    if (!server.InitServer(check ? 0 : SERVERPORT))
    {
        std::cerr << "Server initialization failed. Winsock error: " << server.GetLastError() << '\n';
        return 1;
    }
    std::cout << "Listening on port " << server.GetPort() << ".\n";
    std::thread acceptThread(&Server::AcceptLoop, &server);
    if (check)
    {
        const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(server.GetPort());
        const bool connected = client != INVALID_SOCKET &&
            connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
        if (client != INVALID_SOCKET) closesocket(client);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (connected && server.GetAcceptedCount() == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const bool accepted = connected && server.GetAcceptedCount() == 1;
        server.CloseServer();
        acceptThread.join();
        if (!accepted)
        {
            std::cerr << "FAIL: TCP accept/session creation. Error: " << server.GetLastError() << '\n';
            return 1;
        }
        std::cout << "PASS: server initialization, TCP accept, session worker creation and shutdown.\n";
        return 0;
    }
    std::cout << "TCP connections are accepted. Packet/room processing is not implemented yet.\n"
                 "Press Enter to close the server." << std::endl;
    std::cin.get();
    server.CloseServer();
    acceptThread.join();
    std::cout << "Closed server after accepting " << server.GetAcceptedCount() << " client(s).\n";
    return 0;
}
