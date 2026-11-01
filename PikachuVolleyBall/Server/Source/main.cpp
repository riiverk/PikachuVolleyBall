#include "Server.h"
#include <iostream>
#include <string_view>

int main(int argc, char* argv[])
{
    if (argc == 2 && std::string_view(argv[1]) == "--check")
    {
        std::cout << "11/1 scaffold OK; port=" << SERVERPORT
                  << "; packet types=12; state bytes=" << sizeof(GameStatePacket)
                  << "; network/simulation not implemented yet.\n";
        return 0;
    }
    std::cout << "PikachuVolleyBall Server: 11/1 scaffold.\n"
                 "No listener is started. InitServer is scheduled for 11/2.\n"
                 "Use --check to verify this executable.\n";
    return 0;
}
