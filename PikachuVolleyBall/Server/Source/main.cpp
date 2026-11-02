#include "Server.h"
#include <iostream>
#include <string_view>

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
        server.CloseServer();
        std::cout << "PASS: Winsock startup, bind, listen and cleanup.\n";
        return 0;
    }
    std::cout << "TCP listener is ready. Accept/room processing is not implemented yet.\n"
                 "Press Enter to close the server." << std::endl;
    std::cin.get();
    return 0;
}
