#include "Pathfinding.h"

std::vector<Node> Pathfinding::nodeGrid;

std::vector<sf::Vector2f> Pathfinding::findPath(int startX, int startY, int goalX, int goalY, const std::vector<bool>& collisionGrid, int width, int height)
{
    if (startX < 0 || startY < 0 || goalX < 0 || goalY < 0 ||startX >= width || startY >= height || goalX >= width || goalY >= height) return {};

    if (nodeGrid.size() != (size_t)(width * height))
        nodeGrid.resize(width * height);

    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            int n = y * width + x;
            nodeGrid[n].x = x;
            nodeGrid[n].y = y;
            nodeGrid[n].g = 0;
            nodeGrid[n].h = 0;
            nodeGrid[n].parent = nullptr;
            nodeGrid[n].isInOpenSet = false;
        }
    }

    auto cmp = [](Node* left, Node* right) { return *left > *right; };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> openSet(cmp);
    std::vector<bool> closedSet(width * height, false);

    Node* startNode = &nodeGrid[startY * width + startX];
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

            Node* neighbour = &nodeGrid[nIdx];
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
