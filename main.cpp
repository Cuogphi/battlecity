#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm> // Cho std::remove_if, std::random_shuffle
#include <cstdlib>   // Cho rand(), srand()
#include <ctime>     // Cho time()
#include <limits>    // Cho std::numeric_limits (để xóa bộ đệm cin nếu cần)
#include <cctype>    // Cho std::tolower
#include <cmath>     // Cho std::sqrt, std::round, std::abs
#include <map>       // (Mặc dù không dùng map trong code cuối này)

using namespace std; // Sử dụng không gian tên std để không cần gõ std:: trước các hàm/lớp

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
// Cooldown bắn của người chơi (tính theo frame, khoảng 1/2 tốc độ bắn tối thiểu của địch lv1)
const int PLAYER_SHOT_COOLDOWN_FRAMES = static_cast<int>(round(ORIGINAL_ENEMY_L1_MIN_DELAY_FOR_PLAYER_CALC * 0.5)); // ~43 frames
// Tham số cơ bản cho thời gian chờ bắn của kẻ địch
const int ENEMY_BASE_MIN_DELAY = 85;  // Thời gian chờ tối thiểu cơ bản (frame)
const int ENEMY_BASE_RANGE = 160; // Khoảng ngẫu nhiên cộng thêm vào thời gian chờ cơ bản
const int MIN_POSSIBLE_DELAY = 20; // Thời gian chờ bắn tối thiểu có thể (sau khi trừ theo level)
const int MIN_POSSIBLE_RANGE = 25; // Khoảng ngẫu nhiên tối thiểu có thể (sau khi trừ theo level)
// Lượng giảm thời gian chờ bắn của địch mỗi level
const int DELAY_REDUCTION_PER_LEVEL_MIN = 9;
const int DELAY_REDUCTION_PER_LEVEL_RANGE = 18;
const int TOUGH_ENEMY_HP = 3; // Máu của loại xe tăng địch "trâu bò"
const Uint32 ENEMY_HIT_FLASH_DURATION = 100; // Thời gian nhấp nháy của địch khi bị bắn (ms)

// =============================================================================
// == Enums (Các kiểu liệt kê) ==
// =============================================================================

// Các loại tường khác nhau
enum class WallType {
    BRICK, // Tường gạch (có thể bị phá)
    STEEL, // Tường thép (không thể bị phá)
    WATER, // Nước (chặn xe tăng, không chặn đạn)
    BUSH   // Bụi cỏ (che khuất xe tăng, không chặn gì cả)
};

// Các trạng thái khác nhau của trò chơi
enum class GameState {
    SELECT_MODE, // Trạng thái hiển thị menu chọn chế độ
    PLAYING,     // Trạng thái đang trong màn chơi
    GAME_OVER    // Trạng thái hiển thị màn hình thua cuộc
};

// =============================================================================
// == Khai Báo Trước Các Lớp ==
// =============================================================================
// Giúp các lớp biết đến sự tồn tại của nhau trước khi được định nghĩa đầy đủ
class Wall;
class Bullet;
class PlayerTank;
class EnemyTank;
class Game;

// =============================================================================
// == Khai Báo Trước Hàm Tiện Ích ==
// =============================================================================
// Hàm tải texture từ tệp ảnh
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer);

// =============================================================================
// == Lớp Wall (Tường) ==
// =============================================================================
class Wall {
public:
    int x, y;           // Tọa độ (góc trên trái)
    SDL_Rect rect;      // Hình chữ nhật đại diện cho tường (vị trí và kích thước)
    bool active;        // Trạng thái: true = còn tồn tại, false = đã bị phá
    WallType type;      // Loại tường (gạch, thép, ...)

    // Constructor: Hàm khởi tạo đối tượng Wall
    Wall(int startX, int startY, WallType wallType) :
        x(startX),
        y(startY),
        rect({startX, startY, TILE_SIZE, TILE_SIZE}), // Khởi tạo rect với vị trí và kích thước chuẩn
        active(true),                                // Ban đầu tường luôn tồn tại
        type(wallType) {}                             // Gán loại tường
};

// =============================================================================
// == Lớp Bullet (Đạn) ==
// =============================================================================
class Bullet {
public:
    float x, y;         // Tọa độ chính xác (dùng float để di chuyển mượt hơn)
    float dx, dy;       // Vận tốc theo trục x và y
    SDL_Rect rect;      // Hình chữ nhật đại diện cho đạn để vẽ và kiểm tra va chạm
    bool active;        // Trạng thái: true = đang bay, false = đã trúng hoặc ra ngoài
    float speed;        // Tốc độ bay của đạn

    // Constructor: Hàm khởi tạo đạn
    Bullet(float startX, float startY, int dirX, int dirY) :
        x(startX),
        y(startY),
        dx(0.0f),
        dy(0.0f),
        rect({(int)startX, (int)startY, 8, 8}), // Kích thước viên đạn là 8x8 pixels
        active(true),                           // Ban đầu đạn đang hoạt động
        speed(UNIFIED_BULLET_SPEED)             // Gán tốc độ chuẩn
    {
        // Tính toán vector hướng chuẩn hóa và nhân với tốc độ để có dx, dy
        float length = sqrt(static_cast<float>(dirX * dirX + dirY * dirY)); // Độ dài vector hướng
        if (length > 0) { // Tránh chia cho 0 nếu không có hướng
            dx = (dirX / length) * speed; // Thành phần vận tốc x
            dy = (dirY / length) * speed; // Thành phần vận tốc y
        } else {
            // Mặc định bắn lên nếu không có hướng (dirX=0, dirY=0)
            dx = 0;
            dy = -speed;
        }
        // Căn giữa hình chữ nhật của đạn dựa trên tọa độ tâm (x, y)
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);
    }

    // Hàm di chuyển đạn mỗi frame
    void move() {
        if (!active) { // Không di chuyển nếu đạn không hoạt động
            return;
        }
        // Cập nhật tọa độ chính xác
        x += dx;
        y += dy;
        // Cập nhật tọa độ hình chữ nhật để vẽ và va chạm
        rect.x = (int)(x - rect.w / 2.0f);
        rect.y = (int)(y - rect.h / 2.0f);

        // Hủy đạn nếu bay ra khỏi khu vực chơi (trừ viền tường bao)
        if (rect.x < TILE_SIZE || rect.x + rect.w > SCREEN_WIDTH - TILE_SIZE ||
            rect.y < TILE_SIZE || rect.y + rect.h > SCREEN_HEIGHT - TILE_SIZE)
        {
            active = false;
        }
    }
};

// =============================================================================
// == Lớp EnemyTank (Xe Tăng Địch) ==
// =============================================================================
class EnemyTank {
public:
    int x, y;                     // Tọa độ (góc trên trái)
    int velocityX, velocityY;     // Vận tốc hiện tại
    int lastDirX, lastDirY;       // Hướng cuối cùng mà xe tăng nhìn (để bắn đúng hướng)
    SDL_Rect rect;                // Hình chữ nhật đại diện
    bool active;                  // Trạng thái: true = còn sống, false = đã bị hạ
    vector<Bullet> bullets;       // Danh sách đạn mà xe tăng này đã bắn ra
    int moveDecisionDelay;        // Bộ đếm thời gian chờ trước khi quyết định đổi hướng
    int shootDelay;               // Bộ đếm thời gian chờ trước khi bắn viên đạn tiếp theo
    int level;                    // Level hiện tại (ảnh hưởng đến tốc độ bắn)
    int hitPoints;                // Máu hiện tại
    int initialHitPoints;         // Máu ban đầu (để phân biệt loại tank)
    bool isHit = false;           // Cờ báo hiệu xe tăng vừa bị bắn trúng (để nhấp nháy)
    Uint32 hitStartTime = 0;      // Thời điểm bị bắn trúng (để tính thời gian nhấp nháy)
    Mix_Chunk* shootSound = nullptr;  // Con trỏ đến âm thanh bắn
    Mix_Chunk* destroySound = nullptr; // Con trỏ đến âm thanh bị phá hủy

    // Constructor: Khởi tạo xe tăng địch
    EnemyTank(int startX, int startY, int current_level, int initialHP = 1, Mix_Chunk* s_sound = nullptr, Mix_Chunk* d_sound = nullptr) :
        x(startX), y(startY),
        velocityX(0), velocityY(ENEMY_SPEED), // Bắt đầu di chuyển xuống
        lastDirX(0), lastDirY(1),              // Hướng ban đầu là xuống
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        active(true),                          // Ban đầu còn sống
        moveDecisionDelay(40 + rand() % 80),   // Thời gian chờ đổi hướng ngẫu nhiên ban đầu
        level(current_level),
        hitPoints(initialHP),                  // Gán máu
        initialHitPoints(initialHP),
        shootSound(s_sound),                   // Lưu con trỏ âm thanh
        destroySound(d_sound)
    {
        resetShootCooldown(); // Đặt thời gian chờ bắn ban đầu
    }

    // Hàm đặt lại thời gian chờ bắn (cooldown) dựa trên level
    void resetShootCooldown() {
        // Tính toán giảm thời gian chờ dựa trên level
        int levelAdjMin = (level - 1) * DELAY_REDUCTION_PER_LEVEL_MIN;
        int levelAdjRange = (level - 1) * DELAY_REDUCTION_PER_LEVEL_RANGE;
        // Tính thời gian chờ tối thiểu và khoảng ngẫu nhiên hiện tại, đảm bảo không nhỏ hơn giới hạn
        int currentMin = max(MIN_POSSIBLE_DELAY, ENEMY_BASE_MIN_DELAY - levelAdjMin);
        int currentRange = max(MIN_POSSIBLE_RANGE, ENEMY_BASE_RANGE - levelAdjRange);
        if (currentRange < 1) { // Khoảng ngẫu nhiên phải ít nhất là 1
            currentRange = 1;
        }
        // Đặt thời gian chờ bắn ngẫu nhiên trong khoảng [currentMin, currentMin + currentRange - 1]
        shootDelay = currentMin + rand() % currentRange;
    }

    // Hàm xử lý khi xe tăng địch bị trúng đạn
    void takeHit() {
        if (!active) { // Không xử lý nếu đã bị hạ
            return;
        }
        hitPoints--; // Giảm máu
        isHit = true; // Đặt cờ bị bắn trúng
        hitStartTime = SDL_GetTicks(); // Ghi lại thời điểm bị bắn
        if (hitPoints <= 0) { // Nếu hết máu
            active = false; // Đặt trạng thái đã bị hạ
            if (destroySound) { // Phát âm thanh phá hủy nếu có
                Mix_PlayChannel(-1, destroySound, 0);
            }
            // cout << "Enemy destroyed!" << endl; // Có thể bỏ log này
        }
    }

    // Hàm cập nhật trạng thái nhấp nháy khi bị bắn
    void updateHitStatus() {
        if (isHit && SDL_GetTicks() > hitStartTime + ENEMY_HIT_FLASH_DURATION) {
            isHit = false; // Tắt trạng thái nhấp nháy sau một khoảng thời gian
        }
    }

    // Hàm bắn đạn
    bool shoot() {
        if (!active) { // Không bắn nếu đã bị hạ
            return false;
        }
        // Tính toán vị trí bắt đầu của viên đạn (phía trước nòng súng)
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1; // Bắn sang phải
        else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1; // Bắn sang trái
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1; // Bắn xuống
        else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1; // Bắn lên

        // Tạo viên đạn mới và thêm vào danh sách
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        if (shootSound) { // Phát âm thanh bắn nếu có
            Mix_PlayChannel(-1, shootSound, 0);
        }
        return true;
    }

    // Hàm cập nhật AI (quyết định di chuyển và bắn)
    void updateAIAndVelocity(const vector<Wall>& walls) {
        if (!active) {
            return;
        }

        // Bắn nếu hết cooldown
        if (--shootDelay <= 0) {
             shoot();
             resetShootCooldown();
        }

        // Quyết định đổi hướng nếu hết thời gian chờ hoặc sắp va chạm
        if (--moveDecisionDelay <= 0) {
            moveDecisionDelay = 45 + rand() % 100; // Đặt lại thời gian chờ

            // Kiểm tra xem có sắp va chạm với tường hoặc biên không
            SDL_Rect futureRect = rect;
            int checkDist = TILE_SIZE / 2; // Khoảng cách kiểm tra phía trước
            futureRect.x += velocityX * checkDist;
            futureRect.y += velocityY * checkDist;
            bool potentialCollision = false;

            // Kiểm tra va chạm biên
            if (futureRect.x < TILE_SIZE || futureRect.x + futureRect.w > SCREEN_WIDTH - TILE_SIZE ||
                futureRect.y < TILE_SIZE || futureRect.y + futureRect.h > SCREEN_HEIGHT - TILE_SIZE) {
                potentialCollision = true;
            } else {
                // Kiểm tra va chạm tường (trừ bụi cỏ)
                for (const auto& w : walls) {
                    if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&futureRect, &w.rect)) {
                        potentialCollision = true;
                        break;
                    }
                }
            }

            // Đổi hướng ngẫu nhiên nếu sắp va chạm hoặc theo xác suất ngẫu nhiên
            if (potentialCollision || (rand() % 3 == 0)) { // 1/3 cơ hội đổi hướng ngẫu nhiên
                int currentDir = -1; // Xác định hướng hiện tại (0:U, 1:D, 2:L, 3:R)
                if (velocityY < 0) currentDir = 0; else if (velocityY > 0) currentDir = 1; else if (velocityX < 0) currentDir = 2; else if (velocityX > 0) currentDir = 3;

                int attempts = 0, newDir;
                bool changed = false;
                // Cố gắng chọn một hướng MỚI KHÁC hướng hiện tại
                while (attempts < 4) {
                    newDir = rand() % 4; // Chọn hướng ngẫu nhiên (0-3)
                    if (newDir != currentDir) { // Nếu là hướng mới
                        if (newDir == 0) { velocityX = 0; velocityY = -ENEMY_SPEED; lastDirX = 0; lastDirY = -1; changed = true; break; } // Lên
                        else if (newDir == 1) { velocityX = 0; velocityY = ENEMY_SPEED; lastDirX = 0; lastDirY = 1; changed = true; break; } // Xuống
                        else if (newDir == 2) { velocityX = -ENEMY_SPEED; velocityY = 0; lastDirX = -1; lastDirY = 0; changed = true; break; } // Trái
                        else if (newDir == 3) { velocityX = ENEMY_SPEED; velocityY = 0; lastDirX = 1; lastDirY = 0; changed = true; break; } // Phải
                    }
                    attempts++;
                }
                // Nếu thử 4 lần mà không tìm được hướng mới (ví dụ: kẹt góc), chọn đại một hướng
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

    // Hàm cập nhật vị trí và xử lý va chạm khi di chuyển
    void updatePosition(const vector<Wall>& walls) {
        if (!active || (velocityX == 0 && velocityY == 0)) { // Không di chuyển nếu đã bị hạ hoặc đứng yên
            return;
        }

        int originalX = x; // Lưu vị trí cũ phòng khi cần hoàn tác
        int originalY = y;

        // --- Di chuyển theo trục X ---
        x += velocityX;
        rect.x = x;
        bool collisionX = false;
        // Kiểm tra va chạm tường (trừ bụi cỏ)
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                x = originalX; // Hoàn tác di chuyển X
                rect.x = x;
                collisionX = true;
                break;
            }
        }
        // Kiểm tra va chạm biên X (sau khi kiểm tra tường)
        if (!collisionX) {
            if (x < TILE_SIZE) { // Chạm biên trái
                 x = TILE_SIZE;
            } else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) { // Chạm biên phải
                 x = SCREEN_WIDTH - TILE_SIZE - rect.w;
            }
            rect.x = x; // Cập nhật lại rect.x sau khi kiểm tra biên
        }

        // --- Di chuyển theo trục Y ---
        y += velocityY;
        rect.y = y;
        bool collisionY = false;
        // Kiểm tra va chạm tường (trừ bụi cỏ)
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                y = originalY; // Hoàn tác di chuyển Y
                rect.y = y;
                collisionY = true;
                break;
            }
        }
         // Kiểm tra va chạm biên Y (sau khi kiểm tra tường)
         if (!collisionY) {
            if (y < TILE_SIZE) { // Chạm biên trên
                y = TILE_SIZE;
            } else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) { // Chạm biên dưới
                y = SCREEN_HEIGHT - TILE_SIZE - rect.h;
            }
            rect.y = y; // Cập nhật lại rect.y sau khi kiểm tra biên
        }
    }

    // Hàm cập nhật trạng thái các viên đạn của xe tăng này
    void updateBullets() {
        if (!active) { // Nếu xe tăng đã bị hạ
            bullets.clear(); // Xóa hết đạn của nó
            return;
        }
        // Di chuyển các viên đạn đang hoạt động
        for (auto &b : bullets) {
            if (b.active) {
                b.move();
            }
        }
        // Xóa các viên đạn không còn hoạt động (đã trúng hoặc bay ra ngoài)
        bullets.erase(remove_if(bullets.begin(), bullets.end(),
                      [](const Bullet &b){ return !b.active; }), bullets.end());
    }
};

// =============================================================================
// == Lớp PlayerTank (Xe Tăng Người Chơi) ==
// =============================================================================
class PlayerTank {
public:
    int x, y;                     // Tọa độ (góc trên trái)
    int velocityX, velocityY;     // Vận tốc hiện tại
    int lastDirX, lastDirY;       // Hướng nhìn cuối cùng
    SDL_Rect rect;                // Hình chữ nhật đại diện
    vector<Bullet> bullets;       // Danh sách đạn đã bắn
    int shotDelayCounter;         // Bộ đếm cooldown bắn
    bool isActive = true;         // Trạng thái: true = đang chơi, false = đã bị hạ

    // Constructor: Khởi tạo người chơi
    PlayerTank(int startX = 0, int startY = 0) :
        x(startX), y(startY),
        velocityX(0), velocityY(0),           // Ban đầu đứng yên
        lastDirX(0), lastDirY(-1),            // Ban đầu nhìn lên
        rect({startX, startY, TILE_SIZE, TILE_SIZE}),
        shotDelayCounter(0),                 // Ban đầu có thể bắn ngay
        isActive(true) {}                    // Ban đầu đang hoạt động

    // Hàm đặt lại trạng thái người chơi khi bắt đầu màn mới hoặc reset
    void reset(int startX, int startY) {
        x = startX; y = startY; // Đặt lại vị trí
        rect.x = x; rect.y = y;
        velocityX = 0; velocityY = 0; // Dừng di chuyển
        lastDirX = 0; lastDirY = -1;   // Reset hướng nhìn lên
        bullets.clear();              // Xóa hết đạn cũ
        shotDelayCounter = 0;         // Reset cooldown bắn
        isActive = true;              // Đặt lại trạng thái hoạt động
    }

    // Hàm xử lý khi người chơi bị trúng đạn địch
    void hitByEnemy() {
        if (!isActive) { // Không xử lý nếu đã bị hạ
            return;
        }
        cout << "Player hit!" << endl; // Thông báo (có thể thay bằng hiệu ứng)
        isActive = false; // Đặt trạng thái không hoạt động
        // TODO (Tùy chọn): Nếu có hệ thống mạng sống, sẽ trừ mạng ở đây.
        // Nếu còn mạng, có thể bắt đầu timer hồi sinh thay vì đặt isActive=false ngay.
    }

    // Hàm cập nhật bộ đếm cooldown bắn
    void updateCooldown() {
        if (shotDelayCounter > 0) {
            shotDelayCounter--; // Giảm bộ đếm mỗi frame
        }
    }

    // Hàm cập nhật vị trí và xử lý va chạm khi di chuyển
    // (Phiên bản đơn giản: không xử lý va chạm giữa 2 người chơi)
    void updatePosition(const vector<Wall>& walls, const vector<EnemyTank>& enemies) {
        if (!isActive || (velocityX == 0 && velocityY == 0)) { // Không di chuyển nếu không hoạt động hoặc đứng yên
            return;
        }

        int originalX = x; // Lưu vị trí cũ
        int originalY = y;

        // --- Di chuyển theo trục X ---
        x += velocityX;
        rect.x = x;
        bool collisionX = false;
        // Va chạm tường (trừ bụi cỏ)
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                x = originalX; rect.x = x; collisionX = true; break;
            }
        }
        // Va chạm với xe tăng địch (nếu chưa va chạm tường)
        if (!collisionX) {
            for (const auto& e : enemies) {
                if (e.active && SDL_HasIntersection(&rect, &e.rect)) {
                    x = originalX; rect.x = x; collisionX = true; break;
                }
            }
        }
        // Va chạm biên X (nếu chưa va chạm tường/địch)
        if (!collisionX) {
            if (x < TILE_SIZE) x = TILE_SIZE;
            else if (x + rect.w > SCREEN_WIDTH - TILE_SIZE) x = SCREEN_WIDTH - TILE_SIZE - rect.w;
            rect.x = x;
        }

        // --- Di chuyển theo trục Y ---
        y += velocityY;
        rect.y = y;
        bool collisionY = false;
         // Va chạm tường (trừ bụi cỏ)
        for (const auto& w : walls) {
            if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&rect, &w.rect)) {
                y = originalY; rect.y = y; collisionY = true; break;
            }
        }
        // Va chạm với xe tăng địch (nếu chưa va chạm tường)
        if (!collisionY) {
            for (const auto& e : enemies) {
                if (e.active && SDL_HasIntersection(&rect, &e.rect)) {
                    y = originalY; rect.y = y; collisionY = true; break;
                }
            }
        }
        // Va chạm biên Y (nếu chưa va chạm tường/địch)
        if (!collisionY) {
            if (y < TILE_SIZE) y = TILE_SIZE;
            else if (y + rect.h > SCREEN_HEIGHT - TILE_SIZE) y = SCREEN_HEIGHT - TILE_SIZE - rect.h;
            rect.y = y;
        }
    }

    // Hàm bắn đạn
    bool shoot() {
        // Không bắn nếu: không hoạt động, đang cooldown, hoặc không có hướng (đứng yên hoàn toàn)
        if (!isActive || shotDelayCounter > 0 || (lastDirX == 0 && lastDirY == 0)) {
            return false;
        }
        // Tính vị trí bắt đầu đạn
        float bulletStartX = rect.x + TILE_SIZE / 2.0f;
        float bulletStartY = rect.y + TILE_SIZE / 2.0f;
        if (lastDirX > 0) bulletStartX += TILE_SIZE / 2.0f + 1;
        else if (lastDirX < 0) bulletStartX -= TILE_SIZE / 2.0f + 1;
        if (lastDirY > 0) bulletStartY += TILE_SIZE / 2.0f + 1;
        else if (lastDirY < 0) bulletStartY -= TILE_SIZE / 2.0f + 1;

        // Tạo đạn mới
        bullets.push_back(Bullet(bulletStartX, bulletStartY, lastDirX, lastDirY));
        shotDelayCounter = PLAYER_SHOT_COOLDOWN_FRAMES; // Đặt lại cooldown
        return true; // Bắn thành công
    }

    // Hàm cập nhật trạng thái đạn của người chơi
    void updateBullets() {
        if (!isActive) { // Nếu người chơi không hoạt động
            bullets.clear(); // Xóa hết đạn
            return;
        }
        // Di chuyển các viên đạn đang hoạt động
        for (auto &b : bullets) {
            if (b.active) {
                b.move();
            }
        }
        // Xóa các viên đạn không còn hoạt động
        bullets.erase(remove_if(bullets.begin(), bullets.end(),
                      [](const Bullet &b){ return !b.active; }), bullets.end());
    }
};

// =============================================================================
// == Lớp Game (Quản lý chính) ==
// =============================================================================
class Game {
public:
    // --- Thành phần cốt lõi SDL ---
    SDL_Window* window = nullptr;     // Cửa sổ game
    SDL_Renderer* renderer = nullptr; // Bộ vẽ đồ họa

    // --- Trạng thái Game ---
    bool running = true;                         // Cờ điều khiển vòng lặp chính
    GameState currentState = GameState::SELECT_MODE; // Trạng thái bắt đầu là menu
    int numberOfPlayers = 1;                     // Số người chơi (mặc định là 1, thay đổi ở menu)

    // --- Đối tượng trong Game ---
    vector<Wall> walls;         // Danh sách các bức tường
    PlayerTank player1;         // Đối tượng người chơi 1
    PlayerTank player2;         // Đối tượng người chơi 2 (chỉ hoạt động ở chế độ 2P)
    vector<EnemyTank> enemies;  // Danh sách các xe tăng địch

    // --- Biến quản lý màn chơi và kẻ địch ---
    int enemiesToSpawn = 0;              // Số lượng địch còn lại cần tạo ra trong màn
    int enemiesOnScreen = 0;             // Số lượng địch đang hiện trên màn hình
    int maxEnemiesOnScreen = 4;          // Số lượng địch tối đa cùng lúc trên màn hình
    int currentLevel = 0;                // Màn chơi hiện tại
    const int maxLevels = 5;             // Số màn chơi tối đa (ví dụ)
    int toughEnemiesToSpawnThisLevel = 0; // Số tank địch "trâu" cần tạo trong màn này
    int toughEnemiesSpawnedThisLevel = 0; // Số tank địch "trâu" đã tạo trong màn này

    // --- Textures (Hình ảnh) ---
    SDL_Texture* menuTexture = nullptr;           // Texture cho màn hình menu
    SDL_Texture* brickTexture = nullptr;          // Texture tường gạch
    SDL_Texture* steelTexture = nullptr;          // Texture tường thép
    SDL_Texture* waterTexture = nullptr;          // Texture nước
    SDL_Texture* grassTexture = nullptr;          // Texture bụi cỏ
    SDL_Texture* bulletTexture = nullptr;         // Texture viên đạn
    // Textures cho Player 1
    SDL_Texture* player1TankUpTexture = nullptr;
    SDL_Texture* player1TankDownTexture = nullptr;
    SDL_Texture* player1TankLeftTexture = nullptr;
    SDL_Texture* player1TankRightTexture = nullptr;
    // Textures cho Player 2
    SDL_Texture* player2TankUpTexture = nullptr;
    SDL_Texture* player2TankDownTexture = nullptr;
    SDL_Texture* player2TankLeftTexture = nullptr;
    SDL_Texture* player2TankRightTexture = nullptr;
    // Textures cho Enemy (Loại thường - tank2)
    SDL_Texture* enemyTank2UpTexture = nullptr;
    SDL_Texture* enemyTank2DownTexture = nullptr;
    SDL_Texture* enemyTank2LeftTexture = nullptr;
    SDL_Texture* enemyTank2RightTexture = nullptr;
    // Textures cho Enemy (Loại trâu - tank3)
    SDL_Texture* enemyTank3UpTexture = nullptr;
    SDL_Texture* enemyTank3DownTexture = nullptr;
    SDL_Texture* enemyTank3LeftTexture = nullptr;
    SDL_Texture* enemyTank3RightTexture = nullptr;
    // Texture cho màn hình Game Over
    SDL_Texture* gameOverTexture = nullptr;

    // --- Sounds (Âm thanh) ---
    Mix_Chunk* bulletShotSound = nullptr;    // Âm thanh bắn đạn (chung cho player và enemy)
    Mix_Chunk* tankBrokenSound = nullptr;    // Âm thanh xe tăng địch bị phá hủy
    Mix_Chunk* gameOverSound = nullptr;      // Âm thanh khi thua game
    Mix_Chunk* levelUpSound = nullptr;       // Âm thanh khi qua màn / bắt đầu màn mới
    Mix_Chunk* playerDestroySound = nullptr; // Âm thanh khi người chơi bị phá hủy

    // --- Constructor (Hàm khởi tạo lớp Game) ---
    Game() : player1(), player2() { // Khởi tạo các đối tượng player
        cout << "Initializing Game..." << endl;
        // --- Khởi tạo SDL Core ---
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { // Khởi tạo Video và Audio
            cerr << "SDL Init Error: " << SDL_GetError() << endl;
            running = false; // Không thể chạy nếu SDL không khởi tạo được
            return;
        }
        cout << "SDL Initialized." << endl;

        // --- Khởi tạo SDL_image (để tải PNG, JPG) ---
        int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG; // Cần cờ JPG để tải menu
        if (!(IMG_Init(imgFlags) & imgFlags)) {
            cerr << "SDL_image Error: Could not initialize! " << IMG_GetError() << endl;
            running = false;
            SDL_Quit(); // Dọn dẹp SDL core nếu image lỗi
            return;
        }
        cout << "SDL_image Initialized." << endl;

        // --- Khởi tạo SDL_mixer (để phát âm thanh) ---
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) { // Mở thiết bị âm thanh
            cerr << "Warning: SDL_mixer Error: Could not initialize! " << Mix_GetError() << endl;
            // Có thể tiếp tục chạy game không có âm thanh nếu mixer lỗi
        } else {
            cout << "SDL_mixer Initialized." << endl;
        }

        // --- Tạo cửa sổ Game ---
        window = SDL_CreateWindow("Battle City Clone - Select Mode", // Tiêu đề cửa sổ
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // Vị trí giữa màn hình
                                  SCREEN_WIDTH, SCREEN_HEIGHT, // Kích thước
                                  SDL_WINDOW_SHOWN); // Hiển thị cửa sổ ngay
        if (!window) { // Kiểm tra lỗi tạo cửa sổ
            cerr << "Window Creation Error: " << SDL_GetError() << endl;
            running = false;
            // Dọn dẹp các thư viện đã khởi tạo
            Mix_Quit(); IMG_Quit(); SDL_Quit();
            return;
        }
        cout << "Window Created." << endl;

        // --- Tạo bộ vẽ (Renderer) ---
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        // SDL_RENDERER_ACCELERATED: Dùng phần cứng đồ họa (nhanh hơn)
        // SDL_RENDERER_PRESENTVSYNC: Đồng bộ với tần số quét màn hình (tránh xé hình)
        if (!renderer) { // Kiểm tra lỗi tạo renderer
            cerr << "Renderer Creation Error: " << SDL_GetError() << endl;
            running = false;
            SDL_DestroyWindow(window); // Dọn dẹp cửa sổ
            Mix_Quit(); IMG_Quit(); SDL_Quit();
            return;
        }
        cout << "Renderer Created." << endl;

        // --- Thiết lập trạng thái ban đầu và tải tài nguyên ---
        currentState = GameState::SELECT_MODE; // Bắt đầu ở màn hình menu
        if (!loadMedia()) { // Tải tất cả hình ảnh và âm thanh
             cerr << "ERROR: Failed to load essential media! Exiting." << endl;
             running = false; // Thoát nếu không tải được tài nguyên thiết yếu (như menu)
             // Dọn dẹp SDL đã khởi tạo
             SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
             Mix_Quit(); IMG_Quit(); SDL_Quit();
             return;
        }
        srand(time(0)); // Khởi tạo bộ sinh số ngẫu nhiên
        cout << "Game Initialized Successfully. Showing Menu." << endl;
        // Lưu ý: Hàm setupLevel() sẽ được gọi sau khi người chơi chọn chế độ từ menu
    }

    // --- Destructor (Hàm hủy lớp Game) ---
    ~Game() {
        cout << "Cleaning Game Resources..." << endl;
        // --- Giải phóng Textures --- (Kiểm tra con trỏ trước khi hủy)
        if(menuTexture) SDL_DestroyTexture(menuTexture);
        if(brickTexture) SDL_DestroyTexture(brickTexture); if(steelTexture) SDL_DestroyTexture(steelTexture); if(waterTexture) SDL_DestroyTexture(waterTexture); if(grassTexture) SDL_DestroyTexture(grassTexture);
        if(bulletTexture) SDL_DestroyTexture(bulletTexture);
        if(player1TankUpTexture) SDL_DestroyTexture(player1TankUpTexture); if(player1TankDownTexture) SDL_DestroyTexture(player1TankDownTexture); if(player1TankLeftTexture) SDL_DestroyTexture(player1TankLeftTexture); if(player1TankRightTexture) SDL_DestroyTexture(player1TankRightTexture);
        if(player2TankUpTexture) SDL_DestroyTexture(player2TankUpTexture); if(player2TankDownTexture) SDL_DestroyTexture(player2TankDownTexture); if(player2TankLeftTexture) SDL_DestroyTexture(player2TankLeftTexture); if(player2TankRightTexture) SDL_DestroyTexture(player2TankRightTexture);
        if(enemyTank2UpTexture) SDL_DestroyTexture(enemyTank2UpTexture); if(enemyTank2DownTexture) SDL_DestroyTexture(enemyTank2DownTexture); if(enemyTank2LeftTexture) SDL_DestroyTexture(enemyTank2LeftTexture); if(enemyTank2RightTexture) SDL_DestroyTexture(enemyTank2RightTexture);
        if(enemyTank3UpTexture) SDL_DestroyTexture(enemyTank3UpTexture); if(enemyTank3DownTexture) SDL_DestroyTexture(enemyTank3DownTexture); if(enemyTank3LeftTexture) SDL_DestroyTexture(enemyTank3LeftTexture); if(enemyTank3RightTexture) SDL_DestroyTexture(enemyTank3RightTexture);
        if(gameOverTexture) SDL_DestroyTexture(gameOverTexture);

        // --- Giải phóng Âm thanh --- (Kiểm tra con trỏ trước khi giải phóng)
        if(bulletShotSound) Mix_FreeChunk(bulletShotSound); if(tankBrokenSound) Mix_FreeChunk(tankBrokenSound); if(gameOverSound) Mix_FreeChunk(gameOverSound); if(levelUpSound) Mix_FreeChunk(levelUpSound); if(playerDestroySound) Mix_FreeChunk(playerDestroySound);

        // --- Hủy Renderer và Cửa sổ ---
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);

        // --- Đóng các hệ thống con SDL --- (Thứ tự ngược với khởi tạo)
        Mix_CloseAudio(); // Đóng thiết bị âm thanh trước
        Mix_Quit();       // Rồi mới thoát hệ thống mixer
        IMG_Quit();       // Thoát hệ thống image
        SDL_Quit();       // Thoát hệ thống SDL core
        cout << "Game Resources Cleaned." << endl;
    }

    // --- Hàm Tiện Ích Tải Âm Thanh ---
    Mix_Chunk* loadSound(const std::string& path) {
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str()); // Tải tệp WAV
        if (!chunk) { // Kiểm tra lỗi
            cerr << "Failed to load sound: " << path << " - " << Mix_GetError() << endl;
        } else {
            cout << "Loaded sound: " << path << endl;
        }
        return chunk; // Trả về con trỏ âm thanh (hoặc nullptr nếu lỗi)
    }

    // --- Hàm Tải Tất Cả Tài Nguyên (Hình ảnh & Âm thanh) ---
    bool loadMedia() {
         cout << "Loading media..." << endl;
         bool essential_success = true; // Cờ theo dõi tải thành công tài nguyên thiết yếu

         // --- Tải Ảnh Menu (Thiết yếu) ---
         menuTexture = loadTexture("giao_dien.jpg", renderer); // Tải ảnh menu JPG
         if (!menuTexture) {
             cerr << "ERROR: Failed to load menu image 'giao_dien.jpg'!" << endl;
             return false; // Không thể tiếp tục nếu không có menu
         } else {
              cout << "Loaded texture: giao_dien.jpg" << endl;
         }

         // --- Tải Ảnh Trong Game --- (Theo dõi essential_success)
         brickTexture = loadTexture("brick.png", renderer); if (!brickTexture) essential_success = false;
         steelTexture = loadTexture("steel.png", renderer); if (!steelTexture) essential_success = false;
         waterTexture = loadTexture("water.png", renderer); if (!waterTexture) essential_success = false; // Nước có thể coi là thiết yếu?
         grassTexture = loadTexture("grass.png", renderer); if (!grassTexture) cerr << "Warning: Failed to load grass.png" << endl; // Bụi cỏ không thiết yếu
         bulletTexture = loadTexture("Bullet.png", renderer); if (!bulletTexture) essential_success = false;
         // Player 1
         player1TankUpTexture = loadTexture("tank1U.png", renderer); if (!player1TankUpTexture) essential_success = false;
         player1TankDownTexture = loadTexture("tank1D.png", renderer); if (!player1TankDownTexture) essential_success = false;
         player1TankLeftTexture = loadTexture("tank1L.png", renderer); if (!player1TankLeftTexture) essential_success = false;
         player1TankRightTexture = loadTexture("tank1R.png", renderer); if (!player1TankRightTexture) essential_success = false;
         // Player 2 (Thiết yếu nếu chế độ 2P tồn tại)
         player2TankUpTexture = loadTexture("tank_player2U.png", renderer); if (!player2TankUpTexture) essential_success = false;
         player2TankDownTexture = loadTexture("tank_player2D.png", renderer); if (!player2TankDownTexture) essential_success = false;
         player2TankLeftTexture = loadTexture("tank_player2L.png", renderer); if (!player2TankLeftTexture) essential_success = false;
         player2TankRightTexture = loadTexture("tank_player2R.png", renderer); if (!player2TankRightTexture) essential_success = false;
         // Enemies
         enemyTank2UpTexture = loadTexture("tank2U.png", renderer); if (!enemyTank2UpTexture) essential_success = false;
         enemyTank2DownTexture = loadTexture("tank2D.png", renderer); if (!enemyTank2DownTexture) essential_success = false;
         enemyTank2LeftTexture = loadTexture("tank2L.png", renderer); if (!enemyTank2LeftTexture) essential_success = false;
         enemyTank2RightTexture = loadTexture("tank2R.png", renderer); if (!enemyTank2RightTexture) essential_success = false;
         enemyTank3UpTexture = loadTexture("tank3U.png", renderer); if (!enemyTank3UpTexture) essential_success = false;
         enemyTank3DownTexture = loadTexture("tank3D.png", renderer); if (!enemyTank3DownTexture) essential_success = false;
         enemyTank3LeftTexture = loadTexture("tank3L.png", renderer); if (!enemyTank3LeftTexture) essential_success = false;
         enemyTank3RightTexture = loadTexture("tank3R.png", renderer); if (!enemyTank3RightTexture) essential_success = false;
         // Game Over (Không thiết yếu, game vẫn chạy được)
         gameOverTexture = loadTexture("game_over.png", renderer); if (!gameOverTexture) cerr << "Warning: Failed to load game_over.png!" << endl;

         // --- Tải Âm Thanh --- (Không thiết yếu, game vẫn chạy không tiếng)
         bulletShotSound = loadSound("bullet_shot.wav");
         tankBrokenSound = loadSound("broken.wav");    // Âm thanh địch nổ
         playerDestroySound = loadSound("broken.wav"); // Âm thanh người chơi nổ (có thể dùng âm khác)
         gameOverSound   = loadSound("game_over.wav");
         levelUpSound    = loadSound("level_up.wav");

         if (!essential_success) { // Báo lỗi nếu thiếu ảnh quan trọng
             cerr << "ERROR: Failed to load one or more essential game textures!\n";
         }
         cout << "Media loading finished." << endl;
         return essential_success; // Trả về true nếu tải được các ảnh thiết yếu
    }

    // --- Hàm Thiết Lập Màn Chơi Mới ---
    void setupLevel(int level) {
        cout << "Loading Level " << level << "..." << endl;
        currentLevel = level; // Lưu level hiện tại
        // Đặt tiêu đề cửa sổ hiển thị level và số người chơi
        string title = "Battle City Clone - Level " + to_string(level) + " (" + to_string(numberOfPlayers) + "P)";
        SDL_SetWindowTitle(window, title.c_str());

        // --- Reset các đối tượng của màn chơi ---
        walls.clear();     // Xóa tường cũ
        enemies.clear();   // Xóa địch cũ
        generateWalls(level); // Tạo tường mới cho level

        // --- Reset Người Chơi ---
        // Đặt vị trí P1 hơi lệch trái so với giữa căn cứ
        player1.reset(((MAP_WIDTH / 2) - 2) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE);
        // Đặt vị trí P2 hơi lệch phải, chỉ khi đang ở chế độ 2 người chơi
        if (numberOfPlayers == 2) {
            player2.reset(((MAP_WIDTH / 2) + 1) * TILE_SIZE, (MAP_HEIGHT - 2) * TILE_SIZE);
            player2.isActive = true; // Đảm bảo P2 bắt đầu hoạt động
        } else {
            player2.isActive = false; // Đảm bảo P2 không hoạt động ở chế độ 1 người
        }

        // --- Reset thông số kẻ địch cho màn chơi ---
        // Số lượng địch tổng cộng cần hạ trong màn
        if (level==1) enemiesToSpawn=10; else if (level==2) enemiesToSpawn=15; else if (level==3) enemiesToSpawn=20; else if (level==4) enemiesToSpawn=25; else if (level==5) enemiesToSpawn=30; else enemiesToSpawn=30+(level-5)*5; // Tăng dần sau lv 5
        // Số lượng địch tối đa xuất hiện cùng lúc
        if (level==1) maxEnemiesOnScreen=4; else if (level<=3) maxEnemiesOnScreen=5; else maxEnemiesOnScreen=6+(level-5)/2; // Tăng dần
        // Số lượng địch loại "trâu"
        if (level==1) toughEnemiesToSpawnThisLevel=0; else if (level==2) toughEnemiesToSpawnThisLevel=1; else if (level==3) toughEnemiesToSpawnThisLevel=3; else if (level==4) toughEnemiesToSpawnThisLevel=7; else if (level==5) toughEnemiesToSpawnThisLevel=10; else toughEnemiesToSpawnThisLevel=10+(level-5)*2; // Tăng dần
        toughEnemiesSpawnedThisLevel = 0; // Reset bộ đếm địch trâu đã tạo
        enemiesOnScreen = 0;              // Reset bộ đếm địch trên màn

        // --- Âm thanh bắt đầu màn --- (Chỉ phát từ level 2 trở đi)
        // Âm thanh level up/start được phát trong hàm update() khi màn trước kết thúc
        // if (level > 1 && levelUpSound) { Mix_PlayChannel(-1, levelUpSound, 0); }

        spawnInitialEnemies(); // Tạo ra những kẻ địch đầu tiên
        SDL_Delay(100);       // Chờ một chút để người chơi chuẩn bị
        cout << "Level " << level << " Started." << endl;
    }

    // --- Hàm Tạo Tường --- (Giữ nguyên logic phức tạp này)
    void generateWalls(int level) { walls.clear(); int baseCol = MAP_WIDTH / 2; int baseRow = MAP_HEIGHT - 2; int spawnRowTop = 1; auto isProtectedZone = [&](int r, int c) { if (r <= spawnRowTop + 2 && (c < 4 || c > MAP_WIDTH - 5)) return true; if (r >= baseRow - 1 && (c > baseCol - 3 && c < baseCol + 3)) return true; return false; }; for (int i = 0; i < MAP_HEIGHT; ++i) { walls.push_back(Wall(0, i * TILE_SIZE, WallType::STEEL)); walls.push_back(Wall((MAP_WIDTH - 1) * TILE_SIZE, i * TILE_SIZE, WallType::STEEL)); } for (int j = 1; j < MAP_WIDTH - 1; ++j) { walls.push_back(Wall(j * TILE_SIZE, 0, WallType::STEEL)); walls.push_back(Wall(j * TILE_SIZE, (MAP_HEIGHT - 1) * TILE_SIZE, WallType::STEEL)); } int wallDensityFactor = 28 + level * 2; int steelChance = 5 + level * 2; int waterChance = 3 + level * 2; int bushChance = (level >= 2) ? (level * 3) : 0; for (int i = spawnRowTop + 1; i < baseRow; ++i) { for (int j = 1; j < MAP_WIDTH - 1; ++j) { if (isProtectedZone(i, j)) continue; int placeRoll = rand() % wallDensityFactor; if (placeRoll < 10) { int typeRoll = rand() % 100; WallType currentType; bool placed = false; if (typeRoll < waterChance) { currentType = WallType::WATER; placed = true; } else if (typeRoll < waterChance + steelChance) { currentType = WallType::STEEL; placed = true; } else if (bushChance > 0 && typeRoll < waterChance + steelChance + bushChance) { currentType = WallType::BUSH; placed = true; } else { currentType = WallType::BRICK; placed = true; } if (placed) { walls.push_back(Wall(j * TILE_SIZE, i * TILE_SIZE, currentType)); } if (currentType != WallType::BUSH) { if (level > 2 && rand() % (8 - level + 1) == 0) { if (j + 1 < MAP_WIDTH - 1 && !isProtectedZone(i, j + 1)) walls.push_back(Wall((j + 1) * TILE_SIZE, i * TILE_SIZE, WallType::BRICK)); } if (level > 3 && rand() % (9 - level + 1) == 0) { if (i + 1 < baseRow && !isProtectedZone(i + 1, j)) walls.push_back(Wall(j * TILE_SIZE, (i + 1) * TILE_SIZE, WallType::BRICK)); } } } } } if (level >= 3) { for(int i=4; i<7; ++i) for(int j=4; j<7; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } if (level >= 4) { for(int i=MAP_HEIGHT-6; i<MAP_HEIGHT-3; ++i) for(int j=MAP_WIDTH-7; j<MAP_WIDTH-4; ++j) if(!isProtectedZone(i,j) && rand()%2==0) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::WATER)); } if (level == 5) { for(int i = MAP_HEIGHT/2 - 1; i <= MAP_HEIGHT/2 + 1; ++i ) { for (int j = MAP_WIDTH/2 - 2; j <= MAP_WIDTH/2 + 2; ++j) { if (i == MAP_HEIGHT/2 && j == MAP_WIDTH/2) continue; if (!isProtectedZone(i,j)) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::STEEL)); } } } if (level >= 2 && level < 5) { for(int i = MAP_HEIGHT/2 - 2; i <= MAP_HEIGHT/2 + 2; ++i ) { for (int j = MAP_WIDTH/2 - 3; j <= MAP_WIDTH/2 + 3; ++j) { if (abs(i - MAP_HEIGHT/2) <=1 && abs(j-MAP_WIDTH/2) <=1) continue; if (!isProtectedZone(i,j) && rand()%4 == 0) { bool occupied = false; for(const auto& w : walls) { if (w.rect.x == j*TILE_SIZE && w.rect.y == i*TILE_SIZE && w.type != WallType::BUSH) { occupied = true; break; } } if (!occupied) walls.push_back(Wall(j*TILE_SIZE, i*TILE_SIZE, WallType::BUSH)); } } } } }

    // --- Hàm Tạo Những Kẻ Địch Đầu Tiên ---
    void spawnInitialEnemies() {
        int count = 0;
        // Tạo địch cho đến khi đủ số lượng tối đa trên màn hình hoặc hết địch cần tạo
        while (count < maxEnemiesOnScreen && enemiesToSpawn > 0) {
            if (!trySpawnOneEnemy()) { // Thử tạo 1 con
                cerr << "Warning: Could not spawn initial enemy (maybe no free points?)." << endl;
                break; // Dừng nếu không tạo được nữa
            }
            count++;
        }
    }

    // --- Hàm Thử Tạo Một Kẻ Địch ---
    bool trySpawnOneEnemy() {
        // Không tạo nếu đã đủ số lượng tối đa hoặc hết địch cần tạo
        if (enemiesOnScreen >= maxEnemiesOnScreen || enemiesToSpawn <= 0) {
            return false;
        }

        // Các vị trí có thể tạo địch (hàng trên cùng)
        vector<pair<int, int>> spawnPoints = {
            {TILE_SIZE, TILE_SIZE},                           // Góc trên trái
            {(MAP_WIDTH / 2 - 1) * TILE_SIZE, TILE_SIZE},    // Giữa trên
            {(MAP_WIDTH - 2) * TILE_SIZE, TILE_SIZE}         // Góc trên phải
        };
        random_shuffle(spawnPoints.begin(), spawnPoints.end()); // Xáo trộn thứ tự để ngẫu nhiên

        // Duyệt qua các điểm tạo tiềm năng
        for (const auto& sp : spawnPoints) {
            SDL_Rect spawnRect = {sp.first, sp.second, TILE_SIZE, TILE_SIZE}; // Vùng dự định tạo địch
            bool canSpawn = true; // Giả sử có thể tạo

            // Kiểm tra va chạm với tường (trừ bụi cỏ)
            for (const auto& w : walls) {
                if (w.active && w.type != WallType::BUSH && SDL_HasIntersection(&spawnRect, &w.rect)) {
                    canSpawn = false; break; // Không tạo nếu có tường
                }
            }
            // Kiểm tra va chạm với Player 1 (nếu P1 đang hoạt động)
            if (canSpawn && player1.isActive && SDL_HasIntersection(&spawnRect, &player1.rect)) {
                canSpawn = false;
            }
            // Kiểm tra va chạm với Player 2 (nếu chế độ 2P và P2 đang hoạt động)
            if (canSpawn && numberOfPlayers == 2 && player2.isActive && SDL_HasIntersection(&spawnRect, &player2.rect)) {
                canSpawn = false;
            }
            // Kiểm tra va chạm với các kẻ địch khác đã có trên màn hình
            if (canSpawn) {
                for (const auto& e : enemies) {
                    if (e.active && SDL_HasIntersection(&spawnRect, &e.rect)) {
                        canSpawn = false; break;
                    }
                }
            }

            // Nếu vị trí trống trải
            if (canSpawn) {
                int initialHP = 1; // Máu mặc định
                // Quyết định xem có tạo địch "trâu" không
                if (toughEnemiesSpawnedThisLevel < toughEnemiesToSpawnThisLevel) {
                    initialHP = TOUGH_ENEMY_HP;
                    toughEnemiesSpawnedThisLevel++; // Tăng bộ đếm địch trâu đã tạo
                }
                // Tạo đối tượng EnemyTank mới, truyền vào âm thanh bắn và nổ
                enemies.push_back(EnemyTank(sp.first, sp.second, currentLevel, initialHP, bulletShotSound, tankBrokenSound));
                enemiesOnScreen++; // Tăng số địch trên màn
                enemiesToSpawn--;  // Giảm số địch cần tạo
                return true; // Tạo thành công
            }
        }

        // Nếu duyệt hết các điểm mà không tạo được
        cerr << "Warning: Failed to find a free spawn point for enemy." << endl;
        return false;
    }

    // --- Hàm Xử Lý Sự Kiện --- (Quản lý input dựa trên trạng thái game)
    void handleEvents() {
        SDL_Event event; // Biến lưu trữ sự kiện
        // Lặp qua tất cả các sự kiện đang chờ xử lý trong hàng đợi
        while (SDL_PollEvent(&event)) {
            // Luôn kiểm tra sự kiện đóng cửa sổ
            if (event.type == SDL_QUIT) {
                running = false; // Đặt cờ để thoát vòng lặp chính
                return;          // Thoát khỏi hàm handleEvents ngay
            }

            // Xử lý sự kiện dựa trên trạng thái hiện tại của game
            switch (currentState) {
                case GameState::SELECT_MODE: // Nếu đang ở màn hình menu
                    handleMenuInput(event);   // Gọi hàm xử lý input cho menu
                    break;
                case GameState::PLAYING:     // Nếu đang trong màn chơi
                    handleGameplayInput(event); // Gọi hàm xử lý input cho game
                    break;
                case GameState::GAME_OVER:   // Nếu đang ở màn hình game over
                    // TODO (Tùy chọn): Xử lý input ở đây, ví dụ nhấn Enter để quay lại menu
                    // handleGameOverInput(event);
                    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                       running = false; // Cho phép thoát cả ở màn Game Over
                    }
                    break;
            }
        }
    }

    // --- Hàm Xử Lý Input Cho Menu ---
    void handleMenuInput(const SDL_Event& event) {
        // Chỉ xử lý khi nhấn phím xuống và không phải là phím lặp lại
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            switch (event.key.keysym.sym) { // Kiểm tra mã phím được nhấn
                case SDLK_1: // Nhấn phím số 1
                    cout << "Selected 1 Player mode." << endl;
                    numberOfPlayers = 1;                // Đặt số người chơi
                    currentState = GameState::PLAYING;  // Chuyển sang trạng thái chơi
                    setupLevel(1);                      // Bắt đầu màn chơi 1
                    break;
                case SDLK_2: // Nhấn phím số 2
                    cout << "Selected 2 Players mode." << endl;
                    numberOfPlayers = 2;                // Đặt số người chơi
                    currentState = GameState::PLAYING;  // Chuyển sang trạng thái chơi
                    setupLevel(1);                      // Bắt đầu màn chơi 1
                    break;
                case SDLK_ESCAPE: // Nhấn phím Escape
                    running = false; // Thoát game
                    break;
            }
        }
    }

    // --- Hàm Xử Lý Input Trong Game ---
    void handleGameplayInput(const SDL_Event& event) {
        // Xử lý sự kiện nhấn phím xuống (không lặp lại)
         if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            // --- Input cho Người Chơi 1 (WASD + J) --- Chỉ khi P1 đang hoạt động
            if (player1.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: player1.velocityY = -PLAYER_SPEED; player1.lastDirY = -1; player1.lastDirX = 0; break; // Lên
                    case SDLK_s: player1.velocityY = PLAYER_SPEED; player1.lastDirY = 1; player1.lastDirX = 0; break;  // Xuống
                    case SDLK_a: player1.velocityX = -PLAYER_SPEED; player1.lastDirX = -1; player1.lastDirY = 0; break; // Trái
                    case SDLK_d: player1.velocityX = PLAYER_SPEED; player1.lastDirX = 1; player1.lastDirY = 0; break;  // Phải
                    case SDLK_j: // Bắn
                        if (player1.shoot() && bulletShotSound) { // Nếu bắn thành công và có âm thanh
                            Mix_PlayChannel(-1, bulletShotSound, 0); // Phát âm thanh bắn
                        }
                        break;
                }
            }
            // --- Input cho Người Chơi 2 (Mũi tên + Ctrl) --- Chỉ khi chế độ 2P và P2 đang hoạt động
            if (numberOfPlayers == 2 && player2.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:    player2.velocityY = -PLAYER_SPEED; player2.lastDirY = -1; player2.lastDirX = 0; break; // Lên
                    case SDLK_DOWN:  player2.velocityY = PLAYER_SPEED; player2.lastDirY = 1; player2.lastDirX = 0; break;  // Xuống
                    case SDLK_LEFT:  player2.velocityX = -PLAYER_SPEED; player2.lastDirX = -1; player2.lastDirY = 0; break; // Trái
                    case SDLK_RIGHT: player2.velocityX = PLAYER_SPEED; player2.lastDirX = 1; player2.lastDirY = 0; break;  // Phải
                    case SDLK_RCTRL: // Ctrl Phải
                    case SDLK_LCTRL: // Hoặc Ctrl Trái để bắn
                        if (player2.shoot() && bulletShotSound) { // Nếu bắn thành công và có âm thanh
                            Mix_PlayChannel(-1, bulletShotSound, 0); // Phát âm thanh bắn
                        }
                        break;
                }
            }
            // --- Phím Chung ---
            if (event.key.keysym.sym == SDLK_ESCAPE) { // Nhấn Escape để thoát game
                running = false;
            }
        }
        // Xử lý sự kiện nhả phím (để dừng di chuyển)
        else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
            // --- Nhả phím Người Chơi 1 --- Chỉ khi P1 đang hoạt động
            if (player1.isActive) {
                switch (event.key.keysym.sym) {
                    case SDLK_w: if (player1.velocityY < 0) player1.velocityY = 0; break; // Dừng đi lên
                    case SDLK_s: if (player1.velocityY > 0) player1.velocityY = 0; break; // Dừng đi xuống
                    case SDLK_a: if (player1.velocityX < 0) player1.velocityX = 0; break; // Dừng sang trái
                    case SDLK_d: if (player1.velocityX > 0) player1.velocityX = 0; break; // Dừng sang phải
                }
                // Cập nhật hướng nhìn cuối cùng khi dừng một chiều
                if (player1.velocityX == 0 && player1.velocityY != 0) { player1.lastDirX = 0; player1.lastDirY = (player1.velocityY > 0) ? 1 : -1; }
                else if (player1.velocityY == 0 && player1.velocityX != 0) { player1.lastDirY = 0; player1.lastDirX = (player1.velocityX > 0) ? 1 : -1; }
            }
            // --- Nhả phím Người Chơi 2 --- Chỉ khi chế độ 2P và P2 đang hoạt động
            if (numberOfPlayers == 2 && player2.isActive) {
                 switch (event.key.keysym.sym) {
                    case SDLK_UP:    if (player2.velocityY < 0) player2.velocityY = 0; break; // Dừng đi lên
                    case SDLK_DOWN:  if (player2.velocityY > 0) player2.velocityY = 0; break; // Dừng đi xuống
                    case SDLK_LEFT:  if (player2.velocityX < 0) player2.velocityX = 0; break; // Dừng sang trái
                    case SDLK_RIGHT: if (player2.velocityX > 0) player2.velocityX = 0; break; // Dừng sang phải
                }
                // Cập nhật hướng nhìn cuối cùng khi dừng một chiều
                if (player2.velocityX == 0 && player2.velocityY != 0) { player2.lastDirX = 0; player2.lastDirY = (player2.velocityY > 0) ? 1 : -1; }
                else if (player2.velocityY == 0 && player2.velocityX != 0) { player2.lastDirY = 0; player2.lastDirX = (player2.velocityX > 0) ? 1 : -1; }
            }
        }
    } // End handleGameplayInput

    // --- Hàm Cập Nhật Trạng Thái Game --- (Logic chính của game)
    void update() {
         // Chỉ cập nhật nếu game đang chạy và ở trạng thái PLAYING
         if (!running || currentState != GameState::PLAYING) {
             return;
         }

         // --- Cập nhật Người Chơi đang hoạt động ---
         if (player1.isActive) {
             player1.updateCooldown();         // Cập nhật cooldown bắn
             player1.updatePosition(walls, enemies); // Cập nhật vị trí & va chạm
             player1.updateBullets();          // Cập nhật đạn
         }
         if (numberOfPlayers == 2 && player2.isActive) { // Chỉ cập nhật P2 nếu có 2 người và P2 đang hoạt động
             player2.updateCooldown();
             player2.updatePosition(walls, enemies);
             player2.updateBullets();
         }

         // --- Cập nhật Kẻ Địch đang hoạt động ---
         for (auto& enemy : enemies) {
             if (enemy.active) {
                 enemy.updateHitStatus();       // Cập nhật hiệu ứng nhấp nháy
                 enemy.updateAIAndVelocity(walls); // Cập nhật AI (bắn, đổi hướng)
                 enemy.updatePosition(walls);     // Cập nhật vị trí & va chạm
                 enemy.updateBullets();         // Cập nhật đạn
             }
         }

         // --- Xử Lý Va Chạm ---
         // --- Đạn Người Chơi 1 vs Tường & Địch ---
         if (player1.isActive) { // Chỉ xử lý nếu P1 còn hoạt động
             for (auto& pB : player1.bullets) { // Duyệt qua từng viên đạn của P1
                 if (!pB.active) continue; // Bỏ qua nếu đạn không hoạt động
                 bool hit = false;
                 // Kiểm tra va chạm với Tường (trừ bụi cỏ, nước)
                 for (auto& w : walls) {
                     if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&pB.rect, &w.rect)) {
                         pB.active = false; // Hủy viên đạn
                         if (w.type == WallType::BRICK) w.active = false; // Phá tường gạch
                         hit = true; break; // Dừng kiểm tra tường khác cho viên đạn này
                     }
                 }
                 if (hit) continue; // Nếu đã trúng tường, không cần kiểm tra địch nữa
                 // Kiểm tra va chạm với Địch
                 for (auto& e : enemies) {
                     if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) {
                         pB.active = false; // Hủy viên đạn
                         e.takeHit();       // Địch nhận sát thương
                         hit = true; break; // Một viên đạn chỉ hạ một địch
                     }
                 }
             }
         }
         // --- Đạn Người Chơi 2 vs Tường & Địch --- (Tương tự P1, nếu có 2P và P2 active)
         if (numberOfPlayers == 2 && player2.isActive) {
              for (auto& pB : player2.bullets) {
                 if (!pB.active) continue;
                 bool hit = false;
                 // Vs Walls
                 for (auto& w : walls) {
                     if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&pB.rect, &w.rect)) {
                         pB.active = false; if (w.type == WallType::BRICK) w.active = false; hit = true; break;
                     }
                 }
                 if (hit) continue;
                 // Vs Enemies
                 for (auto& e : enemies) {
                     if (e.active && SDL_HasIntersection(&pB.rect, &e.rect)) {
                         pB.active = false; e.takeHit(); hit = true; break;
                     }
                 }
             }
         }
         // --- Đạn Địch vs Tường & Người Chơi ---
         for (auto& e : enemies) { // Duyệt qua từng kẻ địch
             if (!e.active) continue; // Bỏ qua nếu địch đã bị hạ
             for (auto& eB : e.bullets) { // Duyệt qua từng viên đạn của địch
                 if (!eB.active) continue; // Bỏ qua đạn không hoạt động
                 bool hitWall = false;
                 // Vs Tường (trừ bụi cỏ, nước)
                 for (auto& w : walls) {
                     if (w.active && w.type != WallType::BUSH && w.type != WallType::WATER && SDL_HasIntersection(&eB.rect, &w.rect)) {
                         eB.active = false; // Hủy đạn địch
                         if (w.type == WallType::BRICK) w.active = false; // Phá tường gạch
                         hitWall = true; break;
                     }
                 }
                 if (hitWall) continue; // Nếu trúng tường, bỏ qua kiểm tra người chơi

                 // Vs Người Chơi 1 (nếu P1 đang hoạt động)
                 if (player1.isActive && SDL_HasIntersection(&eB.rect, &player1.rect)) {
                     eB.active = false;         // Hủy đạn địch
                     player1.hitByEnemy();      // Xử lý P1 bị bắn
                     if (playerDestroySound) Mix_PlayChannel(-1, playerDestroySound, 0); // Phát âm thanh P1 nổ
                     // Không chuyển Game Over ngay, kiểm tra điều kiện thua ở cuối hàm update
                 }
                 // Vs Người Chơi 2 (nếu chế độ 2P và P2 đang hoạt động)
                 else if (numberOfPlayers == 2 && player2.isActive && SDL_HasIntersection(&eB.rect, &player2.rect)) {
                      eB.active = false;        // Hủy đạn địch
                      player2.hitByEnemy();     // Xử lý P2 bị bắn
                      if (playerDestroySound) Mix_PlayChannel(-1, playerDestroySound, 0); // Phát âm thanh P2 nổ
                      // Không chuyển Game Over ngay
                 }
             }
         }

         // --- Dọn Dẹp và Tạo Địch Mới ---
         // Xóa kẻ địch đã bị hạ khỏi danh sách
         enemies.erase(remove_if(enemies.begin(), enemies.end(), [](const EnemyTank &e){ return !e.active; }), enemies.end());
         enemiesOnScreen = enemies.size(); // Cập nhật số lượng địch trên màn
         // Tạo thêm địch nếu cần và còn chỗ
         if (enemiesToSpawn > 0 && enemiesOnScreen < maxEnemiesOnScreen) {
             static Uint32 lastSpawnTime = 0; // Biến static để lưu thời điểm tạo cuối cùng
             const Uint32 SPAWN_DELAY = 2000; // Thời gian chờ giữa các lần tạo địch (ms)
             Uint32 currentTime = SDL_GetTicks(); // Thời gian hiện tại
             if (currentTime > lastSpawnTime + SPAWN_DELAY) { // Nếu đủ thời gian chờ
                 if (trySpawnOneEnemy()) { // Thử tạo 1 con
                     lastSpawnTime = currentTime; // Nếu thành công, reset thời gian chờ
                 } else {
                     // Nếu thất bại (ví dụ: không có chỗ trống), thử lại sớm hơn một chút
                     lastSpawnTime = currentTime - SPAWN_DELAY / 2;
                 }
             }
         }

         // --- Kiểm Tra Điều Kiện Thua Game --- (Quan trọng!)
         bool player1_is_out = !player1.isActive; // P1 bị loại nếu không hoạt động
         // P2 bị loại nếu: đang chơi 1 người, HOẶC (đang chơi 2 người VÀ P2 không hoạt động)
         bool player2_is_out = (numberOfPlayers == 1) || (numberOfPlayers == 2 && !player2.isActive);

         // Game Over CHỈ KHI tất cả người chơi cần thiết đều bị loại
         if (player1_is_out && player2_is_out) {
             cout << "All players out! Game Over at Level " << currentLevel << ".\n";
             if (gameOverSound) { // Phát âm thanh thua
                 Mix_PlayChannel(-1, gameOverSound, 0);
             }
             currentState = GameState::GAME_OVER; // Chuyển trạng thái sang Game Over
             return; // Thoát khỏi hàm update ngay lập tức
         }

         // --- Kiểm Tra Điều Kiện Thắng Màn --- (Chỉ kiểm tra nếu chưa Game Over)
         // Thắng màn khi: Đã vào màn chơi (level > 0), không còn địch cần tạo, và không còn địch trên màn hình
         if (currentLevel > 0 && enemiesToSpawn == 0 && enemies.empty()) {
             cout << "\n===============================\n";
             cout << "      LEVEL " << currentLevel << " CLEARED!      \n";
             cout << "===============================\n\n";
             if (levelUpSound) { // Phát âm thanh qua màn
                 Mix_PlayChannel(-1, levelUpSound, 0);
             }
             SDL_Delay(1000); // Dừng một chút để người chơi thấy thông báo

             // Xử lý chuyển màn hoặc kết thúc game
             if (currentLevel < maxLevels) { // Nếu chưa phải màn cuối
                 cout << "Proceeding to next level..." << endl;
                 SDL_Delay(1500);             // Dừng thêm chút nữa
                 setupLevel(currentLevel + 1); // Thiết lập màn chơi tiếp theo
             } else { // Nếu đã hoàn thành màn cuối
                 cout << "*******************************\n";
                 cout << "* CONGRATULATIONS! YOU WIN! *\n";
                 cout << "*******************************\n";
                 SDL_Delay(3000);             // Dừng để xem thông báo thắng
                 running = false;             // Kết thúc game
             }
         }
    } // End update()

    // --- Hàm Vẽ Đồ Họa --- (Vẽ dựa trên trạng thái game)
    void render() {
        if (!renderer) { // Không thể vẽ nếu không có renderer
            return;
        }

        // Chọn cách vẽ dựa trên trạng thái hiện tại
        switch (currentState) {
            case GameState::SELECT_MODE: { // Vẽ màn hình menu
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Xóa nền đen (phòng khi ảnh menu không che hết)
                SDL_RenderClear(renderer);
                if (menuTexture) { // Nếu texture menu đã được tải
                    SDL_RenderCopy(renderer, menuTexture, NULL, NULL); // Vẽ ảnh menu lên toàn bộ cửa sổ
                } else {
                    cerr << "Error: Menu texture is missing, cannot render menu." << endl;
                    // Có thể vẽ chữ thay thế ở đây nếu muốn
                }
                break; // Kết thúc vẽ menu
            }

            case GameState::PLAYING: { // Vẽ cảnh trong game
                // --- Vẽ Nền và Viền ---
                SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255); // Màu xám cho viền
                SDL_RenderClear(renderer); // Xóa toàn bộ màn hình bằng màu viền
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);       // Màu đen cho khu vực chơi
                SDL_Rect playableArea = {TILE_SIZE, TILE_SIZE, SCREEN_WIDTH - 2 * TILE_SIZE, SCREEN_HEIGHT - 2 * TILE_SIZE};
                SDL_RenderFillRect(renderer, &playableArea); // Vẽ khu vực chơi màu đen

                // --- Vẽ Tường (trừ bụi cỏ) ---
                for (auto &wall : walls) {
                    if (!wall.active || wall.type == WallType::BUSH) continue; // Bỏ qua tường bị phá hoặc bụi cỏ
                    SDL_Texture* tex = nullptr;
                    // Chọn texture tương ứng với loại tường
                    switch (wall.type) {
                        case WallType::BRICK: tex = brickTexture; break;
                        case WallType::STEEL: tex = steelTexture; break;
                        case WallType::WATER: tex = waterTexture; break;
                        default: break;
                    }
                    if (tex) { // Nếu có texture hợp lệ
                        SDL_RenderCopy(renderer, tex, nullptr, &wall.rect); // Vẽ tường
                    }
                }

                // --- Vẽ Kẻ Địch ---
                for (auto &enemy : enemies) {
                    if (!enemy.active) continue; // Bỏ qua địch đã bị hạ
                    SDL_Texture* tex = nullptr;
                    // Chọn texture dựa trên loại (máu) và hướng
                    if (enemy.initialHitPoints > 1) { // Địch "trâu" (tank3)
                        if (enemy.lastDirY < 0) tex = enemyTank3UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank3DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank3LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank3RightTexture; else tex = enemyTank3DownTexture; // Mặc định xuống
                    } else { // Địch thường (tank2)
                        if (enemy.lastDirY < 0) tex = enemyTank2UpTexture; else if (enemy.lastDirY > 0) tex = enemyTank2DownTexture; else if (enemy.lastDirX < 0) tex = enemyTank2LeftTexture; else if (enemy.lastDirX > 0) tex = enemyTank2RightTexture; else tex = enemyTank2DownTexture; // Mặc định xuống
                    }
                    if (tex) { // Nếu có texture hợp lệ
                        // Áp dụng hiệu ứng nhấp nháy nếu bị bắn
                        if (enemy.isHit) { SDL_SetTextureColorMod(tex, 255, 100, 100); SDL_SetTextureAlphaMod(tex, 200); } // Hơi đỏ và trong suốt
                        else { SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 255); } // Reset màu và alpha
                        SDL_RenderCopy(renderer, tex, nullptr, &enemy.rect); // Vẽ địch
                        // Reset màu và alpha để không ảnh hưởng lần vẽ sau
                        SDL_SetTextureColorMod(tex, 255, 255, 255); SDL_SetTextureAlphaMod(tex, 255);
                    }
                }

                // --- Vẽ Người Chơi 1 (nếu đang hoạt động) ---
                if (player1.isActive) {
                     SDL_Texture* p1Tex = nullptr;
                     // Chọn texture P1 dựa trên hướng
                     if (player1.lastDirY < 0) p1Tex = player1TankUpTexture; else if (player1.lastDirY > 0) p1Tex = player1TankDownTexture; else if (player1.lastDirX < 0) p1Tex = player1TankLeftTexture; else if (player1.lastDirX > 0) p1Tex = player1TankRightTexture; else p1Tex = player1TankUpTexture; // Mặc định lên
                     if (p1Tex) { // Nếu có texture
                         SDL_RenderCopy(renderer, p1Tex, nullptr, &player1.rect); // Vẽ P1
                     }
                     // Vẽ đạn của P1
                     if (bulletTexture) {
                         for (auto &bullet : player1.bullets) {
                             if (bullet.active) {
                                SDL_RenderCopy(renderer, bulletTexture, nullptr, &bullet.rect);
                             }
                         }
                     }
                }

                // --- Vẽ Người Chơi 2 (nếu đang ở chế độ 2P và đang hoạt động) ---
                if (numberOfPlayers == 2 && player2.isActive) {
                    SDL_Texture* p2Tex = nullptr;
                    // Chọn texture P2 dựa trên hướng
                    if (player2.lastDirY < 0) p2Tex = player2TankUpTexture; else if (player2.lastDirY > 0) p2Tex = player2TankDownTexture; else if (player2.lastDirX < 0) p2Tex = player2TankLeftTexture; else if (player2.lastDirX > 0) p2Tex = player2TankRightTexture; else p2Tex = player2TankUpTexture; // Mặc định lên
                    if (p2Tex) { // Nếu có texture
                        SDL_RenderCopy(renderer, p2Tex, nullptr, &player2.rect); // Vẽ P2
                    }
                     // Vẽ đạn của P2
                     if (bulletTexture) {
                         for (auto &bullet : player2.bullets) {
                             if (bullet.active) {
                                SDL_RenderCopy(renderer, bulletTexture, nullptr, &bullet.rect);
                             }
                         }
                     }
                }

                 // --- Vẽ Đạn Của Địch --- (Vẽ ngay cả khi người chơi đã bị hạ)
                 if (bulletTexture) {
                     for (auto &enemy : enemies) {
                         if(enemy.active) { // Chỉ vẽ đạn của địch còn sống
                             for (auto &bullet : enemy.bullets) {
                                 if (bullet.active) {
                                    SDL_RenderCopy(renderer, bulletTexture, nullptr, &bullet.rect);
                                 }
                             }
                         }
                     }
                 }

                // --- Vẽ Bụi Cỏ (Vẽ sau cùng để che phủ xe tăng) ---
                for (auto &wall : walls) {
                    if (wall.active && wall.type == WallType::BUSH && grassTexture) {
                        SDL_RenderCopy(renderer, grassTexture, nullptr, &wall.rect);
                    }
                }
                break; // Kết thúc vẽ PLAYING
            } // Kết thúc khối PLAYING

            case GameState::GAME_OVER: { // Vẽ màn hình Game Over
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Nền đen
                SDL_RenderClear(renderer);
                if (gameOverTexture) { // Nếu có ảnh Game Over
                    int imgW, imgH;
                    SDL_QueryTexture(gameOverTexture, NULL, NULL, &imgW, &imgH); // Lấy kích thước ảnh
                    // Tính toán vị trí để căn giữa
                    SDL_Rect dstRect = {(SCREEN_WIDTH - imgW) / 2, (SCREEN_HEIGHT - imgH) / 2, imgW, imgH };
                    SDL_RenderCopy(renderer, gameOverTexture, NULL, &dstRect); // Vẽ ảnh
                } else {
                     cerr << "Warning: Game Over texture missing." << endl;
                     // Có thể vẽ chữ "GAME OVER" thay thế ở đây
                }
                break; // Kết thúc vẽ GAME_OVER
            } // Kết thúc khối GAME_OVER
        } // Kết thúc switch(currentState)

        // --- Hiển thị mọi thứ đã vẽ lên màn hình ---
        SDL_RenderPresent(renderer);
    } // End render()

    // --- Vòng Lặp Chính Của Game ---
    void run() {
        const int TARGET_FPS = 60;                      // FPS mục tiêu
        const int FRAME_DELAY = 1000 / TARGET_FPS;     // Thời gian tối đa cho mỗi frame (ms)

        Uint32 frameStart; // Thời điểm bắt đầu frame
        int frameTime;     // Thời gian xử lý frame

        cout << "Starting Game Loop..." << endl;
        // Vòng lặp chạy tantrum nào biến `running` còn true
        while (running) {
            frameStart = SDL_GetTicks(); // Ghi lại thời điểm bắt đầu frame

            handleEvents(); // 1. Xử lý input người dùng (phụ thuộc trạng thái)
            update();       // 2. Cập nhật logic game (phụ thuộc trạng thái)
            render();       // 3. Vẽ màn hình (phụ thuộc trạng thái)

            // --- Giới Hạn Tốc Độ Khung Hình (FPS Limiter) ---
            frameTime = SDL_GetTicks() - frameStart; // Tính thời gian đã trôi qua cho frame này
            if (FRAME_DELAY > frameTime) { // Nếu frame xử lý nhanh hơn thời gian mục tiêu
                SDL_Delay(FRAME_DELAY - frameTime); // Chờ phần thời gian còn lại
            }
        }
        cout << "Exiting Game Loop." << endl;
    } // End run()

}; // Kết thúc định nghĩa lớp Game

// =============================================================================
// == Hàm main (Điểm bắt đầu của chương trình) ==
// =============================================================================
int main(int argc, char* argv[]) {
    { // Tạo một khối (scope) để đảm bảo đối tượng Game được hủy trước khi kết thúc main
        Game game; // Tạo đối tượng Game, constructor sẽ chạy và khởi tạo mọi thứ
        if (game.running) { // Chỉ chạy game nếu quá trình khởi tạo thành công
            game.run();     // Bắt đầu vòng lặp chính của game
        } else {
            cerr << "Game initialization failed. Exiting." << endl;
        }
        // Khi ra khỏi khối này, destructor của 'game' sẽ tự động được gọi để dọn dẹp
    } // Kết thúc scope

    cout << "Application finished." << endl;
    return 0; // Kết thúc chương trình thành công
}

// =============================================================================
// == Định Nghĩa Hàm Tiện Ích ==
// =============================================================================
// Hàm tải texture từ tệp ảnh
SDL_Texture* loadTexture(const std::string &path, SDL_Renderer* renderer) {
    SDL_Texture* newTexture = nullptr;
    // Tải ảnh từ đường dẫn vào một SDL_Surface (dữ liệu pixel thô)
    SDL_Surface* loadedSurface = IMG_Load(path.c_str()); // IMG_Load hỗ trợ nhiều định dạng (PNG, JPG,...)
    if (!loadedSurface) { // Kiểm tra lỗi tải ảnh
        cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << endl;
    } else {
        // Chuyển đổi SDL_Surface thành SDL_Texture (định dạng tối ưu cho GPU)
        newTexture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
        if (!newTexture) { // Kiểm tra lỗi tạo texture
            cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << endl;
        }
        // Giải phóng SDL_Surface không còn cần thiết
        SDL_FreeSurface(loadedSurface);
    }
    return newTexture; // Trả về con trỏ texture (hoặc nullptr nếu lỗi)
}
