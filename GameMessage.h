#ifndef GAMEMESSAGE_H
#define GAMEMESSAGE_H

#include <vector>
#include <cstdint>
#include <memory>
#include <map>
#include <SFML/System/Vector2.hpp>

enum class GameMessageType : uint8_t
{
    PLAYER_MOVE = 1,
    PLAYER_ATTACK = 2,
    MAP_DATA = 3,
    WORLD_STATE = 4
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
        uint32_t tick;

        PlayerAttackMessage(int32_t _id, uint32_t _tick) 
        {
            type = GameMessageType::PLAYER_ATTACK;
            id = _id;
            tick = _tick;
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

class WorldStateMessage : public GameMessage
{
    public:
        uint32_t tick;
        std::map<int32_t, sf::Vector2f> positions;
        WorldStateMessage() { type = GameMessageType::WORLD_STATE; }

        std::vector<uint8_t> serialise() const;
};

class GameMessageFactory
{
    public:
        static std::unique_ptr<GameMessage> create(const std::vector<uint8_t>& bytes);
};

#endif