#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
# define M_PI 	   3.14159265358979323846  /* pi */

// 윈도우 크기
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// 게임 경계
const float GAME_LEFT = -1.0f;
const float GAME_RIGHT = 1.0f;
const float GAME_BOTTOM = -1.0f;
const float GAME_TOP = 1.0f;

// 엔티티 크기
const float PLAYER_SIZE = 0.08f;
const float BULLET_SIZE = 0.015f;
const float ATTACK_SIZE = 0.025f;
const float ENEMY_SIZE = 0.12f;

// 게임 설정
const int PLAYER_LIVES = 3;
const float ENEMY_HEALTH = 100.0f;
const float ATTACK_DAMAGE = 10.0f;
const float RESPAWN_TIME = 2.0f;

// 키 상태 추적
bool keys[256] = { false };
bool specialKeys[256] = { false };

// 타이머 변수
float lastTime = 0;
float gameTime = 0;

// VBO 최적화를 위한 변수
GLuint circleVBO = 0;
GLuint bulletVBO = 0;
bool vbosInitialized = false;
const int CIRCLE_SEGMENTS = 32;

// 벡터 구조체
struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }
    float length() const { return sqrt(x * x + y * y); }
    Vec2 normalized() const {
        float len = length();
        return len > 0 ? Vec2(x / len, y / len) : Vec2(0, 0);
    }
};

// 카메라 애니메이션을 위한 변수
float cameraShake = 0.0f;
float cameraShakeDecay = 5.0f;
Vec2 cameraOffset(0, 0);

// VBO 초기화 함수
void initVBOs() {
    if (vbosInitialized) return;

    // 원형 VBO 초기화
    std::vector<float> circleVertices;
    circleVertices.push_back(0.0f); // 중심점
    circleVertices.push_back(0.0f);

    for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
        float angle = i * 2.0f * M_PI / CIRCLE_SEGMENTS;
        circleVertices.push_back(cos(angle));
        circleVertices.push_back(sin(angle));
    }

    glGenBuffers(1, &circleVBO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float),
        circleVertices.data(), GL_STATIC_DRAW);

    // 총알 모양 VBO 초기화 (타원형)
    std::vector<float> bulletVertices;
    bulletVertices.push_back(0.0f); // 중심점
    bulletVertices.push_back(0.0f);

    for (int i = 0; i <= 16; i++) {
        float angle = i * 2.0f * M_PI / 16;
        bulletVertices.push_back(cos(angle) * 0.6f); // x축 압축
        bulletVertices.push_back(sin(angle) * 1.2f); // y축 확장
    }

    glGenBuffers(1, &bulletVBO);
    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, bulletVertices.size() * sizeof(float),
        bulletVertices.data(), GL_STATIC_DRAW);

    vbosInitialized = true;
}

// 최적화된 원 그리기 함수
void drawOptimizedCircle(float x, float y, float radius) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(radius, radius, 1);

    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glPopMatrix();
}

// 최적화된 총알 그리기 함수
void drawOptimizedBullet(float x, float y, float radius, float rotation = 0) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(rotation * 180.0f / M_PI, 0, 0, 1);
    glScalef(radius, radius, 1);

    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 18);
    glDisableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glPopMatrix();
}

// 카메라 흔들림 효과
void addCameraShake(float intensity) {
    cameraShake = std::max(cameraShake, intensity);
}

void updateCamera(float deltaTime) {
    if (cameraShake > 0) {
        float angle = gameTime * 50.0f;
        cameraOffset.x = sin(angle) * cameraShake * 0.02f;
        cameraOffset.y = cos(angle * 1.3f) * cameraShake * 0.02f;
        cameraShake -= cameraShakeDecay * deltaTime;
        if (cameraShake < 0) cameraShake = 0;
    }
    else {
        cameraOffset.x = 0;
        cameraOffset.y = 0;
    }
}

void applyCameraTransform() {
    glTranslatef(cameraOffset.x, cameraOffset.y, 0);
}

// 게임 오브젝트 기본 클래스
class GameObject {
public:
    Vec2 position;
    Vec2 velocity;
    float size;
    bool active;

    GameObject(Vec2 pos, float s) : position(pos), size(s), active(true) {}
    virtual ~GameObject() {}

    virtual void update(float deltaTime) {
        position = position + velocity * deltaTime;
    }

    virtual void render() = 0;

    bool checkCollision(const GameObject& other) const {
        float dx = position.x - other.position.x;
        float dy = position.y - other.position.y;
        float distance = sqrt(dx * dx + dy * dy);
        return distance < (size + other.size) * 0.8f; // 약간 더 관대한 충돌 판정
    }
};

// 플레이어 클래스 (메이드 캐릭터 모티브)
class Player : public GameObject {
public:
    int lives;
    float respawnTimer;
    bool isRespawning;
    float animTimer;

    Player() : GameObject(Vec2(0, -0.7f), PLAYER_SIZE), lives(PLAYER_LIVES),
        respawnTimer(0), isRespawning(false), animTimer(0) {
    }

    void update(float deltaTime) override {
        animTimer += deltaTime * 3.0f;

        if (isRespawning) {
            respawnTimer -= deltaTime;
            if (respawnTimer <= 0) {
                isRespawning = false;
                active = true;
                position = Vec2(0, -0.7f);
            }
            return;
        }

        GameObject::update(deltaTime);

        // 경계 체크
        if (position.x - size < GAME_LEFT) position.x = GAME_LEFT + size;
        if (position.x + size > GAME_RIGHT) position.x = GAME_RIGHT - size;
        if (position.y - size < GAME_BOTTOM) position.y = GAME_BOTTOM + size;
        if (position.y + size > GAME_TOP) position.y = GAME_TOP - size;
    }

    void render() override {
        if (!active && !isRespawning) return;

        glPushMatrix();
        glTranslatef(position.x, position.y, 0);

        // 몸체 (드레스)
        glColor3f(0.2f, 0.2f, 0.2f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, -size * 0.3f);
        for (int i = 0; i <= 16; i++) {
            float angle = M_PI + i * M_PI / 16;
            glVertex2f(cos(angle) * size * 0.6f, -size * 0.3f + sin(angle) * size * 0.4f);
        }
        glEnd();

        // 상체 (앞치마)
        glColor3f(0.9f, 0.9f, 0.9f);
        glBegin(GL_QUADS);
        glVertex2f(-size * 0.3f, size * 0.1f);
        glVertex2f(size * 0.3f, size * 0.1f);
        glVertex2f(size * 0.3f, -size * 0.2f);
        glVertex2f(-size * 0.3f, -size * 0.2f);
        glEnd();

        // 팔
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(-size * 0.4f, 0, size * 0.15f);
        drawOptimizedCircle(size * 0.4f, 0, size * 0.15f);

        // 머리
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(0, size * 0.4f, size * 0.3f);

        // 머리카락 (핑크색)
        glColor3f(1.0f, 0.7f, 0.8f);
        drawOptimizedCircle(-size * 0.15f, size * 0.5f, size * 0.2f);
        drawOptimizedCircle(size * 0.15f, size * 0.5f, size * 0.2f);
        drawOptimizedCircle(0, size * 0.6f, size * 0.25f);

        // 눈
        glColor3f(0.0f, 0.0f, 0.0f);
        drawOptimizedCircle(-size * 0.1f, size * 0.45f, size * 0.05f);
        drawOptimizedCircle(size * 0.1f, size * 0.45f, size * 0.05f);

        // 눈 하이라이트
        glColor3f(1.0f, 1.0f, 1.0f);
        drawOptimizedCircle(-size * 0.08f, size * 0.47f, size * 0.02f);
        drawOptimizedCircle(size * 0.12f, size * 0.47f, size * 0.02f);

        // 헤드드레스 (리본)
        glColor3f(1.0f, 0.6f, 0.7f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-size * 0.2f, size * 0.7f);
        glVertex2f(-size * 0.05f, size * 0.8f);
        glVertex2f(-size * 0.1f, size * 0.6f);

        glVertex2f(size * 0.2f, size * 0.7f);
        glVertex2f(size * 0.05f, size * 0.8f);
        glVertex2f(size * 0.1f, size * 0.6f);
        glEnd();

        glPopMatrix();
    }

    void takeDamage() {
        if (!active || isRespawning) return;

        lives--;
        addCameraShake(0.5f); // 피격 시 화면 흔들림
        if (lives > 0) {
            active = false;
            isRespawning = true;
            respawnTimer = RESPAWN_TIME;
        }
    }
};

// 공격 클래스
class Attack : public GameObject {
public:
    float rotation;

    Attack(Vec2 pos) : GameObject(pos, ATTACK_SIZE), rotation(0) {
        velocity = Vec2(0, 2.0f); // 위쪽으로 빠르게 이동
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);
        rotation += deltaTime * 10.0f; // 회전 효과

        // 화면을 벗어나면 비활성화
        if (position.y > GAME_TOP + size) {
            active = false;
        }
    }

    void render() override {
        if (!active) return;

        // 마법 공격 같은 별 모양
        glColor3f(0.3f, 0.7f, 1.0f);
        glPushMatrix();
        glTranslatef(position.x, position.y, 0);
        glRotatef(rotation * 180.0f / M_PI, 0, 0, 1);

        glBegin(GL_TRIANGLES);
        // 별 모양 (5개 삼각형)
        for (int i = 0; i < 5; i++) {
            float angle1 = i * 2.0f * M_PI / 5;
            float angle2 = (i + 0.5f) * 2.0f * M_PI / 5;
            float angle3 = (i + 1) * 2.0f * M_PI / 5;

            glVertex2f(0, 0);
            glVertex2f(cos(angle1) * size, sin(angle1) * size);
            glVertex2f(cos(angle2) * size * 0.4f, sin(angle2) * size * 0.4f);

            glVertex2f(0, 0);
            glVertex2f(cos(angle2) * size * 0.4f, sin(angle2) * size * 0.4f);
            glVertex2f(cos(angle3) * size, sin(angle3) * size);
        }
        glEnd();

        // 중심 원
        glColor3f(1.0f, 1.0f, 1.0f);
        drawOptimizedCircle(0, 0, size * 0.3f);

        glPopMatrix();
    }
};

// 총알 클래스 (개선된 모양)
class Bullet : public GameObject {
public:
    float rotation;
    Vec2 direction;

    Bullet(Vec2 pos, Vec2 vel) : GameObject(pos, BULLET_SIZE), rotation(0) {
        velocity = vel;
        direction = vel.normalized();
        // 속도 방향으로 회전 설정
        rotation = atan2(direction.y, direction.x);
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);

        // 화면을 벗어나면 비활성화
        if (position.x < GAME_LEFT - size || position.x > GAME_RIGHT + size ||
            position.y < GAME_BOTTOM - size || position.y > GAME_TOP + size) {
            active = false;
        }
    }

    void render() override {
        if (!active) return;

        // 더 총알다운 빨간색 타원형
        glColor3f(1.0f, 0.3f, 0.3f);
        drawOptimizedBullet(position.x, position.y, size, rotation);

        // 중심 하이라이트
        glColor3f(1.0f, 0.8f, 0.8f);
        drawOptimizedCircle(position.x, position.y, size * 0.4f);
    }
};

// 적 클래스 (팬케이크 위의 캐릭터)
class Enemy : public GameObject {
public:
    float health;
    float shootTimer;
    float moveTimer;
    float animTimer;

    Enemy() : GameObject(Vec2(0, 0.6f), ENEMY_SIZE),
        health(ENEMY_HEALTH), shootTimer(0), moveTimer(0), animTimer(0) {
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);
        animTimer += deltaTime * 2.0f;

        // 간단한 이동 패턴
        moveTimer += deltaTime;
        velocity.x = sin(moveTimer * 0.8f) * 0.4f;

        // 경계 체크
        if (position.x - size < GAME_LEFT) {
            position.x = GAME_LEFT + size;
            velocity.x = abs(velocity.x);
        }
        if (position.x + size > GAME_RIGHT) {
            position.x = GAME_RIGHT - size;
            velocity.x = -abs(velocity.x);
        }

        shootTimer += deltaTime;
    }

    void render() override {
        if (!active) return;

        glPushMatrix();
        glTranslatef(position.x, position.y, 0);

        // 팬케이크 바닥 (플레이트)
        glColor3f(0.8f, 0.8f, 0.9f);
        drawOptimizedCircle(0, -size * 0.8f, size * 0.9f);

        // 팬케이크
        glColor3f(1.0f, 0.85f, 0.4f);
        drawOptimizedCircle(0, -size * 0.6f, size * 0.7f);

        // 팬케이크 윗면 하이라이트
        glColor3f(1.0f, 0.9f, 0.6f);
        drawOptimizedCircle(-size * 0.2f, -size * 0.55f, size * 0.15f);

        // 캐릭터 몸체
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(0, -size * 0.2f, size * 0.25f);

        // 머리
        drawOptimizedCircle(0, size * 0.1f, size * 0.3f);

        // 머리카락 (갈색)
        glColor3f(0.8f, 0.6f, 0.4f);
        drawOptimizedCircle(-size * 0.2f, size * 0.2f, size * 0.2f);
        drawOptimizedCircle(size * 0.2f, size * 0.2f, size * 0.2f);
        drawOptimizedCircle(0, size * 0.3f, size * 0.25f);

        // 눈 (졸린 표정)
        glColor3f(0.0f, 0.0f, 0.0f);
        glBegin(GL_QUADS);
        glVertex2f(-size * 0.15f, size * 0.12f);
        glVertex2f(-size * 0.05f, size * 0.12f);
        glVertex2f(-size * 0.05f, size * 0.08f);
        glVertex2f(-size * 0.15f, size * 0.08f);

        glVertex2f(size * 0.15f, size * 0.12f);
        glVertex2f(size * 0.05f, size * 0.12f);
        glVertex2f(size * 0.05f, size * 0.08f);
        glVertex2f(size * 0.15f, size * 0.08f);
        glEnd();

        // 입 (작은 점)
        glColor3f(0.8f, 0.4f, 0.4f);
        drawOptimizedCircle(0, size * 0.02f, size * 0.03f);

        // 팔 (작게)
        glColor3f(1.0f, 0.9f, 0.8f);
        float armBob = sin(animTimer) * 0.05f;
        drawOptimizedCircle(-size * 0.35f, -size * 0.1f + armBob, size * 0.12f);
        drawOptimizedCircle(size * 0.35f, -size * 0.1f - armBob, size * 0.12f);

        glPopMatrix();
    }

    void takeDamage(float damage) {
        health -= damage;
        addCameraShake(0.3f); // 적 피격 시에도 화면 흔들림
        if (health <= 0) {
            active = false;
            addCameraShake(1.0f); // 적 파괴 시 강한 화면 흔들림
        }
    }

    bool shouldShoot() {
        if (shootTimer >= 0.4f) { // 더 빠른 발사
            shootTimer = 0;
            return true;
        }
        return false;
    }
};

// 게임 클래스
class Game {
public:
    Player player;
    Enemy enemy;
    std::vector<Attack> attacks;
    std::vector<Bullet> bullets;
    bool gameOver;
    bool gameWon;

    Game() : gameOver(false), gameWon(false) {}

    void update(float deltaTime) {
        gameTime += deltaTime;
        updateCamera(deltaTime);

        if (gameOver || gameWon) return;

        // 플레이어 업데이트
        player.update(deltaTime);

        // 적 업데이트
        enemy.update(deltaTime);

        // 적이 총알 발사 (다양한 패턴)
        if (enemy.active && enemy.shouldShoot()) {
            // 플레이어 방향으로 발사
            Vec2 toPlayer = (player.position - enemy.position).normalized();
            bullets.push_back(Bullet(enemy.position, toPlayer * 1.2f));

            // 추가 총알들 (부채꼴 패턴)
            for (int i = -1; i <= 1; i++) {
                if (i == 0) continue;
                float angle = atan2(toPlayer.y, toPlayer.x) + i * 0.3f;
                Vec2 dir(cos(angle), sin(angle));
                bullets.push_back(Bullet(enemy.position, dir * 1.0f));
            }
        }

        // 공격 업데이트
        for (auto& attack : attacks) {
            attack.update(deltaTime);
        }
        attacks.erase(std::remove_if(attacks.begin(), attacks.end(),
            [](const Attack& a) { return !a.active; }), attacks.end());

        // 총알 업데이트
        for (auto& bullet : bullets) {
            bullet.update(deltaTime);
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }), bullets.end());

        // 충돌 체크: 공격 vs 적
        for (auto& attack : attacks) {
            if (attack.active && enemy.active && attack.checkCollision(enemy)) {
                enemy.takeDamage(ATTACK_DAMAGE);
                attack.active = false;
            }
        }

        // 충돌 체크: 총알 vs 플레이어
        for (auto& bullet : bullets) {
            if (bullet.active && player.active && bullet.checkCollision(player)) {
                player.takeDamage();
                bullet.active = false;
            }
        }

        // 게임 종료 조건 체크
        if (player.lives <= 0) {
            gameOver = true;
        }
        if (!enemy.active) {
            gameWon = true;
        }
    }

    void render() {
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        applyCameraTransform();

        // 게임 오브젝트 렌더링
        player.render();
        enemy.render();

        for (auto& attack : attacks) {
            attack.render();
        }

        for (auto& bullet : bullets) {
            bullet.render();
        }

        // UI 렌더링 (카메라 변환 적용 안 함)
        glLoadIdentity();

        // 생명 표시
        for (int i = 0; i < player.lives; i++) {
            glColor3f(0.0f, 1.0f, 0.0f);
            drawOptimizedCircle(-0.9f + i * 0.1f, 0.9f, 0.03f);
        }

        // 적 체력 바
        if (enemy.active) {
            float healthRatio = enemy.health / ENEMY_HEALTH;

            // 체력바 배경
            glColor3f(0.3f, 0.3f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(-0.5f, 0.85f);
            glVertex2f(0.5f, 0.85f);
            glVertex2f(0.5f, 0.8f);
            glVertex2f(-0.5f, 0.8f);
            glEnd();

            // 체력바
            glColor3f(1.0f - healthRatio, healthRatio, 0.0f);
            glBegin(GL_QUADS);
            glVertex2f(-0.5f, 0.85f);
            glVertex2f(-0.5f + healthRatio, 0.85f);
            glVertex2f(-0.5f + healthRatio, 0.8f);
            glVertex2f(-0.5f, 0.8f);
            glEnd();
        }

        // 게임 오버/승리 메시지
        if (gameOver) {
            glColor3f(1.0f, 0.0f, 0.0f);
            glBegin(GL_QUADS);
            glVertex2f(-0.6f, -0.1f);
            glVertex2f(0.6f, -0.1f);
            glVertex2f(0.6f, 0.1f);
            glVertex2f(-0.6f, 0.1f);
            glEnd();
        }
        else if (gameWon) {
            glColor3f(0.0f, 1.0f, 0.0f);
            glBegin(GL_QUADS);
            glVertex2f(-0.6f, -0.1f);
            glVertex2f(0.6f, -0.1f);
            glVertex2f(0.6f, 0.1f);
            glVertex2f(-0.6f, 0.1f);
            glEnd();
        }

        glutSwapBuffers();
    }

    void shootAttack() {
        if (player.active && !player.isRespawning) {
            attacks.push_back(Attack(player.position));
        }
    }

    void handleInput() {
        float moveSpeed = 1.2f;

        // 플레이어 이동 처리
        if (keys['w'] || keys['W'] || specialKeys[GLUT_KEY_UP]) {
            player.velocity.y = moveSpeed;
        }
        else if (keys['s'] || keys['S'] || specialKeys[GLUT_KEY_DOWN]) {
            player.velocity.y = -moveSpeed;
        }
        else {
            player.velocity.y = 0.0f;
        }

        if (keys['a'] || keys['A'] || specialKeys[GLUT_KEY_LEFT]) {
            player.velocity.x = -moveSpeed;
        }
        else if (keys['d'] || keys['D'] || specialKeys[GLUT_KEY_RIGHT]) {
            player.velocity.x = moveSpeed;
        }
        else {
            player.velocity.x = 0.0f;
        }
    }
};

Game game;

// GLUT 콜백 함수들
void display() {
    game.render();
}

void timer(int value) {
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    game.handleInput();
    game.update(deltaTime);

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); // 60 FPS (약 16ms)
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;

    if (key == ' ') { // 스페이스바
        game.shootAttack();
    }
    if (key == 'r' || key == 'R') {
        if (game.gameOver || game.gameWon) {
            game = Game(); // 게임 재시작
            lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
            gameTime = 0;
            cameraShake = 0;
            cameraOffset = Vec2(0, 0);
        }
    }
    if (key == 27) { // ESC 키
        glutLeaveMainLoop();
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    keys[key] = false;
}

void specialKeyboard(int key, int x, int y) {
    specialKeys[key] = true;
}

void specialKeyboardUp(int key, int x, int y) {
    specialKeys[key] = false;
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void cleanup() {
    if (vbosInitialized) {
        glDeleteBuffers(1, &circleVBO);
        glDeleteBuffers(1, &bulletVBO);
    }
}

int main(int argc, char** argv) {
    // GLUT 초기화
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Bullet Hell Shooter - Enhanced");

    // GLEW 초기화
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW 초기화 실패" << std::endl;
        return -1;
    }

    // OpenGL 설정
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // 약간 어두운 배경
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // VBO 초기화
    initVBOs();

    // GLUT 콜백 등록
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyboard);
    glutSpecialUpFunc(specialKeyboardUp);
    glutCloseFunc(cleanup);

    // 타이머 초기화
    lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glutTimerFunc(16, timer, 0);

    // 메인 루프 시작
    glutMainLoop();

    return 0;
}