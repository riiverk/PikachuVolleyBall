#include "Server.h"
#include <cstdio>
#include <cstring>

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
    const ULONGLONG deadline = GetTickCount64() + 3000;
    while (connected && server.GetAcceptedCount() == 0 && GetTickCount64() < deadline)
        Sleep(5);
    context->accepted = connected && server.GetAcceptedCount() == 1;
    server.CloseServer();
    return 0;
}

// Lecture ch06: a separate thread waits for Enter and asks the server to close.
// The accept loop on the main thread sees the stop request and returns.
DWORD WINAPI ConsoleExitThread(LPVOID arg)
{
    Server* server = static_cast<Server*>(arg);
    int ch;
    while ((ch = std::getchar()) != EOF && ch != '\n') {}
    if (ch == EOF) return 0; // No console input: keep the server running.
    server->CloseServer();
    return 0;
}
}

int main(int argc, char* argv[])
{
    const bool check = argc == 2 && std::strcmp(argv[1], "--check") == 0;
    if (argc > 1 && !check)
    {
        std::fprintf(stderr, "Usage: Server.exe [--check]\n");
        return 1;
    }
    Server server;
    if (!server.InitServer(check ? 0 : SERVERPORT))
    {
        // InitServer already printed the failing call with err_display().
        std::fprintf(stderr, "Server initialization failed.\n");
        return 1;
    }
    std::printf("Listening on port %u.\n", static_cast<unsigned>(server.GetPort()));
    if (check)
    {
        CheckContext context{&server};
        HANDLE clientThread = CreateThread(nullptr, 0, CheckClientThread, &context, 0, nullptr);
        if (clientThread == nullptr)
        {
            err_display(static_cast<int>(GetLastError()));
            server.CloseServer();
            std::fprintf(stderr, "Check client thread creation failed.\n");
            return 1;
        }
        server.AcceptLoop();
        WaitForSingleObject(clientThread, INFINITE);
        CloseHandle(clientThread);
        server.CloseServer();
        if (!context.accepted)
        {
            std::fprintf(stderr, "FAIL: TCP accept/session creation. Error: %d\n", server.GetLastError());
            return 1;
        }
        std::printf("PASS: server initialization, TCP accept, session worker creation and shutdown.\n");
        return 0;
    }
    std::printf("Press Enter to close the server.\n");
    HANDLE exitThread = CreateThread(nullptr, 0, ConsoleExitThread, &server, 0, nullptr);
    if (exitThread == nullptr)
    {
        err_display(static_cast<int>(GetLastError()));
        std::printf("Enter key exit is unavailable.\n");
    }
    server.AcceptLoop();
    server.CloseServer();
    // The exit thread may still wait in getchar(); it ends with the process (lecture ch06).
    if (exitThread != nullptr) CloseHandle(exitThread);
    std::printf("Closed server after accepting %zu client(s).\n", server.GetAcceptedCount());
    return 0;
}
