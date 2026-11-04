#include "Server.h"
#include <iostream>
#include <string_view>
#include <chrono>

namespace
{
struct CheckContext
{
    Server* server;
    bool accepted = false;
};

// Only --check uses a helper thread. The server accepts on main in both modes.
DWORD WINAPI CheckClientThread(LPVOID arg)
{
    CheckContext* context = static_cast<CheckContext*>(arg);
    Server& server = *context->server;
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
        Sleep(5);
    context->accepted = connected && server.GetAcceptedCount() == 1;
    server.CloseServer();
    return 0;
}
}

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
    if (check)
    {
        CheckContext context{&server};
        HANDLE clientThread = CreateThread(nullptr, 0, CheckClientThread, &context, 0, nullptr);
        if (clientThread == nullptr)
        {
            const DWORD error = GetLastError();
            server.CloseServer();
            std::cerr << "Check client thread creation failed. Windows error: " << error << '\n';
            return 1;
        }
        server.AcceptLoop();
        WaitForSingleObject(clientThread, INFINITE);
        CloseHandle(clientThread);
        server.CloseServer();
        if (!context.accepted)
        {
            std::cerr << "FAIL: TCP accept/session creation. Error: " << server.GetLastError() << '\n';
            return 1;
        }
        std::cout << "PASS: server initialization, TCP accept, session worker creation and shutdown.\n";
        return 0;
    }
    std::cout << "TCP connections are accepted. Packet/room processing is not implemented yet.\n"
                 "Press Enter to close the server." << std::endl;
    server.EnableConsoleExit();
    server.AcceptLoop();
    server.CloseServer();
    std::cout << "Closed server after accepting " << server.GetAcceptedCount() << " client(s).\n";
    return 0;
}
