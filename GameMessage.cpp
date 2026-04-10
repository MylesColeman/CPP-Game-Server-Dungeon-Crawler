#include "GameMessage.h"

#include <cstring>

std::vector<uint8_t> WorldStateMessage::serialise() const
{
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(type));

    uint8_t tickBytes[4];
    std::memcpy(tickBytes, &tick, 4);
    packet.insert(packet.end(), tickBytes, tickBytes + 4);

    uint32_t count = static_cast<uint32_t>(positions.size());
    uint8_t countBytes[4];
    std::memcpy(countBytes, &count, 4);
    packet.insert(packet.end(), countBytes, countBytes + 4);

    for (auto const& pair : positions)
    {
        int32_t id = pair.first;
        sf::Vector2f pos = pair.second;

        uint8_t idBuf[4], xBuf[4], yBuf[4];
        std::memcpy(idBuf, &id, 4);
        std::memcpy(xBuf, &pos.x, 4);
        std::memcpy(yBuf, &pos.y, 4);
        packet.insert(packet.end(), idBuf, idBuf + 4);
        packet.insert(packet.end(), xBuf, xBuf + 4);
        packet.insert(packet.end(), yBuf, yBuf + 4);
    }
    return packet;
}

std::unique_ptr<GameMessage> GameMessageFactory::create(const std::vector<uint8_t>& bytes)
{
    if (bytes.empty()) return nullptr;

    GameMessageType type = static_cast<GameMessageType>(bytes[0]);

    if (type == GameMessageType::PLAYER_MOVE && bytes.size() >= 13)
    {
        int32_t id;
        float x, y;

        std::memcpy(&id, &bytes[1], 4);
        std::memcpy(&x, &bytes[5], 4);
        std::memcpy(&y, &bytes[9], 4);
        return std::make_unique<PlayerMoveMessage>(id, x, y);
    }

    if (type == GameMessageType::PLAYER_ATTACK && bytes.size() >= 9) {
        int32_t id;
        uint32_t tick;
        std::memcpy(&id, &bytes[1], 4);
        std::memcpy(&tick, &bytes[5], 4);
        return std::make_unique<PlayerAttackMessage>(id, tick);
    }

    if (type == GameMessageType::MAP_DATA && bytes.size() >= 221)
    {
        std::vector<uint8_t> grid(220);
        std::memcpy(grid.data(), &bytes[1], 220);
        return std::make_unique<MapDataMessage>(20, 11, grid);
    }

    if (type == GameMessageType::WORLD_STATE && bytes.size() >= 9)
    {
        uint32_t tick;
        uint32_t count;
        std::memcpy(&tick, &bytes[1], 4);
        std::memcpy(&count, &bytes[5], 4);

        if (bytes.size() < 9 + (count * 12)) return nullptr;

        auto msg = std::make_unique<WorldStateMessage>();
        msg->tick = tick;

        for (uint32_t i = 0; i < count; ++i)
        {
            int32_t id;
            float x, y;
            size_t offset = 9 + (i * 12);

            std::memcpy(&id, &bytes[offset], 4);
            std::memcpy(&x, &bytes[offset + 4], 4);
            std::memcpy(&y, &bytes[offset + 8], 4);

            msg->positions[id] = sf::Vector2f(x, y);
        }
        return msg;
    }

    return nullptr;
}