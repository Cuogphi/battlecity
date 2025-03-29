#include <SDL.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm> // Cho random_shuffle, max
#include <cstdlib>
#include <ctime>
#include <limits> // Cho numeric_limits
#include <cctype> // Cho tolower
#include <cmath>   // Cho sqrt, max

using namespace std;

// --- Các hằng số ---
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 880;
const int TILE_SIZE = 40;
const int MAP_WIDTH = SCREEN_WIDTH / TILE_SIZE;   // 30
const int MAP_HEIGHT = SCREEN_HEIGHT / TILE_SIZE; // 22
const int PLAYER_SPEED = 2;
const int ENEMY_SPEED = 1;
const float BULLET_SPEED_MULTIPLIER = 2.5;

// --- Enum Loại Tường ---
enum class WallType {
    BRICK,
    STEEL,
    WATER
};

// --- Khai báo lớp ---
class Wall;
class Bullet;
class PlayerTank;
class EnemyTank;
class Game;

// --- Lớp Wall ---
class Wall {
public:
    int x, y;
    SDL_Rect rect;
    bool active;
    WallType type;

    Wall(int startX, int startY, WallType wallType) :
        x(startX),
        y(startY),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        active(true),
        type(wallType) {}

    void render(SDL_Renderer* renderer) {
        if (active) {
            switch (type) {
                case WallType::BRICK:
                    SDL_SetRenderDrawColor(renderer, 150, 75, 0, 255); // Nâu
                    break;
                case WallType::STEEL:
                    SDL_SetRenderDrawColor(renderer, 192, 192, 192, 255); // Xám
                    break;
                case WallType::WATER:
                    SDL_SetRenderDrawColor(renderer, 0, 0, 200, 255); // Xanh dương
                    break;
            }
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}; // End Wall

// --- Lớp Bullet ---
class Bullet {
public:
    float x, y, dx, dy;
    SDL_Rect rect;
    bool active;
    float speed;

    Bullet(float startX, float startY, int dirX, int dirY, float baseSpeed = PLAYER_SPEED) :
        x(startX),
        y(startY),
        dx(0.0f),
        dy(0.0f),
        rect({(int)startX, (int)startY, 10, 10}),
        active(true),
        speed(baseSpeed * BULLET_SPEED_MULTIPLIER)
    {
        float length = sqrt(dirX * dirX + dirY * dirY);
        if (length > 0) {
             dx = (dirX / length) * speed;
             dy = (dirY / length) * speed;
        } else {
            active = false;
        }
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);
    }

    void move() {
        if (!active) return;
        x += dx;
        y += dy;
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);

        if (rect.x < TILE_SIZE || rect.x + rect.w > SCREEN_WIDTH - TILE_SIZE ||
            rect.y < TILE_SIZE || rect.y + rect.h > SCREEN_HEIGHT - TILE_SIZE) {
            active = false;
        }
    }

    void render(SDL_Renderer* renderer) {
        if (active) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Trắng
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}; // End Bullet

// --- Lớp EnemyTank ---
class EnemyTank {
public:
    int x, y;
    int velocityX, velocityY;
    int lastDirX, lastDirY;
    SDL_Rect rect;
    bool active;
    vector<Bullet> bullets;
    int moveDecisionDelay;
    int shootDelay;
    int level;
    int baseShootDelayMin;
    int baseShootDelayRange;

    EnemyTank(int startX, int startY, int current_level) :
        x(startX), y(startY),
        velocityX(0), velocityY(ENEMY_SPEED),
        lastDirX(0), lastDirY(1),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        active(true),
        moveDecisionDelay(40 + rand() % 80),
        level(current_level),
        baseShootDelayMin(85),
        baseShootDelayRange(160)
    {
        resetShootCooldown();
    }

    void resetShootCooldown() {
        int levelAdjMin = (level - 1) * 9;
        int levelAdjRange = (level - 1) * 18;
        int currentMin = max(20, baseShootDelayMin - levelAdjMin);
        int currentRange = max(25, baseShootDelayRange - levelAdjRange);
        shootDelay = currentMin + rand() % currentRange;
    }

    void updateAIAndVelocity(const vector<Wall>& walls) {
        if (!active) return;

        if (--shootDelay <= 0) {
            shoot();
            resetShootCooldown();
        }

        if (--moveDecisionDelay <= 0) {
            moveDecisionDelay = 45 + rand() % 100;
            SDL_Rect futureRect = rect;
            int checkDist = TILE_SIZE / 2;
            futureRect.x += velocityX * checkDist;
            futureRect.y += velocityY * checkDist;
            bool potentialCollision = false;

            if (futureRect.x < TILE_SIZE || futureRect.x + futureRect.w > SCREEN_WIDTH - TILE_SIZE ||
                futureRect.y < TILE_SIZE || futureRect.y + futureRect.h > SCREEN_HEIGHT - TILE_SIZE) {
                potentialCollision = true;
            } else {
                for (const auto& w : walls) {
                    if (w.active && SDL_HasIntersection(&futureRect, &w.rect) &&
                        (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) {
                        potentialCollision = true;
                        break;
                    }
                }
            }

            if (potentialCollision || rand() % 3 == 0) {
                int currentDir = -1;
                if (velocityY < 0) currentDir = 0; else if (velocityY > 0) currentDir = 1;
                else if (velocityX < 0) currentDir = 2; else if (velocityX > 0) currentDir = 3;
                int attempts = 0, newDir; bool changed = false;
                while (attempts < 4) {
                    newDir = rand() % 4;
                    if (newDir != currentDir) {
                        if (newDir == 0) { velocityX = 0; velocityY = -ENEMY_SPEED; lastDirX = 0; lastDirY = -1; changed = true; break; }
                        else if (newDir == 1) { velocityX = 0; velocityY = ENEMY_SPEED; lastDirX = 0; lastDirY = 1; changed = true; break; }
                        else if (newDir == 2) { velocityX = -ENEMY_SPEED; velocityY = 0; lastDirX = -1; lastDirY = 0; changed = true; break; }
                        else if (newDir == 3) { velocityX = ENEMY_SPEED; velocityY = 0; lastDirX = 1; lastDirY = 0; changed = true; break; }
                    }
                    attempts++;
                }
                if (!changed) {
                     newDir = rand() % 4;
                     if (newDir == 0) { velocityX = 0; velocityY = -ENEMY_SPEED; lastDirX = 0; lastDirY = -1; }
                     else if (newDir == 1) { velocityX = 0; velocityY = ENEMY_SPEED; lastDirX = 0; lastDirY = 1; }
                     else if (newDir == 2) { velocityX = -ENEMY_SPEED; velocityY = 0; lastDirX = -1; lastDirY = 0; }
                     else { velocityX = ENEMY_SPEED; velocityY = 0; lastDirX = 1; lastDirY = 0; }
                }
            }
        }
    }

    void updatePosition(const vector<Wall>& walls) {
        if (!active || (velocityX == 0 && velocityY == 0)) return;
        int originalX = x, originalY = y;

        x += velocityX; rect.x = x; bool collisionX = false;
        for (const auto& w : walls) {
            if (w.active && SDL_HasIntersection(&rect, &w.rect) &&
                (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) {
                x = originalX; rect.x = x; collisionX = true; break;
            }
        }
        if (!collisionX) {
            if (x < TILE_SIZE) { x = TILE_SIZE; rect.x = x; }
            else if (x > SCREEN_WIDTH - TILE_SIZE - TILE_SIZE) { x = SCREEN_WIDTH - TILE_SIZE - TILE_SIZE; rect.x = x; }
        }

        y += velocityY; rect.y = y; bool collisionY = false;
        for (const auto& w : walls) {
            if (w.active && SDL_HasIntersection(&rect, &w.rect) &&
                (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) {
                y = originalY; rect.y = y; collisionY = true; break;
            }
        }
        if (!collisionY) {
            if (y < TILE_SIZE) { y = TILE_SIZE; rect.y = y; }
            else if (y > SCREEN_HEIGHT - TILE_SIZE - TILE_SIZE) { y = SCREEN_HEIGHT - TILE_SIZE - TILE_SIZE; rect.y = y; }
        }
    }

    void shoot() {
        if (!active) return;
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f;
        else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f;
        else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY, ENEMY_SPEED));
    }

    void updateBullets() {
        if (!active) { bullets.clear(); return; }
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b) { return !b.active; }), bullets.end());
    }

    void render(SDL_Renderer* renderer) {
        if (active) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red
            SDL_RenderFillRect(renderer, &rect);
            for (auto &b : bullets) b.render(renderer);
        }
    }
}; // End EnemyTank

// --- Lớp PlayerTank ---
class PlayerTank {
public:
    int x, y;
    int velocityX, velocityY;
    int lastDirX, lastDirY;
    SDL_Rect rect;
    vector<Bullet> bullets;
    Uint32 lastShotTime;
    const Uint32 shotCooldown = 500;

    PlayerTank(int startX = 0, int startY = 0) :
        x(startX), y(startY),
        velocityX(0), velocityY(0),
        lastDirX(0), lastDirY(-1),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        lastShotTime(0)
        {}

    void reset(int startX, int startY) {
        x = startX; y = startY; rect.x = x; rect.y = y;
        velocityX = 0; velocityY = 0;
        lastDirX = 0; lastDirY = -1;
        bullets.clear(); lastShotTime = 0;
    }

    void updatePosition(const vector<Wall>& walls, const vector<EnemyTank>& enemies) {
        if (velocityX == 0 && velocityY == 0) return;
        int originalX = x, originalY = y;

        x += velocityX; rect.x = x; bool collisionX = false;
        for (const auto& w : walls) {
            if (w.active && SDL_HasIntersection(&rect, &w.rect) &&
                (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) {
                x = originalX; rect.x = x; collisionX = true; break;
            }
        }
        if (!collisionX) {
            for (const auto& e : enemies) {
                if (e.active && SDL_HasIntersection(&rect, &e.rect)) {
                    x = originalX; rect.x = x; collisionX = true; break;
                }
            }
        }
        if (!collisionX) {
            if (x < TILE_SIZE) { x = TILE_SIZE; rect.x = x; }
            else if (x > SCREEN_WIDTH - TILE_SIZE - TILE_SIZE) { x = SCREEN_WIDTH - TILE_SIZE - TILE_SIZE; rect.x = x; }
        }

        y += velocityY; rect.y = y; bool collisionY = false;
        for (const auto& w : walls) {
            if (w.active && SDL_HasIntersection(&rect, &w.rect) &&
                (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) {
                y = originalY; rect.y = y; collisionY = true; break;
            }
        }
        if (!collisionY) {
            for (const auto& e : enemies) {
                if (e.active && SDL_HasIntersection(&rect, &e.rect)) {
                    y = originalY; rect.y = y; collisionY = true; break;
                }
            }
        }
        if (!collisionY) {
            if (y < TILE_SIZE) { y = TILE_SIZE; rect.y = y; }
            else if (y > SCREEN_HEIGHT - TILE_SIZE - TILE_SIZE) { y = SCREEN_HEIGHT - TILE_SIZE - TILE_SIZE; rect.y = y; }
        }
    }

    void shoot() {
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime < lastShotTime + shotCooldown || (lastDirX == 0 && lastDirY == 0)) return;
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f;
        else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f;
        else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY, PLAYER_SPEED));
        lastShotTime = currentTime;
    }

    void updateBullets() {
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b) { return !b.active; }), bullets.end());
    }

    void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow
        SDL_RenderFillRect(renderer, &rect);
        for (auto &b : bullets) b.render(renderer);
    }
}; // End PlayerTank

// --- Lớp Game ---
class Game {
public:
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool running;
    vector<Wall> walls;
    PlayerTank player;
    vector<EnemyTank> enemies;
    int enemiesToSpawn;
    int enemiesOnScreen;
    int maxEnemiesOnScreen;
    int currentLevel;
    const int maxLevels = 5;

    Game() : player() {
        running = true; window = nullptr; renderer = nullptr; currentLevel = 0;
        enemiesToSpawn = 0; enemiesOnScreen = 0; maxEnemiesOnScreen = 4;
        if (SDL_Init(SDL_INIT_VIDEO) < 0) { cerr << "SDL Init Error: " << SDL_GetError() << endl; running = false; return; }
        window = SDL_CreateWindow("Battle City Clone (Simpler Maps)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window) { cerr << "Window Creation Error: " << SDL_GetError() << endl; running = false; SDL_Quit(); return; }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) { cerr << "Renderer Creation Error: " << SDL_GetError() << endl; running = false; SDL_DestroyWindow(window); SDL_Quit(); return; }
        srand(time(0));
        char choice = ' ';
        cout << "===============================\n BATTLE CITY CLONE (Simpler Maps)\n===============================\n";
        cout << "Bat dau choi? (Y/N): ";
        while (true) {
            cin >> choice; cin.ignore(numeric_limits<streamsize>::max(), '\n'); choice = tolower(choice);
            if (choice == 'y') { cout << "Bat dau Level 1!\n-------------------------------\n"; setupLevel(1); break; }
            else if (choice == 'n') { cout << "Thoat game.\n"; running = false; break; }
            else { cout << "Nhap khong hop le. Vui long nhap Y hoac N: "; }
        }
    }

    ~Game() {
        cout << "Cleaning Game...\n"; if (renderer) SDL_DestroyRenderer(renderer); if (window) SDL_DestroyWindow(window); SDL_Quit(); cout << "Game Cleaned!\n";
    }

    void setupLevel(int level) {
        cout << "Loading Level " << level << "...\n"; currentLevel = level;
        string title = "Battle City Clone (Simpler Maps) - Level " + to_string(level);
        SDL_SetWindowTitle(window, title.c_str());
        player.reset(((MAP_WIDTH / 2) - 1) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE);
        walls.clear(); enemies.clear();
        generateWalls(level); // Generate map for the new level
        // Set enemy counts based on level
        if      (level == 1) { enemiesToSpawn = 10; maxEnemiesOnScreen = 4; }
        else if (level == 2) { enemiesToSpawn = 15; maxEnemiesOnScreen = 5; }
        else if (level == 3) { enemiesToSpawn = 20; maxEnemiesOnScreen = 5; }
        else if (level == 4) { enemiesToSpawn = 25; maxEnemiesOnScreen = 6; }
        else if (level == 5) { enemiesToSpawn = 30; maxEnemiesOnScreen = 6; }
        else { enemiesToSpawn = 30 + (level - 5) * 5; maxEnemiesOnScreen = 6 + (level - 5) / 2; }
        enemiesOnScreen = 0;
        spawnInitialEnemies();
        SDL_Delay(500);
    }

    // Generate simpler walls with gradually increasing complexity
    void generateWalls(int level) {
        walls.clear();
        cout << "Generating simpler walls for Level " << level << ".\n";

        int baseCol = MAP_WIDTH / 2; int baseRow = MAP_HEIGHT - 2;
        int spawnRowTop = 1;

        auto isProtectedZone = [&](int r, int c) { // Lambda to check protected zones
            if (r <= spawnRowTop + 2 && (c < 4 || c > MAP_WIDTH - 5)) return true;
            if (r >= baseRow - 1 && (c > baseCol - 3 && c < baseCol + 3)) return true;
            return false;
        };

        // Generate steel borders
        for (int i = 0; i < MAP_HEIGHT; ++i) { walls.push_back(Wall(0, i * TILE_SIZE, WallType::STEEL)); walls.push_back(Wall((MAP_WIDTH - 1) * TILE_SIZE, i * TILE_SIZE, WallType::STEEL)); }
        for (int j = 1; j < MAP_WIDTH - 1; ++j) { walls.push_back(Wall(j * TILE_SIZE, 0, WallType::STEEL)); walls.push_back(Wall(j * TILE_SIZE, (MAP_HEIGHT - 1) * TILE_SIZE, WallType::STEEL)); }

        // Generate inner walls based on level
        int wallDensityFactor = 28 + level * 2; // Lower = denser (Range increases with level)
        int steelChance = 5 + level * 2;       // % chance for steel
        int waterChance = 3 + level * 2;       // % chance for water

        for (int i = spawnRowTop + 1; i < baseRow; ++i) {
            for (int j = 1; j < MAP_WIDTH - 1; ++j) {
                if (isProtectedZone(i, j)) continue;

                int placeRoll = rand() % wallDensityFactor;
                if (placeRoll < 10) { // Only place a wall if roll is low enough
                    int typeRoll = rand() % 100;
                    WallType currentType;
                    if (typeRoll < waterChance) currentType = WallType::WATER;
                    else if (typeRoll < waterChance + steelChance) currentType = WallType::STEEL;
                    else currentType = WallType::BRICK;
                    walls.push_back(Wall(j * TILE_SIZE, i * TILE_SIZE, currentType));

                    // Small chance to add adjacent bricks in higher levels
                    if (level > 2 && rand() % (8 - level + 1) == 0) { // Adjust chance based on level
                        if (j + 1 < MAP_WIDTH - 1 && !isProtectedZone(i, j + 1))
                            walls.push_back(Wall((j + 1) * TILE_SIZE, i * TILE_SIZE, WallType::BRICK));
                    }
                     if (level > 3 && rand() % (9 - level + 1) == 0) {
                         if (i + 1 < baseRow && !isProtectedZone(i + 1, j))
                             walls.push_back(Wall(j * TILE_SIZE, (i + 1) * TILE_SIZE, WallType::BRICK));
                     }
                }
            }
        }

         // Add small fixed features in later levels
        if (level >= 3) { // Small steel block top-left area
             for(int i=4; i<7; ++i) for(int j=4; j<7; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL));
        }
        if (level >= 4) { // Small water pool bottom-right area
            for(int i=MAP_HEIGHT-6; i<MAP_HEIGHT-3; ++i) for(int j=MAP_WIDTH-7; j<MAP_WIDTH-4; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::WATER));
        }
         if (level == 5) { // Add a central steel structure
             for(int i = MAP_HEIGHT/2 - 1; i <= MAP_HEIGHT/2 + 1; ++i ) {
                 for (int j = MAP_WIDTH/2 - 2; j <= MAP_WIDTH/2 + 2; ++j) {
                     if (i == MAP_HEIGHT/2 && j == MAP_WIDTH/2) continue; // Leave center open
                     if (!isProtectedZone(i,j)) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL));
                 }
             }
         }
    }

    // Spawn initial group of enemies
    void spawnInitialEnemies() {
         cout << "Spawning initial enemies...\n";
         int count = 0;
         while (count < maxEnemiesOnScreen && enemiesToSpawn > 0) {
             if (!trySpawnOneEnemy()) break;
             count++;
         }
    }

    // Attempt to spawn one new enemy
    bool trySpawnOneEnemy() {
        if (enemiesOnScreen >= maxEnemiesOnScreen || enemiesToSpawn <= 0) return false;
        vector<pair<int, int>> spawnPoints = {{TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH / 2 - 1) * TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH - 2) * TILE_SIZE, TILE_SIZE}};
        random_shuffle(spawnPoints.begin(), spawnPoints.end());
        for (const auto& sp : spawnPoints) {
            SDL_Rect spawnRect = {sp.first, sp.second, TILE_SIZE, TILE_SIZE};
            bool canSpawn = true;
            for (const auto& w : walls) if (w.active && SDL_HasIntersection(&spawnRect, &w.rect) && (w.type == WallType::BRICK || w.type == WallType::STEEL || w.type == WallType::WATER)) { canSpawn = false; break; }
            if (canSpawn && SDL_HasIntersection(&spawnRect, &player.rect)) canSpawn = false;
            if (canSpawn) for (const auto& e : enemies) if (e.active && SDL_HasIntersection(&spawnRect, &e.rect)) { canSpawn = false; break; }
            if (canSpawn) {
                cout << "Spawning L" << currentLevel << " enemy at (" << sp.first / TILE_SIZE << ", " << sp.second / TILE_SIZE << ")" << endl;
                enemies.push_back(EnemyTank(sp.first, sp.second, currentLevel));
                enemiesOnScreen++;
                enemiesToSpawn--;
                return true;
            }
        }
        return false;
    }

    // Handle user input
    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = false; }
            else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP: player.velocityY = -PLAYER_SPEED; player.lastDirY = -1; player.lastDirX = 0; break;
                    case SDLK_DOWN: player.velocityY = PLAYER_SPEED; player.lastDirY = 1; player.lastDirX = 0; break;
                    case SDLK_LEFT: player.velocityX = -PLAYER_SPEED; player.lastDirX = -1; player.lastDirY = 0; break;
                    case SDLK_RIGHT: player.velocityX = PLAYER_SPEED; player.lastDirX = 1; player.lastDirY = 0; break;
                    case SDLK_SPACE: player.shoot(); break;
                    case SDLK_ESCAPE: running = false; break;
                }
            } else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP: if (player.velocityY < 0) player.velocityY = 0; break;
                    case SDLK_DOWN: if (player.velocityY > 0) player.velocityY = 0; break;
                    case SDLK_LEFT: if (player.velocityX < 0) player.velocityX = 0; break;
                    case SDLK_RIGHT: if (player.velocityX > 0) player.velocityX = 0; break;
                }
            }
        }
    }

    // Update game state (physics, collisions, AI, spawning, win/lose)
    void update() {
        if (!running) return;

        player.updatePosition(walls, enemies);
        player.updateBullets();

        int activeCount = 0;
        for (auto& enemy : enemies) {
            if (enemy.active) {
                activeCount++;
                enemy.updateAIAndVelocity(walls);
                enemy.updatePosition(walls);
                enemy.updateBullets();
            }
        }
        enemiesOnScreen = activeCount; // Update count before potentially removing

        // --- Bullet Collisions ---
        // Player Bullets
        for (auto& pB : player.bullets) {
            if (!pB.active) continue;
            bool hit = false;
            for (auto& w : walls) { // vs Walls
                if (!w.active) continue;
                if (SDL_HasIntersection(&pB.rect, &w.rect)) {
                    if (w.type == WallType::BRICK) { pB.active = false; w.active = false; hit = true; break; }
                    else if (w.type == WallType::STEEL) { pB.active = false; hit = true; break; }
                }
            }
            if (hit) continue;
            for (auto& e : enemies) { // vs Enemies
                if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) {
                    pB.active = false; e.active = false;
                    // cout << "Enemy destroyed! Total left: " << enemiesToSpawn + enemiesOnScreen - 1 << endl;
                    break;
                }
            }
        }
        // Enemy Bullets
        for (auto& e : enemies) {
            if (!e.active) continue;
            for (auto& eB : e.bullets) {
                if (!eB.active) continue;
                bool hit = false;
                for (auto& w : walls) { // vs Walls
                     if (!w.active) continue;
                     if (SDL_HasIntersection(&eB.rect, &w.rect)) {
                         if (w.type == WallType::BRICK) { eB.active = false; w.active = false; hit = true; break; }
                         else if (w.type == WallType::STEEL) { eB.active = false; hit = true; break; }
                     }
                }
                if (hit) continue;
                if (SDL_HasIntersection(&eB.rect, &player.rect)) { // vs Player
                    eB.active = false;
                    cout << "Player hit! Game Over at Level " << currentLevel << ".\n";
                    running = false; return; // Exit update immediately
                }
            }
        }

        // Remove destroyed enemies and update count
        enemies.erase(remove_if(enemies.begin(), enemies.end(), [](const EnemyTank &e) { return !e.active; }), enemies.end());
        enemiesOnScreen = enemies.size();

        // Spawn more enemies if needed
        if (enemiesToSpawn > 0 && enemiesOnScreen < maxEnemiesOnScreen) {
             static Uint32 lastSpawnTime = 0;
             Uint32 currentTime = SDL_GetTicks();
             const Uint32 SPAWN_DELAY = 2000; // 2 second delay
             if (currentTime > lastSpawnTime + SPAWN_DELAY) {
                 trySpawnOneEnemy();
                 lastSpawnTime = currentTime;
             }
        }

        // Check for level clear / game win
        if (enemiesToSpawn == 0 && enemies.empty() && currentLevel > 0) {
            cout << "\n===============================\n      LEVEL " << currentLevel << " CLEARED!      \n===============================\n\n";
            SDL_Delay(1000);
            if (currentLevel < maxLevels) {
                cout << "Proceeding to next level...\n";
                SDL_Delay(1500);
                setupLevel(currentLevel + 1);
            } else {
                cout << "*******************************\n* CONGRATULATIONS! YOU WIN! *\n*******************************\n";
                running = false;
            }
        }
    }

    // Render the game
    void render() {
        if (!renderer) return;
        // Clear screen (border color)
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        SDL_RenderClear(renderer);
        // Draw play area background
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_Rect playableArea = {TILE_SIZE, TILE_SIZE, SCREEN_WIDTH - 2 * TILE_SIZE, SCREEN_HEIGHT - 2 * TILE_SIZE};
        SDL_RenderFillRect(renderer, &playableArea);
        // Draw walls
        for (auto &wall : walls) wall.render(renderer);
        // Draw player
        player.render(renderer);
        // Draw enemies
        for (auto &enemy : enemies) enemy.render(renderer);
        // Present buffer
        SDL_RenderPresent(renderer);
    }

    // Main game loop
    void run() {
        const int TARGET_FPS = 60;
        const int FRAME_DELAY = 1000 / TARGET_FPS;
        Uint32 frameStart; int frameTime;
        while (running) {
            frameStart = SDL_GetTicks();
            handleEvents();
            update();
            render();
            frameTime = SDL_GetTicks() - frameStart;
            if (FRAME_DELAY > frameTime) {
                SDL_Delay(FRAME_DELAY - frameTime);
            }
        }
    }
}; // End class Game

// --- Hàm main ---
int main(int argc, char* argv[]) {
    Game game;
    if (game.running) {
        game.run();
    }
    return 0;
}
