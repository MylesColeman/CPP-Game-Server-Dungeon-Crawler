#ifndef GAMEMESSAGE_H
#define GAMEMESSAGE_H

#include <vector>
#include <cstdint>
#include <memory>
#include <map>
#include <SFML/System/Vector2.hpp>

// Holds all game message types and assigns them an ID
// This ID is used by the client, allowing it to recognise what message has been received and how to deserialise it
enum class GameMessageType : uint8_t
{
    PLAYER_MOVE = 1,
    PLAYER_ATTACK = 2,
    MAP_DATA = 3,
    WORLD_STATE = 4,
    ENTITY_DAMAGED = 5
};

// Defines game messages
class GameMessage 
{
    public:
        virtual ~GameMessage() = default; // Virtual destructor for proper cleanup of derived classes
        GameMessageType type;
};

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Message classes defined here, so they can be constructed by the 'GameMessageFactory' to be read here and elsewhere to be sent to clients
// Message serialise function is only implemented where necessary - most messages are simply received and not sent
// Serialise function converts messages to a vector of bytes (technically ints of 8 bit size) to be sent to clients
// -----------------------------------------------------------------------------------------------------------------------------------------------

// Player movement - has position
class PlayerMoveMessage : public GameMessage 
{
    public:
        int32_t id;
        float posX, posY;

        PlayerMoveMessage(int32_t id, float x, float y) : id(id), posX(x), posY(y)
        {
            type = GameMessageType::PLAYER_MOVE;
        }
};

// Player attack - has tick of attack
class PlayerAttackMessage : public GameMessage 
{
    public:
        int32_t id;
        uint32_t tick;

        PlayerAttackMessage(int32_t id, uint32_t tick) : id(id), tick(tick)
        {
            type = GameMessageType::PLAYER_ATTACK;
        }
};

// Map Data - sends the collision grid to the server
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

// World State - for sending all player/entity positions
class WorldStateMessage : public GameMessage
{
    public:
        uint32_t tick;
        std::map<int32_t, sf::Vector2f> entityPositions;

        WorldStateMessage() 
        {
            type = GameMessageType::WORLD_STATE; 
        }

        std::vector<uint8_t> serialise() const;
};

//Entity Damaged - sends the updated health
class EntityDamagedMessage : public GameMessage
{
    public:
        int32_t targetId; // Entity to update
        int32_t currentHealth; // New health

        EntityDamagedMessage(int32_t id, int32_t health) : targetId(id), currentHealth(health)
        {
            type = GameMessageType::ENTITY_DAMAGED;
        }

        std::vector<uint8_t> serialise() const;
};

// Converts incoming messages from clients back into 'GameMessage's
class GameMessageFactory
{
    public:
        // Creates 'GameMessage's from incoming vector of bytes (technically ints of 8 bit size)
        static std::unique_ptr<GameMessage> create(const std::vector<uint8_t>& bytes); 
};

#endif