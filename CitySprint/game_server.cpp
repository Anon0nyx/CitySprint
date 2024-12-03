#ifdef _WIN32
  #define _WINSOCK_DEPRECATED_NO_WARNINGS
  typedef int socklen_t;
  #include <winsock2.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  #undef max
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
  #define SOCKET int
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  #define closesocket close
  #define WSAGetLastError() (errno)
#endif

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <openssl/sha.h>
#include <unordered_map>
#include <limits>
#include <future>
#include <queue>
#include <functional>
#include <condition_variable>

#include "utilities.h"
#include "logger.h"

// Setting up our constants, function prototypes, and structures below 

// Constants
const int BOARD_WIDTH = 1500;
const int BOARD_HEIGHT = 600;
const int TILE_SIZE = 2;

// Structure to represent a tile
struct Tile {
  int x{};
  int y{};
  std::string color;
};

struct CollidableEntity {
  int id;
  std::vector<int> midpoint;
  int size;
  int defense;
  int attack;
  std::vector<int> collidingEntities; // Vector to store IDs of colliding entities
  std::string color; // Add color to CollidableEntity
};

struct Troop : public CollidableEntity {
  int movement{};
  int attackDistance{};
  int cost{};
  int foodCost{};
};

struct Building : public CollidableEntity {
  int cost{};
  int food{};
  int coins{};
};

struct City : public CollidableEntity {
  int coins{};
  std::vector<Troop> troops;
  std::vector<Building> buildings;
};

// Structure to represent player state
struct PlayerState {
  SOCKET socket;
  int phase{};
  int coins{};
  City cities[2];
  std::shared_ptr<Troop> selectedTroop = nullptr; // Change to shared_ptr
};

// Global Game State
struct GameState {
  std::unordered_map<SOCKET, PlayerState> playerStates;
  std::vector<std::vector<std::string>> board; // 2D board representing tile colors
  std::vector<Tile> changedTiles; // List of changed tiles
  std::mutex stateMutex;
  std::chrono::time_point<std::chrono::steady_clock> lastUpdate; // Add this line
};

class ThreadPool {
public:
  ThreadPool(size_t numThreads);
  ~ThreadPool();

  void enqueue(std::function<void()> task);

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  std::mutex queueMutex;
  std::condition_variable condition;
  bool stop;
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) 
{
  std::cout << "Initializing Thread Pool with: " << std::to_string(numThreads) << " Workers." << std::endl;
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this] {
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(this->queueMutex);
          this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty()) return;
          task = std::move(this->tasks.front());
          this->tasks.pop();
        }

        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() 
{
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) worker.join();
}

void ThreadPool::enqueue(std::function<void()> task) 
{
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.push(std::move(task));
  }
  condition.notify_one();
}

class Semaphore {
public:
    Semaphore(int count = 0) : count(count) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(semaphoreMutex);
        condition.wait(lock, [this] { return count > 0; });
        --count;
    }

    void release() {
        std::unique_lock<std::mutex> lock(semaphoreMutex);
        ++count;
        condition.notify_one();
    }

private:
    std::mutex semaphoreMutex;
    std::condition_variable condition;
    int count;
};

void update_player_state(GameState& game_state, SOCKET socket, const PlayerState& state);
PlayerState get_player_state(GameState& game_state, SOCKET socket);
void remove_player(GameState& game_state, SOCKET socket);
std::string serializePlayerStateToString(const PlayerState& player);
void sendPlayerStateDeltaToClient(const PlayerState& player);
void initializeGameState();
void initializeMaps();
std::string serializeGameStateToString();
void sendGameStateDeltasToClients();
void temporarilyRemoveTroopFromGameState(Troop* troop);
void clearCollidingEntities();
int changeGridPoint(int x, int y, const std::string color);
int insertCharacter(std::vector<int> coords, int radius, const std::string color, int ignoreId);
int updateEntityMidpoint(SOCKET playerSocket, const std::vector<int>& oldMidpoint, const std::vector<int>& newMidpoint);
void applyDamageToCollidingEntities(SOCKET playerSocket, CollidableEntity* entity);
void checkForCollidingTroops();
int isColliding(std::vector<int> circleOne, std::vector<int> circleTwo);
std::shared_ptr<Troop> findNearestTroop(PlayerState& player, const std::vector<int>& coords);
bool isWithinRadius(const std::vector<int>& point, const std::vector<int>& center, int radius);
int checkCollision(const std::vector<int>& circleOne, int ignoreId);
bool moveCharacter(SOCKET playerSocket, CollidableEntity* entityToMove, const std::vector<int>& newCoords);
void moveTroopToPosition(SOCKET playerSocket, std::shared_ptr<Troop> troop, const std::vector<int>& targetCoords);
void handlePlayerMessage(SOCKET clientSocket, const std::string& message);
void gameLogic(SOCKET clientSocket);
void handleWebSocketHandshake(SOCKET clientSocket, const std::string& request);
void acceptPlayer(SOCKET serverSocket);
void boardLoop();

// Declaring our global variables for the game

GameState gameState;
std::map<SOCKET, sockaddr_in> clients;

std::map<std::string, Troop> troopMap;
std::map<std::string, Building> buildingMap;

std::mutex clientsMutex;
Semaphore userSemaphore(2);

size_t clientMessageCount = std::thread::hardware_concurrency() / 2;
size_t clientSubtaskCount = clientMessageCount / 2;
size_t leftoverThreadCount = clientMessageCount / 2;

ThreadPool clientMessageThreadPool(clientMessageCount);
ThreadPool subtaskThreadPool(clientSubtaskCount);
ThreadPool surplusThreadsForClients(leftoverThreadCount/2);

void update_player_state(GameState& game_state, SOCKET socket, const PlayerState& state) 
{
  std::scoped_lock<std::mutex> lock(game_state.stateMutex);
  game_state.playerStates[socket] = state;
}

PlayerState get_player_state(GameState& game_state, SOCKET socket) 
{
  std::scoped_lock<std::mutex> lock(game_state.stateMutex);
  return game_state.playerStates[socket];
}

void remove_player(GameState& game_state, SOCKET socket) 
{
  std::scoped_lock<std::mutex> lock(game_state.stateMutex);
  game_state.playerStates.erase(socket);
}

std::string serializePlayerStateToString(const PlayerState& player) 
{
  std::string result;
  result += "{\"player\": {\"coins\":\"";
  result += std::to_string(player.coins);
  result += "\",\"cities\":[";
  // Append the troops here
  for (auto& cities : player.cities) {
    result += "{\"troops\": [";
    for (auto& troop : cities.troops) {
      result += "{ \"id\": \"";
      result += std::to_string(troop.id);
      result += "\", \"health\": \"";
      result += std::to_string(troop.defense);
      result += "\"},";
    }
    result += "{}";
    result += "]},";
  }
  result += "{}]}}";

  return result;
}

void sendPlayerStateDeltaToClient(const PlayerState& player) 
{
  std::string playerState = serializePlayerStateToString(player);
  std::string frame = encodeWebSocketFrame(playerState);
  int result = send(player.socket, frame.c_str(), static_cast<int>(frame.size()), 0);
  if (result == SOCKET_ERROR) {
    log("Failed to send update to client: " + std::to_string(WSAGetLastError()));
  }
}

// Initialize game board with empty tiles
void initializeGameState() 
{
  int rows = BOARD_HEIGHT / TILE_SIZE;
  int cols = BOARD_WIDTH / TILE_SIZE;
  gameState.board.resize(rows, std::vector<std::string>(cols, "white"));

  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      Tile tile;
      tile.x = x;
      tile.y = y;
      tile.color = "#696969";

      gameState.board[y][x] = tile.color;
      gameState.changedTiles.push_back(tile);
    }
  }
  log("Game state initialized with " + std::to_string(rows) + " rows and " + std::to_string(cols) + " columns.");
}

void initializeMaps() 
{
  // SETUP OUR MAPS
  // Troops
  troopMap["barbarian"] = {
    0, // id
    {0, 0}, // midpoint
    6, // size
    1, // defense
    1, // attack
    {}, // collidingEntities
    "red", // color
    1, // movement
    1, // attackDistance
    15, // cost
    5 // foodCost
  };

  troopMap["giant"] = {
    0,
    {0 ,0},
    12, // size
    10, // defense
    2, // attack
    {}, // collidingEntities
    "green", // color
    1, // movement
    1, // attackDistance
    30, // cost
    10 // foodCost
  };

  // Buildings
  buildingMap["coin farm"] = {
    0, // id
    {0, 0}, // midpoint
    10, // size
    3, // defense
    1, // attack
    {}, // collidingEntities
    "purple", // color
    50, // cost
    0, // food
    1 // coins
  };

  buildingMap["food farm"] = {
    0,
    {0, 0},
    15,
    2,
    1,
    {},
    "orange",
    10,
    2,
    0
  };
  return;
}

// Function to serialize the game state into a simple string format
std::string serializeGameStateToString() 
{
  std::string result;
  result += "{\"game\": { \"board\": \"";
  if (gameState.changedTiles.empty()) {
    for (int y = 0; y < BOARD_HEIGHT / TILE_SIZE; y++) {
      for (int x = 0; x < BOARD_WIDTH / TILE_SIZE; x++) {
        result += std::to_string(x) + "," + std::to_string(y) + "," + gameState.board[y][x] + ";";
      }
    }
  } else {
    for (const auto& tile : gameState.changedTiles) {
      result += std::to_string(tile.x) + "," + std::to_string(tile.y) + "," + tile.color + ";";
    }
  }
  result += "\"}}";
  return result;
}

// Function to send game state updates to all clients
void sendGameStateDeltasToClients() 
{
  std::scoped_lock<std::mutex> lock(gameState.stateMutex);
  if (gameState.changedTiles.empty()) {
    return;
  }
  std::string gameStateStr = serializeGameStateToString();
  std::string frame = encodeWebSocketFrame(gameStateStr);
  for (const auto& client : clients) {
    int result = send(client.first, frame.c_str(), static_cast<int>(frame.size()), 0);
    if (result == SOCKET_ERROR) {
      log("Failed to send to client: " + std::to_string(WSAGetLastError()));
    }
  }
  gameState.changedTiles.clear();
}

void update_game_state(GameState& game_state) 
{
  std::scoped_lock<std::mutex> lock(game_state.stateMutex);

  for (auto& playerPair : game_state.playerStates) {
    PlayerState& player = playerPair.second;
    for (auto& city : player.cities) {
      // Remove destroyed troops
      city.troops.erase(std::remove_if(city.troops.begin(), city.troops.end(),
        [](const Troop& troop) { return troop.defense <= 0; }), city.troops.end());

      // Remove destroyed buildings
      city.buildings.erase(std::remove_if(city.buildings.begin(), city.buildings.end(),
        [](const Building& building) { return building.defense <= 0; }), city.buildings.end());

      // Check if the city itself is destroyed
      if (city.defense <= 0) {
        // Handle city destruction logic here
        // For simplicity, we can just clear the city's troops and buildings
        city.troops.clear();
        city.buildings.clear();
      }
    }
  }
}

// Some more game state functions related to moving troops
void temporarilyRemoveTroopFromGameState(Troop* troop) 
{
  if (!troop || troop->midpoint.size() < 2) return;

  // Temporarily set the troop's coordinates to an off-board value
  troop->midpoint[0] = -1;
  troop->midpoint[1] = -1;
}

void clearCollidingEntities() 
{
  std::scoped_lock<std::mutex> lock(gameState.stateMutex);

  for (auto& playerPair : gameState.playerStates) {
    PlayerState& player = playerPair.second;
    for (auto& city : player.cities) {
      city.collidingEntities.clear();
      for (auto& troop : city.troops) {
        troop.collidingEntities.clear();
      }
      for (auto& building : city.buildings) {
        building.collidingEntities.clear();
      }
    }
  }
}

int changeGridPoint(int x, int y, const std::string color) 
{
  if (color != "#696969") 
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);

  if (x >= 0 && x < BOARD_WIDTH / TILE_SIZE && y >= 0 && y < BOARD_HEIGHT / TILE_SIZE) {
    gameState.board[y][x] = color;
    gameState.changedTiles.push_back({ x, y, color });
  } else {
    log("Invalid grid point (" + std::to_string(x) + ", " + std::to_string(y) + "). No changes made.");
    return 1;
  }
  return 0;
}

// Functionality to insert a character using the bresenham's circle generation algorithm
int insertCharacter(std::vector<int> coords, int radius, const std::string color, int ignoreId = -1) 
{
  int centerX = coords[0];
  int centerY = coords[1];
  int d = 3 - 2 * radius;
  int y = radius;
  int x = 0;

  std::vector<int> circle = { coords[0], coords[1], radius };

  log("Creating a character at (" + std::to_string(centerX) + ", " + std::to_string(centerY) + ")");
  if (color != "#696969") {
    if (checkCollision(circle, ignoreId)) {
      log("Collision detected at (" + std::to_string(centerX) + ", " + std::to_string(centerY) + ")");
      return 0;
    }
  }

  while (y >= x) {
    changeGridPoint(centerX + x, centerY + y, color);
    changeGridPoint(centerX - x, centerY + y, color);
    changeGridPoint(centerX + x, centerY - y, color);
    changeGridPoint(centerX - x, centerY - y, color);
    changeGridPoint(centerX + y, centerY + x, color);
    changeGridPoint(centerX - y, centerY + x, color);
    changeGridPoint(centerX + y, centerY - x, color);
    changeGridPoint(centerX - y, centerY - x, color);

    x++;
    if (d > 0) {
      y--;
      d = d + 4 * (x - y) + 10;
    } else {
      d = d + 4 * x + 6;
    }
  }
  return 1;
}

int updateEntityMidpoint(SOCKET playerSocket, const std::vector<int>& oldMidpoint, const std::vector<int>& newMidpoint) 
{
  std::scoped_lock<std::mutex> lock(gameState.stateMutex);

  auto it = gameState.playerStates.find(playerSocket);
  if (it == gameState.playerStates.end()) {
    log("Player not found in game state.");
    return 0;
  }

  PlayerState& playerState = it->second;

  for (auto& city : playerState.cities) {
    if (city.midpoint == oldMidpoint) {
      city.midpoint = newMidpoint;
      return 1;
    }
    for (auto& troop : city.troops) {
      if (troop.midpoint == oldMidpoint) {
        troop.midpoint = newMidpoint;
        return 1;
      }
    }
    for (auto& building : city.buildings) {
      if (building.midpoint == oldMidpoint) {
        building.midpoint = newMidpoint;
        return 1;
      }
    }
  }
  log("Entity with the specified midpoint not found.");
  return 0;
}


void removeEntityFromGameState(GameState& gameState, PlayerState& playerState, int entityId)
{
  log("Attempting to remove entity with ID: " + std::to_string(entityId));

  for (auto& city : playerState.cities) {
    if (city.id == entityId) {
      log("City found. Removing city from the game state.");

      // Clear the board of the city using its midpoint
      std::vector<int> midpoint = city.midpoint;
      insertCharacter(midpoint, city.size, "#696969", entityId);

      // Clear the city's troops and buildings
      for (auto& troop : city.troops) {
        insertCharacter(troop.midpoint, troop.size, "#696969", troop.id);
      }
      for (auto& building : city.buildings) {
        insertCharacter(building.midpoint, building.size, "#696969", building.id);
      }
      city.troops.clear();
      city.buildings.clear();

      // Clear the city's midpoint to mark it as removed
      city.midpoint.clear();

      log("City " + std::to_string(entityId) + " removed from game state.");
      update_game_state(gameState); // Update game state after removal
      return;
    }

    auto troopIt = std::find_if(city.troops.begin(), city.troops.end(), [entityId](const Troop& troop) {
      return troop.id == entityId;
      });

    if (troopIt != city.troops.end()) {
      log("Troop found. Removing troop from the game state.");

      // Clear the board of the troop using its midpoint
      std::vector<int> midpoint = troopIt->midpoint;
      insertCharacter(midpoint, troopIt->size, "#696969", entityId);

      // Remove the troop from the city's troop list
      city.troops.erase(troopIt);

      log("Troop " + std::to_string(entityId) + " removed from game state.");
      update_game_state(gameState); // Update game state after removal
      return;
    }

    auto buildingIt = std::find_if(city.buildings.begin(), city.buildings.end(), [entityId](const Building& building) {
      return building.id == entityId;
      });

    if (buildingIt != city.buildings.end()) {
      log("Building found. Removing building from the game state.");

      // Clear the board of the building using its midpoint
      std::vector<int> midpoint = buildingIt->midpoint;
      insertCharacter(midpoint, buildingIt->size, "#696969", entityId);

      // Remove the building from the city's building list
      city.buildings.erase(buildingIt);

      log("Building " + std::to_string(entityId) + " removed from game state.");
      update_game_state(gameState); // Update game state after removal
      return;
    }
  }

  log("Entity with ID " + std::to_string(entityId) + " not found in any city.");
}


void handleTroopCollisions() 
{
  std::vector<PlayerState> playerStates;
  {
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);
    for (const auto& playerPair : gameState.playerStates) {
      playerStates.push_back(playerPair.second);
    }
  }

  for (auto& player : playerStates) {
    for (auto& city : player.cities) {
      for (auto& troop : city.troops) {
        applyDamageToCollidingEntities(player.socket, &troop);
      }
    }
  }
}

void applyDamageToCollidingEntities(SOCKET playerSocket, CollidableEntity* entity)
{
  PlayerState* ourPlayer;
  {
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);
    ourPlayer = &gameState.playerStates[playerSocket];
  }

  log("Our player: " + std::to_string(ourPlayer->socket));

  log("Applying damage for moving entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ")");

  for (auto& playerPair : gameState.playerStates) {
    PlayerState& player = playerPair.second;
    log("Testing against player: " + std::to_string(playerPair.first));
    if (playerPair.first == playerSocket) {
      log("Skipping our own player.");
      continue;
    }
    for (auto& city : player.cities) {
      log("Testing against city: " + std::to_string(city.id) + " with size: " + std::to_string(city.size));
      log("Entity size: " + std::to_string(entity->size));
      int newRadius = entity->size + city.size + 4;
      if (isWithinRadius(city.midpoint, entity->midpoint, newRadius)) {
        int cityDamage = entity->attack;
        int movingEntityDamage = city.attack;
        city.defense -= cityDamage;
        entity->defense -= movingEntityDamage;
        entity->collidingEntities.push_back(city.id);
        city.collidingEntities.push_back(entity->id);
        log("Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ") dealt " + std::to_string(cityDamage) + " damage to City " + std::to_string(city.id) + " (Client: " + std::to_string(playerPair.first) + "). City defense: " + std::to_string(city.defense));
        log("City " + std::to_string(city.id) + " (Client: " + std::to_string(playerPair.first) + ") dealt " + std::to_string(movingEntityDamage) + " damage to Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + "). Entity defense: " + std::to_string(entity->defense));

        if (city.defense <= 0) {
          log("City " + std::to_string(city.id) + " (Client: " + std::to_string(playerPair.first) + ") has been destroyed.");
          removeEntityFromGameState(gameState, player, city.id);
        }

        if (entity->defense <= 0) {
          log("Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ") has been destroyed.");
          removeEntityFromGameState(gameState, *ourPlayer, entity->id);
          return;
        }
      }
      for (auto& troop : city.troops) {
        log("Testing against troop: " + std::to_string(troop.midpoint[0]) + ", " + std::to_string(troop.midpoint[1]) + " : " + std::to_string(entity->midpoint[0]) + ", " + std::to_string(entity->midpoint[1]));
        std::vector<int> circleOne, circleTwo;
        int newRadius = (troop.size + entity->size) + 4;
        log(std::to_string(isWithinRadius(troop.midpoint, entity->midpoint, newRadius)));
        if (isWithinRadius(troop.midpoint, entity->midpoint, newRadius)) {
          log("Troops are officially fighting");
          int troopDamage = entity->attack;
          int movingEntityDamage = troop.attack;
          troop.defense -= troopDamage;
          entity->defense -= movingEntityDamage;
          entity->collidingEntities.push_back(troop.id);
          troop.collidingEntities.push_back(entity->id);
          log("Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ") dealt " + std::to_string(troopDamage) + " damage to Troop " + std::to_string(troop.id) + " (Client: " + std::to_string(playerPair.first) + "). Troop defense: " + std::to_string(troop.defense));
          log("Troop " + std::to_string(troop.id) + " (Client: " + std::to_string(playerPair.first) + ") dealt " + std::to_string(movingEntityDamage) + " damage to Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + "). Entity defense: " + std::to_string(entity->defense));

          // Remove troop if its defense is zero or less
          if (troop.defense <= 0) {
            log("Troop " + std::to_string(troop.id) + " (Client: " + std::to_string(playerPair.first) + ") has been destroyed.");
            insertCharacter(troop.midpoint, troop.size, "#696969", troop.id); // Overwrite the spot on the board
            city.troops.erase(std::remove_if(city.troops.begin(), city.troops.end(), [&](const Troop& t) { return t.id == troop.id; }), city.troops.end());
          }

          if (entity->defense <= 0) {
            log("Our troop is dead");
            removeEntityFromGameState(gameState, *ourPlayer, entity->id);
            update_game_state(gameState); // Update game state after removal
            return;
          }
        }
      }
      for (auto& building : city.buildings) {
        std::vector<int> circleOne, circleTwo;
        int newRadius = (building.size + entity->size) + 4;
        log(std::to_string(isWithinRadius(building.midpoint, entity->midpoint, newRadius)));
        if (isWithinRadius(building.midpoint, entity->midpoint, newRadius)) {
          log("building is officially fighting");
          int buildingDamage = entity->attack;
          int movingEntityDamage = building.attack;
          building.defense -= buildingDamage;
          entity->defense -= movingEntityDamage;
          entity->collidingEntities.push_back(building.id);
          building.collidingEntities.push_back(entity->id);
          log("Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ") dealt " + std::to_string(movingEntityDamage) + " damage to Troop " + std::to_string(building.id) + " (Client: " + std::to_string(playerPair.first) + "). Building defense: " + std::to_string(building.defense));
          log("Building " + std::to_string(building.id) + " (Client: " + std::to_string(playerPair.first) + ") dealt " + std::to_string(buildingDamage) + " damage to Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + "). Entity defense: " + std::to_string(entity->defense));

          // Remove troop if its defense is zero or less
          if (building.defense <= 0) {
            log("Building " + std::to_string(building.id) + " (Client: " + std::to_string(playerPair.first) + ") has been destroyed.");
            insertCharacter(building.midpoint, building.size, "#696969", building.id); // Overwrite the spot on the board
            city.buildings.erase(std::remove_if(city.buildings.begin(), city.buildings.end(), [&](const Building& t) { return t.id == building.id; }), city.buildings.end());
          }

          if (entity->defense <= 0) {
            log("Our troop is dead");
            removeEntityFromGameState(gameState, *ourPlayer, entity->id);
            update_game_state(gameState); // Update game state after removal
            return;
          }
        }
      }
    }
  }

  // Remove moving entity if its defense is zero or less
  if (entity->defense <= 0) {
    log("Entity " + std::to_string(entity->id) + " (Client: " + std::to_string(playerSocket) + ") has been destroyed.");
    removeEntityFromGameState(gameState, *ourPlayer, entity->id);
    update_game_state(gameState);
    insertCharacter(entity->midpoint, entity->size, "#696969", entity->id); // Overwrite the spot on the board
    for (auto& city : ourPlayer->cities) {
      city.troops.erase(std::remove_if(city.troops.begin(), city.troops.end(), [&](const Troop& t) { return t.id == entity->id; }), city.troops.end());
    }
  }
  update_game_state(gameState); // Update game state after applying damage
}





// Collision logic and functions
void checkForCollidingTroops() 
{
  std::scoped_lock<std::mutex> lock(gameState.stateMutex);

  for (auto& player: gameState.playerStates) {
    PlayerState& playerState = player.second;
    for (auto& cities : playerState.cities) {
      for (auto& troops : cities.troops) {
        log("Troop: " + std::to_string(troops.id));
      }
    }
  }
}

int isColliding(std::vector<int> circleOne, std::vector<int> circleTwo) 
{
  int xOne = circleOne[0];
  int yOne = circleOne[1];
  int radOne = circleOne[2];

  int xTwo = circleTwo[0];
  int yTwo = circleTwo[1];
  int radTwo = circleTwo[2];

  int xResult = (xTwo - xOne) * (xTwo - xOne);
  int yResult = (yTwo - yOne) * (yTwo - yOne);
  int requiredDistance = radOne + radTwo;

  int actualDistance = static_cast<int>(squareRoot(static_cast<double>(xResult + yResult)));

  log("Checking collision: circleOne (" + std::to_string(xOne) + ", " + std::to_string(yOne) + ", " + std::to_string(radOne) + "), circleTwo (" + std::to_string(xTwo) + ", " + std::to_string(yTwo) + ", " + std::to_string(radTwo) + "), actualDistance: " + std::to_string(actualDistance) + ", requiredDistance: " + std::to_string(requiredDistance));

  if (actualDistance <= requiredDistance)
    return 1;
  return 0;
}

std::shared_ptr<Troop> findNearestTroop(PlayerState& player, const std::vector<int>& coords) 
{
  std::shared_ptr<Troop> nearestTroop = nullptr;
  int minDistance = std::numeric_limits<int>::max();

  for (auto& city : player.cities) {
    if (city.midpoint.empty()) continue; // Skip uninitialized cities
    for (auto& troop : city.troops) {
      if (troop.midpoint.size() < 2) continue; // Skip uninitialized troops
        int dx = coords[0] - troop.midpoint[0];
        int dy = coords[1] - troop.midpoint[1];
        int distance = dx * dx + dy * dy;
        if (distance < minDistance) {
          minDistance = distance;
          nearestTroop = std::make_shared<Troop>(troop);
      }
    }
  }

  return nearestTroop;
}

bool isWithinRadius(const std::vector<int>& point, const std::vector<int>& center, int radius) 
{
  if (point.size() < 2 || center.size() < 2) {
    log("Invalid point or center size. Point size: " + std::to_string(point.size()) + ", Center size: " + std::to_string(center.size()));
    return false;
  }
  int dx = point[0] - center[0];
  int dy = point[1] - center[1];
  int distanceSquared = dx * dx + dy * dy;
  int radiusSquared = radius * radius;
  bool withinRadius = distanceSquared <= radiusSquared;

  if (withinRadius) {
    log("Collision detected: point (" + std::to_string(point[0]) + ", " + std::to_string(point[1]) +
    "), center (" + std::to_string(center[0]) + ", " + std::to_string(center[1]) +
    "), radius " + std::to_string(radius));
  }

  return withinRadius;
}

int checkCollision(const std::vector<int>& circleOne, int ignoreId = -1) 
{
  std::scoped_lock<std::mutex> lock(gameState.stateMutex);

  log("Checking collision for circle with ignoreId: " + std::to_string(ignoreId));

  bool hasCircles = false;
  for (const auto& playerPair : gameState.playerStates) {
    const PlayerState& player = playerPair.second;
    for (const auto& city : player.cities) {
      if (!city.midpoint.empty()) {
        hasCircles = true;
        break;
      }
    }
    if (hasCircles)
      break;
  }

  if (!hasCircles)
    return 0;

  for (const auto& playerPair : gameState.playerStates) {
    const PlayerState& player = playerPair.second;
    for (const auto& troop : player.cities->troops) {
      if (!troop.midpoint.empty() && troop.id != ignoreId) {
        std::vector<int> circleTwo = { troop.midpoint[0], troop.midpoint[1], troop.size };
        if (isColliding(circleOne, circleTwo)) {
          log("Collision detected between entity " + std::to_string(ignoreId) + " and troop " + std::to_string(troop.id));
          return 1;
        }
      }
    }
    for (const auto& building : player.cities->buildings) {
      if (!building.midpoint.empty()) {
        std::vector<int> circleTwo = { building.midpoint[0], building.midpoint[1], building.size };
        if (isColliding(circleOne, circleTwo)) {
          log("Collision detected between entity " + std::to_string(ignoreId) + " and building " + std::to_string(building.id));
          return 1;
        }
      }
    }
    for (const auto& city : player.cities) {
      if (!city.midpoint.empty()) {
        std::vector<int> circleTwo = { city.midpoint[0], city.midpoint[1], city.size };
        if (isColliding(circleOne, circleTwo)) {
          log("Collision detected between entity " + std::to_string(ignoreId) + " and city " + std::to_string(city.id));
          return 1;
        }
      }
    }
  }
  return 0;
}

// Character movement functionality below 
bool moveCharacter(SOCKET playerSocket, CollidableEntity* entityToMove, const std::vector<int>& newCoords) 
{
  if (!entityToMove || entityToMove->midpoint.size() < 2) {
    log("Invalid entity to move.");
    return false;
  }

  int currentX = entityToMove->midpoint[0];
  int currentY = entityToMove->midpoint[1];
  int radius = entityToMove->size;
  std::string color = entityToMove->color;
  int entityId = entityToMove->id;

  std::vector<int> currentCoords = { currentX, currentY };

  log("Moving entity " + std::to_string(entityId) + " (Client: " + std::to_string(playerSocket) + ") from (" + std::to_string(currentX) + ", " + std::to_string(currentY) + ") to (" + std::to_string(newCoords[0]) + ", " + std::to_string(newCoords[1]) + ")");

  // Check for collision at the new coordinates
  std::vector<int> newCircle = { newCoords[0], newCoords[1], radius };
  if (checkCollision(newCircle, entityId)) {
    log("Collision detected at (" + std::to_string(newCoords[0]) + ", " + std::to_string(newCoords[1]) + ")");
    return false;
  }

  // Clear the old position on the board
  insertCharacter(currentCoords, radius, "#696969", entityId);

  // Insert the entity at the new position
  insertCharacter(newCoords, radius, color, entityId);

  // Update the entity's position in the global game state
  int res = updateEntityMidpoint(playerSocket, currentCoords, newCoords);

  // Update the entity's midpoint in the local entity object
  entityToMove->midpoint = newCoords;

  // Add the changed tiles to the changedTiles vector
  {
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);
    gameState.changedTiles.push_back({ currentX, currentY, "#696969" });
    gameState.changedTiles.push_back({ newCoords[0], newCoords[1], color });
  }
  // Send game state deltas to clients
  sendGameStateDeltasToClients();

  return true;
}

void moveTroopToPosition(SOCKET playerSocket, std::shared_ptr<Troop> troop, const std::vector<int>& targetCoords)
{
  while (troop->midpoint != targetCoords) {
    {
      std::scoped_lock<std::mutex> lock(gameState.stateMutex);
      PlayerState& player = gameState.playerStates[playerSocket];
      bool troopExists = false;

      // Check if the troop still exists in the game state
      for (auto& city : player.cities) {
        auto it = std::find_if(city.troops.begin(), city.troops.end(), [&](const Troop& t) {
          return t.id == troop->id;
          });
        if (it != city.troops.end()) {
          troopExists = true;
          break;
        }
      }

      if (!troopExists) {
        log("Troop " + std::to_string(troop->id) + " no longer exists in the game state.");
        return;
      }
    } // Mutex is unlocked here

    std::vector<int> currentCoords = troop->midpoint;
    std::vector<int> nextCoords = currentCoords;

    if (currentCoords[0] < targetCoords[0]) nextCoords[0]++;
    else if (currentCoords[0] > targetCoords[0]) nextCoords[0]--;

    if (currentCoords[1] < targetCoords[1]) nextCoords[1]++;
    else if (currentCoords[1] > targetCoords[1]) nextCoords[1]--;

    log("Attempting to move troop " + std::to_string(troop->id) + " (Client: " + std::to_string(playerSocket) + ") from (" + std::to_string(currentCoords[0]) + ", " + std::to_string(currentCoords[1]) + ") to (" + std::to_string(nextCoords[0]) + ", " + std::to_string(nextCoords[1]) + ")");

    // Check for collision at the next coordinates
    std::vector<int> newCircle = { nextCoords[0], nextCoords[1], troop->size };
    if (checkCollision(newCircle, troop->id)) {
      log("Collision detected at (" + std::to_string(nextCoords[0]) + ", " + std::to_string(nextCoords[1]) + ")");
      applyDamageToCollidingEntities(playerSocket, troop.get());
      return;
    }

    if (!moveCharacter(playerSocket, troop.get(), nextCoords)) {
      log("Failed to move troop " + std::to_string(troop->id) + " (Client: " + std::to_string(playerSocket) + ") to (" + std::to_string(nextCoords[0]) + ", " + std::to_string(nextCoords[1]) + ")");
      return;
    }

    // Log the updated midpoint after each move
    log("Troop " + std::to_string(troop->id) + " (Client: " + std::to_string(playerSocket) + ") moved to (" + std::to_string(troop->midpoint[0]) + ", " + std::to_string(troop->midpoint[1]) + ")");

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Adjust the delay as needed
  }
}


// Functionality for most of the networking stuff below here

// Function to handle messages from a client
void handlePlayerMessage(SOCKET clientSocket, const std::string& message) 
{
  log("Handling client message: " + message);
  std::istringstream iss(message);
  std::string segment;
  std::vector<std::string> segments;

  PlayerState player = get_player_state(gameState, clientSocket);

  while (std::getline(iss, segment, ',')) {
    segments.push_back(segment);
  }

  if (segments.size() != 4) {
    log("Invalid message format.");
    return;
  }

  int x = std::stoi(segments[0]);
  int y = std::stoi(segments[1]);
  std::string color = segments[2];
  std::string characterType = segments[3];

  std::vector<int> coords = { x, y };

  if (x == 1000 && y == 1000) {
    initializeGameState();
    return;
  }

  if (player.phase == 0) {
    log("Player has no cities.");

    // Check if the new city is within 100 tiles of any existing city
    bool tooClose = false;
    for (const auto& playerPair : gameState.playerStates) {
      const PlayerState& otherPlayer = playerPair.second;
      for (const auto& city : otherPlayer.cities) {
        if (!city.midpoint.empty()) {
          int dx = coords[0] - city.midpoint[0];
          int dy = coords[1] - city.midpoint[1];
          int distanceSquared = dx * dx + dy * dy;
          if (distanceSquared < 200 * 200) {
              tooClose = true;
              break;
          }
        }
      }
      if (tooClose) break;
    }

    if (tooClose) {
      log("Cannot create city within 100 tiles of another city.");
      return;
    }

    if (insertCharacter(coords, 20, "yellow")) {
      City newCity;
      newCity.id = generateUniqueId(); // Generate a unique ID for the city
      newCity.midpoint = { coords[0], coords[1] };
      newCity.size = 20;
      newCity.defense = 10;
      newCity.attack = 1;
      newCity.color = "yellow";
      player.cities[0] = newCity;
      player.phase = 1;
      update_player_state(gameState, clientSocket, player);
    }
    return;
  }

  if (characterType == "select") {
    std::shared_ptr<Troop> nearestTroop = findNearestTroop(player, coords);
    if (nearestTroop && isWithinRadius(coords, nearestTroop->midpoint, nearestTroop->size)) {
      player.selectedTroop = nearestTroop;
      log("Troop selected at (" + std::to_string(nearestTroop->midpoint[0]) + ", " + std::to_string(nearestTroop->midpoint[1]) + ")");
    } else {
      log("No troop found at the selected position.");
    }
    update_player_state(gameState, clientSocket, player);
    return;
  }

  if (characterType == "move" && player.selectedTroop) {
    std::future<void> moveFuture = std::async(std::launch::async, moveTroopToPosition, clientSocket, player.selectedTroop, coords);
    player.selectedTroop = nullptr; // Deselect the troop after starting the movement
    update_player_state(gameState, clientSocket, player);
    return;
  }

  // Check if the coordinates are within the radius of a city plus an additional 100 tiles
  bool withinCityRadius = false;
  for (const auto& city : player.cities) {
    if (isWithinRadius(coords, city.midpoint, city.size + 100)) {
      withinCityRadius = true;
      break;
    }
  }

  if (!withinCityRadius) {
    log("Cannot create " + characterType + " outside the radius of a city.");
    return;
  }

  // Existing code for handling other character types (coin, troop, building)
  if (characterType == "coin") {
    City* nearestCity = nullptr;
    int minDistance = std::numeric_limits<int>::max();
    for (auto& city : player.cities) {
      if (city.midpoint.empty()) continue; // Skip uninitialized cities
      int dx = coords[0] - city.midpoint[0];
      int dy = coords[1] - city.midpoint[1];
      int distance = dx * dx + dy * dy;
      if (distance < minDistance) {
        minDistance = distance;
        nearestCity = &city;
      }
    }
    if (nearestCity && isWithinRadius(coords, nearestCity->midpoint, 20)) {
      player.coins++;
      nearestCity->coins++;
      log(std::to_string(player.coins) + " coins collected. City now has " + std::to_string(nearestCity->coins) + " coins.");
    }
  } else if (troopMap.find(characterType) != troopMap.end()) {
    if (player.coins < troopMap[characterType].cost) {
      log("Not enough coins to create troop.");
      return;
    }
    if (!insertCharacter(coords, troopMap[characterType].size, troopMap[characterType].color)) {
      log("Failed to insert troop character.");
      return;
    }
    player.coins -= troopMap[characterType].cost;
    log("Troop created. Player now has " + std::to_string(player.coins) + " coins left.");

    Troop newTroop = troopMap[characterType];
    newTroop.id = generateUniqueId(); // Assign a unique ID to the new troop
    newTroop.midpoint = { coords[0], coords[1] };
    City* nearestCity = nullptr;
    int minDistance = std::numeric_limits<int>::max();
    for (auto& city : player.cities) {
      if (city.midpoint.empty()) continue; // Skip uninitialized cities
      int dx = coords[0] - city.midpoint[0];
      int dy = coords[1] - city.midpoint[1];
      int distance = dx * dx + dy * dy;
      if (distance < minDistance) {
        minDistance = distance;
        nearestCity = &city;
      }
    }
    if (nearestCity) {
      nearestCity->troops.push_back(newTroop);
    }
  } else if (buildingMap.find(characterType) != buildingMap.end()) {
    if (player.coins < buildingMap[characterType].cost) {
      log("Not enough coins to create building.");
      return;
    }
    if (!insertCharacter(coords, buildingMap[characterType].size, buildingMap[characterType].color)) {
      log("Failed to insert building character.");
      return;
    }
    player.coins -= buildingMap[characterType].cost;
    log("Building created. Player now has " + std::to_string(player.coins) + " coins left.");

    Building newBuilding = buildingMap[characterType];
    newBuilding.id = generateUniqueId(); // Assign a unique ID to the new building
    newBuilding.midpoint = { coords[0], coords[1] };
    City* nearestCity = nullptr;
    int minDistance = std::numeric_limits<int>::max();
    for (auto& city : player.cities) {
      if (city.midpoint.empty()) continue; // Skip uninitialized cities
        int dx = coords[0] - city.midpoint[0];
        int dy = coords[1] - city.midpoint[1];
        int distance = dx * dx + dy * dy;
        if (distance < minDistance) {
          minDistance = distance;
          nearestCity = &city;
      }
    }
    if (nearestCity) {
      nearestCity->buildings.push_back(newBuilding);
    }
  }
  update_player_state(gameState, clientSocket, player);
  sendPlayerStateDeltaToClient(player);
}

// Threaded client handling function
void gameLogic(SOCKET clientSocket) 
{
  char buffer[512];
  int bytesReceived;

  // Initialize player state
  PlayerState player_state;
  player_state.socket = clientSocket;
  player_state.coins = 1000; // Example initial state
  player_state.phase = 0;

  // Store the initial state in the GameState structure
  update_player_state(gameState, clientSocket, player_state);

  sendPlayerStateDeltaToClient(player_state);

  while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
    std::string message(buffer, bytesReceived);
    std::string decodedMessage = decodeWebSocketFrame(message);  // Decoding WebSocket frame
    log("Decoded message: " + decodedMessage);

    // Submit the task to the thread pool
    clientMessageThreadPool.enqueue([clientSocket, decodedMessage] {
      handlePlayerMessage(clientSocket, decodedMessage);
      });
  }
  closesocket(clientSocket);
  {
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);
    clients.erase(clientSocket);
  }
  log("Client disconnected.");
}

// Handle WebSocket handshake
void handleWebSocketHandshake(SOCKET clientSocket, const std::string& request) 
{
  std::istringstream requestStream(request);
  std::string line;
  std::string webSocketKey;

  while (std::getline(requestStream, line) && line != "\r") {
    if (line.find("Sec-WebSocket-Key") != std::string::npos) {
      webSocketKey = line.substr(line.find(":") + 2);
      webSocketKey = webSocketKey.substr(0, webSocketKey.length() - 1);
    }
  }

  std::string acceptKey = generateWebSocketAcceptKey(webSocketKey);
  std::ostringstream responseStream;
  responseStream << "HTTP/1.1 101 Switching Protocols\r\n";
  responseStream << "Upgrade: websocket\r\n";
  responseStream << "Connection: Upgrade\r\n";
  responseStream << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";
  std::string response = responseStream.str();

  send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);

  log("Handshake response sent: " + response);

  // Send initial game state after handshake
  std::string gameStateStr = serializeGameStateToString();
  std::string frame = encodeWebSocketFrame(gameStateStr);
  int sendResult = send(clientSocket, frame.c_str(), static_cast<int>(frame.size()), 0);
  if (sendResult == SOCKET_ERROR) {
    log("Failed to send initial state to client: " + std::to_string(WSAGetLastError()));
  } else {
    log("Initial game state sent to client.");
  }
}

// Accept new client connections
void acceptPlayer(SOCKET serverSocket)
{
  sockaddr_in clientAddr;
  socklen_t clientAddrSize = sizeof(clientAddr);

  while (true) {
    SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
    if (clientSocket == INVALID_SOCKET) {
      log("Accept failed: " + std::to_string(WSAGetLastError()));
      continue;
    }

    // Acquire a slot in the semaphore
    userSemaphore.acquire();

    {
      std::scoped_lock<std::mutex> lock(gameState.stateMutex);
      clients[clientSocket] = clientAddr; // This is where we are storing our clients
    }

    log("Client connected.");

    char buffer[1024];
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
      std::string request(buffer, bytesReceived);
      handleWebSocketHandshake(clientSocket, request);
      std::thread([clientSocket] {
        gameLogic(clientSocket);
        // Release the semaphore slot when the game logic is done
        userSemaphore.release();
        }).detach();
    }
    else {
      log("Failed to receive handshake request: " + std::to_string(WSAGetLastError()));
      // Release the semaphore slot if handshake fails
      userSemaphore.release();
    }
  }
}


void updateCoinCounts()
{
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - gameState.lastUpdate);

  if (elapsed.count() >= 1) {
    std::scoped_lock<std::mutex> lock(gameState.stateMutex);

    bool buildingsToSend = false;
    for (auto& playerPair : gameState.playerStates) {
      PlayerState& player = playerPair.second;
      for (auto& city : player.cities) {
          if (!city.buildings.empty()) {
            buildingsToSend = true;
          }
        for (auto& building : city.buildings) {
          //log("Adding " + std::to_string(building.coins) + " coins to city " + std::to_string(city.id));
          city.coins += building.coins;
          player.coins += building.coins;
        }
      }
      // Send the updated player state to the client
      if (buildingsToSend)
        sendPlayerStateDeltaToClient(player);
    }
    gameState.lastUpdate = now; // Update the last update time
  }
}

// This is the more global game loop running in the background, currently facing race condition problems
void boardLoop() 
{
  while (true) {
    clearCollidingEntities(); // Clear previous collisions

    bool hasEntitiesToProcess = false;

    {
      std::scoped_lock<std::mutex> lock(gameState.stateMutex);
      for (auto& playerPair : gameState.playerStates) {
        PlayerState& player = playerPair.second;
        for (auto& city : player.cities) {
          if (!city.collidingEntities.empty()) {
            hasEntitiesToProcess = true;
            break;
          }
          for (auto& troop : city.troops) {
            if (!troop.collidingEntities.empty()) {
              hasEntitiesToProcess = true;
              break;
            }
          }
          for (auto& building : city.buildings) {
            if (!building.collidingEntities.empty()) {
              hasEntitiesToProcess = true;
              break;
            }
          }
        }
        if (hasEntitiesToProcess) break;
      }
    }

    if (hasEntitiesToProcess) {
      handleTroopCollisions();
    }
    //std::thread(updateCoinCounts).detach();
    subtaskThreadPool.enqueue(updateCoinCounts);
    sendGameStateDeltasToClients();
    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Add a delay to avoid busy-waiting
  }
}

int main()
{
  initializeMaps();
  int threadsUsed = clientMessageCount + clientSubtaskCount;
  log("Allocating " + std::to_string(threadsUsed) + " of the CPUs " + std::to_string(std::thread::hardware_concurrency()) + " Available Concurrent Threads.");

  // NETWORK CONFIG
#ifdef _WIN32
  WSADATA wsaData;
  SOCKET serverSocket;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
    return 1;
  }
#else
  SOCKET serverSocket;
#endif

  serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serverSocket == INVALID_SOCKET) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }

  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(9001);

  if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
    std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
    std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 1;
  }

  log("Listening on port 9001.");
  initializeGameState();

  std::thread acceptThread([serverSocket]() {
    acceptPlayer(serverSocket);
    });
  boardLoop(); // Run the board loop in the main thread

  acceptThread.join();

  closesocket(serverSocket);
#ifdef _WIN32
  WSACleanup();
#endif

  // Close the logger
  Logger::getInstance().close();

  return 0;
}

