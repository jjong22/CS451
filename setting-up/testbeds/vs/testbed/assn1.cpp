#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
# define M_PI 	   3.14159265358979323846  /* pi */

// ������ ũ��
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// ���� ���
const float GAME_LEFT = -1.0f;
const float GAME_RIGHT = 1.0f;
const float GAME_BOTTOM = -1.0f;
const float GAME_TOP = 1.0f;

// ��ƼƼ ũ��
const float PLAYER_SIZE = 0.08f;
const float BULLET_SIZE = 0.015f;
const float ATTACK_SIZE = 0.025f;
const float ENEMY_SIZE = 0.12f;

// ���� ����
const int PLAYER_LIVES = 3;
const float ENEMY_HEALTH = 100.0f;
const float ATTACK_DAMAGE = 10.0f;
const float RESPAWN_TIME = 2.0f;

// Ű ���� ����
bool keys[256] = { false };
bool specialKeys[256] = { false };

// Ÿ�̸� ����
float lastTime = 0;
float gameTime = 0;

// VBO ����ȭ�� ���� ����
GLuint circleVBO = 0;
GLuint bulletVBO = 0;
bool vbosInitialized = false;
const int CIRCLE_SEGMENTS = 32;

// ���� ����ü
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

// ī�޶� �ִϸ��̼��� ���� ����
float cameraShake = 0.0f;
float cameraShakeDecay = 5.0f;
Vec2 cameraOffset(0, 0);

// VBO �ʱ�ȭ �Լ�
void initVBOs() {
    if (vbosInitialized) return;

    // ���� VBO �ʱ�ȭ
    std::vector<float> circleVertices;
    circleVertices.push_back(0.0f); // �߽���
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

    // �Ѿ� ��� VBO �ʱ�ȭ (Ÿ����)
    std::vector<float> bulletVertices;
    bulletVertices.push_back(0.0f); // �߽���
    bulletVertices.push_back(0.0f);

    for (int i = 0; i <= 16; i++) {
        float angle = i * 2.0f * M_PI / 16;
        bulletVertices.push_back(cos(angle) * 0.6f); // x�� ����
        bulletVertices.push_back(sin(angle) * 1.2f); // y�� Ȯ��
    }

    glGenBuffers(1, &bulletVBO);
    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, bulletVertices.size() * sizeof(float),
        bulletVertices.data(), GL_STATIC_DRAW);

    vbosInitialized = true;
}

// ����ȭ�� �� �׸��� �Լ�
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

// ����ȭ�� �Ѿ� �׸��� �Լ�
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

// ī�޶� ��鸲 ȿ��
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

// ���� ������Ʈ �⺻ Ŭ����
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
        return distance < (size + other.size) * 0.8f; // �ణ �� ������ �浹 ����
    }
};

// �÷��̾� Ŭ���� (���̵� ĳ���� ��Ƽ��)
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

        // ��� üũ
        if (position.x - size < GAME_LEFT) position.x = GAME_LEFT + size;
        if (position.x + size > GAME_RIGHT) position.x = GAME_RIGHT - size;
        if (position.y - size < GAME_BOTTOM) position.y = GAME_BOTTOM + size;
        if (position.y + size > GAME_TOP) position.y = GAME_TOP - size;
    }

    void render() override {
        if (!active && !isRespawning) return;

        glPushMatrix();
        glTranslatef(position.x, position.y, 0);

        // ��ü (�巹��)
        glColor3f(0.2f, 0.2f, 0.2f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, -size * 0.3f);
        for (int i = 0; i <= 16; i++) {
            float angle = M_PI + i * M_PI / 16;
            glVertex2f(cos(angle) * size * 0.6f, -size * 0.3f + sin(angle) * size * 0.4f);
        }
        glEnd();

        // ��ü (��ġ��)
        glColor3f(0.9f, 0.9f, 0.9f);
        glBegin(GL_QUADS);
        glVertex2f(-size * 0.3f, size * 0.1f);
        glVertex2f(size * 0.3f, size * 0.1f);
        glVertex2f(size * 0.3f, -size * 0.2f);
        glVertex2f(-size * 0.3f, -size * 0.2f);
        glEnd();

        // ��
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(-size * 0.4f, 0, size * 0.15f);
        drawOptimizedCircle(size * 0.4f, 0, size * 0.15f);

        // �Ӹ�
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(0, size * 0.4f, size * 0.3f);

        // �Ӹ�ī�� (��ũ��)
        glColor3f(1.0f, 0.7f, 0.8f);
        drawOptimizedCircle(-size * 0.15f, size * 0.5f, size * 0.2f);
        drawOptimizedCircle(size * 0.15f, size * 0.5f, size * 0.2f);
        drawOptimizedCircle(0, size * 0.6f, size * 0.25f);

        // ��
        glColor3f(0.0f, 0.0f, 0.0f);
        drawOptimizedCircle(-size * 0.1f, size * 0.45f, size * 0.05f);
        drawOptimizedCircle(size * 0.1f, size * 0.45f, size * 0.05f);

        // �� ���̶���Ʈ
        glColor3f(1.0f, 1.0f, 1.0f);
        drawOptimizedCircle(-size * 0.08f, size * 0.47f, size * 0.02f);
        drawOptimizedCircle(size * 0.12f, size * 0.47f, size * 0.02f);

        // ���巹�� (����)
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
        addCameraShake(0.5f); // �ǰ� �� ȭ�� ��鸲
        if (lives > 0) {
            active = false;
            isRespawning = true;
            respawnTimer = RESPAWN_TIME;
        }
    }
};

// ���� Ŭ����
class Attack : public GameObject {
public:
    float rotation;

    Attack(Vec2 pos) : GameObject(pos, ATTACK_SIZE), rotation(0) {
        velocity = Vec2(0, 2.0f); // �������� ������ �̵�
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);
        rotation += deltaTime * 10.0f; // ȸ�� ȿ��

        // ȭ���� ����� ��Ȱ��ȭ
        if (position.y > GAME_TOP + size) {
            active = false;
        }
    }

    void render() override {
        if (!active) return;

        // ���� ���� ���� �� ���
        glColor3f(0.3f, 0.7f, 1.0f);
        glPushMatrix();
        glTranslatef(position.x, position.y, 0);
        glRotatef(rotation * 180.0f / M_PI, 0, 0, 1);

        glBegin(GL_TRIANGLES);
        // �� ��� (5�� �ﰢ��)
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

        // �߽� ��
        glColor3f(1.0f, 1.0f, 1.0f);
        drawOptimizedCircle(0, 0, size * 0.3f);

        glPopMatrix();
    }
};

// �Ѿ� Ŭ���� (������ ���)
class Bullet : public GameObject {
public:
    float rotation;
    Vec2 direction;

    Bullet(Vec2 pos, Vec2 vel) : GameObject(pos, BULLET_SIZE), rotation(0) {
        velocity = vel;
        direction = vel.normalized();
        // �ӵ� �������� ȸ�� ����
        rotation = atan2(direction.y, direction.x);
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);

        // ȭ���� ����� ��Ȱ��ȭ
        if (position.x < GAME_LEFT - size || position.x > GAME_RIGHT + size ||
            position.y < GAME_BOTTOM - size || position.y > GAME_TOP + size) {
            active = false;
        }
    }

    void render() override {
        if (!active) return;

        // �� �Ѿ˴ٿ� ������ Ÿ����
        glColor3f(1.0f, 0.3f, 0.3f);
        drawOptimizedBullet(position.x, position.y, size, rotation);

        // �߽� ���̶���Ʈ
        glColor3f(1.0f, 0.8f, 0.8f);
        drawOptimizedCircle(position.x, position.y, size * 0.4f);
    }
};

// �� Ŭ���� (������ũ ���� ĳ����)
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

        // ������ �̵� ����
        moveTimer += deltaTime;
        velocity.x = sin(moveTimer * 0.8f) * 0.4f;

        // ��� üũ
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

        // ������ũ �ٴ� (�÷���Ʈ)
        glColor3f(0.8f, 0.8f, 0.9f);
        drawOptimizedCircle(0, -size * 0.8f, size * 0.9f);

        // ������ũ
        glColor3f(1.0f, 0.85f, 0.4f);
        drawOptimizedCircle(0, -size * 0.6f, size * 0.7f);

        // ������ũ ���� ���̶���Ʈ
        glColor3f(1.0f, 0.9f, 0.6f);
        drawOptimizedCircle(-size * 0.2f, -size * 0.55f, size * 0.15f);

        // ĳ���� ��ü
        glColor3f(1.0f, 0.9f, 0.8f);
        drawOptimizedCircle(0, -size * 0.2f, size * 0.25f);

        // �Ӹ�
        drawOptimizedCircle(0, size * 0.1f, size * 0.3f);

        // �Ӹ�ī�� (����)
        glColor3f(0.8f, 0.6f, 0.4f);
        drawOptimizedCircle(-size * 0.2f, size * 0.2f, size * 0.2f);
        drawOptimizedCircle(size * 0.2f, size * 0.2f, size * 0.2f);
        drawOptimizedCircle(0, size * 0.3f, size * 0.25f);

        // �� (���� ǥ��)
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

        // �� (���� ��)
        glColor3f(0.8f, 0.4f, 0.4f);
        drawOptimizedCircle(0, size * 0.02f, size * 0.03f);

        // �� (�۰�)
        glColor3f(1.0f, 0.9f, 0.8f);
        float armBob = sin(animTimer) * 0.05f;
        drawOptimizedCircle(-size * 0.35f, -size * 0.1f + armBob, size * 0.12f);
        drawOptimizedCircle(size * 0.35f, -size * 0.1f - armBob, size * 0.12f);

        glPopMatrix();
    }

    void takeDamage(float damage) {
        health -= damage;
        addCameraShake(0.3f); // �� �ǰ� �ÿ��� ȭ�� ��鸲
        if (health <= 0) {
            active = false;
            addCameraShake(1.0f); // �� �ı� �� ���� ȭ�� ��鸲
        }
    }

    bool shouldShoot() {
        if (shootTimer >= 0.4f) { // �� ���� �߻�
            shootTimer = 0;
            return true;
        }
        return false;
    }
};

// ���� Ŭ����
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

        // �÷��̾� ������Ʈ
        player.update(deltaTime);

        // �� ������Ʈ
        enemy.update(deltaTime);

        // ���� �Ѿ� �߻� (�پ��� ����)
        if (enemy.active && enemy.shouldShoot()) {
            // �÷��̾� �������� �߻�
            Vec2 toPlayer = (player.position - enemy.position).normalized();
            bullets.push_back(Bullet(enemy.position, toPlayer * 1.2f));

            // �߰� �Ѿ˵� (��ä�� ����)
            for (int i = -1; i <= 1; i++) {
                if (i == 0) continue;
                float angle = atan2(toPlayer.y, toPlayer.x) + i * 0.3f;
                Vec2 dir(cos(angle), sin(angle));
                bullets.push_back(Bullet(enemy.position, dir * 1.0f));
            }
        }

        // ���� ������Ʈ
        for (auto& attack : attacks) {
            attack.update(deltaTime);
        }
        attacks.erase(std::remove_if(attacks.begin(), attacks.end(),
            [](const Attack& a) { return !a.active; }), attacks.end());

        // �Ѿ� ������Ʈ
        for (auto& bullet : bullets) {
            bullet.update(deltaTime);
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }), bullets.end());

        // �浹 üũ: ���� vs ��
        for (auto& attack : attacks) {
            if (attack.active && enemy.active && attack.checkCollision(enemy)) {
                enemy.takeDamage(ATTACK_DAMAGE);
                attack.active = false;
            }
        }

        // �浹 üũ: �Ѿ� vs �÷��̾�
        for (auto& bullet : bullets) {
            if (bullet.active && player.active && bullet.checkCollision(player)) {
                player.takeDamage();
                bullet.active = false;
            }
        }

        // ���� ���� ���� üũ
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

        // ���� ������Ʈ ������
        player.render();
        enemy.render();

        for (auto& attack : attacks) {
            attack.render();
        }

        for (auto& bullet : bullets) {
            bullet.render();
        }

        // UI ������ (ī�޶� ��ȯ ���� �� ��)
        glLoadIdentity();

        // ���� ǥ��
        for (int i = 0; i < player.lives; i++) {
            glColor3f(0.0f, 1.0f, 0.0f);
            drawOptimizedCircle(-0.9f + i * 0.1f, 0.9f, 0.03f);
        }

        // �� ü�� ��
        if (enemy.active) {
            float healthRatio = enemy.health / ENEMY_HEALTH;

            // ü�¹� ���
            glColor3f(0.3f, 0.3f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(-0.5f, 0.85f);
            glVertex2f(0.5f, 0.85f);
            glVertex2f(0.5f, 0.8f);
            glVertex2f(-0.5f, 0.8f);
            glEnd();

            // ü�¹�
            glColor3f(1.0f - healthRatio, healthRatio, 0.0f);
            glBegin(GL_QUADS);
            glVertex2f(-0.5f, 0.85f);
            glVertex2f(-0.5f + healthRatio, 0.85f);
            glVertex2f(-0.5f + healthRatio, 0.8f);
            glVertex2f(-0.5f, 0.8f);
            glEnd();
        }

        // ���� ����/�¸� �޽���
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

        // �÷��̾� �̵� ó��
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

// GLUT �ݹ� �Լ���
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
    glutTimerFunc(16, timer, 0); // 60 FPS (�� 16ms)
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;

    if (key == ' ') { // �����̽���
        game.shootAttack();
    }
    if (key == 'r' || key == 'R') {
        if (game.gameOver || game.gameWon) {
            game = Game(); // ���� �����
            lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
            gameTime = 0;
            cameraShake = 0;
            cameraOffset = Vec2(0, 0);
        }
    }
    if (key == 27) { // ESC Ű
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
    // GLUT �ʱ�ȭ
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Bullet Hell Shooter - Enhanced");

    // GLEW �ʱ�ȭ
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW �ʱ�ȭ ����" << std::endl;
        return -1;
    }

    // OpenGL ����
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // �ణ ��ο� ���
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // VBO �ʱ�ȭ
    initVBOs();

    // GLUT �ݹ� ���
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyboard);
    glutSpecialUpFunc(specialKeyboardUp);
    glutCloseFunc(cleanup);

    // Ÿ�̸� �ʱ�ȭ
    lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    glutTimerFunc(16, timer, 0);

    // ���� ���� ����
    glutMainLoop();

    return 0;
}