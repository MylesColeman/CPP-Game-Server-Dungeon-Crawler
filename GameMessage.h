#ifndef GAMEMESSAGE_H
#define GAMEMESSAGE_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

enum class GameMessageType : uint8_t 
{
    PLAYER_MOVE = 1, 
    PLAYER_ATTACK = 2,
    MAP_DATA = 3
};

class GameMessage 
{
    public:
        virtual ~GameMessage() = default;
        GameMessageType type;
};

class PlayerMoveMessage : public GameMessage 
{
    public:
        int32_t id;
        float posX, posY;

        PlayerMoveMessage(int32_t _id, float _x, float _y) 
        {
            type = GameMessageType::PLAYER_MOVE;
            id = _id; posX = _x; posY = _y;
        }
};

class PlayerAttackMessage : public GameMessage 
{
    public:
        int32_t id;

        PlayerAttackMessage(int32_t _id) 
        {
            type = GameMessageType::PLAYER_ATTACK;
            id = _id;
        }
};

class MapDataMessage : public GameMessage 
{
    public:
        uint16_t width, height;
        std::vector<uint8_t> grid;
        MapDataMessage(uint16_t w, uint16_t h, std::vector<uint8_t> g) : width(w), height(h), grid(g)
        {
            type = GameMessageType::MAP_DATA;
        }
};

class GameMessageFactory
{
    public:
    static std::unique_ptr<GameMessage> create(const std::vector<uint8_t>& bytes)
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

        if (type == GameMessageType::PLAYER_ATTACK && bytes.size() >= 5) {
            int32_t id;
            std::memcpy(&id, &bytes[1], 4);
            return std::make_unique<PlayerAttackMessage>(id);
        }

        if (type == GameMessageType::MAP_DATA && bytes.size() >= 221)
        {
            std::vector<uint8_t> grid(220);
            std::memcpy(grid.data(), &bytes[1], 220);
            return std::make_unique<MapDataMessage>(20, 11, grid);
        }

        return nullptr;
    }
};

#endif