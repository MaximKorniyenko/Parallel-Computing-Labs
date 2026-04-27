#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <random>
#include <algorithm>
#include <ctime>


struct Header {
    uint32_t commandId;
    uint32_t payloadSize;
};

bool sendAll(const SOCKET sock, const char* buffer, const int size) {
    int totalSent = 0;
    while (totalSent < size) {
        const int sent = send(sock, buffer + totalSent, size - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool receiveAll(SOCKET sock, char* buffer, size_t size) {
    size_t totalReceived = 0;
    while (totalReceived < size) {
        int toRead = (size - totalReceived > 1024 * 1024 * 1024)
                     ? 1024 * 1024 * 1024
                     : static_cast<int>(size - totalReceived);
        int received = recv(sock, buffer + totalReceived, toRead, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void sendCommand(SOCKET sock, uint32_t cmdId, const char* payload, uint32_t payloadSize) {
    Header h;
    h.commandId = htonl(cmdId);
    h.payloadSize = htonl(payloadSize);
    sendAll(sock, reinterpret_cast<char*>(&h), sizeof(Header));
    if (payloadSize > 0 && payload != nullptr) {
        sendAll(sock, payload, payloadSize);
    }
}

void printServerResponse(SOCKET sock) {
    char buffer[1024];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        std::cout << "[Server] " << buffer << std::endl;
    } else {
        std::cout << "[Server] No response or disconnected." << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Connection failed! Start the server first." << std::endl;
        WSACleanup();
        return 1;
    }
    std::cout << "[Client] Connected to the server." << std::endl;

    //DATA PREPARATION
    const int32_t n = 5000;
    std::cout << "[Client] Generating random data for N=" << n << "..." << std::endl;

    std::mt19937 gen(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_real_distribution<double> dis(1.0, 10.0);

    std::vector<double> matrix(static_cast<size_t>(n) * n);
    std::vector<double> vecB(n);
    std::generate(matrix.begin(), matrix.end(), [&]() { return dis(gen); });
    std::generate(vecB.begin(), vecB.end(), [&]() { return dis(gen); });


    //INITIAL CONFIG
    std::cout << "[Client] Sending initial config (N=" << n << ")..." << std::endl;
    struct { int32_t n; int32_t threads; } initCfg;
    initCfg.n = htonl(n);
    initCfg.threads = htonl(1);
    sendCommand(clientSocket, 0x01, reinterpret_cast<char*>(&initCfg), sizeof(initCfg));
    printServerResponse(clientSocket); // Очікуємо "OK"

    //SEND DATA ONCE
    uint32_t dataSize = (static_cast<uint32_t>(n) * n + n) * sizeof(double);
    std::cout << "[Client] Uploading matrix and vector to server..." << std::endl;
    sendCommand(clientSocket, 0x02, nullptr, dataSize); // Header for SEND_DATA
    sendAll(clientSocket, reinterpret_cast<char*>(matrix.data()), matrix.size() * sizeof(double));
    sendAll(clientSocket, reinterpret_cast<char*>(vecB.data()), vecB.size() * sizeof(double));

    printServerResponse(clientSocket);

    std::vector<int32_t> threadConfigs = {3, 6, 12, 24, 48, 96};

    for (int32_t threads : threadConfigs) {
        //CONFIG
        struct { int32_t n; int32_t threads; } cfg;
        cfg.n = htonl(n);
        cfg.threads = htonl(threads);
        sendCommand(clientSocket, 0x01, reinterpret_cast<char*>(&cfg), sizeof(cfg));
        printServerResponse(clientSocket);

        //START
        sendCommand(clientSocket, 0x03, nullptr, 0);
        printServerResponse(clientSocket);

        //STATUS POLLING
        int32_t status = 0;
        while (true) {
            sendCommand(clientSocket, 0x04, nullptr, 0);

            int bytesRead = recv(clientSocket, reinterpret_cast<char*>(&status), sizeof(int32_t), 0);

            if (bytesRead <= 0) {
                std::cout << "\n[Error] Lost connection to server during polling." << std::endl;
                break;
            }

            std::string statusText;
            switch (status) {
                case 0: statusText = "IDLE"; break;
                case 1: statusText = "IN_PROGRESS"; break;
                case 2: statusText = "READY"; break;
                case 3: statusText = "ERROR_STATE"; break;
                default: statusText = "UNKNOWN"; break;
            }

            std::cout << "[Server] STATUS: " << statusText << " (" << status << ")   " << std::endl;

            if (status == 2) break; // READY
            Sleep(10);
        }

        //GET_RESULT & TIME
        sendCommand(clientSocket, 0x05, nullptr, 0);

        double executionTime = 0;
        receiveAll(clientSocket, reinterpret_cast<char*>(&executionTime), sizeof(double));

        std::vector<double> result(n);
        receiveAll(clientSocket, reinterpret_cast<char*>(result.data()), n * sizeof(double));

        std::cout << "[Server] Threads: " << threads << "\t| " << "Execution time: " << executionTime << " s" << std::endl;
    }

    std::cout << "-------------------------------" << std::endl;
    std::cout << "[Client] Sending DISCONNECT command..." << std::endl;
    sendCommand(clientSocket, 0x06, nullptr, 0);

    char byeBuffer[16];
    int bytes = recv(clientSocket, byeBuffer, sizeof(byeBuffer) - 1, 0);
    if (bytes > 0) {
        byeBuffer[bytes] = '\0';
        std::cout << "[Server] " << byeBuffer << std::endl;
    }

    std::cout << "[Client] Disconnecting." << std::endl;
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}