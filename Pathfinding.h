#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>
#include <SFML/System/Vector2.hpp>

struct Node {
    int x, y;
    float g = 0;
    float h = 0;
    Node* parent = nullptr;
    bool isInOpenSet = false;

    float f() const { return g + h; }

    bool operator>(const Node& other) const
    {
        return f() > other.f();
    }
};

class Pathfinding 
{
public:
    static std::vector<sf::Vector2f> findPath(int startX, int startY, int goalX, int goalY, const std::vector<bool>& collisionGrid, int width, int height);

private:
    static std::vector<Node> nodeGrid;

    static float calculateManhattan(int x1, int y1, int x2, int y2) 
    {
        return (std::abs(x1 - x2) + std::abs(y1 - y2)) * 1.001f;
    }

    static std::vector<sf::Vector2f> reconstructPath(Node* goalNode);
};

#endif