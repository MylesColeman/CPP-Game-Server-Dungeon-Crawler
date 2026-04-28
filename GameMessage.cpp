#include "GameMessage.h"

#include <cstring>

#include "GameServer.h"

// Applies a xor encryption key to messages
void GameMessage::applyXor(std::vector<uint8_t>& buffer) 
{
    if (buffer.size() <= 1) return; // Don't encrypt single byte messages so the type ID can be processed
    size_t keyLen = std::strlen(SECRET_KEY);
    // Skip the message type ID so it can be read
    for (size_t i = 1; i < buffer.size(); ++i)
        buffer[i] ^= SECRET_KEY[(i - 1)% keyLen];
}

// -----------------------------------------------------------------------------------------------------------------------
// Serialise function converts messages to a vector of bytes (technically ints of 8 bit size) to be sent to clients
// First byte is the message ID used to identify the message
// -----------------------------------------------------------------------------------------------------------------------

std::vector<uint8_t> WorldStateMessage::serialise() const
{
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(type));

    // Adds the tick to the packet
    uint8_t tickBytes[4];
    std::memcpy(tickBytes, &tick, 4);
    packet.insert(packet.end(), tickBytes, tickBytes + 4);

	// Adds the number of entities to the packet, so the client knows how many to expect when deserialising
    uint32_t count = static_cast<uint32_t>(entityPositions.size());
    uint8_t countBytes[4];
    std::memcpy(countBytes, &count, 4);
    packet.insert(packet.end(), countBytes, countBytes + 4);

	// Loops through all entities and adds their id and position to the byte array
    for (auto const& pair : entityPositions)
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

std::vector<uint8_t> EntityDamagedMessage::serialise() const
{
    std::vector<uint8_t> buffer(9); 

    buffer[0] = static_cast<uint8_t>(type);

    std::memcpy(&buffer[1], &targetId, 4);
    std::memcpy(&buffer[5], &currentHealth, 4);

    return buffer;
}

std::vector<uint8_t> MapTransitionMessage::serialise() const
{
    std::vector<uint8_t> buffer(5);

    buffer[0] = static_cast<uint8_t>(type);

    std::memcpy(&buffer[1], &mapId, 4);

    return buffer;
}

std::vector<uint8_t> ButtonStateMessage::serialise() const
{
    std::vector<uint8_t> buffer(10);

    buffer[0] = static_cast<uint8_t>(type);

    std::memcpy(&buffer[1], &x, 4);
    std::memcpy(&buffer[5], &y, 4);
    buffer[9] = isPressed ? 1 : 0;
    
    return buffer;
}

// Creates 'GameMessage's from incoming vector of bytes (technically ints of 8 bit size)
std::unique_ptr<GameMessage> GameMessageFactory::create(const std::vector<uint8_t>& bytes)
{
	if (bytes.empty()) return nullptr; // Invalid message

	GameMessageType type = static_cast<GameMessageType>(bytes[0]); // Looks at the first byte and decides what type of message it is

    // -----------------------------------------------------------------------------------------
    // Using that first byte instantiates the correct message and returns a smart pointer
    // -----------------------------------------------------------------------------------------

    if (type == GameMessageType::PLAYER_MOVE && bytes.size() >= 1 + GameServer::MOVE_PAYLOAD_SIZE)
    {
        int32_t id;
        float x, y;

        std::memcpy(&id, &bytes[1], 4);
        std::memcpy(&x, &bytes[5], 4);
        std::memcpy(&y, &bytes[9], 4);
        return std::make_unique<PlayerMoveMessage>(id, x, y);
    }

    if (type == GameMessageType::PLAYER_ATTACK && bytes.size() >= 1 + GameServer::ATTACK_PAYLOAD_SIZE) {
        int32_t id;
        uint32_t tick;
        std::memcpy(&id, &bytes[1], 4);
        std::memcpy(&tick, &bytes[5], 4);
        return std::make_unique<PlayerAttackMessage>(id, tick);
    }

    if (type == GameMessageType::MAP_DATA && bytes.size() >= 1 + GameServer::MAP_DATA_SIZE)
    {
        std::vector<uint8_t> grid(GameServer::MAP_DATA_SIZE);
        std::memcpy(grid.data(), &bytes[1], GameServer::MAP_DATA_SIZE);
        return std::make_unique<MapDataMessage>(GameServer::MAP_WIDTH, GameServer::MAP_HEIGHT, grid);
    }

    if (type == GameMessageType::WORLD_STATE && bytes.size() >= GameServer::WORLD_STATE_HEADER)
    {
        uint32_t tick;
        uint32_t count;
        std::memcpy(&tick, &bytes[1], 4);
        std::memcpy(&count, &bytes[5], 4);

		if (bytes.size() < GameServer::WORLD_STATE_HEADER + (count * GameServer::ENTITY_DATA_SIZE)) return nullptr; // Checks if the message is long enough to contain all the entities it claims to have

        auto msg = std::make_unique<WorldStateMessage>();
        msg->tick = tick;

		// Loops through all entities and extracts their id and position from the byte array
        for (uint32_t i = 0; i < count; ++i)
        {
            int32_t id;
            float x, y;
			size_t offset = GameServer::WORLD_STATE_HEADER + (i * GameServer::ENTITY_DATA_SIZE); // 9 bytes for type (Byte(1) + Int(4) + Int(4)), then 12 bytes per entity (Int(4) + Float(4) + Float(4))

            std::memcpy(&id, &bytes[offset], 4);
            std::memcpy(&x, &bytes[offset + 4], 4);
            std::memcpy(&y, &bytes[offset + 8], 4);

            msg->entityPositions[id] = sf::Vector2f(x, y);
        }
        return msg;
    }

	return nullptr; // Unknown or malformed message
}