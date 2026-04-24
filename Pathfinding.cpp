#include "Pathfinding.h"

#include <queue>
#include <cmath>
#include <algorithm>

std::vector<Pathfinding::Node> Pathfinding::grid; // Reusable grid to avoid reallocating memory every time, resized if dimensions change

// Finds the shortest path from start position to goal,
// has knowledge of which 'Node's are blocked so they can be ignored for final path
std::vector<sf::Vector2f> Pathfinding::findPath(int startX, int startY, int goalX, int goalY, const std::vector<bool>& collisionGrid, int width, int height)
{
    // Checks whether the start or goal is out of bounds
    if (startX < 0 || startY < 0 || goalX < 0 || goalY < 0 ||startX >= width || startY >= height || goalX >= width || goalY >= height) 
        return {};

	// Resizes the grid if the dimensions have changed, otherwise reuses the existing grid for efficiency
    if (grid.size() != (size_t)(width * height))
        grid.resize(width * height);

    // Resets the grid each time its called, ensuring a clean slate
    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            int n = y * width + x; // Translates the 2D grid to a 1D grid

            grid[n].x = x;
            grid[n].y = y;
            grid[n].g = 0;
            grid[n].h = 0;
            grid[n].parent = nullptr;
            grid[n].isInOpenSet = false;
        }
    }

	auto comp = [](Node* left, Node* right) { return *left > *right; }; // Comparator for the priority queue to sort nodes based on their f() value
    std::priority_queue<Node*, std::vector<Node*>, decltype(comp)> openSet(comp); // Nodes that need checking, sorted using comp
    std::vector<bool> closedSet(width * height, false); // Nodes that have been checked

    Node* startNode = &grid[startY * width + startX]; // Sets the start node
    startNode->g = 0; // Start node is 0, as its the only explored tile
    startNode->h = calculateManhattan(startX, startY, goalX, goalY); // Calculates the heuristic, estimation of distance, from start to finish

    // Moved to open set, so it can be checked
    startNode->isInOpenSet = true;
    openSet.push(startNode);

    // Whilst theres still nodes to check
    while (!openSet.empty()) 
    {
	    Node* current = openSet.top(); // Takes the fist node, the one with the lowest final cost, and sets it as the current node to check
        // Removes the current node from the open set
        openSet.pop(); 
        current->isInOpenSet = false;

        if (current->x == goalX && current->y == goalY) { return reconstructPath(current); } // Found path

        closedSet[current->y * width + current->x] = true; // Added to closed set, this node has been checked

        // Static directional arrays for node neighbours
        int dx[] = { 1, -1, 0, 0 };
        int dy[] = { 0, 0, 1, -1 };

        // Loops through all neighbours
        for (int i = 0; i < 4; ++i) 
        {
            // Assigns coords to node for neighbour
            int nx = current->x + dx[i];
            int ny = current->y + dy[i];

            if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue; // Skips neighbours that are out of bounds

            int nIdx = ny * width + nx; // Translates the 2D grid to a 1D grid
			if (collisionGrid[nIdx] || closedSet[nIdx]) continue; // Checks whether the cell is blocked by an obstacle/wall, or has already been checked

            Node* neighbour = &grid[nIdx]; // Assigns node index to check neighbour
            float tentativeG = current->g + 1; // Potential cost, from current tile to neighbour

            // Checks whether the neighbour is not in the open set, i.e. its a new tile
            // Also checks whether this is a shortcut, potential is lower than current 'g' - goal
            if (!neighbour->isInOpenSet || tentativeG < neighbour->g) 
            {
                neighbour->g = tentativeG; // As this is a shortcut, assign the potential g to it
                neighbour->h = calculateManhattan(nx, ny, goalX, goalY); // Calculates heuristic, using Manhattan distance and assigns it
                neighbour->parent = current; // Sets parent to the current, the original node that was being checked

				// Checks whether this node isn't in the open set - if it is flips the flag to true
                if (!neighbour->isInOpenSet)
                    neighbour->isInOpenSet = true;
                // Adds the neighbour to the open set, so it can be checked; if its already in the open set, it will be re-added with the lower f value - 
                // meaning it'll be checked sooner
				openSet.push(neighbour); 
            }
        }
    }

    return {}; // Finished checking without finding path to goal - impossible. Returns an empty vector so the player doesn't move
}

// Calculates the Manhattan distance, optimistic distance from a Node to the goal ignoring obstacles
float Pathfinding::calculateManhattan(int x1, int y1, int x2, int y2)
{
    // Looks at the absolute horizontal difference (ignoring whether left or right),
    // and the absolute vertical difference (ignoring whether up or down) then adds them together
    float dx = std::abs(x1 - x2);
    float dy = std::abs(y1 - y2);
    return dx + dy + (x1 * 0.0001f) + (y1 * 0.00001f); // Addition of miniscule values to help break ties
}

// Reconstructs the path from goal back to the starting position
// Uses the parent node of each node as a breadcrumb going back through the list till back at the start position, when hitting null
std::vector<sf::Vector2f> Pathfinding::reconstructPath(Node* goalNode)
{
	std::vector<sf::Vector2f> path; // Vector of positions to return as the path
	Node* temp = goalNode; // Temporary node to traverse back through the parents

	// Only loops through whilst temp, the current node's parent, isn't null - as only the start node should have a null parent
    while (temp != nullptr) 
    {
        path.push_back(sf::Vector2f(temp->x + 0.5f, temp->y + 0.5f));// Adds 0.5f to x and y so player is in the centre of a tile
        temp = temp->parent; // Sets temp to it's parent, acting as a breadcrumb
    }

    std::reverse(path.begin(), path.end()); // Reversed so path starts at the start
    return path;
}