#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm> // Cho std::remove_if, std::random_shuffle, std::max
#include <cstdlib>   // Cho rand(), srand()
#include <ctime>     // Cho time()
#include <limits>    // Cho std::numeric_limits
#include <cctype>    // Cho std::tolower
#include <cmath>     // Cho std::sqrt, std::round, std::abs
#include <map>       // (Không dùng trong code cuối)

using namespace std; // Sử dụng không gian tên std

// =============================================================================
// == Hằng Số Toàn Cục ==
// =============================================================================
const int SCREEN_WIDTH = 1200; // Chiều rộng cửa sổ game
const int SCREEN_HEIGHT = 880; // Chiều cao cửa sổ game
const int TILE_SIZE = 40;      // Kích thước mỗi ô vuông (tile) trên bản đồ
const int MAP_WIDTH = SCREEN_WIDTH / TILE_SIZE;   // Chiều rộng bản đồ theo số ô (30)
const int MAP_HEIGHT = SCREEN_HEIGHT / TILE_SIZE; // Chiều cao bản đồ theo số ô (22)
const int PLAYER_SPEED = 2; // Tốc độ di chuyển của người chơi
const int ENEMY_SPEED = 1;  // Tốc độ di chuyển của kẻ địch
const float UNIFIED_BULLET_SPEED = 6.0f; // Tốc độ chung cho đạn của người chơi và địch
const int ORIGINAL_ENEMY_L1_MIN_DELAY_FOR_PLAYER_CALC = 85; // Tham số gốc để tính cooldown bắn của player
const int PLAYER_SHOT_COOLDOWN_FRAMES = static_cast<int>(round(ORIGINAL_ENEMY_L1_MIN_DELAY_FOR_PLAYER_CALC * 0.5)); // ~43 frames
const int ENEMY_BASE_MIN_DELAY = 85;  // Thời gian chờ tối thiểu cơ bản (frame)
const int ENEMY_BASE_RANGE = 160; // Khoảng ngẫu nhiên cộng thêm vào thời gian chờ cơ bản
const int MIN_POSSIBLE_DELAY = 20; // Thời gian chờ bắn tối thiểu có thể (sau khi trừ theo level)
const int MIN_POSSIBLE_RANGE = 25; // Khoảng ngẫu nhiên tối thiểu có thể (sau khi trừ theo level)
const int DELAY_REDUCTION_PER_LEVEL_MIN = 9;
const int DELAY_REDUCTION_PER_LEVEL_RANGE = 18;
const int TOUGH_ENEMY_HP = 3; // Máu của loại xe tăng địch "trâu bò"
const Uint32 ENEMY_HIT_FLASH_DURATION = 100; // Thời gian nhấp nháy của địch khi bị bắn (ms)

// =============================================================================
// == Enums (Các kiểu liệt kê) ==
// =============================================================================
enum class WallType { BRICK, STEEL, WATER, BUSH };
enum class GameState { SELECT_MODE, PLAYING, GAME_OVER };

// =============================================================================
// == Khai Báo Trước Các Lớp ==
// =============================================================================
class Wall;
class Bullet;
class PlayerTank;
class EnemyTank;
class Game;

// =============================================================================
// == Khai Báo Trước Hàm Tiện Ích ==
// =============================================================================
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer);

// =============================================================================
// == Lớp Wall (Tường) ==
// =============================================================================
class Wall {
public:
    int x, y;
    SDL_Rect rect;
    bool active;
    WallType type;

    Wall(int startX, int startY, WallType wallType) :
        x(startX), y(startY), rect({startX, startY, TILE_SIZE, TILE_SIZE}), active(true), type(wallType) {}
};

// =============================================================================
// == Lớp Bullet (Đạn) ==
// =============================================================================
class Bullet {
public:
    float x, y;
    float dx, dy;
    SDL_Rect rect;
    bool active;
    float speed;

    Bullet(float startX, float startY, int dirX, int dirY) :
        x(startX), y(startY), dx(0.0f), dy(0.0f), rect({(int)startX, (int)startY, 8, 8}), active(true), speed(UNIFIED_BULLET_SPEED)
    {
        float length = sqrt(static_cast<float>(dirX * dirX + dirY * dirY));
        if (length > 0) {
            dx = (dirX / length) * speed;
            dy = (dirY / length) * speed;
        } else {
            dx = 0; dy = -speed; // Mặc định bắn lên
        }
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);
    }

    void move() {
        if (!active) return;
        x += dx; y += dy;
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);
        if (rect.x < TILE_SIZE || rect.x + rect.w > SCREEN_WIDTH - TILE_SIZE ||
            rect.y < TILE_SIZE || rect.y + rect.h > SCREEN_HEIGHT - TILE_SIZE) {
            active = false;
        }
    }
};

// =============================================================================
// == Lớp PlayerTank (Xe Tăng Người Chơi) ==
// =============================================================================
class PlayerTank {
public:
    int x, y;
    int velocityX, velocityY;
    int lastDirX, lastDirY;
    SDL_Rect rect;
    vector<Bullet> bullets;
    int shotDelayCounter;
    bool isActive = true; // Dùng isActive thay vì active để phân biệt với các lớp khác

    PlayerTank(int startX = 0, int startY = 0) :
        x(startX), y(startY), velocityX(0), velocityY(0), lastDirX(0), lastDirY(-1),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}), shotDelayCounter(0), isActive(true) {}

    void reset(int startX, int startY) {
        x = startX; y = startY; rect.x = x; rect.y = y;
        velocityX = 0; velocityY = 0; lastDirX = 0; lastDirY = -1;
        bullets.clear(); shotDelayCounter = 0; isActive = true;
    }

    void hitByEnemy() {
        if (!isActive) return;
        cout << "Player hit!" << endl;
        isActive = false;
        // Trong game thực tế, bạn có thể giảm mạng hoặc bắt đầu hồi sinh ở đây
    }

    void updateCooldown() {
        if (shotDelayCounter > 0) shotDelayCounter--;
    }

    void updatePosition(const vector<Wall>& walls, const vector<EnemyTank>& enemies); // Định nghĩa sau EnemyTank

    bool shoot() {
        if (!isActive || shotDelayCounter > 0 || (lastDirX == 0 && lastDirY == 0)) return false;
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1; else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1; else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        shotDelayCounter = PLAYER_SHOT_COOLDOWN_FRAMES;
        return true;
    }

    void updateBullets() {
        if (!isActive) { bullets.clear(); return; }
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b){ return !b.active; }), bullets.end());
    }
};


// =============================================================================
// == Lớp EnemyTank (Xe Tăng Địch) - CÓ AI CẢI TIẾN ==
// =============================================================================
class EnemyTank {
public:
    int x, y;
    int velocityX, velocityY;
    int lastDirX, lastDirY;
    SDL_Rect rect;
    bool active; // Dùng active cho địch
    vector<Bullet> bullets;
    int moveDecisionDelay;
    int shootDelay;
    int level;
    int hitPoints;
    int initialHitPoints;
    bool isHit = false;
    Uint32 hitStartTime = 0;
    Mix_Chunk* shootSound = nullptr;
    Mix_Chunk* destroySound = nullptr;

    EnemyTank(int startX, int startY, int current_level, int initialHP = 1, Mix_Chunk* s_sound = nullptr, Mix_Chunk* d_sound = nullptr) :
        x(startX), y(startY), velocityX(0), velocityY(ENEMY_SPEED), lastDirX(0), lastDirY(1),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}), active(true),
        moveDecisionDelay(40 + rand() % 80), level(current_level), hitPoints(initialHP),
        initialHitPoints(initialHP), shootSound(s_sound), destroySound(d_sound)
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
        hitPoints--; isHit = true; hitStartTime = SDL_GetTicks();
        if (hitPoints <= 0) {
            active = false;
            if (destroySound) Mix_PlayChannel(-1, destroySound, 0);
        }
    }

    void updateHitStatus() {
        if (isHit && SDL_GetTicks() > hitStartTime + ENEMY_HIT_FLASH_DURATION) {
            isHit = false;
        }
    }

    bool shoot() {
        if (!active) return false;
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1; else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1; else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1;
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        if (shootSound) Mix_PlayChannel(-1, shootSound, 0);
        return true;
    }

    // --- HÀM AI CẢI TIẾN ---
    void updateAIAndVelocity(const PlayerTank& p1, const PlayerTank& p2, int numPlayers, const vector<Wall>& walls);

    // --- HÀM KIỂM TRA DI CHUYỂN HỢP LỆ ---
    bool isMoveValid(int nextX, int nextY, const vector<Wall>& walls) const {
        SDL_Rect futureRect = {nextX, nextY, TILE_SIZE, TILE_SIZE};
        if (nextX < TILE_SIZE || nextX + TILE_SIZE > SCREEN_WIDTH - TILE_SIZE ||
            nextY < TILE_SIZE || nextY + TILE_SIZE > SCREEN_HEIGHT - TILE_SIZE) {
            return false; // Va biên
        }
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&futureRect, &w.rect)) {
                return false; // Va tường
            }
        }
        // TODO (Optional): Check collision with other EnemyTanks
        return true; // Hợp lệ
    }

    void updatePosition(const vector<Wall>& walls) {
        if (!active || (velocityX == 0 && velocityY == 0)) return;

        int originalX = x, originalY = y;

        // Di chuyển X
        x += velocityX; rect.x = x;
        bool collisionX = false;
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                x = originalX; rect.x = x; collisionX = true; break;
            }
        }
        if (!collisionX) { // Kiểm tra biên X sau tường
            if (x < TILE_SIZE) x = TILE_SIZE;
            else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) x = SCREEN_WIDTH - TILE_SIZE - rect.w;
            rect.x = x;
        }

        // Di chuyển Y
        y += velocityY; rect.y = y;
        bool collisionY = false;
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                y = originalY; rect.y = y; collisionY = true; break;
            }
        }
         if (!collisionY) { // Kiểm tra biên Y sau tường
            if (y < TILE_SIZE) y = TILE_SIZE;
            else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) y = SCREEN_HEIGHT - TILE_SIZE - rect.h;
            rect.y = y;
        }
    }

    void updateBullets() {
        if (!active) { bullets.clear(); return; }
        for (auto &b : bullets) if (b.active) b.move();
        bullets.erase(remove_if(bullets.begin(), bullets.end(), [](const Bullet &b){ return !b.active; }), bullets.end());
    }
};


// --- ĐỊNH NGHĨA HÀM AI CẢI TIẾN CHO ENEMY TANK ---
void EnemyTank::updateAIAndVelocity(const PlayerTank& p1, const PlayerTank& p2, int numPlayers, const vector<Wall>& walls) {
    if (!active) {
        return; // Không làm gì nếu đã bị hạ
    }

    // --- A. Xác Định Mục Tiêu ---
    const PlayerTank* targetPlayer = nullptr;
    float minDistSq = std::numeric_limits<float>::max();
    int targetX = -1, targetY = -1; // Tọa độ mục tiêu

    if (p1.isActive) {
        float dx1 = p1.x + TILE_SIZE / 2.0f - (this->x + TILE_SIZE / 2.0f);
        float dy1 = p1.y + TILE_SIZE / 2.0f - (this->y + TILE_SIZE / 2.0f);
        float distSq1 = dx1 * dx1 + dy1 * dy1;
        if (distSq1 < minDistSq) {
            minDistSq = distSq1; targetPlayer = &p1; targetX = p1.x; targetY = p1.y;
        }
    }
    if (numPlayers == 2 && p2.isActive) {
        float dx2 = p2.x + TILE_SIZE / 2.0f - (this->x + TILE_SIZE / 2.0f);
        float dy2 = p2.y + TILE_SIZE / 2.0f - (this->y + TILE_SIZE / 2.0f);
        float distSq2 = dx2 * dx2 + dy2 * dy2;
        if (distSq2 < minDistSq) {
            minDistSq = distSq2; targetPlayer = &p2; targetX = p2.x; targetY = p2.y;
        }
    }

    // --- B. Quyết Định Bắn ---
    if (--shootDelay <= 0) {
        bool shouldShoot = false;
        if (targetPlayer) {
            float dx = targetX - this->x; float dy = targetY - this->y;
            bool alignedX = (lastDirX != 0) && (std::abs(dy) < TILE_SIZE * 0.6f) && ((dx > 0 && lastDirX > 0) || (dx < 0 && lastDirX < 0));
            bool alignedY = (lastDirY != 0) && (std::abs(dx) < TILE_SIZE * 0.6f) && ((dy > 0 && lastDirY > 0) || (dy < 0 && lastDirY < 0));
            if (alignedX || alignedY) shouldShoot = true;
        }
        if (!shouldShoot && (rand() % 5 == 0)) shouldShoot = true; // 1/5 cơ hội bắn ngẫu nhiên

        if (shouldShoot) {
            shoot(); resetShootCooldown();
        } else {
             resetShootCooldown(); shootDelay = std::max(MIN_POSSIBLE_DELAY, shootDelay / 3 + 5);
        }
    }

    // --- C. Quyết Định Di Chuyển ---
    if (--moveDecisionDelay <= 0) {
        moveDecisionDelay = 40 + rand() % 80;

        struct MoveOption { int vx, vy, dirX, dirY; };
        vector<MoveOption> options = { {0, -ENEMY_SPEED, 0, -1}, {0, ENEMY_SPEED, 0, 1}, {-ENEMY_SPEED, 0, -1, 0}, {ENEMY_SPEED, 0, 1, 0} };
        random_shuffle(options.begin(), options.end());

        int bestVx = 0, bestVy = 0; int bestDirX = lastDirX, bestDirY = lastDirY; // Giữ hướng cũ làm mặc định nếu bị kẹt
        bool foundValidMove = false;

        const float CHASE_DISTANCE_THRESHOLD_SQ = (TILE_SIZE * 7.0f) * (TILE_SIZE * 7.0f);
        bool chaseMode = (targetPlayer && minDistSq < CHASE_DISTANCE_THRESHOLD_SQ);

        if (chaseMode) {
            float dx = targetX - this->x; float dy = targetY - this->y;
            int chaseVx = 0, chaseVy = 0; int chaseDirX = lastDirX, chaseDirY = lastDirY;

            if (std::abs(dx) > std::abs(dy) + TILE_SIZE * 0.2f) {
                chaseVx = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED; chaseDirX = (dx > 0) ? 1 : -1; chaseDirY = 0;
            } else if (std::abs(dy) > std::abs(dx) + TILE_SIZE * 0.2f) {
                chaseVy = (dy > 0) ? ENEMY_SPEED : -ENEMY_SPEED; chaseDirY = (dy > 0) ? 1 : -1; chaseDirX = 0;
            } else {
                bool movingTowardsTarget = !((velocityX > 0 && dx < 0) || (velocityX < 0 && dx > 0) || (velocityY > 0 && dy < 0) || (velocityY < 0 && dy > 0));
                if (movingTowardsTarget && (velocityX != 0 || velocityY != 0)) {
                    chaseVx = velocityX; chaseVy = velocityY; chaseDirX = lastDirX; chaseDirY = lastDirY;
                } else {
                    if (rand() % 2 == 0 && std::abs(dx) > TILE_SIZE * 0.1f) {
                       chaseVx = (dx > 0) ? ENEMY_SPEED : -ENEMY_SPEED; chaseDirX = (dx > 0) ? 1 : -1; chaseDirY = 0;
                    } else if (std::abs(dy) > TILE_SIZE * 0.1f) {
                       chaseVy = (dy > 0) ? ENEMY_SPEED : -ENEMY_SPEED; chaseDirY = (dy > 0) ? 1 : -1; chaseDirX = 0;
                    } else { chaseMode = false; }
                }
            }

            if (chaseMode && isMoveValid(this->x + chaseVx, this->y + chaseVy, walls)) {
                 bestVx = chaseVx; bestVy = chaseVy; bestDirX = chaseDirX; bestDirY = chaseDirY;
                 foundValidMove = true;
            }
        }

        if (!foundValidMove) {
            int reverseVx = 0, reverseVy = 0, reverseDirX = 0, reverseDirY = 0; // Lưu hướng quay đầu nếu có
            bool reversePossible = false;

            for (const auto& option : options) {
                bool isReversing = (option.vx == -this->velocityX && option.vy == -this->velocityY && (velocityX !=0 || velocityY !=0));
                if (isMoveValid(this->x + option.vx, this->y + option.vy, walls)) {
                    if (!isReversing) { // Ưu tiên hướng không quay đầu
                        bestVx = option.vx; bestVy = option.vy; bestDirX = option.dirX; bestDirY = option.dirY;
                        foundValidMove = true;
                        break; // Tìm được hướng tốt
                    } else { // Ghi nhận hướng quay đầu nhưng tiếp tục tìm
                        reverseVx = option.vx; reverseVy = option.vy; reverseDirX = option.dirX; reverseDirY = option.dirY;
                        reversePossible = true;
                    }
                }
            }
            // Nếu không tìm được hướng nào khác và có thể quay đầu -> chọn quay đầu
            if (!foundValidMove && reversePossible) {
                bestVx = reverseVx; bestVy = reverseVy; bestDirX = reverseDirX; bestDirY = reverseDirY;
                foundValidMove = true;
            }
        }

        if (foundValidMove) {
             if(bestVx != velocityX || bestVy != velocityY || (velocityX == 0 && velocityY == 0)) {
                velocityX = bestVx; velocityY = bestVy; lastDirX = bestDirX; lastDirY = bestDirY;
             }
        } else {
             velocityX = 0; velocityY = 0; // Bị kẹt, đứng yên
             // Giữ nguyên lastDirX, lastDirY
        }
    } // End if (--moveDecisionDelay <= 0)
}


// --- ĐỊNH NGHĨA HÀM PlayerTank::updatePosition ---
// Cần định nghĩa sau khi EnemyTank đã được định nghĩa đầy đủ
void PlayerTank::updatePosition(const vector<Wall>& walls, const vector<EnemyTank>& enemies) {
    if (!isActive || (velocityX == 0 && velocityY == 0)) return;

    int originalX = x, originalY = y;

    // Di chuyển X và kiểm tra va chạm
    x += velocityX; rect.x = x;
    bool collisionX = false;
    for (const auto& w : walls) if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { x = originalX; rect.x = x; collisionX = true; break; }
    if (!collisionX) for (const auto& e : enemies) if (e.active && SDL_HasIntersection(&rect, &e.rect)) { x = originalX; rect.x = x; collisionX = true; break; }
    if (!collisionX) { // Kiểm tra biên X
        if (x < TILE_SIZE) x = TILE_SIZE;
        else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) x = SCREEN_WIDTH - TILE_SIZE - rect.w;
        rect.x = x;
    }

    // Di chuyển Y và kiểm tra va chạm
    y += velocityY; rect.y = y;
    bool collisionY = false;
    for (const auto& w : walls) if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) { y = originalY; rect.y = y; collisionY = true; break; }
    if (!collisionY) for (const auto& e : enemies) if (e.active && SDL_HasIntersection(&rect, &e.rect)) { y = originalY; rect.y = y; collisionY = true; break; }
    if (!collisionY) { // Kiểm tra biên Y
        if (y < TILE_SIZE) y = TILE_SIZE;
        else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) y = SCREEN_HEIGHT - TILE_SIZE - rect.h;
        rect.y = y;
    }
}


// =============================================================================
// == Lớp Game (Quản lý chính) ==
// =============================================================================
class Game {
public:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;
    GameState currentState = GameState::SELECT_MODE;
    int numberOfPlayers = 1;
    vector<Wall> walls;
    PlayerTank player1;
    PlayerTank player2;
    vector<EnemyTank> enemies;
    int enemiesToSpawn = 0;
    int enemiesOnScreen = 0;
    int maxEnemiesOnScreen = 4;
    int currentLevel = 0;
    const int maxLevels = 5;
    int toughEnemiesToSpawnThisLevel = 0;
    int toughEnemiesSpawnedThisLevel = 0;

    // Textures
    SDL_Texture* menuTexture = nullptr;
    SDL_Texture* brickTexture = nullptr; SDL_Texture* steelTexture = nullptr; SDL_Texture* waterTexture = nullptr; SDL_Texture* grassTexture = nullptr;
    SDL_Texture* bulletTexture = nullptr;
    SDL_Texture* player1TankUpTexture = nullptr; SDL_Texture* player1TankDownTexture = nullptr; SDL_Texture* player1TankLeftTexture = nullptr; SDL_Texture* player1TankRightTexture = nullptr;
    SDL_Texture* player2TankUpTexture = nullptr; SDL_Texture* player2TankDownTexture = nullptr; SDL_Texture* player2TankLeftTexture = nullptr; SDL_Texture* player2TankRightTexture = nullptr;
    SDL_Texture* enemyTank2UpTexture = nullptr; SDL_Texture* enemyTank2DownTexture = nullptr; SDL_Texture* enemyTank2LeftTexture = nullptr; SDL_Texture* enemyTank2RightTexture = nullptr;
    SDL_Texture* enemyTank3UpTexture = nullptr; SDL_Texture* enemyTank3DownTexture = nullptr; SDL_Texture* enemyTank3LeftTexture = nullptr; SDL_Texture* enemyTank3RightTexture = nullptr;
    SDL_Texture* gameOverTexture = nullptr;

    // Sounds
    Mix_Chunk* bulletShotSound = nullptr; Mix_Chunk* tankBrokenSound = nullptr; Mix_Chunk* gameOverSound = nullptr; Mix_Chunk* levelUpSound = nullptr; Mix_Chunk* playerDestroySound = nullptr;

    Game() : player1(), player2() {
        cout << "Initializing Game..." << endl;
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { cerr << "SDL Init Error: " << SDL_GetError() << endl; running = false; return; }
        cout << "SDL Initialized." << endl;
        int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
        if (!(IMG_Init(imgFlags) & imgFlags)) { cerr << "SDL_image Error: " << IMG_GetError() << endl; running = false; SDL_Quit(); return; }
        cout << "SDL_image Initialized." << endl;
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) { cerr << "Warning: SDL_mixer Error: " << Mix_GetError() << endl; } else { cout << "SDL_mixer Initialized." << endl; }

        window = SDL_CreateWindow("Battle City Clone - Select Mode", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window) { cerr << "Window Creation Error: " << SDL_GetError() << endl; running = false; Mix_Quit(); IMG_Quit(); SDL_Quit(); return; }
        cout << "Window Created." << endl;

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) { cerr << "Renderer Creation Error: " << SDL_GetError() << endl; running = false; SDL_DestroyWindow(window); Mix_Quit(); IMG_Quit(); SDL_Quit(); return; }
        cout << "Renderer Created." << endl;

        currentState = GameState::SELECT_MODE;
        if (!loadMedia()) { cerr << "ERROR: Failed to load essential media! Exiting." << endl; running = false; SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); Mix_Quit(); IMG_Quit(); SDL_Quit(); return; }
        srand(time(0));
        cout << "Game Initialized Successfully. Showing Menu." << endl;
    }

    ~Game() {
        cout << "Cleaning Game Resources..." << endl;
        if(menuTexture) SDL_DestroyTexture(menuTexture); if(brickTexture) SDL_DestroyTexture(brickTexture); if(steelTexture) SDL_DestroyTexture(steelTexture); if(waterTexture) SDL_DestroyTexture(waterTexture); if(grassTexture) SDL_DestroyTexture(grassTexture); if(bulletTexture) SDL_DestroyTexture(bulletTexture);
        if(player1TankUpTexture) SDL_DestroyTexture(player1TankUpTexture); if(player1TankDownTexture) SDL_DestroyTexture(player1TankDownTexture); if(player1TankLeftTexture) SDL_DestroyTexture(player1TankLeftTexture); if(player1TankRightTexture) SDL_DestroyTexture(player1TankRightTexture);
        if(player2TankUpTexture) SDL_DestroyTexture(player2TankUpTexture); if(player2TankDownTexture) SDL_DestroyTexture(player2TankDownTexture); if(player2TankLeftTexture) SDL_DestroyTexture(player2TankLeftTexture); if(player2TankRightTexture) SDL_DestroyTexture(player2TankRightTexture);
        if(enemyTank2UpTexture) SDL_DestroyTexture(enemyTank2UpTexture); if(enemyTank2DownTexture) SDL_DestroyTexture(enemyTank2DownTexture); if(enemyTank2LeftTexture) SDL_DestroyTexture(enemyTank2LeftTexture); if(enemyTank2RightTexture) SDL_DestroyTexture(enemyTank2RightTexture);
        if(enemyTank3UpTexture) SDL_DestroyTexture(enemyTank3UpTexture); if(enemyTank3DownTexture) SDL_DestroyTexture(enemyTank3DownTexture); if(enemyTank3LeftTexture) SDL_DestroyTexture(enemyTank3LeftTexture); if(enemyTank3RightTexture) SDL_DestroyTexture(enemyTank3RightTexture);
        if(gameOverTexture) SDL_DestroyTexture(gameOverTexture);
        if(bulletShotSound) Mix_FreeChunk(bulletShotSound); if(tankBrokenSound) Mix_FreeChunk(tankBrokenSound); if(gameOverSound) Mix_FreeChunk(gameOverSound); if(levelUpSound) Mix_FreeChunk(levelUpSound); if(playerDestroySound) Mix_FreeChunk(playerDestroySound);
        if (renderer) SDL_DestroyRenderer(renderer); if (window) SDL_DestroyWindow(window);
        Mix_CloseAudio(); Mix_Quit(); IMG_Quit(); SDL_Quit();
        cout << "Game Resources Cleaned." << endl;
    }

    Mix_Chunk* loadSound(const std::string& path) {
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (!chunk) cerr << "Failed to load sound: " << path << " - " << Mix_GetError() << endl;
        else cout << "Loaded sound: " << path << endl;
        return chunk;
    }

    bool loadMedia() {
         cout << "Loading media..." << endl; bool essential_success = true;
         menuTexture = loadTexture("giao_dien.jpg", renderer); if (!menuTexture) return false; else cout << "Loaded texture: giao_dien.jpg" << endl;
         brickTexture = loadTexture("brick.png", renderer); if (!brickTexture) essential_success = false;
         steelTexture = loadTexture("steel.png", renderer); if (!steelTexture) essential_success = false;
         waterTexture = loadTexture("water.png", renderer); if (!waterTexture) essential_success = false;
         grassTexture = loadTexture("grass.png", renderer); if (!grassTexture) cerr << "Warning: Failed to load grass.png" << endl;
         bulletTexture = loadTexture("Bullet.png", renderer); if (!bulletTexture) essential_success = false;
         player1TankUpTexture = loadTexture("tank1U.png", renderer); if (!player1TankUpTexture) essential_success = false; player1TankDownTexture = loadTexture("tank1D.png", renderer); if (!player1TankDownTexture) essential_success = false; player1TankLeftTexture = loadTexture("tank1L.png", renderer); if (!player1TankLeftTexture) essential_success = false; player1TankRightTexture = loadTexture("tank1R.png", renderer); if (!player1TankRightTexture) essential_success = false;
         player2TankUpTexture = loadTexture("tank_player2U.png", renderer); if (!player2TankUpTexture) essential_success = false; player2TankDownTexture = loadTexture("tank_player2D.png", renderer); if (!player2TankDownTexture) essential_success = false; player2TankLeftTexture = loadTexture("tank_player2L.png", renderer); if (!player2TankLeftTexture) essential_success = false; player2TankRightTexture = loadTexture("tank_player2R.png", renderer); if (!player2TankRightTexture) essential_success = false;
         enemyTank2UpTexture = loadTexture("tank2U.png", renderer); if (!enemyTank2UpTexture) essential_success = false; enemyTank2DownTexture = loadTexture("tank2D.png", renderer); if (!enemyTank2DownTexture) essential_success = false; enemyTank2LeftTexture = loadTexture("tank2L.png", renderer); if (!enemyTank2LeftTexture) essential_success = false; enemyTank2RightTexture = loadTexture("tank2R.png", renderer); if (!enemyTank2RightTexture) essential_success = false;
         enemyTank3UpTexture = loadTexture("tank3U.png", renderer); if (!enemyTank3UpTexture) essential_success = false; enemyTank3DownTexture = loadTexture("tank3D.png", renderer); if (!enemyTank3DownTexture) essential_success = false; enemyTank3LeftTexture = loadTexture("tank3L.png", renderer); if (!enemyTank3LeftTexture) essential_success = false; enemyTank3RightTexture = loadTexture("tank3R.png", renderer); if (!enemyTank3RightTexture) essential_success = false;
         gameOverTexture = loadTexture("game_over.png", renderer); if (!gameOverTexture) cerr << "Warning: Failed to load game_over.png!" << endl;
         bulletShotSound = loadSound("bullet_shot.wav"); tankBrokenSound = loadSound("broken.wav"); playerDestroySound = loadSound("broken.wav"); gameOverSound = loadSound("game_over.wav"); levelUpSound = loadSound("level_up.wav");
         if (!essential_success) cerr << "ERROR: Failed to load one or more essential game textures!\n";
         cout << "Media loading finished." << endl; return essential_success;
    }

    void setupLevel(int level) {
        cout << "Loading Level " << level << "..." << endl; currentLevel = level;
        string title = "Battle City Clone - Level " + to_string(level) + " (" + to_string(numberOfPlayers) + "P)"; SDL_SetWindowTitle(window, title.c_str());
        walls.clear(); enemies.clear(); generateWalls(level);
        player1.reset(((MAP_WIDTH / 2) - 2) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE);
        if (numberOfPlayers == 2) player2.reset(((MAP_WIDTH / 2) + 1) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE); else player2.isActive = false;
        if (level==1) enemiesToSpawn=10; else if (level==2) enemiesToSpawn=15; else if (level==3) enemiesToSpawn=20; else if (level==4) enemiesToSpawn=25; else if (level==5) enemiesToSpawn=30; else enemiesToSpawn=30+(level-5)*5;
        if (level==1) maxEnemiesOnScreen=4; else if (level<=3) maxEnemiesOnScreen=5; else maxEnemiesOnScreen=6+(level-5)/2;
        if (level==1) toughEnemiesToSpawnThisLevel=0; else if (level==2) toughEnemiesToSpawnThisLevel=1; else if (level==3) toughEnemiesToSpawnThisLevel=3; else if (level==4) toughEnemiesToSpawnThisLevel=7; else if (level==5) toughEnemiesToSpawnThisLevel=10; else toughEnemiesToSpawnThisLevel=10+(level-5)*2;
        toughEnemiesSpawnedThisLevel = 0; enemiesOnScreen = 0;
        spawnInitialEnemies(); SDL_Delay(100); cout << "Level " << level << " Started." << endl;
    }

    // Hàm GenerateWalls giữ nguyên như cũ (rất phức tạp)
    void generateWalls(int level) { walls.clear(); int baseCol = MAP_WIDTH / 2; int baseRow = MAP_HEIGHT - 2; int spawnRowTop = 1; auto isProtectedZone = [&](int r, int c) { if (r <= spawnRowTop + 2 && (c < 4 || c > MAP_WIDTH - 5)) return true; if (r >= baseRow - 1 && (c > baseCol - 3 && c < baseCol + 3)) return true; return false; }; for (int i = 0; i < MAP_HEIGHT; ++i) { walls.push_back(Wall(0, i * TILE_SIZE, WallType::STEEL)); walls.push_back(Wall((MAP_WIDTH - 1) * TILE_SIZE, i * TILE_SIZE, WallType::STEEL)); } for (int j = 1; j < MAP_WIDTH - 1; ++j) { walls.push_back(Wall(j * TILE_SIZE, 0, WallType::STEEL)); walls.push_back(Wall(j * TILE_SIZE, (MAP_HEIGHT - 1) * TILE_SIZE, WallType::STEEL)); } int wallDensityFactor = 28 + level * 2; int steelChance = 5 + level * 2; int waterChance = 3 + level * 2; int bushChance = (level >= 2) ? (level * 3) : 0; for (int i = spawnRowTop + 1; i < baseRow; ++i) { for (int j = 1; j < MAP_WIDTH - 1; ++j) { if (isProtectedZone(i, j)) continue; int placeRoll = rand() % wallDensityFactor; if (placeRoll < 10) { int typeRoll = rand() % 100; WallType currentType; bool placed = false; if (typeRoll < waterChance) { currentType = WallType::WATER; placed = true; } else if (typeRoll < waterChance + steelChance) { currentType = WallType::STEEL; placed = true; } else if (bushChance > 0 && typeRoll < waterChance + steelChance + bushChance) { currentType = WallType::BUSH; placed = true; } else { currentType = WallType::BRICK; placed = true; } if (placed) { walls.push_back(Wall(j * TILE_SIZE, i * TILE_SIZE, currentType)); } if (currentType != WallType::BUSH) { if (level > 2 && rand() % (8 - level + 1) == 0) { if (j + 1 < MAP_WIDTH - 1 && !isProtectedZone(i, j + 1)) walls.push_back(Wall((j + 1) * TILE_SIZE, i * TILE_SIZE, WallType::BRICK)); } if (level > 3 && rand() % (9 - level + 1) == 0) { if (i + 1 < baseRow && !isProtectedZone(i + 1, j)) walls.push_back(Wall(j * TILE_SIZE, (i + 1) * TILE_SIZE, WallType::BRICK)); } } } } } if (level >= 3) { for(int i=4; i<7; ++i) for(int j=4; j<7; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } if (level >= 4) { for(int i=MAP_HEIGHT-6; i<MAP_HEIGHT-3; ++i) for(int j=MAP_WIDTH-7; j<MAP_WIDTH-4; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::WATER)); } if (level == 5) { for(int i = MAP_HEIGHT/2 - 1; i <= MAP_HEIGHT/2 + 1; ++i ) { for (int j = MAP_WIDTH/2 - 2; j <= MAP_WIDTH/2 + 2; ++j) { if (i == MAP_HEIGHT/2 && j == MAP_WIDTH/2) continue; if (!isProtectedZone(i,j)) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } } } if (level >= 2 && level < 5) { for(int i = MAP_HEIGHT/2 - 2; i <= MAP_HEIGHT/2 + 2; ++i ) { for (int j = MAP_WIDTH/2 - 3; j <= MAP_WIDTH/2 + 3; ++j) { if (abs(i - MAP_HEIGHT/2) <=1 && abs(j-MAP_WIDTH/2) <=1) continue; if (!isProtectedZone(i,j) && rand()%4 == 0) { bool occupied = false; for(const auto& w : walls) { if (w.rect.x == j*TILE_SIZE && w.rect.y == i*TILE_SIZE && w.type != WallType::BUSH) { occupied = true; break; } } if (!occupied) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::BUSH)); } } } } }

    void spawnInitialEnemies() {
        int count = 0;
        while (count < maxEnemiesOnScreen && enemiesToSpawn > 0) {
            if (!trySpawnOneEnemy()) {
                cerr << "Warning: Could not spawn initial enemy." << endl; break;
            } count++;
        }
    }

    bool trySpawnOneEnemy() {
        if (enemiesOnScreen >= maxEnemiesOnScreen || enemiesToSpawn <= 0) return false;
        vector<pair<int, int>> spawnPoints = { {TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH / 2 - 1) * TILE_SIZE, TILE_SIZE}, {(MAP_WIDTH - 2) * TILE_SIZE, TILE_SIZE} };
        random_shuffle(spawnPoints.begin(), spawnPoints.end());
        for (const auto& sp : spawnPoints) {
            SDL_Rect spawnRect = {sp.first, sp.second, TILE_SIZE, TILE_SIZE};
            bool canSpawn = true;
            for (const auto& w : walls) if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&spawnRect, &w.rect)) { canSpawn = false; break; }
            if (canSpawn && player1.isActive && SDL_HasIntersection(&spawnRect, &player1.rect)) canSpawn = false;
            if (canSpawn && numberOfPlayers == 2 && player2.isActive && SDL_HasIntersection(&spawnRect, &player2.rect)) canSpawn = false;
            if (canSpawn) for (const auto& e : enemies) if (e.active && SDL_HasIntersection(&spawnRect, &e.rect)) { canSpawn = false; break; }
            if (canSpawn) {
                int initialHP = 1;
                if (toughEnemiesSpawnedThisLevel < toughEnemiesToSpawnThisLevel) { initialHP = TOUGH_ENEMY_HP; toughEnemiesSpawnedThisLevel++; }
                enemies.push_back(EnemyTank(sp.first, sp.second, currentLevel, initialHP, bulletShotSound, tankBrokenSound));
                enemiesOnScreen++; enemiesToSpawn--; return true;
            }
        }
        cerr << "Warning: Failed to find a free spawn point for enemy." << endl; return false;
    }

    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) { running = false; return; }
            switch (currentState) {
                case GameState::SELECT_MODE: handleMenuInput(event); break;
                case GameState::PLAYING:     handleGameplayInput(event); break;
                case GameState::GAME_OVER:   if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false; break; // Cho phép thoát ở Game Over
            }
        }
    }

    void handleMenuInput(const SDL_Event& event) {
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            switch (event.key.keysym.sym) {
                case SDLK_1: cout << "Selected 1 Player mode." << endl; numberOfPlayers = 1; currentState = GameState::PLAYING; setupLevel(1); break;
                case SDLK_2: cout << "Selected 2 Players mode." << endl; numberOfPlayers = 2; currentState = GameState::PLAYING; setupLevel(1); break;
                case SDLK_ESCAPE: running = false; break;
            }
        }
    }

    void handleGameplayInput(const SDL_Event& event) {
         if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            if (player1.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: player1.velocityY = -PLAYER_SPEED; player1.lastDirY = -1; player1.lastDirX = 0; break;
                    case SDLK_s: player1.velocityY = PLAYER_SPEED; player1.lastDirY = 1; player1.lastDirX = 0; break;
                    case SDLK_a: player1.velocityX = -PLAYER_SPEED; player1.lastDirX = -1; player1.lastDirY = 0; break;
                    case SDLK_d: player1.velocityX = PLAYER_SPEED; player1.lastDirX = 1; player1.lastDirY = 0; break;
                    case SDLK_j: if (player1.shoot() && bulletShotSound) Mix_PlayChannel(-1, bulletShotSound, 0); break;
                }
            }
            if (numberOfPlayers == 2 && player2.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    player2.velocityY = -PLAYER_SPEED; player2.lastDirY = -1; player2.lastDirX = 0; break;
                    case SDLK_DOWN:  player2.velocityY = PLAYER_SPEED; player2.lastDirY = 1; player2.lastDirX = 0; break;
                    case SDLK_LEFT:  player2.velocityX = -PLAYER_SPEED; player2.lastDirX = -1; player2.lastDirY = 0; break;
                    case SDLK_RIGHT: player2.velocityX = PLAYER_SPEED; player2.lastDirX = 1; player2.lastDirY = 0; break;
                    case SDLK_RCTRL: case SDLK_LCTRL: if (player2.shoot() && bulletShotSound) Mix_PlayChannel(-1, bulletShotSound, 0); break;
                }
            }
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
        else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
            if (player1.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: if (player1.velocityY < 0) player1.velocityY = 0; break;
                    case SDLK_s: if (player1.velocityY > 0) player1.velocityY = 0; break;
                    case SDLK_a: if (player1.velocityX < 0) player1.velocityX = 0; break;
                    case SDLK_d: if (player1.velocityX > 0) player1.velocityX = 0; break;
                }
                if (player1.velocityX == 0 && player1.velocityY != 0) { player1.lastDirX = 0; player1.lastDirY = (player1.velocityY > 0) ? 1 : -1; }
                else if (player1.velocityY == 0 && player1.velocityX != 0) { player1.lastDirY = 0; player1.lastDirX = (player1.velocityX > 0) ? 1 : -1; }
            }
            if (numberOfPlayers == 2 && player2.isActive) {
                 switch (event.key.keysym.sym) {
                    case SDLK_UP:    if (player2.velocityY < 0) player2.velocityY = 0; break;
                    case SDLK_DOWN:  if (player2.velocityY > 0) player2.velocityY = 0; break;
                    case SDLK_LEFT:  if (player2.velocityX < 0) player2.velocityX = 0; break;
                    case SDLK_RIGHT: if (player2.velocityX > 0) player2.velocityX = 0; break;
                }
                if (player2.velocityX == 0 && player2.velocityY != 0) { player2.lastDirX = 0; player2.lastDirY = (player2.velocityY > 0) ? 1 : -1; }
                else if (player2.velocityY == 0 && player2.velocityX != 0) { player2.lastDirY = 0; player2.lastDirX = (player2.velocityX > 0) ? 1 : -1; }
            }
        }
    } // End handleGameplayInput

    void update() {
         if (!running || currentState != GameState::PLAYING) return;

         // Cập nhật Người Chơi
         if (player1.isActive) { player1.updateCooldown(); player1.updatePosition(walls, enemies); player1.updateBullets(); }
         if (numberOfPlayers == 2 && player2.isActive) { player2.updateCooldown(); player2.updatePosition(walls, enemies); player2.updateBullets(); }

         // Cập nhật Kẻ Địch
         for (auto& enemy : enemies) {
             if (enemy.active) {
                 enemy.updateHitStatus();
                 // --- !!! GỌI HÀM AI MỚI !!! ---
                 enemy.updateAIAndVelocity(player1, player2, numberOfPlayers, walls);
                 // -----------------------------
                 enemy.updatePosition(walls);
                 enemy.updateBullets();
             }
         }

         // Xử Lý Va Chạm Đạn Player 1
         if (player1.isActive) {
             for (auto& pB : player1.bullets) {
                 if (!pB.active) continue; bool hit = false;
                 for (auto& w : walls) if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&pB.rect, &w.rect)) { pB.active = false; if (w.type == WallType::BRICK) w.active = false; hit = true; break; }
                 if (hit) continue;
                 for (auto& e : enemies) if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) { pB.active = false; e.takeHit(); hit = true; break; }
             }
         }
         // Xử Lý Va Chạm Đạn Player 2
         if (numberOfPlayers == 2 && player2.isActive) {
              for (auto& pB : player2.bullets) {
                 if (!pB.active) continue; bool hit = false;
                 for (auto& w : walls) if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&pB.rect, &w.rect)) { pB.active = false; if (w.type == WallType::BRICK) w.active = false; hit = true; break; }
                 if (hit) continue;
                 for (auto& e : enemies) if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) { pB.active = false; e.takeHit(); hit = true; break; }
             }
         }
         // Xử Lý Va Chạm Đạn Địch
         for (auto& e : enemies) {
             if (!e.active) continue;
             for (auto& eB : e.bullets) {
                 if (!eB.active) continue; bool hitWall = false;
                 for (auto& w : walls) if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&eB.rect, &w.rect)) { eB.active = false; if (w.type == WallType::BRICK) w.active = false; hitWall = true; break; }
                 if (hitWall) continue;
                 if (player1.isActive && SDL_HasIntersection(&eB.rect, &player1.rect)) { eB.active = false; player1.hitByEnemy(); if (playerDestroySound) Mix_PlayChannel(-1, playerDestroySound, 0); }
                 else if (numberOfPlayers == 2 && player2.isActive && SDL_HasIntersection(&eB.rect, &player2.rect)) { eB.active = false; player2.hitByEnemy(); if (playerDestroySound) Mix_PlayChannel(-1, playerDestroySound, 0); }
             }
         }

         // Dọn Dẹp Địch và Tạo Mới
         enemies.erase(remove_if(enemies.begin(), enemies.end(), [](const EnemyTank &e){ return !e.active; }), enemies.end());
         enemiesOnScreen = enemies.size();
         if (enemiesToSpawn > 0 && enemiesOnScreen < maxEnemiesOnScreen) {
             static Uint32 lastSpawnTime = 0; const Uint32 SPAWN_DELAY = 2000;
             Uint32 currentTime = SDL_GetTicks();
             if (currentTime > lastSpawnTime + SPAWN_DELAY) {
                 if (trySpawnOneEnemy()) lastSpawnTime = currentTime; else lastSpawnTime = currentTime - SPAWN_DELAY / 2;
             }
         }

         // Kiểm Tra Thua Game
         bool player1_is_out = !player1.isActive;
         bool player2_is_out = (numberOfPlayers == 1) || (numberOfPlayers == 2 && !player2.isActive);
         if (player1_is_out && player2_is_out) {
             cout << "All players out! Game Over at Level " << currentLevel << ".\n";
             if (gameOverSound) Mix_PlayChannel(-1, gameOverSound, 0);
             currentState = GameState::GAME_OVER; return;
         }

         // Kiểm Tra Thắng Màn
         if (currentLevel > 0 && enemiesToSpawn == 0 && enemies.empty()) {
             cout << "\n===============================\n LEVEL " << currentLevel << " CLEARED! \n===============================\n\n";
             if (levelUpSound) Mix_PlayChannel(-1, levelUpSound, 0);
             SDL_Delay(1000);
             if (currentLevel < maxLevels) {
                 cout << "Proceeding to next level..." << endl; SDL_Delay(1500); setupLevel(currentLevel + 1);
             } else {
                 cout << "*******************************\n* CONGRATULATIONS! YOU WIN! *\n*******************************\n";
                 SDL_Delay(3000); running = false;
             }
         }
    } // End update()

    void render() {
        if (!renderer) return;
        switch (currentState) {
            case GameState::SELECT_MODE: {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
                if (menuTexture) SDL_RenderCopy(renderer, menuTexture, NULL, NULL);
                else cerr << "Error: Menu texture is missing." << endl;
                break;
            }
            case GameState::PLAYING: {
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); SDL_RenderClear(renderer);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_Rect playableArea = {TILE_SIZE, TILE_SIZE, SCREEN_WIDTH - 2 * TILE_SIZE, SCREEN_HEIGHT - 2 * TILE_SIZE}; SDL_RenderFillRect(renderer, &playableArea);

                // Vẽ tường (trừ bụi cỏ)
                for (auto &wall : walls) {
                    if (!wall.active || wall.type == WallType::BUSH) continue; SDL_Texture* tex = nullptr;
                    switch (wall.type) { case WallType::BRICK: tex = brickTexture; break; case WallType::STEEL: tex = steelTexture; break; case WallType::WATER: tex = waterTexture; break; default: break; }
                    if (tex) SDL_RenderCopy(renderer, tex, nullptr, &wall.rect);
                }
                // Vẽ Địch
                for (auto &enemy : enemies) {
                    if (!enemy.active) continue; SDL_Texture* tex = nullptr;
                    if (enemy.initialHitPoints > 1) { // Tank 3
                        if (enemy.lastDirY < 0) tex = enemyTank3UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank3DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank3LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank3RightTexture; else tex = enemyTank3DownTexture;
                    } else { // Tank 2
                        if (enemy.lastDirY < 0) tex = enemyTank2UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank2DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank2LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank2RightTexture; else tex = enemyTank2DownTexture;
                    }
                    if (tex) {
                        if (enemy.isHit) { SDL_SetTextureColorMod(tex, 255, 100, 100); SDL_SetTextureAlphaMod(tex, 200); } else { SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 255); }
                        SDL_RenderCopy(renderer, tex, nullptr, &enemy.rect);
                        SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 255); // Reset
                    }
                }
                // Vẽ Player 1
                if (player1.isActive) {
                     SDL_Texture* p1Tex = nullptr;
                     if (player1.lastDirY < 0) p1Tex = player1TankUpTexture; else if (player1.lastDirY > 0) p1Tex = player1TankDownTexture; else if (player1.lastDirX < 0) p1Tex = player1TankLeftTexture; else if (player1.lastDirX > 0) p1Tex = player1TankRightTexture; else p1Tex = player1TankUpTexture;
                     if (p1Tex) SDL_RenderCopy(renderer, p1Tex, nullptr, &player1.rect);
                     if (bulletTexture) for (auto &b : player1.bullets) if (b.active) SDL_RenderCopy(renderer, bulletTexture, nullptr, &b.rect);
                }
                // Vẽ Player 2
                if (numberOfPlayers == 2 && player2.isActive) {
                    SDL_Texture* p2Tex = nullptr;
                    if (player2.lastDirY < 0) p2Tex = player2TankUpTexture; else if (player2.lastDirY > 0) p2Tex = player2TankDownTexture; else if (player2.lastDirX < 0) p2Tex = player2TankLeftTexture; else if (player2.lastDirX > 0) p2Tex = player2TankRightTexture; else p2Tex = player2TankUpTexture;
                    if (p2Tex) SDL_RenderCopy(renderer, p2Tex, nullptr, &player2.rect);
                    if (bulletTexture) for (auto &b : player2.bullets) if (b.active) SDL_RenderCopy(renderer, bulletTexture, nullptr, &b.rect);
                }
                 // Vẽ Đạn Địch
                 if (bulletTexture) {
                     for (auto &enemy : enemies) if(enemy.active) for (auto &b : enemy.bullets) if (b.active) SDL_RenderCopy(renderer, bulletTexture, nullptr, &b.rect);
                 }
                // Vẽ Bụi Cỏ (Sau cùng)
                for (auto &wall : walls) if (wall.active && wall.type == WallType::BUSH && grassTexture) SDL_RenderCopy(renderer, grassTexture, nullptr, &wall.rect);
                break;
            }
            case GameState::GAME_OVER: {
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
                if (gameOverTexture) {
                    int imgW, imgH; SDL_QueryTexture(gameOverTexture, NULL, NULL, &imgW, &imgH);
                    SDL_Rect dstRect = {(SCREEN_WIDTH - imgW) / 2, (SCREEN_HEIGHT - imgH) / 2, imgW, imgH }; SDL_RenderCopy(renderer, gameOverTexture, NULL, &dstRect);
                } else cerr << "Warning: Game Over texture missing." << endl;
                break;
            }
        }
        SDL_RenderPresent(renderer);
    } // End render()

    void run() {
        const int TARGET_FPS = 60; const int FRAME_DELAY = 1000 / TARGET_FPS;
        Uint32 frameStart; int frameTime;
        cout << "Starting Game Loop..." << endl;
        while (running) {
            frameStart = SDL_GetTicks();
            handleEvents();
            update();
            render();
            frameTime = SDL_GetTicks() - frameStart;
            if (FRAME_DELAY > frameTime) SDL_Delay(FRAME_DELAY - frameTime);
        }
        cout << "Exiting Game Loop." << endl;
    } // End run()

}; // End class Game


// =============================================================================
// == Hàm main ==
// =============================================================================
int main(int argc, char* argv[]) {
    {
        Game game;
        if (game.running) {
            game.run();
        } else {
            cerr << "Game initialization failed. Exiting." << endl;
        }
        // Destructor của game sẽ tự động chạy ở đây khi ra khỏi scope
    }
    cout << "Application finished." << endl;
    return 0;
}

// =============================================================================
// == Định Nghĩa Hàm Tiện Ích ==
// =============================================================================
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer) {
    SDL_Texture* newTexture = nullptr;
    SDL_Surface* loadedSurface = IMG_Load(path.c_str());
    if (!loadedSurface) {
        cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << endl;
    } else {
        newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
        if (!newTexture) {
            cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << endl;
        }
        SDL_FreeSurface(loadedSurface);
    }
    return newTexture;
}
