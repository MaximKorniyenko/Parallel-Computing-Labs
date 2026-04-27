#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <winsock2.h>
#include <ws2tcpip.h>

enum Command {
    CONFIG = 0x01,
    SEND_DATA = 0x02,
    START = 0x03,
    STATUS = 0x04,
    GET_RESULT = 0x05,
    DISCONNECT = 0x06
};

struct Header {
    uint32_t commandId;
    uint32_t payloadSize;
};

enum Status { IDLE, IN_PROGRESS, READY, ERROR_STATE };

struct ClientContext {
    int n = 0;
    int numThreads = 1;
    std::vector<double> matrix;
    std::vector<double> vecB;
    std::vector<double> vecC;
    double executionTime = 0.0;
    std::atomic<Status> currentStatus{ IDLE };
};

bool receiveAll(const SOCKET sock, char* buffer, const int size) {
    int totalReceived = 0;
    while (totalReceived < size) {
        const int received = recv(sock, buffer + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void multiply_range(ClientContext* ctx, const int start_row, const int end_row) {
    const int n = ctx->n;

    const double* matrix = ctx->matrix.data();
    const double* vecB = ctx->vecB.data();
    double* vecC = ctx->vecC.data();

    for (int i = start_row; i < end_row; i++) {
        double sum = 0;
        const double* row_ptr = &matrix[i * n];

        for (int j = 0; j < n; ++j) {
            sum += row_ptr[j] * vecB[j];
        }
        vecC[i] = sum;
    }
}

void taskProcessingThread(const std::shared_ptr<ClientContext> ctx) {
    ctx->currentStatus = IN_PROGRESS;
    const int n = ctx->n;
    const int numThreads = ctx->numThreads;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    const int rows_per_thread = n / numThreads;
    for (int i = 0; i < numThreads; ++i) {
        int start_row = i * rows_per_thread;
        int end_row  = (i == numThreads - 1) ? n : (i + 1) * rows_per_thread;
        workers.emplace_back(multiply_range, ctx.get(), start_row , end_row );
    }

    for (auto& t : workers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    ctx->executionTime = diff.count();

    ctx->currentStatus = READY;
}

void handleClient(const SOCKET clientSocket) {
    auto ctx = std::make_shared<ClientContext>();

    Header header{};

    bool keepRunning = true;

    while (keepRunning) {
        if (!receiveAll(clientSocket, reinterpret_cast<char *>(&header), sizeof(Header))) break;

        const uint32_t cmd = ntohl(header.commandId);
        uint32_t size = ntohl(header.payloadSize);

        switch (cmd) {
            case 0x01: { // CONFIG
                if (ctx->currentStatus == IN_PROGRESS) {
                    std::cout << "[Error] Cannot modify data while processing!" << std::endl;
                    break;
                }
                struct Config { int32_t n; int32_t threads; } cfg{};
                if (size > sizeof(cfg)) {
                    std::cout << "[Config] Invalid size." << std::endl;
                    break;
                }

                if (!receiveAll(clientSocket, (char*)&cfg, sizeof(cfg))) break;
                ctx->n = ntohl(cfg.n);
                ctx->numThreads = ntohl(cfg.threads);
                ctx->matrix.resize(ctx->n * ctx->n);
                ctx->vecB.resize(ctx->n);
                ctx->vecC.resize(ctx->n);

                std::cout << "[Config] N: " << ctx->n << ", Threads: " << ctx->numThreads << std::endl;
                send(clientSocket, "OK", 2, 0);
                break;
            }
            case 0x02: { // SEND_DATA
                if (ctx->currentStatus == IN_PROGRESS) {
                    std::cout << "[Error] Cannot modify data while processing!" << std::endl;
                    break;
                }
                if (size > ctx->n * ctx->n * sizeof(double) + ctx->n * sizeof(double)) {
                    std::cout << "[Data] Invalid size." << std::endl;
                    break;
                }

                Status expected = {IN_PROGRESS};

                if (!receiveAll(clientSocket, reinterpret_cast<char *>(ctx->matrix.data()), ctx->n * ctx->n * sizeof(double))) break;
                if (!receiveAll(clientSocket, reinterpret_cast<char *>(ctx->vecB.data()), ctx->n * sizeof(double))) break;

                std::cout << "[Data] Received matrix and vector." << std::endl;
                send(clientSocket, "RECEIVED", 8, 0);
                break;
            }
            case 0x03: { // START
                Status expected = IDLE;
                if (ctx->currentStatus.compare_exchange_strong(expected, IN_PROGRESS) ||
                   (expected = READY, ctx->currentStatus.compare_exchange_strong(expected, IN_PROGRESS))) {
                    std::thread(taskProcessingThread, ctx).detach();
                    send(clientSocket, "STARTED", 7, 0);
                }
                break;
            }
            case 0x04: { // STATUS
                Status s = ctx->currentStatus;
                send(clientSocket, (char*)&s, sizeof(Status), 0);
                break;
            }
            case 0x05: { // GET_RESULT [cite: 26]
                if (ctx->currentStatus == READY) {
                    send(clientSocket, reinterpret_cast<char*>(&ctx->executionTime), sizeof(double), 0);
                    send(clientSocket, reinterpret_cast<char*>(ctx->vecC.data()), ctx->n * sizeof(double), 0);
                } else {
                    double dummyTime = -1.0;
                    send(clientSocket, reinterpret_cast<char*>(&dummyTime), sizeof(double), 0);
                    send(clientSocket, "NOT_READY", 9, 0);
                }
                break;
            }
            case 0x06: { // DISCONNECT
                std::cout << "[Disconnect] Client requested graceful disconnection." << std::endl;

                send(clientSocket, "BYE", 3, 0);

                keepRunning = false;
                break;
            }
            default: {
                std::cout << "[Error] Unknown command received: " << cmd << std::endl;
                break;
            }
        }
    }
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    std::cout << "[Server] Server running on port 8080..." << std::endl;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            std::thread(handleClient, clientSocket).detach();
        }
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}