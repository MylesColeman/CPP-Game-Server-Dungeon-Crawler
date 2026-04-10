#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <vector>
#include <SFML/System/Vector2.hpp>

class Pathfinding 
{
public:
    // Finds the shortest path from start position to goal,
    // has knowledge of which 'Node's are blocked so they can be ignored for final path
    static std::vector<sf::Vector2f> findPath(int startX, int startY, int goalX, int goalY, const std::vector<bool>& collisionGrid, int width, int height);

private:
    // Node used by A* 'Pathfinding' represents an individual tile/cell in the grid
    struct Node
    {
        int x, y; // Defines position
        float g = 0; // Goal - how many steps currently taken
        float h = 0; // Heuristic - educated guess on distance to target
        float f() const { return g + h; } // Final cost - goal + heuristic
        Node* parent = nullptr; // Holds the previous node, acting as a breadcrumb trail back to the start with the shortest path
        
        bool isInOpenSet = false; // By default unchecked, only moved to open set if relevant for checking

		bool operator>(const Node& other) const { return f() > other.f(); } // For priority queue to sort nodes based on their f() value
    };

    static std::vector<Node> grid;

    // Calculates the Manhattan distance, optimistic distance from a Node to the goal ignoring obstacles
    static float calculateManhattan(int x1, int y1, int x2, int y2); 
    static std::vector<sf::Vector2f> reconstructPath(Node* goalNode); // Reconstructs the path from goal back to the starting position
};

#endif