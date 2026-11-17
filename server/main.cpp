#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
constexpr int kPort = 7777;
constexpr std::size_t kMaxPlayers = 20;
constexpr auto kTick = std::chrono::milliseconds(100);

struct Player {
    int id = 0;
    int socket = -1;
    std::string name = "Player";
    float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
    std::mutex sendMutex;
};

std::atomic<bool> running{true};
std::mutex playersMutex;
std::unordered_map<int, std::shared_ptr<Player>> players;
int nextId = 1;

bool sendLine(const std::shared_ptr<Player>& player, const std::string& line) {
    std::lock_guard<std::mutex> lock(player->sendMutex);
    const std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = send(player->socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string makeSnapshot() {
    std::lock_guard<std::mutex> lock(playersMutex);
    std::ostringstream out;
    out << "STATE " << players.size();
    for (const auto& entry : players) {
        const auto& p = *entry.second;
        out << " " << p.id << " " << p.name << " "
            << p.x << " " << p.y << " " << p.z << " " << p.yaw;
    }
    return out.str();
}

void broadcastLoop() {
    while (running) {
        std::this_thread::sleep_for(kTick);
        std::vector<std::shared_ptr<Player>> clients;
        {
            std::lock_guard<std::mutex> lock(playersMutex);
            for (const auto& entry : players) clients.push_back(entry.second);
        }
        const std::string snapshot = makeSnapshot();
        for (const auto& client : clients) {
            if (!sendLine(client, snapshot)) {
                shutdown(client->socket, SHUT_RDWR);
            }
        }
    }
}

void removePlayer(int id, int socket) {
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        auto it = players.find(id);
        if (it != players.end() && it->second->socket == socket) {
            std::cout << "[LEAVE] " << it->second->name << " (" << players.size() - 1 << "/" << kMaxPlayers << ")\n";
            players.erase(it);
        }
    }
    shutdown(socket, SHUT_RDWR);
    close(socket);
}

void clientLoop(int socket) {
    auto player = std::make_shared<Player>();
    player->socket = socket;
    {
        std::lock_guard<std::mutex> lock(playersMutex);
        if (players.size() >= kMaxPlayers) {
            const std::string full = "FULL\n";
            send(socket, full.data(), full.size(), MSG_NOSIGNAL);
            close(socket);
            return;
        }
        player->id = nextId++;
        players.emplace(player->id, player);
    }

    sendLine(player, "WELCOME " + std::to_string(player->id) + " " + std::to_string(kMaxPlayers));
    std::cout << "[JOIN] player-" << player->id << " (" << players.size() << "/" << kMaxPlayers << ")\n";

    char buffer[1024];
    std::string pending;
    while (running) {
        const ssize_t count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        pending.append(buffer, static_cast<std::size_t>(count));
        if (pending.size() > 8192) break;

        std::size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            std::istringstream input(line);
            std::string command;
            input >> command;

            if (command == "JOIN") {
                std::string name;
                input >> name;
                if (!name.empty()) {
                    for (char& c : name) {
                        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') c = '_';
                    }
                    player->name = name.substr(0, 16);
                    std::cout << "[NAME] player-" << player->id << " is " << player->name << "\n";
                }
            } else if (command == "MOVE") {
                float x, y, z, yaw;
                if (input >> x >> y >> z >> yaw &&
                    std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(yaw)) {
                    player->x = x; player->y = y; player->z = z; player->yaw = yaw;
                }
            } else if (command == "PING") {
                sendLine(player, "PONG");
            }
        }
    }
    removePlayer(player->id, socket);
}
} // namespace

int main(int argc, char** argv) {
    int port = kPort;
    if (argc > 1) {
        try { port = std::stoi(argv[1]); }
        catch (...) { std::cerr << "Invalid port.\n"; return 1; }
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) { perror("socket"); return 1; }

    int reuse = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind"); close(serverSocket); return 1;
    }
    if (listen(serverSocket, 32) < 0) {
        perror("listen"); close(serverSocket); return 1;
    }

    std::signal(SIGINT, [](int) { running = false; });
    std::signal(SIGTERM, [](int) { running = false; });
    std::cout << "Bolt Survival test server listening on 0.0.0.0:" << port
              << " (max " << kMaxPlayers << " players)\n" << std::flush;

    std::thread broadcaster(broadcastLoop);
    while (running) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int client = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (client < 0) {
            if (!running) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        std::thread(clientLoop, client).detach();
    }

    close(serverSocket);
    running = false;
    if (broadcaster.joinable()) broadcaster.join();
    std::cout << "Server stopped.\n";
    return 0;
}
