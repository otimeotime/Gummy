#include "../common/network/PacketStructs.hpp"
#include "../common/network/PacketUtils.hpp"
#include "../common/network/TCPSocket.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_running{true};
std::atomic<uint32_t> g_playerId{UINT32_MAX};
std::atomic<bool> g_myTurn{false};
std::atomic<uint32_t> g_roomState{0};

void ReceiverLoop(TCPSocket* sock) {
    uint32_t lastPrintedTick = 0;
    auto lastPrint = std::chrono::steady_clock::now();

    while (g_running) {
        Packet p;
        if (!PacketUtils::ReceivePacket(sock, p)) {
            std::cerr << "Receiver: disconnected" << std::endl;
            g_running = false;
            break;
        }

        if (p.header.type != PacketType::RES_INGAME_STATE) {
            continue;
        }

        ResIngameState s = p.GetPayload<ResIngameState>();
        g_roomState = s.roomState;

        bool mine = false;
        for (uint8_t i = 0; i < s.playerCount && i < INGAME_MAX_PLAYERS; i++) {
            if (s.players[i].id == g_playerId.load()) {
                mine = (s.players[i].isMyTurn != 0);
                break;
            }
        }
        g_myTurn = mine;

        // Print at most ~1Hz
        auto now = std::chrono::steady_clock::now();
        if (s.tick != lastPrintedTick && (now - lastPrint) > std::chrono::milliseconds(900)) {
            lastPrintedTick = s.tick;
            lastPrint = now;

            std::cout << "tick=" << s.tick
                      << " state=" << s.roomState
                      << " timer=" << s.turnTimer
                      << " myTurn=" << (mine ? "yes" : "no")
                      << " players=" << (int)s.playerCount
                      << " proj=" << (int)s.projectileCount
                      << std::endl;

            for (uint8_t i = 0; i < s.playerCount && i < INGAME_MAX_PLAYERS; i++) {
                const auto& pl = s.players[i];
                std::cout << "  P" << pl.id
                          << " hp=" << pl.hp
                          << " alive=" << (int)pl.isAlive
                          << " turn=" << (int)pl.isMyTurn
                          << " x=" << pl.x
                          << " y=" << pl.y
                          << " ang=" << pl.angle
                          << " pow=" << pl.power
                          << std::endl;
            }
        }
    }
}
}

int main(int argc, char** argv) {
    std::string ip = "127.0.0.1";
    int port = 9090;
    if (argc >= 2) ip = argv[1];
    if (argc >= 3) port = std::atoi(argv[2]);

    try {
        TCPSocket sock;
        sock.Connect(ip, port);
        std::cout << "Connected to ingame server " << ip << ":" << port << std::endl;

        ReqIngameJoin join{};
        join.matchId = 1;
        join.userId = 0;
        join.mapName[0] = '\0';
        PacketUtils::SendPacket(&sock, PacketType::REQ_INGAME_JOIN, join);

        Packet resp;
        if (!PacketUtils::ReceivePacket(&sock, resp) || resp.header.type != PacketType::RES_INGAME_JOIN) {
            std::cerr << "Failed to join (no RES_INGAME_JOIN)" << std::endl;
            return 1;
        }

        ResIngameJoin joined = resp.GetPayload<ResIngameJoin>();
        if (!joined.isSuccess) {
            std::cerr << "Join failed: " << joined.message << std::endl;
            return 1;
        }

        g_playerId = joined.playerId;
        std::cout << "Joined match " << joined.matchId << " as player " << joined.playerId << std::endl;

        std::thread rx(ReceiverLoop, &sock);

        // Simple demo input loop:
        // - If it's our turn, charge power for ~1s then FIRE.
        // - Otherwise STOP.
        uint32_t seq = 0;
        float chargeTime = 0.0f;

        auto last = std::chrono::steady_clock::now();
        while (g_running) {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<float> dt = now - last;
            last = now;

            ReqIngameInput in{};
            in.matchId = joined.matchId;
            in.playerId = joined.playerId;
            in.seq = ++seq;
            in.value = 0.0f;

            // RoomState lives server-side; current values in GameRoom.hpp are:
            // WAITING_FOR_PLAYERS=0, PLAYING_TURN=1, FIRING_PHASE=2, GAME_OVER=3
            if (g_roomState.load() == 1u && g_myTurn.load()) {
                // Add slight angle change so projectiles vary a bit.
                ReqIngameInput angle{};
                angle.matchId = joined.matchId;
                angle.playerId = joined.playerId;
                angle.seq = ++seq;
                angle.command = INGAME_CMD_ADJUST_ANGLE;
                angle.value = 0.2f;
                PacketUtils::SendPacket(&sock, PacketType::REQ_INGAME_INPUT, angle);

                chargeTime += dt.count();
                if (chargeTime < 1.0f) {
                    in.command = INGAME_CMD_ADJUST_POWER;
                    in.value = 60.0f * dt.count();
                    PacketUtils::SendPacket(&sock, PacketType::REQ_INGAME_INPUT, in);
                } else {
                    in.command = INGAME_CMD_FIRE;
                    PacketUtils::SendPacket(&sock, PacketType::REQ_INGAME_INPUT, in);
                    chargeTime = 0.0f;
                }
            } else {
                in.command = INGAME_CMD_STOP;
                PacketUtils::SendPacket(&sock, PacketType::REQ_INGAME_INPUT, in);
                chargeTime = 0.0f;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (rx.joinable()) rx.join();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }
}
