#include <GL/glew.h>
#include <GL/freeglut.h>
// #include <GL/glut.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define M_PI 3.14159265358979323846

// Vector struct (3D)
struct Vec3 {
    float x, y, z;

    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }

    float length() const {
        return sqrt(x * x + y * y + z * z);
    }

    Vec3 normalize() const {
        float len = length();
        if (len > 0) {
            return Vec3(x / len, y / len, z / len);
        }
        return Vec3(0, 0, 0);
    }

    glm::vec3 toGLM() const {
        return glm::vec3(x, y, z);
    }
};

class Player;
extern Player player;

glm::vec3 globalViewPos(0, 0, 8); // TOP_PERSPECTIVE 기본값
glm::mat4 computeShadowMatrix(const glm::vec3& lightDir, float floorY);
glm::mat4 computeShadowMatrix(const glm::vec3& lightDir);

// Window size
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// Game boundaries (3D)
const float GAME_LEFT = -2.0f;
const float GAME_RIGHT = 2.0f;
const float GAME_BOTTOM = -2.0f;
const float GAME_TOP = 2.0f;
const float GAME_NEAR = -2.0f;
const float GAME_FAR = 2.0f;

// Entity sizes
const float PLAYER_SIZE = 0.30f;
const float BULLET_SIZE = 0.08f;
const float ATTACK_SIZE = 0.10f;
const float ENEMY_SIZE = 0.80f;
const float ORBIT_SIZE = 0.10f;

// Game constants
const int PLAYER_LIVES = 3;
const float ENEMY_HEALTH = 100.0f;
const float ATTACK_DAMAGE = 10.0f;
const float RESPAWN_TIME = 2.0f;

// Point light orbit parameters
float pointLightOrbitSpeed = 2.0f;
float pointLightOrbitRadius = 3.0f;
float pointLightVerticalSpeed = 0.5f;

// Key states
bool keys[256] = { false };
bool specialKeys[256] = { false };

// Timing
float lastTime = 0;
float gameTime = 0;

// Camera modes
enum CameraMode {
    TOP_PERSPECTIVE,
    THIRD_PERSON
};

// Render modes
enum RenderMode {
    OPAQUE_POLYGON,
    WIREFRAME,
    HIDDEN_LINE_WIREFRAME
};

// Shading modes
enum ShadingMode {
    GOURAUD,
    PHONG,
    PHONG_NORMAL_MAP
};

ShadingMode currentShading = GOURAUD;

struct DirectionalLight {
    Vec3 direction;
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;

    DirectionalLight()
        : direction(0.5f, -1.0f, 0.8f),
        ambient(0.2f, 0.2f, 0.2f),
        diffuse(0.7f, 0.7f, 0.7f),
        specular(0.5f, 0.5f, 0.5f) {
    }
};

// 최대 N개의 point light 지원
const int MAX_POINT_LIGHTS = 4;

struct PointLight {
    Vec3 position;
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
    float constant;
    float linear;
    float quadratic;
    bool enabled;

    PointLight()
        : position(0, 0, 0),
        ambient(0.1f, 0.1f, 0.1f),
        diffuse(0.8f, 0.8f, 0.6f),
        specular(1.0f, 1.0f, 0.8f),
        constant(1.0f),
        linear(0.09f),
        quadratic(0.032f),
        enabled(true)
    {
    }
};

// === Planar Shadow (Additional Goal) ===
float floorY = 0.0f;
float floorZ = -1.0f;  // z = floorZ 에 고정된 평면 (XY 평면)
glm::vec3 shadowColor(0.0f, 0.0f, 0.0f);
float shadowAlpha = 0.5f;
// Dynamic ground plane follows player movement direction
glm::vec3 groundPlaneNormal(0.0f, 1.0f, 0.0f);  // Dynamic normal
float groundPlaneDistance = 0.0f;                // Distance from origin

DirectionalLight dirLight;
PointLight pointLights[MAX_POINT_LIGHTS];
int numActivePointLights = 0;

CameraMode currentCamera = TOP_PERSPECTIVE;
RenderMode currentRender = OPAQUE_POLYGON;

// ADDITIONAL GOALS toggles
bool smoothShadingEnabled = true;  // Goal 1: Smooth shading vs flat shading
bool glowEffectEnabled = true;      // Goal 2: Glow effect for projectiles
bool trailEffectEnabled = true;     // Goal 3: Motion trails
bool damageVisualizationEnabled = true; // Goal 4: Health-based color visualization

// Shader program IDs
GLuint shaderProgram;
GLuint wireframeShaderProgram;
GLuint hiddenLineShaderProgram;
GLuint glowShaderProgram;  // Additional: Glow effect shader
GLuint trailShaderProgram; // Additional: Trail effect shader

GLuint shadowShaderProgram;
GLint uMVP_shadow;
GLint uShadowColor;
GLint uShadowAlpha;

// Shader uniform locations
GLint uMVP, uColor, uModelMatrix, uTime, uSmoothShading;
GLint uMVP_wire, uColor_wire;
GLint uMVP_hidden, uColor_hidden, uModelMatrix_hidden;
GLint uMVP_glow, uColor_glow, uGlowIntensity;
GLint uMVP_trail, uColor_trail, uAlpha_trail;

// Texture IDs (Assignment 4)
GLuint defaultTexture;      // White texture for models without texture
GLuint defaultNormalMap;    // Default normal map (flat)

// Shading-specific shader programs
GLuint gouraudShaderProgram;
GLuint phongShaderProgram;
GLuint phongNormalMapShaderProgram;

// Uniform locations for shading
GLint uMVP_shading, uModelMatrix_shading, uNormalMatrix_shading;
GLint uViewPos_shading, uObjectColor_shading;
GLint uDirLight_direction, uDirLight_ambient, uDirLight_diffuse, uDirLight_specular;
GLint uPointLight_position, uPointLight_ambient, uPointLight_diffuse, uPointLight_specular;
GLint uPointLight_constant, uPointLight_linear, uPointLight_quadratic;
GLint uTexture_shading, uNormalMap_shading, uHasNormalMap;

// ADDITIONAL GOAL 3: Trail system
struct TrailPoint {
    Vec3 position;
    float life;
    float maxLife;
    Vec3 color;

    TrailPoint(Vec3 pos, Vec3 col, float lifetime = 0.5f)
        : position(pos), life(lifetime), maxLife(lifetime), color(col) {
    }
};

std::vector<TrailPoint> trailPoints;

GLuint loadTexture(const std::string& filepath);    // <-- 추가
GLuint loadNormalMap(const std::string& filepath);  // <-- 추가

// OBJ Model class with VAO/VBO and normal calculation
class OBJModel {
public:
    struct Vertex {
        float x, y, z;
    };

    struct Face {
        int v1, v2, v3;
        int vt1, vt2, vt3;
        int vn1, vn2, vn3;

        Face() : v1(0), v2(0), v3(0),
            vt1(-1), vt2(-1), vt3(-1),
            vn1(-1), vn2(-1), vn3(-1) {
        }
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<Vertex> normals;  // ADDITIONAL GOAL 1: For smooth shading

    // OpenGL buffers
    GLuint VAO, VBO, EBO, normalVBO;
    std::vector<float> vertexData;
    std::vector<float> normalData;
    std::vector<unsigned int> indices;

    std::vector<float> texCoords;  // UV coordinates
    GLuint texCoordVBO;

    GLuint diffuseTexture;
    GLuint normalMapTexture;
    bool hasNormalMap;
    bool hasTexCoords;

    OBJModel() : VAO(0), VBO(0), EBO(0), normalVBO(0), texCoordVBO(0),
        diffuseTexture(0), normalMapTexture(0), hasNormalMap(false),
        hasTexCoords(false) {
    }

    bool loadOBJ(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "Failed to open " << filename << std::endl;
            return false;
        }

        // 임시 저장소
        std::vector<Vertex> tempVertices;
        std::vector<Vec3> tempNormals;
        std::vector<std::pair<float, float>> tempTexCoords;

        // 1. 기존 데이터 깨끗이 비우기 (중복 방지)
        vertices.clear();
        faces.clear();
        normals.clear();
        indices.clear();

        // 버퍼용 데이터도 초기화
        vertexData.clear();
        normalData.clear();
        texCoords.clear(); // float vector

        std::string line;
        while (std::getline(file, line)) {
            if (line.substr(0, 2) == "v ") {
                std::istringstream iss(line.substr(2));
                Vertex vertex;
                iss >> vertex.x >> vertex.y >> vertex.z;
                tempVertices.push_back(vertex);
            }
            else if (line.substr(0, 3) == "vt ") {
                std::istringstream iss(line.substr(3));
                float u, v;
                iss >> u >> v;
                tempTexCoords.push_back({ u, v });
            }
            else if (line.substr(0, 3) == "vn ") {
                std::istringstream iss(line.substr(3));
                Vec3 normal;
                iss >> normal.x >> normal.y >> normal.z;
                tempNormals.push_back(normal);
            }
            else if (line.substr(0, 2) == "f ") {
                std::string lineData = line.substr(2);
                std::replace(lineData.begin(), lineData.end(), '\t', ' ');

                std::istringstream iss(lineData);
                std::vector<std::string> faceVerts;
                std::string vertStr;
                while (iss >> vertStr) {
                    faceVerts.push_back(vertStr);
                }

                // 인덱스 보정 (음수 인덱스 지원)
                auto fixIndex = [](int idx, int size) -> int {
                    if (idx > 0) return idx - 1;
                    if (idx < 0) return size + idx;
                    return -1;
                    };

                // Triangulation (사각형 -> 삼각형 분할)
                for (size_t i = 0; i < faceVerts.size() - 2; ++i) {
                    std::string vStr[3] = { faceVerts[0], faceVerts[i + 1], faceVerts[i + 2] };

                    // face 정보는 나중에 쓰지 않더라도 파싱 로직 유지를 위해 남겨둠
                    Face face;
                    int vIdx[3], vts[3] = { -1,-1,-1 }, vns[3] = { -1,-1,-1 };

                    for (int j = 0; j < 3; ++j) {
                        size_t firstSlash = vStr[j].find('/');
                        size_t secondSlash = (firstSlash != std::string::npos) ? vStr[j].find('/', firstSlash + 1) : std::string::npos;

                        int rawV = std::stoi(vStr[j]);
                        vIdx[j] = fixIndex(rawV, (int)tempVertices.size());

                        if (firstSlash != std::string::npos) {
                            if (secondSlash == std::string::npos || secondSlash > firstSlash + 1) {
                                std::string vtString = vStr[j].substr(firstSlash + 1, secondSlash - firstSlash - 1);
                                if (!vtString.empty()) vts[j] = fixIndex(std::stoi(vtString), (int)tempTexCoords.size());
                            }
                        }
                        if (secondSlash != std::string::npos) {
                            std::string vnString = vStr[j].substr(secondSlash + 1);
                            if (!vnString.empty()) vns[j] = fixIndex(std::stoi(vnString), (int)tempNormals.size());
                        }
                    }

                    // Face 구조체에 저장 (디버깅용, 실제 렌더링엔 indices 사용)
                    face.v1 = vIdx[0]; face.v2 = vIdx[1]; face.v3 = vIdx[2];
                    faces.push_back(face);

                    // === 2. 정점 중복 제거 및 인덱싱 (핵심) ===
                    // 여기서 바로 vertices와 indices를 채웁니다.
                    for (int j = 0; j < 3; ++j) {
                        // 유효성 검사
                        if (vIdx[j] < 0 || vIdx[j] >= (int)tempVertices.size()) vIdx[j] = 0;

                        // 여기서 중복 검사를 생략하고 단순화(Vertex Explosion 방지 최우선)할 수도 있으나,
                        // 텍스처/노말 매핑을 위해 중복 허용 방식으로 데이터를 평탄화(Flatten)합니다.
                        // (복잡한 Map 중복제거 대신, 각 면의 정점을 고유한 정점으로 분리)

                        Vertex v = tempVertices[vIdx[j]];
                        vertices.push_back(v);

                        // 텍스처 좌표 처리
                        if (vts[j] >= 0 && vts[j] < (int)tempTexCoords.size()) {
                            // setupBuffers에서 처리하기 위해 임시 저장 필요하지만
                            // 구조가 복잡하므로 여기서는 단순화하여 vertices와 1:1 매칭되는 TexCoords 배열을 직접 만듭니다.
                            // (class 멤버인 texCoords는 float vector이므로 나중에 채움)
                        }

                        // 노말 처리
                        if (vns[j] >= 0 && vns[j] < (int)tempNormals.size()) {
                            normals.push_back({ tempNormals[vns[j]].x, tempNormals[vns[j]].y, tempNormals[vns[j]].z });
                        }
                        else {
                            normals.push_back({ 0,0,0 }); // 나중에 계산
                        }

                        // 인덱스 생성 (단순 순차 증가)
                        indices.push_back((unsigned int)vertices.size() - 1);
                    }

                    // 텍스처 좌표는 별도로 vertices와 갯수를 맞춰줍니다.
                    for (int j = 0; j < 3; ++j) {
                        if (vts[j] >= 0 && vts[j] < (int)tempTexCoords.size()) {
                            this->texCoords.push_back(tempTexCoords[vts[j]].first);
                            this->texCoords.push_back(tempTexCoords[vts[j]].second);
                        }
                        else {
                            // 기본 UV
                            float u = 0.5f + atan2(tempVertices[vIdx[j]].z, tempVertices[vIdx[j]].x) / (2.0f * M_PI);
                            float v = 0.5f - asin(tempVertices[vIdx[j]].y) / M_PI;
                            this->texCoords.push_back(u);
                            this->texCoords.push_back(v);
                        }
                    }
                }
            }
        }
        file.close();

        hasTexCoords = !tempTexCoords.empty();
        std::cout << "Loaded " << filename << ": " << vertices.size() << " vertices." << std::endl;

        // 3. 노말이 없거나 부족하면 재계산
        if (tempNormals.empty() || normals.size() != vertices.size()) {
            calculateNormals();
        }

        // 4. GPU 버퍼 생성 및 데이터 업로드
        setupBuffers();

        return true;
    }

    void calculateNormals() {
        // vertices 개수에 맞춰 초기화
        normals.assign(vertices.size(), { 0, 0, 0 });

        // indices를 이용해 삼각형 순회
        for (size_t i = 0; i < indices.size(); i += 3) {
            unsigned int idx1 = indices[i];
            unsigned int idx2 = indices[i + 1];
            unsigned int idx3 = indices[i + 2];

            Vec3 v1(vertices[idx1].x, vertices[idx1].y, vertices[idx1].z);
            Vec3 v2(vertices[idx2].x, vertices[idx2].y, vertices[idx2].z);
            Vec3 v3(vertices[idx3].x, vertices[idx3].y, vertices[idx3].z);

            Vec3 edge1 = v2 - v1;
            Vec3 edge2 = v3 - v1;

            Vec3 normal(
                edge1.y * edge2.z - edge1.z * edge2.y,
                edge1.z * edge2.x - edge1.x * edge2.z,
                edge1.x * edge2.y - edge1.y * edge2.x
            );

            normals[idx1].x += normal.x; normals[idx1].y += normal.y; normals[idx1].z += normal.z;
            normals[idx2].x += normal.x; normals[idx2].y += normal.y; normals[idx2].z += normal.z;
            normals[idx3].x += normal.x; normals[idx3].y += normal.y; normals[idx3].z += normal.z;
        }

        // 정규화
        for (auto& n : normals) {
            float len = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len > 0) { n.x /= len; n.y /= len; n.z /= len; }
        }
    }

    // setupBuffers: GPU에 데이터를 전송하는 역할만 수행 (데이터 생성 X)
    void setupBuffers(const std::vector<std::pair<float, float>>& unused = {}) {
        // 기존 데이터를 평탄화(Flatten)
        vertexData.clear();
        normalData.clear();

        for (const auto& v : vertices) {
            vertexData.push_back(v.x);
            vertexData.push_back(v.y);
            vertexData.push_back(v.z);
        }

        for (const auto& n : normals) {
            normalData.push_back(n.x);
            normalData.push_back(n.y);
            normalData.push_back(n.z);
        }

        // texCoords는 loadOBJ에서 이미 채워져 있음 (flat array)

        if (VAO == 0) glGenVertexArrays(1, &VAO);
        if (VBO == 0) glGenBuffers(1, &VBO);
        if (normalVBO == 0) glGenBuffers(1, &normalVBO);
        if (texCoordVBO == 0) glGenBuffers(1, &texCoordVBO);
        if (EBO == 0) glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        // Position
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal
        glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
        glBufferData(GL_ARRAY_BUFFER, normalData.size() * sizeof(float), normalData.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);

        // TexCoord
        glBindBuffer(GL_ARRAY_BUFFER, texCoordVBO);
        glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(float), texCoords.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(2);

        // EBO (Indices) - 중요: 기존 indices 그대로 사용
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
        std::cout << "  -> Setup buffers complete." << std::endl;
    }

    void setTextures(const std::string& diffusePath, const std::string& normalMapPath = "") {
        // Load diffuse texture
        diffuseTexture = loadTexture(diffusePath);

        // Load normal map
        if (!normalMapPath.empty()) {
            normalMapTexture = loadNormalMap(normalMapPath);  // 전용 함수 사용
            hasNormalMap = true;
            std::cout << "  → Model has normal map enabled" << std::endl;
        }
        else {
            normalMapTexture = defaultNormalMap;
            hasNormalMap = false;
            std::cout << "  → Model using flat normal map" << std::endl;
        }
    }

    void setTextures(GLuint diffuse, GLuint normalMap = 0) {
        diffuseTexture = diffuse;
        if (normalMap != 0) {
            normalMapTexture = normalMap;
            hasNormalMap = true;
        }
        else {
            normalMapTexture = defaultNormalMap;
            hasNormalMap = false;
        }
    }


    void render() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    ~OBJModel() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (normalVBO) glDeleteBuffers(1, &normalVBO);
        if (texCoordVBO) glDeleteBuffers(1, &texCoordVBO);
        if (EBO) glDeleteBuffers(1, &EBO);
    }
};

// Global models
OBJModel jetModel, droneModel, sphereModel, starModel, donutModel, triangleModel, riceModel, himekaModel;

// Create default white texture
GLuint createDefaultTexture() {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    unsigned char whitePixel[] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return textureID;
}

// Create default normal map (pointing up in tangent space: 0.5, 0.5, 1.0 in RGB)
GLuint createDefaultNormalMap() {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    unsigned char normalPixel[] = { 128, 128, 255, 255 }; // (0.5, 0.5, 1.0) in RGB
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalPixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return textureID;
}

// Load texture from file
GLuint loadTexture(const std::string& filepath) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);  // OpenGL UV 좌표계에 맞춤
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Texture parameters for better quality
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Anisotropic filtering (if supported)
        float maxAnisotropy;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

        stbi_image_free(data);
        std::cout << "✓ Loaded texture: " << filepath
            << " (" << width << "x" << height << ", " << nrChannels << " channels)" << std::endl;
    }
    else {
        std::cout << "✗ Failed to load texture: " << filepath << std::endl;
        std::cout << "  Error: " << stbi_failure_reason() << std::endl;
        stbi_image_free(data);
        return createDefaultTexture();
    }

    return textureID;
}

GLuint loadNormalMap(const std::string& filepath) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format;
        if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;
        else
            format = GL_RGB;

        glBindTexture(GL_TEXTURE_2D, textureID);

        // Important: Use RGB format for normal maps (not sRGB)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        std::cout << "✓ Loaded normal map: " << filepath
            << " (" << width << "x" << height << ")" << std::endl;
    }
    else {
        std::cout << "✗ Failed to load normal map: " << filepath << std::endl;
        std::cout << "  Error: " << stbi_failure_reason() << std::endl;
        stbi_image_free(data);
        return createDefaultNormalMap();
    }

    return textureID;
}

// Shader compilation helper
GLuint compileShader(const char* source, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "Shader compilation failed:\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "Shader linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void initShaders() {
    // =================================================================
    // ASSIGNMENT 4: Gouraud Shading Shader
    // =================================================================
    const char* gouraudVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform mat4 uModelMatrix;
uniform mat3 uNormalMatrix;
uniform vec3 uViewPos;
uniform vec3 uObjectColor;

// Directional light
uniform vec3 uDirLight_direction;
uniform vec3 uDirLight_ambient;
uniform vec3 uDirLight_diffuse;
uniform vec3 uDirLight_specular;

const int MAX_POINT_LIGHTS = 4;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

out vec3 vertexColor;
out vec2 TexCoord;

vec3 calcDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-uDirLight_direction);
    
    // Ambient
    vec3 ambient = uDirLight_ambient * uObjectColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = uDirLight_diffuse * diff * uObjectColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = uDirLight_specular * spec;
    
    return ambient + diffuse + specular;
}

vec3 calcPointLights(vec3 fragPos, vec3 normal, vec3 viewDir) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < uNumPointLights; ++i) {
        vec3 lightDir = normalize(uPointLights[i].position - fragPos);
        float distance = length(uPointLights[i].position - fragPos);
        float attenuation = 1.0 / (uPointLights[i].constant
                                   + uPointLights[i].linear * distance
                                   + uPointLights[i].quadratic * distance * distance);

        vec3 ambient  = uPointLights[i].ambient * uObjectColor;
        float diff    = max(dot(normal, lightDir), 0.0);
        vec3 diffuse  = uPointLights[i].diffuse * diff * uObjectColor;
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec   = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        vec3 specular = uPointLights[i].specular * spec;

        sum += (ambient + diffuse + specular) * attenuation;
    }
    return sum;
}

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    
    vec3 FragPos = vec3(uModelMatrix * vec4(aPos, 1.0));
    vec3 Normal = normalize(uNormalMatrix * aNormal);
    vec3 viewDir = normalize(uViewPos - FragPos);

    // Calculate lighting in vertex shader (Gouraud)
    vec3 result = calcDirectionalLight(Normal, viewDir);
    result += calcPointLights(FragPos, Normal, viewDir);
    vertexColor = result;
    TexCoord = aTexCoord;
}
)";

    const char* gouraudFragmentSource = R"(
#version 330 core
in vec3 vertexColor;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    vec4 texColor = texture(uTexture, TexCoord);
    FragColor = vec4(vertexColor, 1.0) * texColor;
}
)";

    gouraudShaderProgram = createShaderProgram(gouraudVertexSource, gouraudFragmentSource);

    // =================================================================
    // ASSIGNMENT 4: Phong Shading Shader
    // =================================================================
    const char* phongVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform mat4 uModelMatrix;
uniform mat3 uNormalMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    FragPos = vec3(uModelMatrix * vec4(aPos, 1.0));
    Normal = uNormalMatrix * aNormal;
    TexCoord = aTexCoord;
}
)";

    const char* phongFragmentSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uObjectColor;

// Directional light
uniform vec3 uDirLight_direction;
uniform vec3 uDirLight_ambient;
uniform vec3 uDirLight_diffuse;
uniform vec3 uDirLight_specular;

const int MAX_POINT_LIGHTS = 4;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

uniform sampler2D uTexture;

vec3 calcDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-uDirLight_direction);
    
    // Ambient
    vec3 ambient = uDirLight_ambient * uObjectColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = uDirLight_diffuse * diff * uObjectColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = uDirLight_specular * spec;
    
    return ambient + diffuse + specular;
}

vec3 calcPointLights(vec3 normal, vec3 viewDir) {
    vec3 sum = vec3(0.0);

    for (int i = 0; i < uNumPointLights; ++i) {
        vec3 lightDir = normalize(uPointLights[i].position - FragPos);
        float distance = length(uPointLights[i].position - FragPos);
        float attenuation = 1.0 / (uPointLights[i].constant
                                   + uPointLights[i].linear * distance
                                   + uPointLights[i].quadratic * distance * distance);

        vec3 ambient  = uPointLights[i].ambient * uObjectColor;

        float diff    = max(dot(normal, lightDir), 0.0);
        vec3 diffuse  = uPointLights[i].diffuse * diff * uObjectColor;

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec   = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        vec3 specular = uPointLights[i].specular * spec;

        sum += (ambient + diffuse + specular) * attenuation;
    }

    return sum;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    
    // Calculate lighting in fragment shader (Phong)
    vec3 result = calcDirectionalLight(norm, viewDir);
    result += calcPointLights(norm, viewDir);
    
    vec4 texColor = texture(uTexture, TexCoord);
    FragColor = vec4(result, 1.0) * texColor;
}
)";

    phongShaderProgram = createShaderProgram(phongVertexSource, phongFragmentSource);

    // =================================================================
    // ASSIGNMENT 4: Phong Shading with Normal Mapping
    // =================================================================
    const char* phongNormalMapVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform mat4 uModelMatrix;
uniform mat3 uNormalMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out mat3 TBN;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    FragPos = vec3(uModelMatrix * vec4(aPos, 1.0));
    Normal = uNormalMatrix * aNormal;
    TexCoord = aTexCoord;
    
    // Calculate TBN matrix for normal mapping
    vec3 T = normalize(vec3(uModelMatrix * vec4(1.0, 0.0, 0.0, 0.0)));
    vec3 N = normalize(Normal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
}
)";

    const char* phongNormalMapFragmentSource = R"(
#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;

out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uObjectColor;

// Directional light
uniform vec3 uDirLight_direction;
uniform vec3 uDirLight_ambient;
uniform vec3 uDirLight_diffuse;
uniform vec3 uDirLight_specular;

const int MAX_POINT_LIGHTS = 4;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

uniform sampler2D uTexture;
uniform sampler2D uNormalMap;
uniform bool uHasNormalMap;

vec3 calcDirectionalLight(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-uDirLight_direction);
    
    // Ambient
    vec3 ambient = uDirLight_ambient * uObjectColor;
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = uDirLight_diffuse * diff * uObjectColor;
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = uDirLight_specular * spec;
    
    return ambient + diffuse + specular;
}

vec3 calcPointLights(vec3 normal, vec3 viewDir) {
    vec3 sum = vec3(0.0);

    for (int i = 0; i < uNumPointLights; ++i) {
        vec3 lightDir = normalize(uPointLights[i].position - FragPos);
        float distance = length(uPointLights[i].position - FragPos);
        float attenuation = 1.0 / (uPointLights[i].constant
                                   + uPointLights[i].linear * distance
                                   + uPointLights[i].quadratic * distance * distance);

        vec3 ambient  = uPointLights[i].ambient * uObjectColor;

        float diff    = max(dot(normal, lightDir), 0.0);
        vec3 diffuse  = uPointLights[i].diffuse * diff * uObjectColor;

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec   = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        vec3 specular = uPointLights[i].specular * spec;

        sum += (ambient + diffuse + specular) * attenuation;
    }

    return sum;
}

void main() {
    // Get normal from normal map
    vec3 norm;
    if (uHasNormalMap) {
        norm = texture(uNormalMap, TexCoord).rgb;
        norm = norm * 2.0 - 1.0;  // Transform from [0,1] to [-1,1]
        norm = normalize(TBN * norm);
    } else {
        norm = normalize(Normal);
    }
    
    vec3 viewDir = normalize(uViewPos - FragPos);
    
    // Calculate lighting with normal-mapped surface
    vec3 result = calcDirectionalLight(norm, viewDir);
    result += calcPointLights(norm, viewDir);
    
    vec4 texColor = texture(uTexture, TexCoord);
    FragColor = vec4(result, 1.0) * texColor;
}
)";

    phongNormalMapShaderProgram = createShaderProgram(phongNormalMapVertexSource, phongNormalMapFragmentSource);

    // =================================================================
    // Keep existing wireframe and other shaders (유지)
    // =================================================================
    const char* wireVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* wireFragmentSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

    wireframeShaderProgram = createShaderProgram(wireVertexSource, wireFragmentSource);
    uMVP_wire = glGetUniformLocation(wireframeShaderProgram, "uMVP");
    uColor_wire = glGetUniformLocation(wireframeShaderProgram, "uColor");

    hiddenLineShaderProgram = createShaderProgram(wireVertexSource, wireFragmentSource);
    uMVP_hidden = glGetUniformLocation(hiddenLineShaderProgram, "uMVP");
    uColor_hidden = glGetUniformLocation(hiddenLineShaderProgram, "uColor");
    uModelMatrix_hidden = glGetUniformLocation(hiddenLineShaderProgram, "uModelMatrix");

    // Glow shader (keep existing)
    const char* glowVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* glowFragmentSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
uniform float uGlowIntensity;
void main() {
    vec3 glowColor = uColor * (1.0 + uGlowIntensity);
    FragColor = vec4(glowColor, 1.0);
}
)";

    glowShaderProgram = createShaderProgram(glowVertexSource, glowFragmentSource);
    uMVP_glow = glGetUniformLocation(glowShaderProgram, "uMVP");
    uColor_glow = glGetUniformLocation(glowShaderProgram, "uColor");
    uGlowIntensity = glGetUniformLocation(glowShaderProgram, "uGlowIntensity");

    // Trail shader (keep existing)
    const char* trailVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* trailFragmentSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    FragColor = vec4(uColor, uAlpha);
}
)";
    // ==================== SHADOW SHADER (planar) ====================
    const char* shadowVertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

    const char* shadowFragmentSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 uShadowColor;
uniform float uShadowAlpha;

void main() {
    FragColor = vec4(uShadowColor, uShadowAlpha);
}
)";

    trailShaderProgram = createShaderProgram(trailVertexSource, trailFragmentSource);
    uMVP_trail = glGetUniformLocation(trailShaderProgram, "uMVP");
    uColor_trail = glGetUniformLocation(trailShaderProgram, "uColor");
    uAlpha_trail = glGetUniformLocation(trailShaderProgram, "uAlpha");

    // Create default textures
    defaultTexture = createDefaultTexture();
    defaultNormalMap = createDefaultNormalMap();

    shadowShaderProgram = createShaderProgram(shadowVertexSource, shadowFragmentSource);
    uMVP_shadow = glGetUniformLocation(shadowShaderProgram, "uMVP");
    uShadowColor = glGetUniformLocation(shadowShaderProgram, "uShadowColor");
    uShadowAlpha = glGetUniformLocation(shadowShaderProgram, "uShadowAlpha");
}


// Camera matrices
glm::mat4 projectionMatrix;
glm::mat4 viewMatrix;

void setupCamera() {
    switch (currentCamera) {
    case TOP_PERSPECTIVE:
        projectionMatrix = glm::perspective(glm::radians(45.0f),
            (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);
        viewMatrix = glm::lookAt(
            glm::vec3(0, 0, 8),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
        );
        break;

    case THIRD_PERSON:
        projectionMatrix = glm::perspective(glm::radians(45.0f),
            (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);
        break;
    }
}

// Base GameObject class
class GameObject {
public:
    Vec3 position;
    Vec3 velocity;
    Vec3 rotation;
    float size;
    bool active;
    OBJModel* model;
    Vec3 color;

    // ADDITIONAL GOAL: Animation
    Vec3 targetRotation;
    float rotationSmooth;

    GameObject(float x = 0, float y = 0, float z = 0, float s = 1.0f, OBJModel* m = nullptr)
        : position(x, y, z), velocity(0, 0, 0), rotation(0, 0, 0),
        size(s), active(true), model(m), color(1, 1, 1),
        targetRotation(0, 0, 0), rotationSmooth(0.1f) {
    }

    virtual ~GameObject() {}

    virtual void update(float deltaTime) {
        position = position + velocity * deltaTime;

        // Smooth rotation interpolation
        rotation.x += (targetRotation.x - rotation.x) * rotationSmooth;
        rotation.y += (targetRotation.y - rotation.y) * rotationSmooth;
        rotation.z += (targetRotation.z - rotation.z) * rotationSmooth;
    }

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position.toGLM());
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(size, size, size));
        return model;
    }

    virtual void render() {
        if (!active || !model) return;

        glm::mat4 modelMatrix = getModelMatrix();
        glm::mat4 mvp = projectionMatrix * viewMatrix * modelMatrix;

        if (currentRender == OPAQUE_POLYGON) {
            // ===== 1. 본체 렌더 =====
            // Select shader based on shading mode
            GLuint currentProgram;
            switch (currentShading) {
            case GOURAUD:
                currentProgram = gouraudShaderProgram;
                break;
            case PHONG:
                currentProgram = phongShaderProgram;
                break;
            case PHONG_NORMAL_MAP:
                currentProgram = phongNormalMapShaderProgram;
                break;
            }

            glUseProgram(currentProgram);

            // Matrices
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
            glUniformMatrix4fv(glGetUniformLocation(currentProgram, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
            glUniformMatrix4fv(glGetUniformLocation(currentProgram, "uModelMatrix"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glUniformMatrix3fv(glGetUniformLocation(currentProgram, "uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

            // Camera position
            glm::vec3 viewPos = globalViewPos;
            glUniform3fv(glGetUniformLocation(currentProgram, "uViewPos"), 1, glm::value_ptr(viewPos));

            // Object color
            glUniform3f(glGetUniformLocation(currentProgram, "uObjectColor"), color.x, color.y, color.z);

            // Directional light
            glm::vec3 dirLightDirection = dirLight.direction.toGLM();
            glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_direction"), 1, glm::value_ptr(dirLightDirection));

            glm::vec3 dirLightAmbient = dirLight.ambient.toGLM();
            glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_ambient"), 1, glm::value_ptr(dirLightAmbient));

            glm::vec3 dirLightDiffuse = dirLight.diffuse.toGLM();
            glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_diffuse"), 1, glm::value_ptr(dirLightDiffuse));

            glm::vec3 dirLightSpecular = dirLight.specular.toGLM();
            glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_specular"), 1, glm::value_ptr(dirLightSpecular));

            // === Multiple Point Lights ===
            glUniform1i(glGetUniformLocation(currentProgram, "uNumPointLights"), numActivePointLights);

            for (int i = 0; i < numActivePointLights; ++i) {
                if (!pointLights[i].enabled) continue;

                std::string base = "uPointLights[" + std::to_string(i) + "]";

                glm::vec3 pos = pointLights[i].position.toGLM();
                glm::vec3 amb = pointLights[i].ambient.toGLM();
                glm::vec3 diff = pointLights[i].diffuse.toGLM();
                glm::vec3 spec = pointLights[i].specular.toGLM();

                glUniform3fv(glGetUniformLocation(currentProgram, (base + ".position").c_str()), 1, glm::value_ptr(pos));
                glUniform3fv(glGetUniformLocation(currentProgram, (base + ".ambient").c_str()), 1, glm::value_ptr(amb));
                glUniform3fv(glGetUniformLocation(currentProgram, (base + ".diffuse").c_str()), 1, glm::value_ptr(diff));
                glUniform3fv(glGetUniformLocation(currentProgram, (base + ".specular").c_str()), 1, glm::value_ptr(spec));
                glUniform1f(glGetUniformLocation(currentProgram, (base + ".constant").c_str()), pointLights[i].constant);
                glUniform1f(glGetUniformLocation(currentProgram, (base + ".linear").c_str()), pointLights[i].linear);
                glUniform1f(glGetUniformLocation(currentProgram, (base + ".quadratic").c_str()), pointLights[i].quadratic);
            }

            // Textures
            glActiveTexture(GL_TEXTURE0);
            if (model->diffuseTexture != 0) {
                glBindTexture(GL_TEXTURE_2D, model->diffuseTexture);
            }
            else {
                glBindTexture(GL_TEXTURE_2D, defaultTexture);
            }
            glUniform1i(glGetUniformLocation(currentProgram, "uTexture"), 0);

            if (currentShading == PHONG_NORMAL_MAP) {
                glActiveTexture(GL_TEXTURE1);
                if (model->normalMapTexture != 0) {
                    glBindTexture(GL_TEXTURE_2D, model->normalMapTexture);
                    glUniform1i(glGetUniformLocation(currentProgram, "uHasNormalMap"), model->hasNormalMap ? 1 : 0);
                }
                else {
                    glBindTexture(GL_TEXTURE_2D, defaultNormalMap);
                    glUniform1i(glGetUniformLocation(currentProgram, "uHasNormalMap"), 0);
                }
                glUniform1i(glGetUniformLocation(currentProgram, "uNormalMap"), 1);
            }

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            model->render();

            // ===== 2. 그림자 렌더 =====
            glm::vec3 lightDir = dirLight.direction.toGLM();
            glm::mat4 shadowMat = computeShadowMatrix(lightDir);
            glm::mat4 shadowModel = shadowMat * modelMatrix;
            glm::mat4 shadowMVP = projectionMatrix * viewMatrix * shadowModel;

            glUseProgram(shadowShaderProgram);
            glUniformMatrix4fv(uMVP_shadow, 1, GL_FALSE, glm::value_ptr(shadowMVP));
            glUniform3fv(uShadowColor, 1, glm::value_ptr(shadowColor));
            glUniform1f(uShadowAlpha, shadowAlpha);

            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            model->render();

            glDisable(GL_BLEND);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
        else if (currentRender == WIREFRAME) {
            glUseProgram(wireframeShaderProgram);
            glUniformMatrix4fv(uMVP_wire, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(uColor_wire, color.x, color.y, color.z);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            model->render();
        }
        else if (currentRender == HIDDEN_LINE_WIREFRAME) {
            glUseProgram(hiddenLineShaderProgram);
            glUniformMatrix4fv(uMVP_hidden, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(uColor_hidden, 0, 0, 0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            model->render();
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glUniform3f(uColor_hidden, color.x, color.y, color.z);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            model->render();
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
    }
    bool checkCollision(const GameObject& other) const {
        float distance = (position - other.position).length();
        return distance < (size + other.size);
    }

    bool isOutOfBounds() const {
        return position.x < GAME_LEFT - size || position.x > GAME_RIGHT + size ||
            position.y < GAME_BOTTOM - size || position.y > GAME_TOP + size ||
            position.z < GAME_NEAR - size || position.z > GAME_FAR + size;
    }
};

// Player class with enhanced animation
class Player : public GameObject {
public:
    int lives;
    float respawnTimer;
    bool invulnerable;
    float invulnerabilityTimer;
    std::vector<GameObject> orbitEntities;

    // Animation state
    Vec3 lastPosition;

    Player() : GameObject(0, -1.5f, 0, PLAYER_SIZE, &himekaModel) {
        lives = PLAYER_LIVES;
        respawnTimer = 0;
        invulnerable = false;
        invulnerabilityTimer = 0;
        color = Vec3(0.3f, 0.7f, 1.0f);
        lastPosition = position;

        for (int i = 0; i < 3; i++) {
            GameObject orbit(0, 0, 0, ORBIT_SIZE, &donutModel);
            orbit.color = Vec3(1.0f, 1.0f, 0.0f);
            orbitEntities.push_back(orbit);
        }
    }

    void update(float deltaTime) override {
        if (!active) {
            respawnTimer += deltaTime;
            if (respawnTimer >= RESPAWN_TIME) {
                respawn();
            }
            return;
        }

        // [수정 1] 이동 로직을 Y축 -> Z축으로 변경 (XZ 평면 이동)
        Vec3 movement(0, 0, 0);
        if (keys['a'] || keys['A']) movement.x -= 1.0f;
        if (keys['d'] || keys['D']) movement.x += 1.0f;
        
        // W, S키가 이제 위아래(Y)가 아니라 앞뒤(Z)로 움직입니다.
        if (keys['w'] || keys['W']) movement.y += 1.0f; // 앞으로 (Z 감소)
        if (keys['s'] || keys['S']) movement.y -= 1.0f; // 뒤로 (Z 증가)

        if (movement.length() > 0) {
            movement = movement.normalize() * 2.0f;
            velocity = movement;

            // [수정 2] 모델이 앞을 보게 회전 (-90도 베이스) + 움직임에 따른 틸트 애니메이션
            // 기본 -90도(앞) + 위아래 움직임에 따른 기울기(movement.z)
            targetRotation.x = -90.0f + movement.y * 10.0f; 
            
            // 좌우 롤링 (기존 유지)
            targetRotation.y = -movement.x * 20.0f;  

            // 트레일 효과 (Trail)
            if (trailEffectEnabled && (int)(gameTime * 30) % 2 == 0) {
                // 트레일 위치도 발 밑이 아니라 엔진 뒤쪽으로 조정
                trailPoints.push_back(TrailPoint(position + Vec3(0, 0, size),
                    Vec3(0.3f, 0.7f, 1.0f), 0.5f));
            }
        }
        else {
            velocity = Vec3(0, 0, 0);
            // 멈췄을 때도 앞을 보게 유지 (-90도)
            targetRotation = Vec3(-90.0f, 0, 0); 
        }

        GameObject::update(deltaTime);

        // [수정 3] 경계 체크도 Y축 -> Z축으로 변경
        if (position.x - size < GAME_LEFT) position.x = GAME_LEFT + size;
        if (position.x + size > GAME_RIGHT) position.x = GAME_RIGHT - size;
        
        // Y축은 고정 (바닥 위)
        // 바닥이 -3.0f이므로 플레이어는 그보다 위에 떠 있어야 함 (예: -1.5f)
        position.z= 0.0f; 

        // Z축 경계 체크 (NEAR ~ FAR)
        if (position.y - size < GAME_NEAR) position.y = GAME_NEAR + size;
        if (position.y + size > GAME_FAR) position.y = GAME_FAR - size;

        if (invulnerable) {
            invulnerabilityTimer -= deltaTime;
            if (invulnerabilityTimer <= 0) invulnerable = false;
        }

        for (int i = 0; i < orbitEntities.size(); i++) {
            float angle = gameTime * 2.0f + (i * 2.0f * M_PI / orbitEntities.size());
            float radius = 0.3f;
            // 오빗 엔티티도 눕혀서 돌게 수정 (XZ 평면 회전)
            orbitEntities[i].position = Vec3(
                position.x + cos(angle) * radius,
                position.y, // 높이 고정
                position.z + sin(angle) * radius
            );
            orbitEntities[i].targetRotation.y = gameTime * 180.0f;
        }
        
        if (position != lastPosition) {
            velocity = (position - lastPosition).normalize();
        }
        lastPosition = position;
    }

    void render() override {
        if (!active) return;

        if (!invulnerable || (int)(invulnerabilityTimer * 10) % 2 == 0) {
            GameObject::render();
        }

        for (auto& orbit : orbitEntities) {
            orbit.render();
        }
    }

    void takeDamage() {
        if (invulnerable) return;
        lives--;

        if (lives > 0 && !orbitEntities.empty()) {
            orbitEntities.pop_back();
        }

        if (lives <= 0) {
            active = false;
        }
        else {
            invulnerable = true;
            invulnerabilityTimer = 2.0f;
        }
    }

    void respawn() {
        if (lives > 0) {
            active = true;
            position = Vec3(0, -1.5f, 0);
            velocity = Vec3(0, 0, 0);
            rotation = Vec3(0, 0, 0);
            respawnTimer = 0;
            invulnerable = true;
            invulnerabilityTimer = 2.0f;
        }
    }
};

// ADDITIONAL GOAL 4: Destructible Enemy with health visualization
class DestructibleEnemy : public GameObject {
public:
    float health;
    float maxHealth;
    float shootTimer;
    float animationTime;

    // Destructible parts
    struct EnemyPart {
        Vec3 localPosition;
        Vec3 rotation;
        float health;
        float maxHealth;
        bool destroyed;
        OBJModel* model;

        EnemyPart(Vec3 pos, OBJModel* m, float h = 10.0f)
            : localPosition(pos), rotation(0, 0, 0), health(h), maxHealth(h),
            destroyed(false), model(m) {
        }
    };

    std::vector<EnemyPart> parts;

    DestructibleEnemy(float x, float y, float z) : GameObject(x, y, z, ENEMY_SIZE, &droneModel) {
        health = ENEMY_HEALTH;
        maxHealth = health;
        shootTimer = 0;
        animationTime = 0;
        color = Vec3(1.0f, 0.3f, 0.3f);

        const float PART_DISTANCE = 1.0f;

        // Create destructible parts
        parts.push_back(EnemyPart(Vec3(PART_DISTANCE, 0, 0), &sphereModel, 25.0f));
        parts.push_back(EnemyPart(Vec3(-PART_DISTANCE, 0, 0), &sphereModel, 25.0f));
    }

    void update(float deltaTime) override {
        animationTime += deltaTime;
        shootTimer += deltaTime;

        velocity = Vec3(cos(animationTime) * 0.5f, sin(animationTime * 0.7f) * 0.3f, 0);
        targetRotation.y = animationTime * 50;

        GameObject::update(deltaTime);

        // ADDITIONAL GOAL 4: Health-based color visualization
        if (damageVisualizationEnabled) {
            float healthRatio = health / maxHealth;
            color = Vec3(1.0f, healthRatio * 0.7f, healthRatio * 0.3f);
        }

        for (auto& part : parts) {
            if (!part.destroyed) {
                part.rotation.y += deltaTime * 60.0f;
            }
        }

        if (position.x < GAME_LEFT) position.x = GAME_LEFT;
        if (position.x > GAME_RIGHT) position.x = GAME_RIGHT;
        if (position.y < GAME_BOTTOM) position.y = GAME_BOTTOM;
        if (position.y > GAME_TOP) position.y = GAME_TOP;
    }

    void render() override {
        // Render main body first
        GameObject::render();

        // Render parts
        for (const auto& part : parts) {
            if (part.destroyed || !part.model) continue;

            glm::mat4 parentModel = getModelMatrix();
            glm::mat4 partModel = glm::translate(parentModel, part.localPosition.toGLM());
            partModel = glm::rotate(partModel, glm::radians(part.rotation.y), glm::vec3(0, 1, 0));
            partModel = glm::scale(partModel, glm::vec3(0.5f, 0.5f, 0.5f));
            glm::mat4 mvp = projectionMatrix * viewMatrix * partModel;

            float partHealthRatio = part.health / part.maxHealth;
            Vec3 partColor(0.8f, partHealthRatio, 0.2f);

            if (currentRender == OPAQUE_POLYGON) {
                // Select shader based on shading mode
                GLuint currentProgram;
                switch (currentShading) {
                case GOURAUD:
                    currentProgram = gouraudShaderProgram;
                    break;
                case PHONG:
                    currentProgram = phongShaderProgram;
                    break;
                case PHONG_NORMAL_MAP:
                    currentProgram = phongNormalMapShaderProgram;
                    break;
                }

                glUseProgram(currentProgram);

                // Matrices
                glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(partModel)));
                glUniformMatrix4fv(glGetUniformLocation(currentProgram, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(glGetUniformLocation(currentProgram, "uModelMatrix"), 1, GL_FALSE, glm::value_ptr(partModel));
                glUniformMatrix3fv(glGetUniformLocation(currentProgram, "uNormalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));

                // Camera position
                glm::vec3 viewPos;
                if (currentCamera == THIRD_PERSON && player.active) {
                    viewPos = glm::vec3(
                        player.position.x,
                        player.position.y - 1.5f,
                        player.position.z + 2.0f
                    );
                }
                else {
                    // TOP_PERSPECTIVE
                    viewPos = glm::vec3(0, 0, 8);
                }
                glUniform3fv(glGetUniformLocation(currentProgram, "uViewPos"), 1, glm::value_ptr(viewPos));

                // Object color (part color with health visualization)
                glUniform3f(glGetUniformLocation(currentProgram, "uObjectColor"), partColor.x, partColor.y, partColor.z);

                glm::vec3 dirLightDirection = dirLight.direction.toGLM();
                glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_direction"), 1, glm::value_ptr(dirLightDirection));

                glm::vec3 dirLightAmbient = dirLight.ambient.toGLM();
                glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_ambient"), 1, glm::value_ptr(dirLightAmbient));

                glm::vec3 dirLightDiffuse = dirLight.diffuse.toGLM();
                glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_diffuse"), 1, glm::value_ptr(dirLightDiffuse));

                glm::vec3 dirLightSpecular = dirLight.specular.toGLM();
                glUniform3fv(glGetUniformLocation(currentProgram, "uDirLight_specular"), 1, glm::value_ptr(dirLightSpecular));

                // === Multiple Point Lights ===
                glUniform1i(glGetUniformLocation(currentProgram, "uNumPointLights"), numActivePointLights);

                for (int i = 0; i < numActivePointLights; ++i) {
                    if (!pointLights[i].enabled) continue;

                    std::string base = "uPointLights[" + std::to_string(i) + "]";

                    glm::vec3 pos = pointLights[i].position.toGLM();
                    glm::vec3 amb = pointLights[i].ambient.toGLM();
                    glm::vec3 diff = pointLights[i].diffuse.toGLM();
                    glm::vec3 spec = pointLights[i].specular.toGLM();

                    glUniform3fv(glGetUniformLocation(currentProgram, (base + ".position").c_str()), 1, glm::value_ptr(pos));
                    glUniform3fv(glGetUniformLocation(currentProgram, (base + ".ambient").c_str()), 1, glm::value_ptr(amb));
                    glUniform3fv(glGetUniformLocation(currentProgram, (base + ".diffuse").c_str()), 1, glm::value_ptr(diff));
                    glUniform3fv(glGetUniformLocation(currentProgram, (base + ".specular").c_str()), 1, glm::value_ptr(spec));
                    glUniform1f(glGetUniformLocation(currentProgram, (base + ".constant").c_str()), pointLights[i].constant);
                    glUniform1f(glGetUniformLocation(currentProgram, (base + ".linear").c_str()), pointLights[i].linear);
                    glUniform1f(glGetUniformLocation(currentProgram, (base + ".quadratic").c_str()), pointLights[i].quadratic);

                    // Textures
                    glActiveTexture(GL_TEXTURE0);
                    if (part.model->diffuseTexture != 0) {
                        glBindTexture(GL_TEXTURE_2D, part.model->diffuseTexture);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, defaultTexture);
                    }
                    glUniform1i(glGetUniformLocation(currentProgram, "uTexture"), 0);

                    if (currentShading == PHONG_NORMAL_MAP) {
                        glActiveTexture(GL_TEXTURE1);
                        if (part.model->normalMapTexture != 0) {
                            glBindTexture(GL_TEXTURE_2D, part.model->normalMapTexture);
                            glUniform1i(glGetUniformLocation(currentProgram, "uHasNormalMap"), part.model->hasNormalMap ? 1 : 0);
                        }
                        else {
                            glBindTexture(GL_TEXTURE_2D, defaultNormalMap);
                            glUniform1i(glGetUniformLocation(currentProgram, "uHasNormalMap"), 0);
                        }
                        glUniform1i(glGetUniformLocation(currentProgram, "uNormalMap"), 1);
                    }
                } // 💡 이 닫는 괄호가 누락되어 있었습니다.

                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                part.model->render();
            }
            else if (currentRender == WIREFRAME) {
                glUseProgram(wireframeShaderProgram);
                glUniformMatrix4fv(uMVP_wire, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniform3f(uColor_wire, partColor.x, partColor.y, partColor.z);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                part.model->render();
            }
            else if (currentRender == HIDDEN_LINE_WIREFRAME) {
                glUseProgram(hiddenLineShaderProgram);
                glUniformMatrix4fv(uMVP_hidden, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniform3f(uColor_hidden, 0, 0, 0);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                part.model->render();
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glUniform3f(uColor_hidden, partColor.x, partColor.y, partColor.z);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
                part.model->render();
                glDisable(GL_POLYGON_OFFSET_LINE);
            }
        }
    }

};

// ADDITIONAL GOAL 2: Enhanced Attack with glow effect
class Attack : public GameObject {
public:
    Attack(float x, float y, float z) : GameObject(x, y, z, ATTACK_SIZE, &starModel) {
        velocity = Vec3(0, 2.0f, 0);
        color = Vec3(1.0f, 1.0f, 0.0f);
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);
        targetRotation.z += deltaTime * 360.0f;

        // ADDITIONAL GOAL 3: Trail
        if (trailEffectEnabled && (int)(gameTime * 60) % 2 == 0) {
            trailPoints.push_back(TrailPoint(position, Vec3(1.0f, 1.0f, 0.2f), 0.3f));
        }

        if (isOutOfBounds()) active = false;
    }

    void render() override {
        // ADDITIONAL GOAL 2: Render with glow effect
        if (glowEffectEnabled && currentRender == OPAQUE_POLYGON) {
            glm::mat4 modelMatrix = getModelMatrix();
            // Slightly larger for glow
            glm::mat4 glowModel = glm::scale(modelMatrix, glm::vec3(1.2f, 1.2f, 1.2f));
            glm::mat4 mvp = projectionMatrix * viewMatrix * glowModel;

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glUseProgram(glowShaderProgram);
            glUniformMatrix4fv(uMVP_glow, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(uColor_glow, color.x, color.y, color.z);
            glUniform1f(uGlowIntensity, 0.5f + 0.3f * sin(gameTime * 5.0f));

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            model->render();

            glDisable(GL_BLEND);
        }

        // Normal render
        GameObject::render();
    }
};

class Bullet : public GameObject {
public:
    Bullet(float x, float y, float z, Vec3 dir) : GameObject(x, y, z, BULLET_SIZE, &sphereModel) {
        velocity = dir * 1.5f;
        color = Vec3(1.0f, 0.0f, 0.0f);
    }

    void update(float deltaTime) override {
        GameObject::update(deltaTime);
        targetRotation.x += deltaTime * 180.0f;
        targetRotation.y += deltaTime * 270.0f;
        if (isOutOfBounds()) active = false;
    }
};

// Game state
Player player;
std::vector<DestructibleEnemy> enemies;
std::vector<Attack> attacks;
std::vector<Bullet> bullets;
bool gameOver = false;
bool gameWon = false;

void loadModels() {
    std::cout << "\n=== Loading 3D Models and Textures ===" << std::endl;

    // Load OBJ files
    std::cout << "\nLoading OBJ models..." << std::endl;
    jetModel.loadOBJ("assets/jet.obj");
    droneModel.loadOBJ("assets/starship.obj");
    sphereModel.loadOBJ("assets/sphere.obj");
    starModel.loadOBJ("assets/star_smooth.obj");
    donutModel.loadOBJ("assets/ellipsoid.obj");
    triangleModel.loadOBJ("assets/star_sharp.obj");
    riceModel.loadOBJ("assets/rice.obj");
    himekaModel.loadOBJ("assets/jet.obj");

    // Assign textures and normal maps
    std::cout << "\nAssigning textures and normal maps..." << std::endl;

    std::cout << "Jet model:" << std::endl;
    jetModel.setTextures("assets/diffuse_jet.png", "assets/normal_industrial.png");

    std::cout << "Starship model (enemy):" << std::endl;
    droneModel.setTextures("assets/diffuse_starship.png", "assets/normal_industrial.png");

    std::cout << "Sphere model (bullets/parts):" << std::endl;
    sphereModel.setTextures("assets/diffuse_primary.png", "assets/normal_marble.png");

    std::cout << "Star model (attacks):" << std::endl;
    starModel.setTextures("assets/diffuse_star.png", "assets/normal_organic.png");

    std::cout << "Ellipsoid model (orbits):" << std::endl;
    donutModel.setTextures("assets/diffuse_tertiary.png", "assets/normal_organic.png");

    std::cout << "Triangle model:" << std::endl;
    triangleModel.setTextures("assets/diffuse_secondary.png", "assets/normal_flat.png");

    std::cout << "Rice model:" << std::endl;
    riceModel.setTextures("assets/diffuse_rice.png", "assets/normal_organic.png");

    std::cout << "Sonic model (player):" << std::endl;
    himekaModel.setTextures("assets/diffuse_jet.png", "assets/normal_industrial.png");

    std::cout << "\n✓ All models and textures loaded successfully!" << std::endl;
    std::cout << "==========================================\n" << std::endl;
}


void renderBoundary() {
    GLuint boundaryVAO, boundaryVBO;
    float boundaryVertices[] = {
        GAME_LEFT, GAME_BOTTOM, GAME_NEAR,
        GAME_RIGHT, GAME_BOTTOM, GAME_NEAR,
        GAME_RIGHT, GAME_BOTTOM, GAME_FAR,
        GAME_LEFT, GAME_BOTTOM, GAME_FAR,
        GAME_LEFT, GAME_TOP, GAME_NEAR,
        GAME_RIGHT, GAME_TOP, GAME_NEAR,
        GAME_RIGHT, GAME_TOP, GAME_FAR,
        GAME_LEFT, GAME_TOP, GAME_FAR
    };

    glGenVertexArrays(1, &boundaryVAO);
    glGenBuffers(1, &boundaryVBO);
    glBindVertexArray(boundaryVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boundaryVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(boundaryVertices), boundaryVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glm::mat4 mvp = projectionMatrix * viewMatrix;
    glUseProgram(wireframeShaderProgram);
    glUniformMatrix4fv(uMVP_wire, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(uColor_wire, 0.5f, 0.5f, 0.5f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(boundaryVAO);

    unsigned int edges[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    for (int i = 0; i < 24; i += 2) {
        glDrawArrays(GL_LINES, edges[i], 2);
    }

    glDeleteVertexArrays(1, &boundaryVAO);
    glDeleteBuffers(1, &boundaryVBO);
}

// Render fixed ground plane on XY plane at z = floorZ
void renderGroundPlane() {
    static GLuint groundVAO = 0, groundVBO = 0, groundEBO = 0;
    if (groundVAO == 0) {
        glGenVertexArrays(1, &groundVAO);
        glGenBuffers(1, &groundVBO);
        glGenBuffers(1, &groundEBO);

        glBindVertexArray(groundVAO);

        float z = floorZ;

        float groundVertices[] = {
            GAME_LEFT,  GAME_BOTTOM, z,
            GAME_RIGHT, GAME_BOTTOM, z,
            GAME_RIGHT, GAME_TOP,    z,
            GAME_LEFT,  GAME_TOP,    z
        };

        unsigned int groundIndices[] = { 0,1,2, 0,2,3 };

        glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, groundEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(groundIndices), groundIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    glm::mat4 model(1.0f);
    glm::mat4 mvp = projectionMatrix * viewMatrix * model;

    glUseProgram(wireframeShaderProgram);
    glUniformMatrix4fv(uMVP_wire, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(uColor_wire, 0.12f, 0.12f, 0.15f);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(groundVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_FILL);
}

// z = floorZ 평면(XY plane)에 대한 shadow projection matrix (directional light 기준)
glm::mat4 computeShadowMatrix(const glm::vec3& lightDir) {
    // Plane: z = floorZ → 0*x + 0*y + 1*z + d = 0  → d = -floorZ
    glm::vec4 plane(0.0f, 0.0f, 1.0f, -floorZ);
    glm::vec4 L(lightDir.x, lightDir.y, lightDir.z, 0.0f); // directional light (w=0)

    float dotPL = glm::dot(plane, L);

    glm::mat4 S(1.0f);
    S[0][0] = dotPL - L.x * plane.x; S[0][1] = -L.x * plane.y; S[0][2] = -L.x * plane.z; S[0][3] = -L.x * plane.w;
    S[1][0] = -L.y * plane.x; S[1][1] = dotPL - L.y * plane.y; S[1][2] = -L.y * plane.z; S[1][3] = -L.y * plane.w;
    S[2][0] = -L.z * plane.x; S[2][1] = -L.z * plane.y; S[2][2] = dotPL - L.z * plane.z; S[2][3] = -L.z * plane.w;
    S[3][0] = -L.w * plane.x; S[3][1] = -L.w * plane.y; S[3][2] = -L.w * plane.z; S[3][3] = dotPL - L.w * plane.w;

    return S;
}

// Render point light visualization
void renderPointLight() {
    if (!player.active) return;

    glUseProgram(glowShaderProgram);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // 0번과 1번 라이트 두 개를 모두 시각화
    for (int i = 0; i < 2; ++i) {
        if (!pointLights[i].enabled) continue;

        glm::mat4 lightModel = glm::mat4(1.0f);
        glm::vec3 tmpPos = pointLights[i].position.toGLM();
        lightModel = glm::translate(lightModel, tmpPos);
        lightModel = glm::scale(lightModel, glm::vec3(0.15f, 0.15f, 0.15f));
        glm::mat4 mvp = projectionMatrix * viewMatrix * lightModel;

        glUniformMatrix4fv(uMVP_glow, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(uColor_glow,
            pointLights[i].diffuse.x,
            pointLights[i].diffuse.y,
            pointLights[i].diffuse.z);
        glUniform1f(uGlowIntensity, 0.8f + 0.2f * sin(gameTime * 4.0f));

        sphereModel.render();
    }

    glDisable(GL_BLEND);
}


// Render directional light debug visualization (optional)
void renderDirectionalLight() {
    // Draw an arrow or indicator showing directional light direction
    // This is optional - you can skip this if you don't want visual indicator

    // Position the indicator in a corner of the scene
    Vec3 indicatorPos(-3.5f, 3.5f, 0.0f);
    Vec3 lightEnd = indicatorPos + dirLight.direction * 0.5f;

    // Simple line rendering (you can make this fancier)
    GLuint lineVAO, lineVBO;
    float lineVertices[] = {
        indicatorPos.x, indicatorPos.y, indicatorPos.z,
        lightEnd.x, lightEnd.y, lightEnd.z
    };

    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);

    glBindVertexArray(lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glm::mat4 mvp = projectionMatrix * viewMatrix;
    glUseProgram(wireframeShaderProgram);
    glUniformMatrix4fv(uMVP_wire, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(uColor_wire,
        dirLight.diffuse.x,
        dirLight.diffuse.y,
        dirLight.diffuse.z);

    glLineWidth(3.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);

    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &lineVBO);
}

// ADDITIONAL GOAL 3: Render trail system
void renderTrails() {
    if (!trailEffectEnabled || trailPoints.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& trail : trailPoints) {
        float alpha = trail.life / trail.maxLife;

        // Simple sphere for trail points
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 tmpPos = trail.position.toGLM();

        model = glm::translate(model, tmpPos);
        model = glm::scale(model, glm::vec3(0.03f, 0.03f, 0.03f));
        glm::mat4 mvp = projectionMatrix * viewMatrix * model;

        glUseProgram(trailShaderProgram);
        glUniformMatrix4fv(uMVP_trail, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform3f(uColor_trail, trail.color.x, trail.color.y, trail.color.z);
        glUniform1f(uAlpha_trail, alpha);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        sphereModel.render();
    }

    glDisable(GL_BLEND);
}

void update(float deltaTime) {
    if (gameOver || gameWon) return;

    gameTime += deltaTime;

    // Update trail points
    for (auto& trail : trailPoints) {
        trail.life -= deltaTime;
    }
    trailPoints.erase(std::remove_if(trailPoints.begin(), trailPoints.end(),
        [](const TrailPoint& t) { return t.life <= 0; }), trailPoints.end());

    // === UPDATE LIGHTS ===
    // Point light revolves around player
    if (player.active) {
        float angle = gameTime * pointLightOrbitSpeed;  // Rotation speed
        float radius = 3.0f;             // Orbit radius
        pointLights[0].position = Vec3(
            player.position.x + cos(angle) * pointLightOrbitRadius,
            player.position.y + sin(angle * pointLightVerticalSpeed) * 1.0f,
            player.position.z + sin(angle) * pointLightOrbitRadius
        );

        // 1번: 위상만 다른 동일한 라이트 하나 더
        float angle1 = angle + M_PI; // 180도 반대 위치에서 회전
        pointLights[1].position = Vec3(
            player.position.x + cos(angle1) * radius,
            player.position.y + sin(angle1 * pointLightVerticalSpeed) * 1.0f,
            player.position.z + sin(angle1) * radius
        );
    }
    player.update(deltaTime);

    for (auto& enemy : enemies) {
        enemy.update(deltaTime);
        if (enemy.active && enemy.shootTimer > 1.5f) {
            Vec3 direction = (player.position - enemy.position).normalize();
            bullets.push_back(Bullet(enemy.position.x, enemy.position.y, enemy.position.z, direction));
            enemy.shootTimer = 0;
        }
    }

    for (auto& attack : attacks) attack.update(deltaTime);
    for (auto& bullet : bullets) bullet.update(deltaTime);

    // Collision detection
    for (auto& attack : attacks) {
        if (!attack.active) continue;
        bool hit_part = false; // 파트 피격 여부를 추적

        for (auto& enemy : enemies) {
            if (!enemy.active) continue;

            // 🌟 A. 파트별 충돌 검사 (먼저 수행)
            for (auto& part : enemy.parts) {
                if (part.destroyed) continue;

                // 파트의 월드 좌표 계산: 본체 위치 + 파트의 로컬 위치
                Vec3 partWorldPos = enemy.position + part.localPosition;

                // 파트와 공격 간의 거리 계산
                float distance = (attack.position - partWorldPos).length();

                // 파트의 피격 범위 (크기: ENEMY_SIZE=0.40, BULLET_SIZE=0.08)
                const float PART_COLLISION_RADIUS = 0.15f;

                if (distance < PART_COLLISION_RADIUS + ATTACK_SIZE) {
                    // 🌟 파트가 직접 맞았으므로, 파트의 체력만 감소시킵니다.
                    part.health -= ATTACK_DAMAGE;
                    hit_part = true;

                    if (part.health <= 0) {
                        part.destroyed = true;
                    }

                    // 공격 오브젝트 소멸
                    attack.active = false;
                    break; // 이 파트가 맞았으니 더 이상 검사할 필요 없음
                }
            }

            if (attack.active == false) break; // 공격이 이미 파트에 맞아 소멸했다면, 다음 공격으로 넘어감

            // 🌟 B. 파트에 맞지 않았고, 본체와 충돌하는 경우 (후순위)
            if (!hit_part && attack.checkCollision(enemy)) {
                // 본체에 직접 피해를 줍니다. (DestructibleEnemy::takeDamage는 이제 필요 없습니다)
                enemy.health -= ATTACK_DAMAGE;
                attack.active = false;
                if (enemy.health <= 0) enemy.active = false;
            }
        }
    }

    for (auto& bullet : bullets) {
        if (!bullet.active) continue;
        if (player.active && bullet.checkCollision(player)) {
            player.takeDamage();
            bullet.active = false;
        }
    }

    attacks.erase(std::remove_if(attacks.begin(), attacks.end(),
        [](const Attack& a) { return !a.active; }), attacks.end());
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [](const Bullet& b) { return !b.active; }), bullets.end());
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](const DestructibleEnemy& e) { return !e.active; }), enemies.end());

    if (enemies.empty()) gameWon = true;
    if (player.lives <= 0 && !player.active) gameOver = true;
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCamera();

    // Update third-person camera position based on player
    if (currentCamera == THIRD_PERSON && player.active) {
        // Position camera behind and above the player
        glm::vec3 cameraOffset = glm::vec3(0.0f, -1.5f, 2.0f);
        glm::vec3 cameraPos = glm::vec3(
            player.position.x + cameraOffset.x,
            player.position.y + cameraOffset.y,
            player.position.z + cameraOffset.z
        );

        viewMatrix = glm::lookAt(
            cameraPos,                    // Camera position
            player.position.toGLM(),      // Look at player
            glm::vec3(0, 1, 0)            // Up vector
        );
    }

    // === RENDER LIGHT SOURCES ===
    if (currentRender == OPAQUE_POLYGON) {
        renderPointLight();
    }

    player.render();
    for (auto& enemy : enemies) enemy.render();
    for (auto& attack : attacks) attack.render();
    for (auto& bullet : bullets) bullet.render();

    renderBoundary();
    renderGroundPlane();
    renderTrails();

    glutSwapBuffers();
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;

    if (key == '1') {
        currentCamera = TOP_PERSPECTIVE;
        std::cout << "Camera: Top-View Perspective" << std::endl;
    }
    else if (key == '3') {
        currentCamera = THIRD_PERSON;
        std::cout << "Camera: Third-Person Perspective" << std::endl;
    }

    else if (key == '4') currentRender = OPAQUE_POLYGON;
    else if (key == '5') currentRender = WIREFRAME;
    else if (key == '6') currentRender = HIDDEN_LINE_WIREFRAME;

    // ADDITIONAL GOALS toggles
    else if (key == '7') {
        smoothShadingEnabled = !smoothShadingEnabled;
        std::cout << "Smooth Shading: " << (smoothShadingEnabled ? "ON" : "OFF") << std::endl;
    }
    else if (key == '8') {
        glowEffectEnabled = !glowEffectEnabled;
        std::cout << "Glow Effect: " << (glowEffectEnabled ? "ON" : "OFF") << std::endl;
    }
    else if (key == '9') {
        trailEffectEnabled = !trailEffectEnabled;
        std::cout << "Trail Effect: " << (trailEffectEnabled ? "ON" : "OFF") << std::endl;
    }
    else if (key == '0') {
        damageVisualizationEnabled = !damageVisualizationEnabled;
        std::cout << "Damage Visualization: " << (damageVisualizationEnabled ? "ON" : "OFF") << std::endl;
    }

    else if (key == ' ') {
        if (player.active) {
            attacks.push_back(Attack(player.position.x, player.position.y + player.size, player.position.z));
        }
    }

    else if (key == 'r' || key == 'R') {
        if (gameOver || gameWon) {
            gameOver = false;
            gameWon = false;
            player = Player();
            enemies.clear();
            attacks.clear();
            bullets.clear();
            trailPoints.clear();

            enemies.push_back(DestructibleEnemy(-1.0f, 1.0f, 0));
            enemies.push_back(DestructibleEnemy(0.0f, 1.5f, 0));
            enemies.push_back(DestructibleEnemy(1.0f, 1.0f, 0));
        }
    }
    // Shading mode toggle (w key) - Assignment 4
    else if (key == 'w' || key == 'W') {
        currentShading = static_cast<ShadingMode>((currentShading + 1) % 3);
        std::string shadingName[] = { "Gouraud", "Phong", "Phong + Normal Map" };
        std::cout << "Shading Mode: " << shadingName[currentShading] << std::endl;
    }
    // === LIGHT CONTROL KEYS ===
    // Toggle directional light
    else if (key == 'l' || key == 'L') {
        static bool dirLightEnabled = true;
        dirLightEnabled = !dirLightEnabled;
        if (dirLightEnabled) {
            dirLight.ambient = Vec3(0.2f, 0.2f, 0.2f);
            dirLight.diffuse = Vec3(0.7f, 0.7f, 0.7f);
            dirLight.specular = Vec3(0.5f, 0.5f, 0.5f);
        }
        else {
            dirLight.ambient = Vec3(0.0f, 0.0f, 0.0f);
            dirLight.diffuse = Vec3(0.0f, 0.0f, 0.0f);
            dirLight.specular = Vec3(0.0f, 0.0f, 0.0f);
        }
        std::cout << "Directional Light: " << (dirLightEnabled ? "ON" : "OFF") << std::endl;
    }

    // Toggle point light
    else if (key == 'p' || key == 'P') {
        static bool pointLightsEnabled = true;
        pointLightsEnabled = !pointLightsEnabled;

        for (int i = 0; i < numActivePointLights; i++) {
            pointLights[i].enabled = pointLightsEnabled;
        }

        std::cout << "Point Lights: " << (pointLightsEnabled ? "ON" : "OFF") << std::endl;
    }
    // Texture debugging
    else if (key == 't' || key == 'T') {
        static bool showTextures = true;
        showTextures = !showTextures;

        // Toggle between textured and colored mode
        if (showTextures) {
            // Use actual textures (already loaded)
            std::cout << "Textures: ON" << std::endl;
        }
        else {
            // Use default white texture (will show only lighting)
            for (auto model : { &jetModel, &droneModel, &sphereModel, &starModel,
                            &donutModel, &triangleModel, &riceModel, &himekaModel }) {
                GLuint oldDiffuse = model->diffuseTexture;
                model->diffuseTexture = defaultTexture;
            }
            std::cout << "Textures: OFF (showing lighting only)" << std::endl;
        }
    }

    // Normal map debugging
    else if (key == 'n' || key == 'N') {
        static bool showNormalMaps = true;
        showNormalMaps = !showNormalMaps;

        for (auto model : { &jetModel, &droneModel, &sphereModel, &starModel,
                            &donutModel, &triangleModel, &riceModel, &himekaModel }) {
            model->hasNormalMap = showNormalMaps && (model->normalMapTexture != defaultNormalMap);
        }

        std::cout << "Normal Maps: " << (showNormalMaps ? "ON" : "OFF") << std::endl;
    }

    // Increase/decrease point light orbit speed
    else if (key == '+' || key == '=') {
        pointLightOrbitSpeed += 0.5f;
        std::cout << "Point light orbit speed: " << pointLightOrbitSpeed << std::endl;
    }
    else if (key == '-' || key == '_') {
        pointLightOrbitSpeed = std::max(0.5f, pointLightOrbitSpeed - 0.5f);
        std::cout << "Point light orbit speed: " << pointLightOrbitSpeed << std::endl;
    }
    // Increase/decrease point light orbit radius
    else if (key == '[' || key == '{') {
        pointLightOrbitRadius = std::max(1.0f, pointLightOrbitRadius - 0.5f);
        std::cout << "Point light orbit radius: " << pointLightOrbitRadius << std::endl;
    }
    else if (key == ']' || key == '}') {
        pointLightOrbitRadius += 0.5f;
        std::cout << "Point light orbit radius: " << pointLightOrbitRadius << std::endl;
    }
    else if (key == 'c' || key == 'C') {
        if (currentCamera == TOP_PERSPECTIVE) {
            currentCamera = THIRD_PERSON;
            std::cout << "Camera: Third-Person Perspective" << std::endl;
        }
        else {
            currentCamera = TOP_PERSPECTIVE;
            std::cout << "Camera: Top-View Perspective" << std::endl;
        }
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    keys[key] = false;
}

void timer(int value) {
    float currentTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    update(deltaTime);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void reshape(int width, int height) {
    glViewport(0, 0, width, height);
}
void initPointLights() {
    // 0번: 기존 플레이어를 도는 라이트
    pointLights[0].ambient = Vec3(0.05f, 0.05f, 0.05f);
    pointLights[0].diffuse = Vec3(0.8f, 0.8f, 0.6f);
    pointLights[0].specular = Vec3(1.0f, 1.0f, 0.8f);
    pointLights[0].constant = 1.0f;
    pointLights[0].linear = 0.09f;
    pointLights[0].quadratic = 0.032f;
    pointLights[0].enabled = true;

    // 1번: 스테이지 왼쪽 위 코너
    pointLights[1].position = Vec3(-3.0f, 1.5f, 2.5f);
    pointLights[1].ambient = Vec3(0.03f, 0.03f, 0.05f);
    pointLights[1].diffuse = Vec3(0.3f, 0.4f, 0.8f);
    pointLights[1].specular = Vec3(0.6f, 0.7f, 1.0f);
    pointLights[1].constant = 1.0f;
    pointLights[1].linear = 0.14f;
    pointLights[1].quadratic = 0.07f;
    pointLights[1].enabled = true;

    // 2번: 스테이지 오른쪽 위 코너
    pointLights[2].position = Vec3(3.0f, 1.5f, 2.5f);
    pointLights[2].ambient = Vec3(0.05f, 0.03f, 0.03f);
    pointLights[2].diffuse = Vec3(0.8f, 0.4f, 0.3f);
    pointLights[2].specular = Vec3(1.0f, 0.7f, 0.6f);
    pointLights[2].constant = 1.0f;
    pointLights[2].linear = 0.14f;
    pointLights[2].quadratic = 0.07f;
    pointLights[2].enabled = true;

    numActivePointLights = 3;
}
void init() {
    glewInit();
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f);

    initShaders();  // 이 안에서 defaultTexture, defaultNormalMap 생성됨
    initPointLights();
    loadModels();   // 이 안에서 실제 텍스처 로딩

    enemies.push_back(DestructibleEnemy(-1.0f, 1.0f, 0));
    enemies.push_back(DestructibleEnemy(0.0f, 1.5f, 0));
    enemies.push_back(DestructibleEnemy(1.0f, 1.0f, 0));

    lastTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutCreateWindow("Assignment 4: Enhanced GLSL 3D Bullet Hell");

    init();

    glutDisplayFunc(render);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutTimerFunc(16, timer, 0);

    std::cout << "=== Assignment 4: 3D Shading ===" << std::endl;
    std::cout << "\nBasic Controls:" << std::endl;
    std::cout << "WASD: Move player" << std::endl;
    std::cout << "Space: Shoot" << std::endl;
    std::cout << "\nShading & Lighting:" << std::endl;
    std::cout << "W: Toggle Shading Mode (Gouraud / Phong / Phong+NormalMap)" << std::endl;
    std::cout << "L: Toggle Directional Light ON/OFF" << std::endl;
    std::cout << "P: Toggle Point Light ON/OFF" << std::endl;
    std::cout << "\nTexture Controls:" << std::endl;
    std::cout << "T: Toggle Textures ON/OFF" << std::endl;
    std::cout << "N: Toggle Normal Maps ON/OFF" << std::endl;
    std::cout << "\nCamera Controls:" << std::endl;
    std::cout << "C: Toggle Camera (Top-View ↔ Third-Person)" << std::endl;
    std::cout << "  1: Top-View Perspective (shortcut)" << std::endl;
    std::cout << "  3: Third-Person Perspective (shortcut)" << std::endl;
    std::cout << "\nRender Mode (Keys 4-6):" << std::endl;
    std::cout << "4: Opaque Polygon" << std::endl;
    std::cout << "5: Wireframe" << std::endl;
    std::cout << "6: Hidden Line Removal Wireframe" << std::endl;
    std::cout << "\nR: Reset game" << std::endl;

    glutMainLoop();
    return 0;
}