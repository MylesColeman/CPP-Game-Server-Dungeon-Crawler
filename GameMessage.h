#ifndef GAMEMESSAGE_H
#define GAMEMESSAGE_H

#include <vector>
#include <cstdint>
#include <cstring>
#include <memory>

enum class GameMessageType : uint8_t { PLAYER_MOVE = 1 };

class GameMessage {
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
        return nullptr;
    }
};

#endif