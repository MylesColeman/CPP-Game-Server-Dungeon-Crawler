#include <queue>
#include <cmath>
#include <algorithm>
#include "Pathfinding.h"

std::vector<Node> Pathfinding::grid;

std::vector<sf::Vector2f> Pathfinding::findPath(int startX, int startY, int goalX, int goalY, const std::vector<bool>& collisionGrid, int width, int height)
{
    if (startX < 0 || startY < 0 || goalX < 0 || goalY < 0 ||startX >= width || startY >= height || goalX >= width || goalY >= height) return {};

    if (grid.size() != (size_t)(width * height))
        grid.resize(width * height);

    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            int n = y * width + x;
            grid[n].x = x;
            grid[n].y = y;
            grid[n].g = 0;
            grid[n].h = 0;
            grid[n].parent = nullptr;
            grid[n].isInOpenSet = false;
        }
    }

    auto cmp = [](Node* left, Node* right) { return *left > *right; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> openSet(cmp);
    std::vector<bool> closedSet(width * height, false);

    Node* startNode = &grid[startY * width + startX];
    startNode->g = 0;
    startNode->h = calculateManhattan(startX, startY, goalX, goalY);
    startNode->isInOpenSet = true;
    openSet.push(startNode);

    while (!openSet.empty()) 
    {
        Node* current = openSet.top();
        openSet.pop();
        current->isInOpenSet = false;

        if (current->x == goalX && current->y == goalY) { return reconstructPath(current); }

        closedSet[current->y * width + current->x] = true;

        int dx[] = { 1, -1, 0, 0 };
        int dy[] = { 0, 0, 1, -1 };

        for (int i = 0; i < 4; ++i) 
        {
            int nx = current->x + dx[i];
            int ny = current->y + dy[i];

            if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;

            int nIdx = ny * width + nx;
            if (collisionGrid[nIdx] || closedSet[nIdx]) continue;

            Node* neighbour = &grid[nIdx];
            float tentativeG = current->g + 1;

            if (!neighbour->isInOpenSet || tentativeG < neighbour->g) 
            {
                neighbour->g = tentativeG;
                neighbour->h = calculateManhattan(nx, ny, goalX, goalY);
                neighbour->parent = current;

                if (!neighbour->isInOpenSet)
                    neighbour->isInOpenSet = true;
                openSet.push(neighbour);
            }
        }
    }

    return {};
}

float Pathfinding::calculateManhattan(int x1, int y1, int x2, int y2)
{
    float dx = std::abs(x1 - x2);
    float dy = std::abs(y1 - y2);
    return dx + dy + (dx * 0.001f);
}

std::vector<sf::Vector2f> Pathfinding::reconstructPath(Node* goalNode)
{
    std::vector<sf::Vector2f> path;
    Node* temp = goalNode;

    while (temp != nullptr) 
    {
        path.push_back(sf::Vector2f(temp->x + 0.5f, temp->y + 0.5f));
        temp = temp->parent;
    }

    std::reverse(path.begin(), path.end());
    return path;
}
