#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h> // <<< THÊM INCLUDE CHO SDL_MIXER
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <cctype>
#include <cmath>
#include <map>

using namespace std;

// --- Các hằng số ---
const int SCREEN_WIDTH = 1200;
const int SCREEN_HEIGHT = 880;
const int TILE_SIZE = 40;
const int MAP_WIDTH = SCREEN_WIDTH / TILE_SIZE;   // 30
const int MAP_HEIGHT = SCREEN_HEIGHT / TILE_SIZE; // 22
const int PLAYER_SPEED = 2;
const int ENEMY_SPEED = 1;
const float UNIFIED_BULLET_SPEED = 6.0f;
const int ORIGINAL_ENEMY_L1_MIN_DELAY_FOR_PLAYER_CALC = 85;
const int PLAYER_SHOT_COOLDOWN_FRAMES = static_cast<int>(round(ORIGINAL_ENEMY_L1_MIN_DELAY_FOR_PLAYER_CALC * 0.5)); // = 43
const int ENEMY_BASE_MIN_DELAY = 85;
const int ENEMY_BASE_RANGE = 160;
const int MIN_POSSIBLE_DELAY = 20;
const int MIN_POSSIBLE_RANGE = 25;
const int DELAY_REDUCTION_PER_LEVEL_MIN = 9;
const int DELAY_REDUCTION_PER_LEVEL_RANGE = 18;
const int TOUGH_ENEMY_HP = 3;
const Uint32 ENEMY_HIT_FLASH_DURATION = 100;

// --- Enum Loại Tường ---
enum class WallType { BRICK, STEEL, WATER, BUSH };

// --- Khai báo lớp ---
class Wall; class Bullet; class PlayerTank; class EnemyTank; class Game;

// --- Hàm tiện ích tải Texture và Sound ---
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer);
Mix_Chunk* loadSound(const std::string& path); // <<< KHAI BÁO HÀM TẢI ÂM THANH

// --- Lớp Wall ---
class Wall {
public:
    int x, y;
    SDL_Rect rect;
    bool active;
    WallType type;

    Wall(int startX, int startY, WallType wallType) :
        x(startX), y(startY),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        active(true), type(wallType) {}
};

// --- Lớp Bullet ---
class Bullet {
public:
    float x, y, dx, dy;
    SDL_Rect rect;
    bool active;
    float speed;

    Bullet(float startX, float startY, int dirX, int dirY) :
        x(startX), y(startY), dx(0.0f), dy(0.0f),
        rect({(int)startX, (int)startY, 8, 8}),
        active(true), speed(UNIFIED_BULLET_SPEED)
    {
        float length = sqrt(static_cast<float>(dirX * dirX + dirY * dirY));
        if (length > 0) { dx = (dirX / length) * speed; dy = (dirY / length) * speed; }
        else { dx = 0; dy = -speed; }
        rect.x = (int)(x - rect.w / 2.0f); rect.y = (int)(y - rect.h / 2.0f);
    }

    void move() {
        if (!active) return;
        x += dx; y += dy;
        rect.x = (int)(x - rect.w / 2.0f); rect.y = (int)(y - rect.h / 2.0f);
        if (rect.x < TILE_SIZE || rect.x + rect.w > SCREEN_WIDTH - TILE_SIZE ||
            rect.y < TILE_SIZE || rect.y + rect.h > SCREEN_HEIGHT - TILE_SIZE) {
            active = false;
        }
    }
};

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
    int hitPoints;
    int initialHitPoints;
    bool isHit = false;
    Uint32 hitStartTime = 0;
    // --- Âm thanh cho Enemy ---
    Mix_Chunk* shootSound = nullptr; // <<< Âm thanh bắn
    Mix_Chunk* destroySound = nullptr; // <<< Âm thanh bị phá hủy

    EnemyTank(int startX, int startY, int current_level, int initialHP = 1, Mix_Chunk* s_sound = nullptr, Mix_Chunk* d_sound = nullptr) : // <<< Nhận con trỏ âm thanh
        x(startX), y(startY), velocityX(0), velocityY(ENEMY_SPEED),
        lastDirX(0), lastDirY(1), rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        active(true), moveDecisionDelay(40 + rand() % 80), level(current_level),
        hitPoints(initialHP), initialHitPoints(initialHP),
        shootSound(s_sound), destroySound(d_sound) // <<< Lưu con trỏ âm thanh
    {
        resetShootCooldown();
    }

    void resetShootCooldown() {
        int levelAdjMin = (level - 1) * DELAY_REDUCTION_PER_LEVEL_MIN;
        int levelAdjRange = (level - 1) * DELAY_REDUCTION_PER_LEVEL_RANGE;
        int currentMin = max(MIN_POSSIBLE_DELAY, ENEMY_BASE_MIN_DELAY - levelAdjMin);
        int currentRange = max(MIN_POSSIBLE_RANGE, ENEMY_BASE_RANGE - levelAdjRange);
        if (currentRange < 1) currentRange = 1;
        shootDelay = currentMin + rand() % currentRange;
    }

    void takeHit() {
        if (!active) return;
        hitPoints--;
        cout << "Enemy hit! HP left: " << hitPoints << endl;

        isHit = true;
        hitStartTime = SDL_GetTicks();

        if (hitPoints <= 0) {
            active = false;
            // <<< PHÁT ÂM THANH KHI BỊ PHÁ HỦY (broken.wav)
            if (destroySound) Mix_PlayChannel(-1, destroySound, 0);
            cout << "Enemy destroyed!" << endl;
        }
    }

    void updateHitStatus() {
        if (isHit && SDL_GetTicks() > hitStartTime + ENEMY_HIT_FLASH_DURATION) {
            isHit = false;
        }
    }

    // <<< Trả về true nếu bắn thành công
    bool shoot() {
        if (!active) return false;
        float bulletStartX = rect.x + TILE_SIZE / 2.0f; float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1; else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1; else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        // <<< PHÁT ÂM THANH KHI BẮN (bullet_shot.wav)
        if (shootSound) Mix_PlayChannel(-1, shootSound, 0);
        return true;
    }


    void updateAIAndVelocity(const vector<Wall>& walls) {
        if (!active) return;
        // <<< Gọi shoot() và reset cooldown
        if (--shootDelay <= 0) {
             shoot(); // Gọi hàm shoot đã có phát âm thanh
             resetShootCooldown();
        }
        if (--moveDecisionDelay <= 0) {
            moveDecisionDelay = 45 + rand() % 100;
            SDL_Rect futureRect = rect; int checkDist = TILE_SIZE / 2;
            futureRect.x += velocityX * checkDist; futureRect.y += velocityY * checkDist;
            bool potentialCollision = false;
            if (futureRect.x < TILE_SIZE || futureRect.x + futureRect.w > SCREEN_WIDTH - TILE_SIZE || futureRect.y < TILE_SIZE || futureRect.y + futureRect.h > SCREEN_HEIGHT - TILE_SIZE) { potentialCollision = true; }
            else { for (const auto& w : walls) { if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&futureRect, &w.rect)) { potentialCollision = true; break; } } }
            if (potentialCollision || (rand() % 3 == 0)) {
                int currentDir = -1; if (velocityY < 0) currentDir = 0; else if (velocityY > 0) currentDir = 1; else if (velocityX < 0) currentDir = 2; else if (velocityX > 0) currentDir = 3;
                int attempts = 0, newDir; bool changed = false;
                while (attempts < 4) {
                    newDir = rand() % 4; if (newDir != currentDir) {
                        if (newDir == 0) { velocityX = 0; velocityY = -ENEMY_SPEED; lastDirX = 0; lastDirY = -1; changed = true; break; }
                        else if (newDir == 1) { velocityX = 0; velocityY = ENEMY_SPEED; lastDirX = 0; lastDirY = 1; changed = true; break; }
                        else if (newDir == 2) { velocityX = -ENEMY_SPEED; velocityY = 0; lastDirX = -1; lastDirY = 0; changed = true; break; }
                        else if (newDir == 3) { velocityX = ENEMY_SPEED; velocityY = 0; lastDirX = 1; lastDirY = 0; changed = true; break; }
                    } attempts++;
                }
                if (!changed) { newDir = rand() % 4; if (newDir == 0) { velocityX = 0; velocityY = -ENEMY_SPEED; lastDirX = 0; lastDirY = -1; } else if (newDir == 1) { velocityX = 0; velocityY = ENEMY_SPEED; lastDirX = 0; lastDirY = 1; } else if (newDir == 2) { velocityX = -ENEMY_SPEED; velocityY = 0; lastDirX = -1; lastDirY = 0; } else { velocityX = ENEMY_SPEED; velocityY = 0; lastDirX = 1; lastDirY = 0; } }
            }
        }
     }

    void updatePosition(const vector<Wall>& walls) {
        if (!active || (velocityX == 0 && velocityY == 0)) return;
        int originalX = x, originalY = y;
        x += velocityX; rect.x = x; bool collisionX = false;
        for (const auto& w : walls) { if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { x = originalX; rect.x = x; collisionX = true; break; } }
        if (!collisionX) { if (x < TILE_SIZE) { x = TILE_SIZE; rect.x = x; } else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) { x = SCREEN_WIDTH - TILE_SIZE - rect.w; rect.x = x; } }
        y += velocityY; rect.y = y; bool collisionY = false;
        for (const auto& w : walls) { if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { y = originalY; rect.y = y; collisionY = true; break; } }
        if (!collisionY) { if (y < TILE_SIZE) { y = TILE_SIZE; rect.y = y; } else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) { y = SCREEN_HEIGHT - TILE_SIZE - rect.h; rect.y = y; } }
    }

    void updateBullets() {
        if (!active) { bullets.clear(); return; }
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b) { return !b.active; }), bullets.end());
    }
};

// --- Lớp PlayerTank ---
class PlayerTank {
public:
    int x, y;
    int velocityX, velocityY;
    int lastDirX, lastDirY;
    SDL_Rect rect;
    vector<Bullet> bullets;
    int shotDelayCounter;

    PlayerTank(int startX = 0, int startY = 0) :
        x(startX), y(startY), velocityX(0), velocityY(0),
        lastDirX(0), lastDirY(-1), rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        shotDelayCounter(0) {}

    void reset(int startX, int startY) {
        x = startX; y = startY; rect.x = x; rect.y = y;
        velocityX = 0; velocityY = 0; lastDirX = 0; lastDirY = -1;
        bullets.clear(); shotDelayCounter = 0;
    }

    void updateCooldown() {
        if (shotDelayCounter > 0) shotDelayCounter--;
    }

    void updatePosition(const vector<Wall>& walls, const vector<EnemyTank>& enemies) {
        if (velocityX == 0 && velocityY == 0) return;
        int originalX = x, originalY = y;
        x += velocityX; rect.x = x; bool collisionX = false;
        for (const auto& w : walls) { if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { x = originalX; rect.x = x; collisionX = true; break; } }
        if (!collisionX) { for (const auto& e : enemies) { if (e.active && SDL_HasIntersection(&rect, &e.rect)) { x = originalX; rect.x = x; collisionX = true; break; } } }
        if (!collisionX) { if (x < TILE_SIZE) { x = TILE_SIZE; rect.x = x; } else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) { x = SCREEN_WIDTH - TILE_SIZE - rect.w; rect.x = x; } }
        y += velocityY; rect.y = y; bool collisionY = false;
        for (const auto& w : walls) { if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { y = originalY; rect.y = y; collisionY = true; break; } }
        if (!collisionY) { for (const auto& e : enemies) { if (e.active && SDL_HasIntersection(&rect, &e.rect)) { y = originalY; rect.y = y; collisionY = true; break; } } }
        if (!collisionY) { if (y < TILE_SIZE) { y = TILE_SIZE; rect.y = y; } else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) { y = SCREEN_HEIGHT - TILE_SIZE - rect.h; rect.y = y; } }
    }

    // <<< Trả về true nếu bắn thành công
    bool shoot() {
        if (shotDelayCounter > 0 || (lastDirX == 0 && lastDirY == 0)) return false; // <<< Trả về false nếu không bắn được
        float bulletStartX = rect.x + TILE_SIZE / 2.0f; float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1; else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1; else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        shotDelayCounter = PLAYER_SHOT_COOLDOWN_FRAMES;
        return true; // <<< Trả về true vì đã bắn
    }

    void updateBullets() {
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b) { return !b.active; }), bullets.end());
    }
};

// --- Lớp Game ---
class Game {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;
    vector<Wall> walls;
    PlayerTank player;
    vector<EnemyTank> enemies;
    int enemiesToSpawn = 0;
    int enemiesOnScreen = 0;
    int maxEnemiesOnScreen = 4;
    int currentLevel = 0;
    const int maxLevels = 5;
    int toughEnemiesToSpawnThisLevel = 0;
    int toughEnemiesSpawnedThisLevel = 0;

    // Textures
    SDL_Texture* brickTexture = nullptr;
    SDL_Texture* steelTexture = nullptr;
    SDL_Texture* waterTexture = nullptr;
    SDL_Texture* grassTexture = nullptr;
    SDL_Texture* bulletTexture = nullptr;
    SDL_Texture* playerTankUpTexture = nullptr;
    SDL_Texture* playerTankDownTexture = nullptr;
    SDL_Texture* playerTankLeftTexture = nullptr;
    SDL_Texture* playerTankRightTexture = nullptr;
    SDL_Texture* enemyTank2UpTexture = nullptr;
    SDL_Texture* enemyTank2DownTexture = nullptr;
    SDL_Texture* enemyTank2LeftTexture = nullptr;
    SDL_Texture* enemyTank2RightTexture = nullptr;
    SDL_Texture* enemyTank3UpTexture = nullptr;
    SDL_Texture* enemyTank3DownTexture = nullptr;
    SDL_Texture* enemyTank3LeftTexture = nullptr;
    SDL_Texture* enemyTank3RightTexture = nullptr;

    // --- Âm thanh ---
    Mix_Chunk* bulletShotSound = nullptr; // <<< Âm thanh bắn đạn
    Mix_Chunk* tankBrokenSound = nullptr; // <<< Âm thanh tank bị phá hủy
    Mix_Chunk* gameOverSound = nullptr;   // <<< Âm thanh thua game
    Mix_Chunk* levelUpSound = nullptr;    // <<< Âm thanh qua màn/bắt đầu màn

    Game() : player() {
        // --- Khởi tạo SDL ---
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { // <<< THÊM SDL_INIT_AUDIO
             cerr << "SDL Init Error: " << SDL_GetError() << endl; running = false; return;
        }

        // --- Khởi tạo SDL_image ---
        int imgFlags = IMG_INIT_PNG;
        if (!(IMG_Init(imgFlags) & imgFlags)) {
            cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << endl;
            running = false; SDL_Quit(); return;
        }

        // --- Khởi tạo SDL_mixer ---
        // Tham số: Tần số, Định dạng, Số kênh (2=stereo), Kích thước chunk
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << endl;
            // Có thể chọn không thoát nếu chỉ âm thanh lỗi, hoặc thoát hẳn:
            // running = false; IMG_Quit(); SDL_Quit(); return;
        } else {
            cout << "SDL_mixer initialized successfully!" << endl;
        }


        // --- Tạo cửa sổ và renderer ---
        window = SDL_CreateWindow("Battle City Clone (Sprites + Sound)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window) {
            cerr << "Window Creation Error: " << SDL_GetError() << endl;
            running = false; Mix_Quit(); IMG_Quit(); SDL_Quit(); return; // <<< Thêm Mix_Quit()
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
             cerr << "Renderer Creation Error: " << SDL_GetError() << endl;
             running = false; SDL_DestroyWindow(window); Mix_Quit(); IMG_Quit(); SDL_Quit(); return; // <<< Thêm Mix_Quit()
        }

        // --- Tải tài nguyên ---
        if (!loadMedia()) { // loadMedia giờ sẽ tải cả âm thanh
            running = false; return;
        }

        srand(time(0));
        char choice = ' '; cout << "=========================================\n BATTLE CITY CLONE (Sprites + Sound)\n=========================================\n"; cout << "Bat dau choi? (Y/N): ";
        while (true) { cin >> choice; cin.ignore(numeric_limits<streamsize>::max(), '\n'); choice = tolower(choice); if (choice == 'y') { cout << "Bat dau Level 1!\n-------------------------------\n"; setupLevel(1); break; } else if (choice == 'n') { cout << "Thoat game.\n"; running = false; break; } else { cout << "Nhap khong hop le. Vui long nhap Y hoac N: "; } }
     }

    ~Game() {
        cout << "Cleaning Game...\n";
        // Giải phóng Textures
        SDL_DestroyTexture(brickTexture); SDL_DestroyTexture(steelTexture); SDL_DestroyTexture(waterTexture); SDL_DestroyTexture(grassTexture); SDL_DestroyTexture(bulletTexture);
        SDL_DestroyTexture(playerTankUpTexture); SDL_DestroyTexture(playerTankDownTexture); SDL_DestroyTexture(playerTankLeftTexture); SDL_DestroyTexture(playerTankRightTexture);
        SDL_DestroyTexture(enemyTank2UpTexture); SDL_DestroyTexture(enemyTank2DownTexture); SDL_DestroyTexture(enemyTank2LeftTexture); SDL_DestroyTexture(enemyTank2RightTexture);
        SDL_DestroyTexture(enemyTank3UpTexture); SDL_DestroyTexture(enemyTank3DownTexture); SDL_DestroyTexture(enemyTank3LeftTexture); SDL_DestroyTexture(enemyTank3RightTexture);

        // <<< Giải phóng Âm thanh ---
        Mix_FreeChunk(bulletShotSound); bulletShotSound = nullptr;
        Mix_FreeChunk(tankBrokenSound); tankBrokenSound = nullptr;
        Mix_FreeChunk(gameOverSound);   gameOverSound = nullptr;
        Mix_FreeChunk(levelUpSound);    levelUpSound = nullptr;

        // Giải phóng Renderer và Window
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);

        // <<< Đóng các hệ thống con ---
        Mix_CloseAudio(); // Đóng SDL_mixer trước Mix_Quit
        Mix_Quit();       // Đóng hẳn SDL_mixer
        IMG_Quit();       // Đóng SDL_image
        SDL_Quit();       // Đóng SDL

        cout << "Game Cleaned!\n";
    }

    // <<< Hàm tải âm thanh ---
    Mix_Chunk* loadSound(const std::string& path) {
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (chunk == nullptr) {
            cerr << "Failed to load sound effect! SDL_mixer Error: " << path << " - " << Mix_GetError() << endl;
        } else {
            cout << "Loaded sound: " << path << endl;
        }
        return chunk;
    }

    bool loadMedia() {
         cout << "Loading media...\n"; bool success = true;
         // Tải Textures (như cũ)
         brickTexture = loadTexture("brick.png", renderer); if (!brickTexture) success = false;
         steelTexture = loadTexture("steel.png", renderer); if (!steelTexture) success = false;
         waterTexture = loadTexture("water.png", renderer); if (!waterTexture) success = false;
         grassTexture = loadTexture("grass.png", renderer); if (!grassTexture) success = false;
         bulletTexture = loadTexture("Bullet.png", renderer); if (!bulletTexture) success = false;
         playerTankUpTexture = loadTexture("tank1U.png", renderer); if (!playerTankUpTexture) success = false;
         playerTankDownTexture = loadTexture("tank1D.png", renderer); if (!playerTankDownTexture) success = false;
         playerTankLeftTexture = loadTexture("tank1L.png", renderer); if (!playerTankLeftTexture) success = false;
         playerTankRightTexture = loadTexture("tank1R.png", renderer); if (!playerTankRightTexture) success = false;
         enemyTank2UpTexture = loadTexture("tank2U.png", renderer); if (!enemyTank2UpTexture) success = false;
         enemyTank2DownTexture = loadTexture("tank2D.png", renderer); if (!enemyTank2DownTexture) success = false;
         enemyTank2LeftTexture = loadTexture("tank2L.png", renderer); if (!enemyTank2LeftTexture) success = false;
         enemyTank2RightTexture = loadTexture("tank2R.png", renderer); if (!enemyTank2RightTexture) success = false;
         enemyTank3UpTexture = loadTexture("tank3U.png", renderer); if (!enemyTank3UpTexture) success = false;
         enemyTank3DownTexture = loadTexture("tank3D.png", renderer); if (!enemyTank3DownTexture) success = false;
         enemyTank3LeftTexture = loadTexture("tank3L.png", renderer); if (!enemyTank3LeftTexture) success = false;
         enemyTank3RightTexture = loadTexture("tank3R.png", renderer); if (!enemyTank3RightTexture) success = false;

         // <<< Tải Âm thanh ---
         bulletShotSound = loadSound("bullet_shot.wav"); if (!bulletShotSound) success = false; // Có thể chọn không coi là lỗi nghiêm trọng
         tankBrokenSound = loadSound("broken.wav");      if (!tankBrokenSound) success = false;
         gameOverSound   = loadSound("game_over.wav");   if (!gameOverSound)   success = false;
         levelUpSound    = loadSound("level_up.wav");    if (!levelUpSound)    success = false;

         if (!success) cerr << "Failed to load one or more media files!\n"; else cout << "Media loaded successfully!\n";
         return success; // Có thể trả về true ngay cả khi âm thanh lỗi, tùy quyết định
    }

    void setupLevel(int level) {
        cout << "Loading Level " << level << "...\n"; currentLevel = level; string title = "Battle City Clone (Sprites + Sound) - Level " + to_string(level); SDL_SetWindowTitle(window, title.c_str()); player.reset(((MAP_WIDTH / 2) - 1) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE); walls.clear(); enemies.clear(); generateWalls(level);

        if      (level == 1) { enemiesToSpawn = 10; } else if (level == 2) { enemiesToSpawn = 15; } else if (level == 3) { enemiesToSpawn = 20; } else if (level == 4) { enemiesToSpawn = 25; } else if (level == 5) { enemiesToSpawn = 30; } else { enemiesToSpawn = 30 + (level - 5) * 5; }
        if      (level == 1) { maxEnemiesOnScreen = 4; } else if (level <= 3) { maxEnemiesOnScreen = 5; } else { maxEnemiesOnScreen = 6 + (level - 5) / 2; }
        if      (level == 1) toughEnemiesToSpawnThisLevel = 0; else if (level == 2) toughEnemiesToSpawnThisLevel = 1; else if (level == 3) toughEnemiesToSpawnThisLevel = 3; else if (level == 4) toughEnemiesToSpawnThisLevel = 7; else if (level == 5) toughEnemiesToSpawnThisLevel = 10; else toughEnemiesToSpawnThisLevel = 10 + (level - 5) * 2;

        toughEnemiesSpawnedThisLevel = 0;
        enemiesOnScreen = 0;
        // <<< PHÁT ÂM THANH BẮT ĐẦU LEVEL (level_up.wav)
        if (levelUpSound) Mix_PlayChannel(-1, levelUpSound, 0);
        spawnInitialEnemies();
        SDL_Delay(200); // Giảm delay 1 chút để âm thanh kịp phát
     }

    // --- generateWalls (Không thay đổi, giữ nguyên như code gốc) ---
    void generateWalls(int level) {
        walls.clear(); cout << "Generating walls for Level " << level << ".\n"; int baseCol = MAP_WIDTH / 2; int baseRow = MAP_HEIGHT - 2; int spawnRowTop = 1; auto isProtectedZone = [&](int r, int c) { if (r <= spawnRowTop + 2 && (c < 4 || c > MAP_WIDTH - 5)) return true; if (r >= baseRow - 1 && (c > baseCol - 3 && c < baseCol + 3)) return true; return false; }; for (int i = 0; i < MAP_HEIGHT; ++i) { walls.push_back(Wall(0, i * TILE_SIZE, WallType::STEEL)); walls.push_back(Wall((MAP_WIDTH - 1) * TILE_SIZE, i * TILE_SIZE, WallType::STEEL)); } for (int j = 1; j < MAP_WIDTH - 1; ++j) { walls.push_back(Wall(j * TILE_SIZE, 0, WallType::STEEL)); walls.push_back(Wall(j * TILE_SIZE, (MAP_HEIGHT - 1) * TILE_SIZE, WallType::STEEL)); } int wallDensityFactor = 28 + level * 2; int steelChance = 5 + level * 2; int waterChance = 3 + level * 2; int bushChance = (level >= 2) ? (level * 3) : 0; for (int i = spawnRowTop + 1; i < baseRow; ++i) { for (int j = 1; j < MAP_WIDTH - 1; ++j) { if (isProtectedZone(i, j)) continue; int placeRoll = rand() % wallDensityFactor; if (placeRoll < 10) { int typeRoll = rand() % 100; WallType currentType; bool placed = false; if (typeRoll < waterChance) { currentType = WallType::WATER; placed = true; } else if (typeRoll < waterChance + steelChance) { currentType = WallType::STEEL; placed = true; } else if (bushChance > 0 && typeRoll < waterChance + steelChance + bushChance) { currentType = WallType::BUSH; placed = true; } else { currentType = WallType::BRICK; placed = true; } if (placed) { walls.push_back(Wall(j * TILE_SIZE, i * TILE_SIZE, currentType)); } if (currentType != WallType::BUSH) { if (level > 2 && rand() % (8 - level + 1) == 0) { if (j + 1 < MAP_WIDTH - 1 && !isProtectedZone(i, j + 1)) walls.push_back(Wall((j + 1) * TILE_SIZE, i * TILE_SIZE, WallType::BRICK)); } if (level > 3 && rand() % (9 - level + 1) == 0) { if (i + 1 < baseRow && !isProtectedZone(i + 1, j)) walls.push_back(Wall(j * TILE_SIZE, (i + 1) * TILE_SIZE, WallType::BRICK)); } } } } } if (level >= 3) { for(int i=4; i<7; ++i) for(int j=4; j<7; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } if (level >= 4) { for(int i=MAP_HEIGHT-6; i<MAP_HEIGHT-3; ++i) for(int j=MAP_WIDTH-7; j<MAP_WIDTH-4; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::WATER)); } if (level == 5) { for(int i = MAP_HEIGHT/2 - 1; i <= MAP_HEIGHT/2 + 1; ++i ) { for (int j = MAP_WIDTH/2 - 2; j <= MAP_WIDTH/2 + 2; ++j) { if (i == MAP_HEIGHT/2 && j == MAP_WIDTH/2) continue; if (!isProtectedZone(i,j)) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } } } if (level >= 2 && level < 5) { for(int i = MAP_HEIGHT/2 - 2; i <= MAP_HEIGHT/2 + 2; ++i ) { for (int j = MAP_WIDTH/2 - 3; j <= MAP_WIDTH/2 + 3; ++j) { if (abs(i - MAP_HEIGHT/2) <=1 && abs(j-MAP_WIDTH/2) <=1) continue; if (!isProtectedZone(i,j) && rand()%4 == 0) { bool occupied = false; for(const auto& w : walls) { if (w.rect.x == j*TILE_SIZE && w.rect.y == i*TILE_SIZE && w.type != WallType::BUSH) { occupied = true; break; } } if (!occupied) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::BUSH)); } } } }
    }

    void spawnInitialEnemies() {
        cout << "Spawning initial enemies...\n"; int count = 0;
        while (count < maxEnemiesOnScreen && enemiesToSpawn > 0) {
            if (!trySpawnOneEnemy()) { cout << "Warning: Could not spawn initial enemy.\n"; break; }
            count++;
        }
        cout << "Initial spawn complete. Enemies on screen: " << enemiesOnScreen << ", To spawn: " << enemiesToSpawn << endl;
    }

    bool trySpawnOneEnemy() {
        if (enemiesOnScreen >= maxEnemiesOnScreen || enemiesToSpawn <= 0) return false;
        vector<pair<int, int>> spawnPoints = {{TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH / 2 - 1) * TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH - 2) * TILE_SIZE, TILE_SIZE}};
        random_shuffle(spawnPoints.begin(), spawnPoints.end());
        for (const auto& sp : spawnPoints) {
            SDL_Rect spawnRect = {sp.first, sp.second, TILE_SIZE, TILE_SIZE}; bool canSpawn = true;
            for (const auto& w : walls) if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&spawnRect, &w.rect)) { canSpawn = false; break; }
            if (canSpawn && SDL_HasIntersection(&spawnRect, &player.rect)) canSpawn = false;
            if (canSpawn) for (const auto& e : enemies) if (e.active && SDL_HasIntersection(&spawnRect, &e.rect)) { canSpawn = false; break; }
            if (canSpawn) {
                int initialHP = 1;
                if (toughEnemiesSpawnedThisLevel < toughEnemiesToSpawnThisLevel) {
                    initialHP = TOUGH_ENEMY_HP;
                    toughEnemiesSpawnedThisLevel++;
                }
                cout << "Spawning L" << currentLevel << " enemy (HP: " << initialHP << ") at (" << sp.first / TILE_SIZE << ", " << sp.second / TILE_SIZE << ")" << endl;
                // <<< TRUYỀN CON TRỎ ÂM THANH KHI TẠO ENEMY
                enemies.push_back(EnemyTank(sp.first, sp.second, currentLevel, initialHP, bulletShotSound, tankBrokenSound));
                enemiesOnScreen++; enemiesToSpawn--; return true;
            }
        }
        cout << "Warning: Failed to find a free spawn point.\n"; return false;
     }

    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP: player.velocityY = -PLAYER_SPEED; player.lastDirY = -1; player.lastDirX = 0; break;
                    case SDLK_DOWN: player.velocityY = PLAYER_SPEED; player.lastDirY = 1; player.lastDirX = 0; break;
                    case SDLK_LEFT: player.velocityX = -PLAYER_SPEED; player.lastDirX = -1; player.lastDirY = 0; break;
                    case SDLK_RIGHT: player.velocityX = PLAYER_SPEED; player.lastDirX = 1; player.lastDirY = 0; break;
                    case SDLK_SPACE:
                        // <<< KIỂM TRA NẾU BẮN THÀNH CÔNG THÌ PHÁT ÂM THANH
                        if (player.shoot()) {
                             if (bulletShotSound) Mix_PlayChannel(-1, bulletShotSound, 0);
                        }
                        break;
                    case SDLK_ESCAPE: running = false; break;
                }
            } else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    if (player.velocityY < 0) player.velocityY = 0; break;
                    case SDLK_DOWN:  if (player.velocityY > 0) player.velocityY = 0; break;
                    case SDLK_LEFT:  if (player.velocityX < 0) player.velocityX = 0; break;
                    case SDLK_RIGHT: if (player.velocityX > 0) player.velocityX = 0; break;
                }
                if (player.velocityX == 0 && player.velocityY != 0) {
                    player.lastDirX = 0; player.lastDirY = (player.velocityY > 0) ? 1 : -1;
                } else if (player.velocityY == 0 && player.velocityX != 0) {
                    player.lastDirY = 0; player.lastDirX = (player.velocityX > 0) ? 1 : -1;
                }
            }
        }
     }

    void update() {
         if (!running) return;
         player.updateCooldown(); player.updatePosition(walls, enemies); player.updateBullets();

         for (auto& enemy : enemies) {
             if (enemy.active) {
                 enemy.updateHitStatus();
                 enemy.updateAIAndVelocity(walls); // Đã xử lý âm thanh bắn bên trong
                 enemy.updatePosition(walls);
                 enemy.updateBullets();
             }
         }

         // Va chạm đạn Player
         for (auto& pB : player.bullets) {
             if (!pB.active) continue; bool pBulletHit = false;
             for (auto& w : walls) {
                 if (!w.active || w.type == WallType::BUSH || w.type == WallType::WATER) continue;
                 if (SDL_HasIntersection(&pB.rect, &w.rect)) {
                     pB.active = false;
                     if (w.type == WallType::BRICK) {
                         w.active = false;
                         // Nếu muốn âm thanh khi tường gạch vỡ, thêm vào đây:
                         // if (brickDestroySound) Mix_PlayChannel(-1, brickDestroySound, 0);
                     }
                     pBulletHit = true; break;
                 }
             }
             if (pBulletHit) continue;
             for (auto& e : enemies) {
                 if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) {
                     pB.active = false;
                     e.takeHit(); // Đã xử lý âm thanh tank vỡ bên trong takeHit
                     pBulletHit = true; break;
                 }
             }
         }

         // Va chạm đạn Enemy
         for (auto& e : enemies) {
             if (!e.active) continue;
             for (auto& eB : e.bullets) {
                 if (!eB.active) continue; bool eBulletHit = false;
                 for (auto& w : walls) {
                      if (!w.active || w.type == WallType::BUSH || w.type == WallType::WATER) continue;
                      if (SDL_HasIntersection(&eB.rect, &w.rect)) {
                          eB.active = false;
                          if (w.type == WallType::BRICK) {
                               w.active = false;
                               // Âm thanh tường gạch vỡ (nếu có)
                          }
                           eBulletHit = true; break;
                       }
                 }
                 if (eBulletHit) continue;

                 // <<< VA CHẠM VỚI PLAYER -> GAME OVER
                 if (SDL_HasIntersection(&eB.rect, &player.rect)) {
                     eB.active = false;
                     cout << "Player hit! Game Over at Level " << currentLevel << ".\n";
                     // <<< PHÁT ÂM THANH GAME OVER (game_over.wav)
                     if (gameOverSound) Mix_PlayChannel(-1, gameOverSound, 0);
                     SDL_Delay(1500); // Chờ chút để nghe âm thanh
                     running = false;
                     return; // Thoát khỏi update ngay
                 }
             }
         }

         // Dọn dẹp enemy không active
         enemies.erase(remove_if(enemies.begin(), enemies.end(), [](const EnemyTank &e) { return !e.active; }), enemies.end());
         enemiesOnScreen = enemies.size();

         // Spawn thêm enemy nếu cần
         if (enemiesToSpawn > 0 && enemiesOnScreen < maxEnemiesOnScreen) {
             static Uint32 lastSpawnTime = 0;
             const Uint32 SPAWN_DELAY = 2000; // 2 giây
             Uint32 currentTime = SDL_GetTicks();
             if (currentTime > lastSpawnTime + SPAWN_DELAY) {
                 if (trySpawnOneEnemy()) {
                     lastSpawnTime = currentTime;
                 } else {
                     // Nếu không spawn được, thử lại sớm hơn một chút
                     lastSpawnTime = currentTime - SPAWN_DELAY / 2;
                 }
             }
         }

         // Kiểm tra hoàn thành Level
         if (currentLevel > 0 && enemiesToSpawn == 0 && enemies.empty()) {
             cout << "\n===============================\n      LEVEL " << currentLevel << " CLEARED!      \n===============================\n\n";
             // <<< PHÁT ÂM THANH QUA MÀN (level_up.wav) - Phát lại khi qua màn
             if (levelUpSound) Mix_PlayChannel(-1, levelUpSound, 0);
             SDL_Delay(1000);
             if (currentLevel < maxLevels) {
                 cout << "Proceeding to next level...\n";
                 SDL_Delay(1500); // Chờ thêm chút
                 setupLevel(currentLevel + 1); // setupLevel sẽ phát âm thanh bắt đầu level
             } else {
                 cout << "*******************************\n* CONGRATULATIONS! YOU WIN! *\n*******************************\n";
                 SDL_Delay(2000); // Chờ nghe tiếng rồi thoát
                 running = false;
             }
         }
     }

    // --- render (Không thay đổi, giữ nguyên như code gốc) ---
    void render() {
        if (!renderer) return;
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); SDL_RenderClear(renderer); // Viền
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect playableArea = {TILE_SIZE, TILE_SIZE, SCREEN_WIDTH - 2 * TILE_SIZE, SCREEN_HEIGHT - 2 * TILE_SIZE}; SDL_RenderFillRect(renderer, &playableArea); // Nền
        // Vẽ tường cứng
        for (auto &wall : walls) { if (!wall.active) continue; SDL_Texture* tex = nullptr; switch (wall.type) { case WallType::BRICK: tex = brickTexture; break; case WallType::STEEL: tex = steelTexture; break; case WallType::WATER: tex = waterTexture; break; default: break; } if (tex) SDL_RenderCopy(renderer, tex, nullptr, &wall.rect); }
        // Vẽ Enemies
        for (auto &enemy : enemies) {
             if (!enemy.active) continue;
             SDL_Texture* tex = nullptr;
             if (enemy.initialHitPoints > 1) { // Tank Tough (tank3)
                 if (enemy.lastDirY < 0) tex = enemyTank3UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank3DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank3LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank3RightTexture; else tex = enemyTank3DownTexture;
             } else { // Tank Thường (tank2)
                 if (enemy.lastDirY < 0) tex = enemyTank2UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank2DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank2LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank2RightTexture; else tex = enemyTank2DownTexture;
             }
             if (tex) {
                 if (enemy.isHit) { SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 150); } else { SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 255); }
                 SDL_RenderCopy(renderer, tex, nullptr, &enemy.rect);
                 SDL_SetTextureAlphaMod(tex, 255); SDL_SetTextureColorMod(tex, 255, 255, 255);
             }
        }
        // Vẽ Player (tank1)
        SDL_Texture* playerTex = nullptr; if (player.lastDirY < 0) playerTex = playerTankUpTexture; else if (player.lastDirY > 0) playerTex = playerTankDownTexture; else if (player.lastDirX < 0) playerTex = playerTankLeftTexture; else if (player.lastDirX > 0) playerTex = playerTankRightTexture; else playerTex = playerTankUpTexture; if (playerTex) SDL_RenderCopy(renderer, playerTex, nullptr, &player.rect);
        // Vẽ bụi cây
        for (auto &wall : walls) { if (wall.active && wall.type == WallType::BUSH && grassTexture) SDL_RenderCopy(renderer, grassTexture, nullptr, &wall.rect); }
        // Vẽ Đạn
        if (bulletTexture) { for (auto &bullet : player.bullets) if (bullet.active) SDL_RenderCopy(renderer, bulletTexture, nullptr, &bullet.rect); for (auto &enemy : enemies) for (auto &bullet : enemy.bullets) if (bullet.active) SDL_RenderCopy(renderer, bulletTexture, nullptr, &bullet.rect); }
        SDL_RenderPresent(renderer);
    }

    void run() {
        const int TARGET_FPS = 60; const int FRAME_DELAY = 1000 / TARGET_FPS;
        Uint32 frameStart; int frameTime;
        while (running) {
            frameStart = SDL_GetTicks(); handleEvents(); update(); render();
            frameTime = SDL_GetTicks() - frameStart;
            if (FRAME_DELAY > frameTime) SDL_Delay(FRAME_DELAY - frameTime);
        }
     }
};

// --- Hàm main ---
int main(int argc, char* argv[]) {
    Game game;
    if (game.running) {
        game.run();
    }
    // Game destructor sẽ tự động dọn dẹp
    return 0;
}

// --- Hàm tiện ích tải Texture --- (Định nghĩa)
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer) {
    SDL_Texture* newTexture = nullptr;
    SDL_Surface* loadedSurface = IMG_Load(path.c_str());
    if (loadedSurface == nullptr) {
        cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << endl;
    } else {
        newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
        if (newTexture == nullptr) {
            cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << endl;
        }
        SDL_FreeSurface(loadedSurface);
    }
    return newTexture;
}
