#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Header/Util.h"

// ========== KONSTANTE ==========
const float TARGET_FPS = 75.0f;
const float FRAME_TIME = 1.0f / TARGET_FPS;
const int NUM_STATIONS = 10;
const float BUS_SPEED = 0.15f;
const float STATION_WAIT_TIME = 10.0f;

// ========== STRUKTURE ==========
struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Station {
    Vec2 position;
    int number;
};

struct Passenger {
    glm::vec3 position;
    glm::vec3 targetPosition;
    glm::vec3 finalPosition;
    float moveSpeed;
    bool isMoving;
    int characterModel;
    bool isInspector;
    int waypointIndex;  // 0=start, 1=outside door, 2=in doorway, 3=inside, 4=final seat
    
    // Random boje za putnika
    glm::vec3 shirtColor;
    glm::vec3 pantsColor;
    glm::vec3 hairColor;
    
    // Animacija hodanja
    float walkAnimTime;
    float legSwingAmount;
    
    Passenger() : position(0), targetPosition(0), finalPosition(0), moveSpeed(1.0f), 
                  isMoving(false), characterModel(0), isInspector(false), waypointIndex(0),
                  shirtColor(0.3f, 0.5f, 0.8f), pantsColor(0.2f, 0.2f, 0.6f), hairColor(0.2f, 0.15f, 0.1f),
                  walkAnimTime(0.0f), legSwingAmount(0.15f) {}
};

// ========== GLOBALNE PROMENLJIVE ==========
Station stations[NUM_STATIONS];
int currentStation = 0;
int nextStation = 1;
float busProgress = 0.0f;
bool busAtStation = true;
float stationTimer = 0.0f;
int passengers = 0;
bool isInspectorInBus = false;
int totalFines = 0;
int inspectorExitStation = -1;

bool leftMousePressed = false;
bool rightMousePressed = false;
bool keyKPressed = false;

unsigned int pathVAO, pathVBO;
unsigned int circleVAO, circleVBO;

// 3D promenljive
bool firstMouse = true;
float lastX = 500.0f, lastY = 500.0f;
float yaw = -90.0f, pitch = -5.0f;
glm::vec3 cameraFront = glm::vec3(0.0, 0.0, -1.0);
float fov = 60.0f;

bool useTex = false;
bool transparent = false;
float doorOffset = 0.4f;
bool doorOpening = false;
bool doorClosing = false;
const float doorSpeed = 0.02f;
const float doorMaxOffset = 0.4f;

float wheelRotation = 0.0f;
const float wheelRotationSpeed = 2.0f;
const float wheelMaxRotation = 45.0f;

float busShakeOffset = 0.0f;
float busShakeTime = 0.0f;
const float busShakeSpeed = 3.0f;
const float busShakeAmplitude = 0.005f;

std::vector<Passenger> activePassengers;
bool passengerEntering = false;
bool passengerExiting = false;
float passengerAnimTimer = 0.0f;
const float passengerAnimDuration = 0.8f;  // Brže ulazenje - 0.8s umesto 2s

// Stanje depth testa i face cullinga (kontrolisano tasterima)
bool depthTestEnabled = true;
bool faceCullingEnabled = false;

// Framebuffer za 2D display
unsigned int displayFBO = 0;
unsigned int displayTexture = 0;
unsigned int displayRBO = 0;
const int DISPLAY_WIDTH = 800;
const int DISPLAY_HEIGHT = 600;

// 3D svet - putanja i stanice
unsigned int roadVAO, roadVBO;
unsigned int station3DVAO, station3DVBO;
const float ROAD_LENGTH = 500.0f;  // Dužina puta ispred autobusa
const float STATION_DISTANCE = 50.0f;  // Razmak između stanica

// ========== CALLBACK FUNKCIJE ==========
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_K && action == GLFW_PRESS) {
        keyKPressed = true;
    }
    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        if (doorOffset < doorMaxOffset && !doorClosing) {
            doorOpening = true;
            doorClosing = false;
        }
        else if (doorOffset >= doorMaxOffset || doorOpening) {
            doorOpening = false;
            doorClosing = true;
        }
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (yaw > 0.0f)
        yaw = 0.0f;
    if (yaw < -180.0f)
        yaw = -180.0f;

    if (pitch > 90.0f)
        pitch = 90.0f;
    if (pitch < -90.0f)
        pitch = -90.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        leftMousePressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rightMousePressed = true;
    }
}

// ========== HELPER FUNKCIJE ==========
Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

Vec2 bezierQuadratic(Vec2 p0, Vec2 p1, Vec2 p2, float t) {
    float u = 1.0f - t;
    return Vec2(
        u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
        u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y
    );
}

void initStations() {

    stations[0].position = Vec2(-0.65f, 0.55f);   // Top-left area
    stations[1].position = Vec2(-0.25f, 0.65f);   // Top-center-left
    stations[2].position = Vec2(0.35f, 0.60f);    // Top-right area
    stations[3].position = Vec2(0.70f, 0.25f);    // Right side, upper
    stations[4].position = Vec2(0.75f, -0.15f);   // Right side, lower
    stations[5].position = Vec2(0.45f, -0.55f);   // Bottom-right
    stations[6].position = Vec2(0.0f, -0.65f);    // Bottom-center
    stations[7].position = Vec2(-0.50f, -0.50f);  // Bottom-left
    stations[8].position = Vec2(-0.75f, -0.10f);  // Left side, lower
    stations[9].position = Vec2(-0.70f, 0.20f);   // Left side, upper

    for (int i = 0; i < NUM_STATIONS; i++) {
        stations[i].number = i;
    }
}

void setupPathVAO() {
    std::vector<float> pathVertices;

    for (int i = 0; i < NUM_STATIONS; i++) {
        int nextIdx = (i + 1) % NUM_STATIONS;
        Vec2 p0 = stations[i].position;
        Vec2 p2 = stations[nextIdx].position;

        Vec2 dir = Vec2(p2.x - p0.x, p2.y - p0.y);
        float dist = sqrt(dir.x * dir.x + dir.y * dir.y);
        Vec2 normal = Vec2(-dir.y, dir.x);

        if (dist > 0.0001f) {
            normal.x /= dist;
            normal.y /= dist;
        }


        float curvature = 0.12f + 0.08f * sin(i * 0.7f);

        float curveDir = (i % 3 == 0) ? -1.0f : 1.0f;

        Vec2 midPoint = Vec2((p0.x + p2.x) / 2.0f, (p0.y + p2.y) / 2.0f);
        Vec2 controlPoint = Vec2(
            midPoint.x + normal.x * curvature * curveDir,
            midPoint.y + normal.y * curvature * curveDir
        );

        int segments = 30;
        for (int j = 0; j <= segments; j++) {
            float t = (float)j / (float)segments;
            Vec2 point = bezierQuadratic(p0, controlPoint, p2, t);
            pathVertices.push_back(point.x);
            pathVertices.push_back(point.y);
        }
    }

    glGenVertexArrays(1, &pathVAO);
    glGenBuffers(1, &pathVBO);

    glBindVertexArray(pathVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pathVBO);
    glBufferData(GL_ARRAY_BUFFER, pathVertices.size() * sizeof(float), pathVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void setupCircleVAO() {
    std::vector<float> circleVertices;
    int segments = 50;

    for (int i = 0; i <= segments; i++) {
        float angle = (2.0f * 3.14159f * i) / segments;
        circleVertices.push_back(cos(angle));
        circleVertices.push_back(sin(angle));
    }

    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void setModelMatrix(unsigned int shaderProgram, float x, float y, float width, float height) {
    float model[16] = {
        width, 0.0f, 0.0f, 0.0f,
        0.0f, height, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, model);
}

void renderTexture(unsigned int texture, float x, float y, float w, float h, float alpha, unsigned int shaderProgram, unsigned int VAO) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), alpha);
    setModelMatrix(shaderProgram, x, y, w, h);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void renderCircle(float x, float y, float radius, float r, float g, float b, unsigned int shaderProgram) {
    setModelMatrix(shaderProgram, x, y, radius, radius);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), r, g, b);
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 1);

    glBindVertexArray(circleVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 52);

    glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 0);
}

// ========== 3D HELPER FUNKCIJE ==========
void setupRoad3D() {
    std::vector<float> roadVertices;
    
    // Kreiranje puta ispod autobusa 
    float roadWidth = 50.0f;
    float roadStart = 50.0f;
    float roadEnd = -ROAD_LENGTH;
    
    // Asfalt
    roadVertices.insert(roadVertices.end(), {
        // Leva strana puta
        -roadWidth / 2, -1.2f, roadStart,   0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         roadWidth / 2, -1.2f, roadStart,   0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         roadWidth / 2, -1.2f, roadEnd,     0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -roadWidth / 2, -1.2f, roadEnd,     0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // Bela linija (centralna) - malo iznad asfalta
    roadVertices.insert(roadVertices.end(), {
        -0.1f, -1.19f, roadStart,   1.0f, 1.0f, 1.0f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         0.1f, -1.19f, roadStart,   1.0f, 1.0f, 1.0f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         0.1f, -1.19f, roadEnd,     1.0f, 1.0f, 1.0f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -0.1f, -1.19f, roadEnd,     1.0f, 1.0f, 1.0f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // Trava sa leve strane - sire
    roadVertices.insert(roadVertices.end(), {
        -500.0f, -1.2f, roadStart,   0.2f, 0.6f, 0.2f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
        -roadWidth / 2, -1.2f, roadStart,   0.2f, 0.6f, 0.2f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
        -roadWidth / 2, -1.2f, roadEnd,     0.2f, 0.6f, 0.2f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -500.0f, -1.2f, roadEnd,     0.2f, 0.6f, 0.2f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // Trava sa desne strane - sire
    roadVertices.insert(roadVertices.end(), {
         roadWidth / 2, -1.2f, roadStart,   0.2f, 0.6f, 0.2f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         500.0f, -1.2f, roadStart,   0.2f, 0.6f, 0.2f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         500.0f, -1.2f, roadEnd,     0.2f, 0.6f, 0.2f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
         roadWidth / 2, -1.2f, roadEnd,     0.2f, 0.6f, 0.2f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    glGenVertexArrays(1, &roadVAO);
    glGenBuffers(1, &roadVBO);
    
    glBindVertexArray(roadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, roadVBO);
    glBufferData(GL_ARRAY_BUFFER, roadVertices.size() * sizeof(float), roadVertices.data(), GL_STATIC_DRAW);
    
    unsigned int stride = (3 + 4 + 2 + 3) * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
}

void setupStation3D() {
    std::vector<float> stationVertices;
    
    float stationWidth = 3.0f;   // Šira stanica
    float stationHeight = 0.3f;  // Niska platforma
    float stationDepth = 1.5f;
    float roofHeight = 2.5f;     // Visina krova iznad poda
    
    // ===== PLATFORMA (POD) =====
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2, -1.2f + stationHeight, -stationDepth/2,   0.5f, 0.5f, 0.5f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2, -1.2f + stationHeight, -stationDepth/2,   0.5f, 0.5f, 0.5f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2, -1.2f + stationHeight,  stationDepth/2,   0.5f, 0.5f, 0.5f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -stationWidth/2, -1.2f + stationHeight,  stationDepth/2,   0.5f, 0.5f, 0.5f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // Prednja strana platforme
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2, -1.2f, -stationDepth/2,   0.4f, 0.4f, 0.4f, 1.0f,   0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2, -1.2f, -stationDepth/2,   0.4f, 0.4f, 0.4f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2, -1.2f + stationHeight, -stationDepth/2,   0.4f, 0.4f, 0.4f, 1.0f,   1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        -stationWidth/2, -1.2f + stationHeight, -stationDepth/2,   0.4f, 0.4f, 0.4f, 1.0f,   0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    });
    
    // ===== ZADNJI ZID (Zaštitni zid) =====
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2, -1.2f + stationHeight, stationDepth/2,   0.8f, 0.8f, 0.7f, 1.0f,   0.0f, 0.0f,   0.0f, 0.0f, -1.0f,
         stationWidth/2, -1.2f + stationHeight, stationDepth/2,   0.8f, 0.8f, 0.7f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f, -1.0f,
         stationWidth/2, -1.2f + roofHeight, stationDepth/2,   0.8f, 0.8f, 0.7f, 1.0f,   1.0f, 1.0f,   0.0f, 0.0f, -1.0f,
        -stationWidth/2, -1.2f + roofHeight, stationDepth/2,   0.8f, 0.8f, 0.7f, 1.0f,   0.0f, 1.0f,   0.0f, 0.0f, -1.0f,
    });
    
    // ===== STUBOVI (Levi i desni) =====
    // Levi stub
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2 + 0.2f, -1.2f + stationHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
        -stationWidth/2 + 0.4f, -1.2f + stationHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
        -stationWidth/2 + 0.4f, -1.2f + roofHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        -stationWidth/2 + 0.2f, -1.2f + roofHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    });
    
    // Desni stub
    stationVertices.insert(stationVertices.end(), {
         stationWidth/2 - 0.4f, -1.2f + stationHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2 - 0.2f, -1.2f + stationHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2 - 0.2f, -1.2f + roofHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2 - 0.4f, -1.2f + roofHeight, -stationDepth/2 + 0.2f,   0.3f, 0.3f, 0.3f, 1.0f,   0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    });
    
    // ===== KROV =====
    // Donji deo krova
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2 - 0.3f, -1.2f + roofHeight, -stationDepth/2 - 0.5f,   0.7f, 0.1f, 0.1f, 1.0f,   0.0f, 0.0f,   0.0f, -1.0f, 0.0f,
         stationWidth/2 + 0.3f, -1.2f + roofHeight, -stationDepth/2 - 0.5f,   0.7f, 0.1f, 0.1f, 1.0f,   1.0f, 0.0f,   0.0f, -1.0f, 0.0f,
         stationWidth/2 + 0.3f, -1.2f + roofHeight, stationDepth/2,   0.7f, 0.1f, 0.1f, 1.0f,   1.0f, 1.0f,   0.0f, -1.0f, 0.0f,
        -stationWidth/2 - 0.3f, -1.2f + roofHeight, stationDepth/2,   0.7f, 0.1f, 0.1f, 1.0f,   0.0f, 1.0f,   0.0f, -1.0f, 0.0f,
    });
    
    // Vrh krova
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2 - 0.3f, -1.2f + roofHeight + 0.2f, -stationDepth/2 - 0.5f,   0.8f, 0.2f, 0.2f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2 + 0.3f, -1.2f + roofHeight + 0.2f, -stationDepth/2 - 0.5f,   0.8f, 0.2f, 0.2f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2 + 0.3f, -1.2f + roofHeight + 0.2f, stationDepth/2,   0.8f, 0.2f, 0.2f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -stationWidth/2 - 0.3f, -1.2f + roofHeight + 0.2f, stationDepth/2,   0.8f, 0.2f, 0.2f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // ===== KLUPA =====
    // Površina klupe
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2 + 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.6f,   0.6f, 0.4f, 0.2f, 1.0f,   0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2 - 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.6f,   0.6f, 0.4f, 0.2f, 1.0f,   1.0f, 0.0f,   0.0f, 1.0f, 0.0f,
         stationWidth/2 - 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.4f,   0.6f, 0.4f, 0.2f, 1.0f,   1.0f, 1.0f,   0.0f, 1.0f, 0.0f,
        -stationWidth/2 + 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.4f,   0.6f, 0.4f, 0.2f, 1.0f,   0.0f, 1.0f,   0.0f, 1.0f, 0.0f,
    });
    
    // Naslon klupe
    stationVertices.insert(stationVertices.end(), {
        -stationWidth/2 + 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.4f,   0.5f, 0.3f, 0.15f, 1.0f,   0.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2 - 0.8f, -1.2f + stationHeight + 0.5f, stationDepth/2 - 0.4f,   0.5f, 0.3f, 0.15f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
         stationWidth/2 - 0.8f, -1.2f + stationHeight + 1.0f, stationDepth/2 - 0.4f,   0.5f, 0.3f, 0.15f, 1.0f,   1.0f, 1.0f,   0.0f, 0.0f, 1.0f,
        -stationWidth/2 + 0.8f, -1.2f + stationHeight + 1.0f, stationDepth/2 - 0.4f,   0.5f, 0.3f, 0.15f, 1.0f,   0.0f, 1.0f,   0.0f, 0.0f, 1.0f,
    });
    
    glGenVertexArrays(1, &station3DVAO);
    glGenBuffers(1, &station3DVBO);
    
    glBindVertexArray(station3DVAO);
    glBindBuffer(GL_ARRAY_BUFFER, station3DVBO);
    glBufferData(GL_ARRAY_BUFFER, stationVertices.size() * sizeof(float), stationVertices.data(), GL_STATIC_DRAW);
    
    unsigned int stride = (3 + 4 + 2 + 3) * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);
}

void setupDisplayFramebuffer() {
    glGenFramebuffers(1, &displayFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, displayFBO);

    glGenTextures(1, &displayTexture);
    glBindTexture(GL_TEXTURE_2D, displayTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, displayTexture, 0);

    glGenRenderbuffers(1, &displayRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, displayRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, displayRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Framebuffer nije kompletan!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void render2DDisplay(unsigned int shader2D, unsigned int VAO2D, unsigned int* numberTextures,
                 unsigned int busTexture, unsigned int doorClosedTexture, 
                 unsigned int doorOpenTexture, unsigned int passengersLabelTexture,
                 unsigned int finesLabelTexture, unsigned int controlTexture) {
    
glBindFramebuffer(GL_FRAMEBUFFER, displayFBO);
glViewport(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
glClearColor(0.15f, 0.2f, 0.25f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

// PRIVREMENO iskljuci depth test samo za 2D renderovanje u framebuffer
GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
glDisable(GL_DEPTH_TEST);

glUseProgram(shader2D);
glBindVertexArray(VAO2D);

    glUniform1i(glGetUniformLocation(shader2D, "uUseColor"), 1);
    glUniform3f(glGetUniformLocation(shader2D, "uColor"), 0.8f, 0.1f, 0.1f);
    glUniform1f(glGetUniformLocation(shader2D, "uAlpha"), 1.0f);

    float identityMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(shader2D, "uModel"), 1, GL_FALSE, identityMatrix);

    glBindVertexArray(pathVAO);
    for (int i = 0; i < NUM_STATIONS; i++) {
        glDrawArrays(GL_LINE_STRIP, i * 31, 31);
    }

    glUniform1i(glGetUniformLocation(shader2D, "uUseColor"), 0);

    for (int i = 0; i < NUM_STATIONS; i++) {
        renderCircle(stations[i].position.x, stations[i].position.y, 0.06f, 0.8f, 0.1f, 0.1f, shader2D);
    }

    glBindVertexArray(VAO2D);
    for (int i = 0; i < NUM_STATIONS; i++) {
        renderTexture(numberTextures[i], stations[i].position.x, stations[i].position.y,
            0.05f, 0.06f, 1.0f, shader2D, VAO2D);
    }
    
    Vec2 busPos;
    if (busAtStation) {
        busPos = stations[currentStation].position;
    }
    else {
        int prevIdx = currentStation;
        int nextIdx = nextStation;
        Vec2 p0 = stations[prevIdx].position;
        Vec2 p2 = stations[nextIdx].position;

        Vec2 dir = Vec2(p2.x - p0.x, p2.y - p0.y);
        float dist = sqrt(dir.x * dir.x + dir.y * dir.y);
        Vec2 normal = Vec2(-dir.y, dir.x);

        if (dist > 0.0001f) {
            normal.x /= dist;
            normal.y /= dist;
        }

        float curvature = 0.12f + 0.08f * sin(prevIdx * 0.7f);
        float curveDir = (prevIdx % 3 == 0) ? -1.0f : 1.0f;

        Vec2 midPoint = Vec2((p0.x + p2.x) / 2.0f, (p0.y + p2.y) / 2.0f);
        Vec2 controlPoint = Vec2(
            midPoint.x + normal.x * curvature * curveDir,
            midPoint.y + normal.y * curvature * curveDir
        );

        busPos = bezierQuadratic(p0, controlPoint, p2, busProgress);
    }
    renderTexture(busTexture, busPos.x, busPos.y, 0.15f, 0.08f, 1.0f, shader2D, VAO2D);

    unsigned int doorTexture = busAtStation ? doorOpenTexture : doorClosedTexture;
    renderTexture(doorTexture, -0.85f, 0.75f, 0.12f, 0.18f, 1.0f, shader2D, VAO2D);

    renderTexture(passengersLabelTexture, -0.90f, -0.65f, 0.20f, 0.08f, 1.0f, shader2D, VAO2D);

    int tens = passengers / 10;
    int ones = passengers % 10;
    renderTexture(numberTextures[tens], -0.90f, -0.75f, 0.08f, 0.1f, 1.0f, shader2D, VAO2D);
    renderTexture(numberTextures[ones], -0.80f, -0.75f, 0.08f, 0.1f, 1.0f, shader2D, VAO2D);

    renderTexture(finesLabelTexture, -0.90f, -0.83f, 0.20f, 0.08f, 1.0f, shader2D, VAO2D);

    int finesTens = (totalFines / 10) % 10;
    int finesOnes = totalFines % 10;
    renderTexture(numberTextures[finesTens], -0.90f, -0.93f, 0.08f, 0.1f, 1.0f, shader2D, VAO2D);
    renderTexture(numberTextures[finesOnes], -0.80f, -0.93f, 0.08f, 0.1f, 1.0f, shader2D, VAO2D);

    if (isInspectorInBus) {
        renderTexture(controlTexture, 0.85f, 0.75f, 0.12f, 0.12f, 1.0f, shader2D, VAO2D);
    }

    // VRATI prethodno stanje depth testa
    if (depthTestWasEnabled) {
        glEnable(GL_DEPTH_TEST);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void addPassenger(bool isInsp = false) {
    Passenger p;
    // WAYPOINT 0: Pocetna pozicija - van autobusa DIREKTNO ISPRED VRATA
    p.position = glm::vec3(1.2f, -0.3f, -0.075f);  // Direktno ispred vrata (centralno)
    
    // WAYPOINT 1: Pozicija u pragu vrata (ulazi pravo)
    p.targetPosition = glm::vec3(1.05f, -0.3f, -0.075f);  // U pragu vrata
    
    // Konačna pozicija - pozadi vozaca (grid raspored)
    int rowOffset = activePassengers.size() / 4;
    int colOffset = activePassengers.size() % 4;
    
    p.finalPosition = glm::vec3(
        0.5f + colOffset * 0.25f,  // Desno od vozača
        -0.3f,                      // Ista visina
        0.8f + rowOffset * 0.4f     // Pozadi vozača
    );
    
    p.moveSpeed = 1.2f;  // Brža brzina
    p.isMoving = true;
    p.waypointIndex = 0;  // Počinje od waypoint-a 0
    p.characterModel = isInsp ? 15 : (rand() % 15);
    p.isInspector = isInsp;
    
    // Random boje za putnike, kontrolor ima uniformu
    if (isInsp) {
        p.shirtColor = glm::vec3(0.1f, 0.1f, 0.1f);
        p.pantsColor = glm::vec3(0.05f, 0.05f, 0.05f);
        // Kontrolor nema kosu (ima kapicu)
    } else {
        p.shirtColor = glm::vec3(
            0.2f + (rand() % 80) / 100.0f,
            0.2f + (rand() % 80) / 100.0f,
            0.2f + (rand() % 80) / 100.0f
        );
        p.pantsColor = glm::vec3(
            0.1f + (rand() % 50) / 100.0f,
            0.1f + (rand() % 50) / 100.0f,
            0.1f + (rand() % 50) / 100.0f
        );
        // Random boja kose (braon, crna, plava, crvena)
        int hairType = rand() % 4;
        if (hairType == 0) {
            p.hairColor = glm::vec3(0.2f, 0.15f, 0.1f);  // Braon
        } else if (hairType == 1) {
            p.hairColor = glm::vec3(0.05f, 0.05f, 0.05f);  // Crna
        } else if (hairType == 2) {
            p.hairColor = glm::vec3(0.9f, 0.85f, 0.5f);  // Plava
        } else {
            p.hairColor = glm::vec3(0.4f, 0.1f, 0.05f);  // Crvena
        }
    }
    
    activePassengers.push_back(p);
}

void removePassenger(bool removeInspector = false) {
    if (activePassengers.empty()) return;
    
    int removeIdx = -1;
    
    if (removeInspector) {
        for (int i = 0; i < activePassengers.size(); i++) {
            if (activePassengers[i].isInspector) {
                removeIdx = i;
                break;
            }
        }
    } else {
        removeIdx = activePassengers.size() - 1;
    }
    
    if (removeIdx >= 0) {
        // Postavi waypoint za izlazak (obrnuti redosled od ulaska)
        activePassengers[removeIdx].waypointIndex = 10;  // Počinje izlazak (sa sedišta)
        // Prvo ide u centar hodnika (biće izračunato u updatePassengers)
        activePassengers[removeIdx].isMoving = true;
    }
}

void updatePassengers(float dt) {
    for (auto it = activePassengers.begin(); it != activePassengers.end(); ) {
        if (it->isMoving) {
            glm::vec3 direction = it->targetPosition - it->position;
            float distance = glm::length(direction);
            
            // Animacija hodanja - povećava vreme animacije dok se kreće
            it->walkAnimTime += dt * 8.0f;  // Brzina animacije hodanja
            
            if (distance < 0.05f) {
                // Stigao do trenutnog waypoint-a
                it->position = it->targetPosition;
                
                // ULAZAK U AUTOBUS (waypoint 0-3) - PRAVO KROZ VRATA
                if (it->waypointIndex == 0) {
                    // U pragu vrata → idi unutra (pravo kroz vrata, samo X se menja)
                    it->waypointIndex = 1;
                    it->targetPosition = glm::vec3(0.85f, -0.3f, -0.075f);  // Unutra, pravo
                }
                else if (it->waypointIndex == 1) {
                    // Unutra od vrata → idi u hodnik (počinje bočno kretanje ka sedištu)
                    it->waypointIndex = 2;
                    // Sada može da se pomera bočno ka svom sedištu
                    float centerZ = (it->finalPosition.z - 0.075f) / 2.0f - 0.075f;
                    it->targetPosition = glm::vec3(0.7f, -0.3f, centerZ);  // Pomeraj se ka sedištu
                }
                else if (it->waypointIndex == 2) {
                    // U hodniku → idi ka sedištu (direktno)
                    it->waypointIndex = 3;
                    it->targetPosition = it->finalPosition;
                }
                else if (it->waypointIndex == 3) {
                    // Stigao na sedište → STANI
                    it->isMoving = false;
                    it->walkAnimTime = 0.0f;  // Resetuj animaciju kada stane
                }
                
                // IZLAZAK IZ AUTOBUSA (waypoint 10-13) - PRAVO KROZ VRATA
                else if (it->waypointIndex == 10) {
                    // Sa sedišta → idi ka hodniku (sredina puta)
                    it->waypointIndex = 11;
                    float centerZ = (it->position.z - 0.075f) / 2.0f - 0.075f;
                    it->targetPosition = glm::vec3(0.7f, -0.3f, centerZ);  // Hodnik
                }
                else if (it->waypointIndex == 11) {
                    // Iz hodnika → idi ka vratima (centralno, direktno)
                    it->waypointIndex = 12;
                    it->targetPosition = glm::vec3(0.85f, -0.3f, -0.075f);  // Kod vrata, centralno
                }
                else if (it->waypointIndex == 12) {
                    // Kod vrata (unutra) → idi u prag vrata (pravo napolje)
                    it->waypointIndex = 13;
                    it->targetPosition = glm::vec3(1.05f, -0.3f, -0.075f);  // Prag vrata
                }
                else if (it->waypointIndex == 13) {
                    // U pragu → idi napolje (pravo van autobusa)
                    it->waypointIndex = 14;
                    it->targetPosition = glm::vec3(1.2f, -0.3f, -0.075f);  // Spolja, centralno
                }
                else if (it->waypointIndex == 14) {
                    // Napolje → OBRIŠI putnika
                    it = activePassengers.erase(it);
                    continue;
                }
            } else {
                // Normalno kretanje ka target poziciji
                glm::vec3 moveDir = glm::normalize(direction);
                
                // Uspori kod prolaska kroz vrata (waypoint 0 i 12-13)
                if (it->waypointIndex == 0 || it->waypointIndex == 12 || it->waypointIndex == 13) {
                    it->position += moveDir * (it->moveSpeed * 0.7f) * dt;  // 70% brzine u vratima
                } else {
                    it->position += moveDir * it->moveSpeed * dt;  // Normalna brzina
                }
            }
        } else {
            // Kada ne hoda, resetuj animaciju
            it->walkAnimTime = 0.0f;
        }
        ++it;
    }
}

// ========== MAIN ==========
int main()
{
    srand(time(NULL));

    // ========== INICIJALIZACIJA GLFW ==========
    if (!glfwInit()) {
        std::cout << "GLFW nije inicijalizovan!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "3D Autobus - Projekat", monitor, NULL);

    if (window == NULL) {
        std::cout << "Prozor nije kreiran!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // ========== INICIJALIZACIJA GLEW ==========
    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW nije inicijalizovan!" << std::endl;
        return -1;
    }

    std::cout << "OpenGL verzija: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL verzija: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // ========== PODESAVANJA OPENGL ==========
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, mode->width, mode->height);
    glLineWidth(3.0f);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    
    // Face culling podesavanja - inicijalno ISKLJUCENO
    glCullFace(GL_BACK);  // Definise koje strane se odstranjuju (zadnje)
    glFrontFace(GL_CCW);  // Definise front face (counter-clockwise)

    // ========== UCITAVANJE SEJDERA ==========
    std::cout << "\n=== UCITAVANJE SEJDERA ===" << std::endl;
    unsigned int shader2D = createShader("Resource Files/Shaders/basic.vert", "Resource Files/Shaders/basic.frag");
    unsigned int shader3D = createShader("Resource Files/Shaders/basic3d.vert", "Resource Files/Shaders/basic3d.frag");
    
    if (shader2D == 0 || shader3D == 0) {
        std::cout << "GRESKA: Sejderi nisu ucitani!" << std::endl;
        return -1;
    }
    std::cout << "Sejderi uspesno ucitani!" << std::endl;

    // ========== UCITAVANJE TEKSTURA ==========
    std::cout << "\n=== UCITAVANJE TEKSTURA ===" << std::endl;

    unsigned int busTexture = loadImageToTexture("Resource Files/Textures/2d_bus.png");
    unsigned int stationTexture = loadImageToTexture("Resource Files/Textures/bus_station.png");
    unsigned int controlTexture = loadImageToTexture("Resource Files/Textures/bus_control.png");
    unsigned int doorClosedTexture = loadImageToTexture("Resource Files/Textures/closed_doors.png");
    unsigned int doorOpenTexture = loadImageToTexture("Resource Files/Textures/opened_doors.png");
    unsigned int authorTexture = loadImageToTexture("Resource Files/Textures/author_text.png");
    unsigned int passengersLabelTexture = loadImageToTexture("Resource Files/Textures/passangers_label.png");
    unsigned int finesLabelTexture = loadImageToTexture("Resource Files/Textures/fines.png");

    unsigned int numberTextures[10];
    for (int i = 0; i < 10; i++) {
        std::string path = "Resource Files/Textures/number_" + std::to_string(i) + ".png";
        numberTextures[i] = loadImageToTexture(path.c_str());
    }

    if (busTexture == 0 || stationTexture == 0 || doorClosedTexture == 0 || passengersLabelTexture == 0 || finesLabelTexture == 0) {
        std::cout << "GRESKA: Neke teksture nisu ucitane!" << std::endl;
        return -1;
    }

    std::cout << "=== SVE TEKSTURE USPESNO UCITANE ===" << std::endl;

    // ========== VAO/VBO/EBO ZA 2D TEKSTURE ==========
    float vertices2D[] = {
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
        -0.5f,  0.5f,   0.0f, 1.0f
    };

    unsigned int indices2D[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int VAO2D, VBO2D, EBO2D;
    glGenVertexArrays(1, &VAO2D);
    glGenBuffers(1, &VBO2D);
    glGenBuffers(1, &EBO2D);

    glBindVertexArray(VAO2D);

    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices2D), vertices2D, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO2D);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices2D), indices2D, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // ========== VAO/VBO ZA 3D SCENU ==========
    float vertices3D[] = {
        //X    Y    Z      R    G    B    A         S   T      Nx Ny Nz
        
        // KONTROLNI PANEL
        -0.8,-0.3,-0.5,   0.2, 0.2, 0.2, 1.0,     0,  0,     0, 0, 1,
         0.8,-0.3,-0.5,   0.2, 0.2, 0.2, 1.0,     1,  0,     0, 0, 1,
         0.8,-0.5,-0.3,   0.2, 0.2, 0.2, 1.0,     1,  1,     0, 0, 1,
        -0.8,-0.5,-0.3,   0.2, 0.2, 0.2, 1.0,     0,  1,     0, 0, 1,
        
        -0.8,-0.3,-0.5,   0.3, 0.3, 0.3, 1.0,     0,  0,     0, 1, 0,
        -0.8,-0.5,-0.3,   0.3, 0.3, 0.3, 1.0,     0,  1,     0, 1, 0,
         0.8,-0.5,-0.3,   0.3, 0.3, 0.3, 1.0,     1,  1,     0, 1, 0,
         0.8,-0.3,-0.5,   0.3, 0.3, 0.3, 1.0,     1,  0,     0, 1, 0,
        
        // VETROBRANSKO STAKLO
        -0.9, 0.4,-0.6,   0.6, 0.7, 0.8, 0.15,    0,  0,     0, 0, 1,
         0.9, 0.4,-0.6,   0.6, 0.7, 0.8, 0.15,    1,  0,     0, 0, 1,
         0.9,-0.3,-0.5,   0.6, 0.7, 0.8, 0.15,    1,  1,     0, 0, 1,
        -0.9,-0.3,-0.5,   0.6, 0.7, 0.8, 0.15,    0,  1,     0, 0, 1,
        
        // RAM VETROBRANA - gore
        -0.9, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  0,     0, 1, 0,
         0.9, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  0,     0, 1, 0,
         0.9, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  1,     0, 1, 0,
        -0.9, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  1,     0, 1, 0,
        
        // RAM VETROBRANA - levo
        -0.9, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  0,     -1, 0, 0,
        -0.9, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  1,     -1, 0, 0,
        -1.0, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  1,     -1, 0, 0,
        -1.0, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  0,     -1, 0, 0,
        
        // RAM VETROBRANA - desno
         0.9, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  0,     1, 0, 0,
         1.0, 0.4,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  0,     1, 0, 0,
         1.0, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     1,  1,     1, 0, 0,
         0.9, 0.5,-0.6,   0.1, 0.1, 0.1, 1.0,     0,  1,     1, 0, 0,
        
        // LEVA STRANA KABINE
        -1.0, 0.5, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  0,     -1, 0, 0,
        -1.0, 0.5,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  0,     -1, 0, 0,
        -1.0,-0.6,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  1,     -1, 0, 0,
        -1.0,-0.6, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  1,     -1, 0, 0,
        
        // DESNA STRANA KABINE - gornji deo
         1.0, 0.5, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  0,     1, 0, 0,
         1.0, 0.35, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  1,     1, 0, 0,
         1.0, 0.35,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  1,     1, 0, 0,
         1.0, 0.5,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  0,     1, 0, 0,
        
        // POD KABINE
        -1.0,-0.6, 0.5,   0.25, 0.25, 0.25, 1.0,     0,  0,     0, -1, 0,
        -1.0,-0.6,-0.6,   0.25, 0.25, 0.25, 1.0,     0,  1,     0, -1, 0,
         1.0,-0.6,-0.6,   0.25, 0.25, 0.25, 1.0,     1,  1,     0, -1, 0,
         1.0,-0.6, 0.5,   0.25, 0.25, 0.25, 1.0,     1,  0,     0, -1, 0,
        
        // PLAFON KABINE
        -1.0, 0.5, 0.5,   0.9, 0.9, 0.85, 1.0,     0,  0,     0, 1, 0,
         1.0, 0.5, 0.5,   0.9, 0.9, 0.85, 1.0,     1,  0,     0, 1, 0,
         1.0, 0.5,-0.6,   0.9, 0.9, 0.85, 1.0,     1,  1,     0, 1, 0,
        -1.0, 0.5,-0.6,   0.9, 0.9, 0.85, 1.0,     0,  1,     0, 1, 0,
        
        // ZADNJA STRANA KABINE
        -1.0, 0.5, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  0,     0, 0, 1,
        -1.0,-0.6, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  1,     0, 0, 1,
         1.0,-0.6, 0.5,   0.85, 0.80, 0.70, 1.0,     1,  1,     0, 0, 1,
         1.0, 0.5, 0.5,   0.85, 0.80, 0.70, 1.0,     1,  0,     0, 0, 1,
        
        // VOLAN
        -0.15, -0.15, -0.4,   0.1, 0.1, 0.1, 1.0,     0,  0,     0, 0, 1,
         0.15, -0.15, -0.4,   0.1, 0.1, 0.1, 1.0,     1,  0,     0, 0, 1,
         0.15, -0.35, -0.4,   0.1, 0.1, 0.1, 1.0,     1,  1,     0, 0, 1,
        -0.15, -0.35, -0.4,   0.1, 0.1, 0.1, 1.0,     0,  1,     0, 0, 1,
        
        // 2D DISPLEJ
         0.2,  -0.05, -0.4,   0.1, 0.2, 0.4, 1.0,     0,  1,     0, 0, 1,
         0.5,  -0.05, -0.4,   0.1, 0.2, 0.4, 1.0,     1,  1,     0, 0, 1,
         0.5,  -0.35, -0.4,   0.1, 0.2, 0.4, 1.0,     1,  0,     0, 0, 1,
         0.2,  -0.35, -0.4,   0.1, 0.2, 0.4, 1.0,     0,  0,     0, 0, 1,
        
        // DESNA STRANA KABINE - prednji deo
         1.0, 0.35,-0.25,   0.85, 0.80, 0.70, 1.0,     0,  0,     1, 0, 0,
         1.0,-0.55,-0.25,   0.85, 0.80, 0.70, 1.0,     0,  1,     1, 0, 0,
         1.0,-0.55,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  1,     1, 0, 0,
         1.0, 0.35,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  0,     1, 0, 0,
        
        // DESNA STRANA KABINE - zadnji deo
         1.0, 0.35, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  0,     1, 0, 0,
         1.0,-0.55, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  1,     1, 0, 0,
         1.0,-0.55, 0.1,   0.85, 0.80, 0.70, 1.0,     1,  1,     1, 0, 0,
         1.0, 0.35, 0.1,   0.85, 0.80, 0.70, 1.0,     1,  0,     1, 0, 0,
        
        // DESNA STRANA KABINE - donji deo
         1.0,-0.55, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  0,     1, 0, 0,
         1.0,-0.6, 0.5,   0.85, 0.80, 0.70, 1.0,     0,  1,     1, 0, 0,
         1.0,-0.6,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  1,     1, 0, 0,
         1.0,-0.55,-0.6,   0.85, 0.80, 0.70, 1.0,     1,  0,     1, 0, 0,
        
        // VRATA - okvir gornji
         1.0, 0.35, 0.1,   0.05, 0.05, 0.05, 1.0,     0,  0,     1, 0, 0,
         1.02, 0.35, 0.1,   0.05, 0.05, 0.05, 1.0,     1,  0,     1, 0, 0,
         1.02, 0.35,-0.25,   0.05, 0.05, 0.05, 1.0,     1,  1,     1, 0, 0,
         1.0, 0.35,-0.25,   0.05, 0.05, 0.05, 1.0,     0,  1,     1, 0, 0,
        
        // Okvir vrata - levi
         1.0, 0.35,-0.25,   0.05, 0.05, 0.05, 1.0,     0,  0,     1, 0, 0,
         1.02, 0.35,-0.25,   0.05, 0.05, 0.05, 1.0,     1,  0,     1, 0, 0,
         1.02,-0.55,-0.25,   0.05, 0.05, 0.05, 1.0,     1,  1,     1, 0, 0,
         1.0,-0.55,-0.25,   0.05, 0.05, 0.05, 1.0,     0,  1,     1, 0, 0,
        
        // Okvir vrata - desni
         1.0, 0.35, 0.1,   0.05, 0.05, 0.05, 1.0,     0,  0,     1, 0, 0,
         1.02, 0.35, 0.1,   0.05, 0.05, 0.05, 1.0,     1,  0,     1, 0, 0,
         1.02,-0.55, 0.1,   0.05, 0.05, 0.05, 1.0,     1,  1,     1, 0, 0,
         1.0,-0.55, 0.1,   0.05, 0.05, 0.05, 1.0,     0,  1,     1, 0, 0,
        
        // Okvir vrata - donji
         1.0,-0.55, 0.1,   0.05, 0.05, 0.05, 1.0,     0,  0,     1, 0, 0,
         1.02,-0.55, 0.1,   0.05, 0.05, 0.05, 1.0,     1,  0,     1, 0, 0,
         1.02,-0.55,-0.25,   0.05, 0.05, 0.05, 1.0,     1,  1,     1, 0, 0,
         1.0,-0.55,-0.25,   0.05, 0.05, 0.05, 1.0,     0,  1,     1, 0, 0,
        
        // Površina vrata
         1.015, 0.33, 0.08,   0.7, 0.7, 0.75, 1.0,     0,  0,     1, 0, 0,
         1.015, 0.33,-0.23,   0.7, 0.7, 0.75, 1.0,     1,  0,     1, 0, 0,
         1.015,-0.53,-0.23,   0.7, 0.7, 0.75, 1.0,     1,  1,     1, 0, 0,
         1.015,-0.53, 0.08,   0.7, 0.7, 0.75, 1.0,     0,  1,     1, 0, 0,
        
        // SEDIŠTE VOZAČA - naslonska površina
        -0.3, 0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     0,  0,     0, 0, 1,
         0.3, 0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     1,  0,     0, 0, 1,
         0.3,-0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     1,  1,     0, 0, 1,
        -0.3,-0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     0,  1,     0, 0, 1,
        
        // SEDIŠTE VOZAČA - površina za sedenje
        -0.3,-0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     0,  0,     0, 1, 0,
         0.3,-0.2, 0.3,   0.2, 0.3, 0.5, 1.0,     1,  0,     0, 1, 0,
         0.3,-0.2, 0.0,   0.2, 0.3, 0.5, 1.0,     1,  1,     0, 1, 0,
        -0.3,-0.2, 0.0,   0.2, 0.3, 0.5, 1.0,     0,  1,     0, 1, 0,
    };
    
    unsigned int stride = (3 + 4 + 2 + 3) * sizeof(float);
    
    unsigned int VAO3D;
    glGenVertexArrays(1, &VAO3D);
    glBindVertexArray(VAO3D);

    unsigned int VBO3D;
    glGenBuffers(1, &VBO3D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO3D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices3D), vertices3D, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // HUMANOID VAO FOR PASSENGERS - POBOLJŠAN IZGLED
    std::vector<float> humanoidVertices;
    
    // GLAVA - zaobljena, realistične proporcije
    float headVertices[] = {
        // Prednja strana glave (blago zaobljena)
        -0.05f,  0.18f,  0.07f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.05f,  0.18f,  0.07f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana glave
        -0.05f,  0.18f, -0.07f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.05f,  0.18f, -0.07f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana glave (zaobljena)
        -0.06f,  0.19f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.06f,  0.19f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana glave (zaobljena)
         0.06f,  0.19f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.06f,  0.19f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        // Vrh glave (zaobljen)
        -0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        -0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         0.05f,  0.30f,  0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         0.05f,  0.30f, -0.06f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        // Vrat (povezuje glavu i trup)
        -0.03f,  0.18f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.03f,  0.18f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.03f,  0.18f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        -0.03f,  0.18f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
    };
    
    // TRUP - prirodnije proporcionisan
    float torsoVertices[] = {
        // Prednja strana trupa (širok na ramenima)
        -0.09f, -0.05f,  0.06f,  0.3f, 0.5f, 0.8f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.09f, -0.05f,  0.06f,  0.3f, 0.5f, 0.8f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.09f,  0.17f,  0.05f,  0.3f, 0.5f, 0.8f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.09f,  0.17f,  0.05f,  0.3f, 0.5f, 0.8f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana trupa
        -0.09f, -0.05f, -0.04f,  0.3f, 0.5f, 0.8f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.09f,  0.17f, -0.04f,  0.3f, 0.5f, 0.8f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.09f,  0.17f, -0.04f,  0.3f, 0.5f, 0.8f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.09f, -0.05f, -0.04f,  0.3f, 0.5f, 0.8f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana trupa
        -0.09f, -0.05f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.09f, -0.05f,  0.06f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.09f,  0.17f,  0.05f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.09f,  0.17f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana trupa
         0.09f, -0.05f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.09f,  0.17f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.09f,  0.17f,  0.05f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.09f, -0.05f,  0.06f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        // Gornji deo trupa (ramena)
        -0.09f,  0.17f, -0.04f,  0.32f, 0.52f, 0.82f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        -0.09f,  0.17f,  0.05f,  0.32f, 0.52f, 0.82f, 1.0f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         0.09f,  0.17f,  0.05f,  0.32f, 0.52f, 0.82f, 1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
         0.09f,  0.17f, -0.04f,  0.32f, 0.52f, 0.82f, 1.0f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        // Donji deo trupa (struk)
        -0.09f, -0.05f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.09f, -0.05f, -0.04f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.09f, -0.05f,  0.06f,  0.28f, 0.48f, 0.78f, 1.0f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        -0.09f, -0.05f,  0.06f,  0.28f, 0.48f, 0.78f, 1.0f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
    };
    
    // LEVA RUKA - cilindrični oblik
    float leftArmVertices[] = {
        // Prednja strana
        -0.12f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.09f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.09f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.12f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana
        -0.12f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.12f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.09f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.09f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana
        -0.12f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.12f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.12f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.12f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana (blizu trupa)
        -0.09f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        -0.09f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        -0.09f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        -0.09f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
    };
    
    // DESNA RUKA - cilindrični oblik
    float rightArmVertices[] = {
        // Prednja strana
         0.09f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.12f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.12f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         0.09f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana
         0.09f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         0.09f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.12f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.12f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana (blizu trupa)
         0.09f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
         0.09f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
         0.09f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
         0.09f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana
         0.12f, -0.04f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.12f,  0.16f, -0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.12f,  0.16f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.12f, -0.04f,  0.03f,  1.0f, 0.85f, 0.7f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
    };
    
    // LEVA NOGA - cilindrični oblik
    float leftLegVertices[] = {
        // Prednja strana
        -0.06f, -0.28f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.02f, -0.28f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.02f, -0.05f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.06f, -0.05f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana
        -0.06f, -0.28f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.06f, -0.05f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.02f, -0.05f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.02f, -0.28f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana
        -0.06f, -0.28f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.06f, -0.28f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.06f, -0.05f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.06f, -0.05f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana
        -0.02f, -0.28f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        -0.02f, -0.05f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        -0.02f, -0.05f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        -0.02f, -0.28f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
    };
    
    // DESNA NOGA - cilindrični oblik
    float rightLegVertices[] = {
        // Prednja strana
         0.02f, -0.28f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.06f, -0.28f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.06f, -0.05f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         0.02f, -0.05f,  0.04f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Zadnja strana
         0.02f, -0.28f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
         0.02f, -0.05f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.06f, -0.05f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.06f, -0.28f, -0.02f,  0.2f, 0.2f, 0.6f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        // Leva strana
         0.02f, -0.28f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
         0.02f, -0.28f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
         0.02f, -0.05f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
         0.02f, -0.05f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana
         0.06f, -0.28f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.06f, -0.05f, -0.02f,  0.18f, 0.18f, 0.58f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.06f, -0.05f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.06f, -0.28f,  0.04f,  0.18f, 0.18f, 0.58f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
    };
    
    // KOSA (za obične putnike) - jednostavna frizura
    float hairVertices[] = {
        // Gornji deo kose (pokriva vrh glave)
        -0.06f,  0.30f,  0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.06f,  0.30f,  0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.06f,  0.35f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        -0.06f,  0.35f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        // Prednji deo kose (šiške)
        -0.06f,  0.28f,  0.09f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.06f,  0.28f,  0.09f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.06f,  0.32f,  0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.06f,  0.32f,  0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Leva strana kose
        -0.07f,  0.28f, -0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.07f,  0.28f,  0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.06f,  0.34f,  0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        -0.06f,  0.34f, -0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
        // Desna strana kose
         0.07f,  0.28f, -0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.06f,  0.34f, -0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.06f,  0.34f,  0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
         0.07f,  0.28f,  0.06f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        // Zadnji deo kose
        -0.06f,  0.30f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        -0.06f,  0.34f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.06f,  0.34f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
         0.06f,  0.30f, -0.08f,  0.2f, 0.15f, 0.1f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
    };
    
    // KAPICA (za kontrolora) - policijska šilterica
    float capVertices[] = {
        // Vrh kapice (glavni deo)
        -0.07f,  0.30f,  0.07f,  0.05f, 0.05f, 0.1f, 1.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.07f,  0.30f,  0.07f,  0.05f, 0.05f, 0.1f, 1.0f,  1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.07f,  0.33f,  0.0f,   0.05f, 0.05f, 0.1f, 1.0f,  1.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        -0.07f,  0.33f,  0.0f,   0.05f, 0.05f, 0.1f, 1.0f,  0.0f, 1.0f,  0.0f, 1.0f, 0.0f,
        // Prednja strana kapice
        -0.07f,  0.30f,  0.07f,  0.05f, 0.05f, 0.1f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.07f,  0.33f,  0.0f,   0.05f, 0.05f, 0.1f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         0.07f,  0.33f,  0.0f,   0.05f, 0.05f, 0.1f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
         0.07f,  0.30f,  0.07f,  0.05f, 0.05f, 0.1f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        // Šilterica (štitnik kapice)
        -0.09f,  0.28f,  0.08f,  0.02f, 0.02f, 0.05f, 1.0f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.09f,  0.28f,  0.08f,  0.02f, 0.02f, 0.05f, 1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.09f,  0.27f,  0.16f,  0.02f, 0.02f, 0.05f, 1.0f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.09f,  0.27f,  0.16f,  0.02f, 0.02f, 0.05f, 1.0f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        // Donji deo štitnika
        -0.09f,  0.27f,  0.16f,  0.02f, 0.02f, 0.05f, 1.0f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.09f,  0.27f,  0.16f,  0.02f, 0.02f, 0.05f, 1.0f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
         0.09f,  0.28f,  0.08f,  0.02f, 0.02f, 0.05f, 1.0f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        -0.09f,  0.28f,  0.08f,  0.02f, 0.02f, 0.05f, 1.0f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
    };
    
    // Kombinuj sve delove u jedan buffer - BEZ KAPICE (kapica se renderuje zasebno)
    for (int i = 0; i < sizeof(headVertices) / sizeof(float); i++) humanoidVertices.push_back(headVertices[i]);
    for (int i = 0; i < sizeof(torsoVertices) / sizeof(float); i++) humanoidVertices.push_back(torsoVertices[i]);
    for (int i = 0; i < sizeof(leftArmVertices) / sizeof(float); i++) humanoidVertices.push_back(leftArmVertices[i]);
    for (int i = 0; i < sizeof(rightArmVertices) / sizeof(float); i++) humanoidVertices.push_back(rightArmVertices[i]);
    for (int i = 0; i < sizeof(leftLegVertices) / sizeof(float); i++) humanoidVertices.push_back(leftLegVertices[i]);
    for (int i = 0; i < sizeof(rightLegVertices) / sizeof(float); i++) humanoidVertices.push_back(rightLegVertices[i]);
    // KAPICA SE NE DODAJE OVDE - renderuje se zasebno samo za kontrolora

    unsigned int humanoidVAO, humanoidVBO;
    glGenVertexArrays(1, &humanoidVAO);
    glGenBuffers(1, &humanoidVBO);

    glBindVertexArray(humanoidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, humanoidVBO);
    glBufferData(GL_ARRAY_BUFFER, humanoidVertices.size() * sizeof(float), humanoidVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    // ========== VAO ZA KOSU (ZA OBIČNE PUTNIKE) ==========
    std::vector<float> hairOnlyVertices;
    for (int i = 0; i < sizeof(hairVertices) / sizeof(float); i++) {
        hairOnlyVertices.push_back(hairVertices[i]);
    }
    
    unsigned int hairVAO, hairVBO;
    glGenVertexArrays(1, &hairVAO);
    glGenBuffers(1, &hairVBO);
    
    glBindVertexArray(hairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hairVBO);
    glBufferData(GL_ARRAY_BUFFER, hairOnlyVertices.size() * sizeof(float), hairOnlyVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);

    // ========== VAO ZA KAPICU (SAMO ZA KONTROLORA) ==========
    std::vector<float> capOnlyVertices;
    for (int i = 0; i < sizeof(capVertices) / sizeof(float); i++) {
        capOnlyVertices.push_back(capVertices[i]);
    }
    
    unsigned int capVAO, capVBO;
    glGenVertexArrays(1, &capVAO);
    glGenBuffers(1, &capVBO);
    
    glBindVertexArray(capVAO);
    glBindBuffer(GL_ARRAY_BUFFER, capVBO);
    glBufferData(GL_ARRAY_BUFFER, capOnlyVertices.size() * sizeof(float), capOnlyVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    glBindVertexArray(0);

    // ========== INICIJALIZACIJA ==========
    initStations();
    setupPathVAO();
    setupCircleVAO();
    setupDisplayFramebuffer();
    setupRoad3D();
    setupStation3D();
    
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 cameraPos = glm::vec3(0.0, 0.0, 0.15);
    glm::vec3 cameraUp = glm::vec3(0.0, 1.0, 0.0);
    
    // ========== PHONG LIGHTING SETUP ==========
    // Pozicija svetla (UNUTAR autobusa - u centru plafona)
    glm::vec3 lightPos = glm::vec3(0.0f, 0.4f, 0.0f);
    
    // Boja svetla (neutralna bela svetlost - kao sijalica u autobusu)
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 0.95f);
    
    auto lastTime = std::chrono::high_resolution_clock::now();

    std::cout << "\n========================================" << std::endl;
    std::cout << "=== PROGRAM POKRENUT ===" << std::endl;
    std::cout << "Kontrole:" << std::endl;
    std::cout << "  Levi klik - dodaj putnika" << std::endl;
    std::cout << "  Desni klik - ukloni putnika" << std::endl;
    std::cout << "  K - kontrola ulazi" << std::endl;
    std::cout << "  O - otvori/zatvori vrata" << std::endl;
    std::cout << "  1/2 - ukljuci/iskljuci depth test" << std::endl;
    std::cout << "  3/4 - ukljuci/iskljuci face culling" << std::endl;
    std::cout << "  Mis - pomeri pogled" << std::endl;
    std::cout << "  WASD - rotiraj scenu" << std::endl;
    std::cout << "  Strelice - pomeri kameru" << std::endl;
    std::cout << "  ESC - izlaz" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ========== GLAVNA PETLJA ==========
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - lastTime;

        if (deltaTime.count() < FRAME_TIME) {
            continue;
        }
        float dt = deltaTime.count();
        lastTime = currentTime;

        // ========== LOGIKA ==========
        // Testiranje dubine
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            depthTestEnabled = true;
            glEnable(GL_DEPTH_TEST);
            std::cout << "Depth Test: UKLJUČEN" << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            depthTestEnabled = false;
            glDisable(GL_DEPTH_TEST);
            std::cout << "Depth Test: ISKLJUČEN" << std::endl;
        }

        // Odstranjivanje lica
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
            faceCullingEnabled = true;
            glEnable(GL_CULL_FACE);
            std::cout << "Face Culling: UKLJUČEN" << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
            faceCullingEnabled = false;
            glDisable(GL_CULL_FACE);
            std::cout << "Face Culling: ISKLJUČEN" << std::endl;
        }

        // Transformisanje
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            model = glm::rotate(model, glm::radians(-0.5f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            model = glm::rotate(model, glm::radians(0.5f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            model = glm::rotate(model, glm::radians(-0.5f), glm::vec3(1.0f, 0.0f, 1.0f));
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            model = glm::rotate(model, glm::radians(0.5f), glm::vec3(1.0f, 0.0f, 1.0f));
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            cameraPos += 0.01f * glm::normalize(glm::vec3(cameraFront.z, 0, -cameraFront.x));
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            cameraPos -= 0.01f * glm::normalize(glm::vec3(cameraFront.z, 0, -cameraFront.x));
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            cameraPos += 0.01f * glm::normalize(glm::vec3(cameraFront.x, 0, cameraFront.z));
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            cameraPos -= 0.01f * glm::normalize(glm::vec3(cameraFront.x, 0, cameraFront.z));
        }

        // Rotacija volana i truckanje
        bool isManualDriving = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        
        bool isBusMoving = !busAtStation;

        if (isManualDriving || isBusMoving) {
            wheelRotation = sin(glfwGetTime() * 0.8f) * 15.0f;
            busShakeTime += 0.016f;
            busShakeOffset = sin(busShakeTime * busShakeSpeed) * busShakeAmplitude;
        } else {
            if (wheelRotation > 0.5f) {
                wheelRotation -= wheelRotationSpeed * 0.5f;
            } else if (wheelRotation < -0.5f) {
                wheelRotation += wheelRotationSpeed * 0.5f;
            } else {
                wheelRotation = 0.0f;
            }
            busShakeOffset = 0.0f;
            busShakeTime = 0.0f;
        }

        // Logika simulacije autobusa
        if (busAtStation) {
            stationTimer += dt;

            if (leftMousePressed && !passengerEntering && !passengerExiting) {
                if (passengers < 50) {
                    passengers++;
                    addPassenger(false);
                    passengerEntering = true;
                    passengerAnimTimer = 0.0f;
                    std::cout << "Usao putnik. Ukupno: " << passengers << std::endl;
                }
            }
            if (rightMousePressed && !passengerEntering && !passengerExiting) {
                if (passengers > 0) {
                    passengers--;
                    removePassenger(false);
                    passengerExiting = true;
                    passengerAnimTimer = 0.0f;
                    std::cout << "Izasao putnik. Ukupno: " << passengers << std::endl;
                }
            }

            if (keyKPressed && !isInspectorInBus && !passengerEntering && !passengerExiting) {
                if (passengers < 50) {
                    isInspectorInBus = true;
                    passengers++;
                    addPassenger(true);
                    inspectorExitStation = (currentStation + 1) % NUM_STATIONS;
                    passengerEntering = true;
                    passengerAnimTimer = 0.0f;
                    std::cout << ">>> KONTROLA USLA U AUTOBUS na stanici " << currentStation << " <<<" << std::endl;
                } else {
                    std::cout << ">>> KONTROLA NE MOZE DA UDJE - AUTOBUS JE PUN (50 putnika) <<<" << std::endl;
                }
            }

            if (stationTimer >= STATION_WAIT_TIME) {
                busAtStation = false;
                stationTimer = 0.0f;
                busProgress = 0.0f;
                doorClosing = true;
                doorOpening = false;
                std::cout << "Autobus krece ka stanici " << nextStation << std::endl;
            }
        }
        else {
            busProgress += BUS_SPEED * dt;
            if (busProgress >= 1.0f) {
                busProgress = 1.0f;
                busAtStation = true;
                stationTimer = 0.0f;
                currentStation = nextStation;
                nextStation = (currentStation + 1) % NUM_STATIONS;
                std::cout << "Autobus stigao na stanicu " << currentStation << std::endl;

                doorOpening = true;
                doorClosing = false;

                if (isInspectorInBus && currentStation == inspectorExitStation) {
                    passengers--;
                    removePassenger(true);
                    int passengersWithoutInspector = passengers;
                    int maxFines = passengersWithoutInspector > 0 ? passengersWithoutInspector : 0;
                    int fines = (maxFines > 0) ? (rand() % (maxFines + 1)) : 0;
                    totalFines += fines;
                    std::cout << ">>> KONTROLA IZASLA na stanici " << currentStation << "! Naplaceno " << fines << " kazni. Ukupno kazni: " << totalFines << " <<<" << std::endl;
                    isInspectorInBus = false;
                    inspectorExitStation = -1;
                }
            }
        }
        
        // Automatska animacija vrata
        if (doorOpening) {
            doorOffset += doorSpeed;
            if (doorOffset >= doorMaxOffset) {
                doorOffset = doorMaxOffset;
                doorOpening = false;
            }
        }
        if (doorClosing) {
            doorOffset -= doorSpeed;
            if (doorOffset <= 0.0f) {
                doorOffset = 0.0f;
                doorClosing = false;
            }
        }

        if (passengerEntering || passengerExiting) {
            passengerAnimTimer += dt;
            if (passengerAnimTimer >= passengerAnimDuration) {
                passengerEntering = false;
                passengerExiting = false;
                passengerAnimTimer = 0.0f;
            }
        }

        updatePassengers(dt);

        leftMousePressed = false;
        rightMousePressed = false;
        keyKPressed = false;

        // ========== RENDEROVANJE 2D DISPLEJA ==========
        render2DDisplay(shader2D, VAO2D, numberTextures, busTexture, doorClosedTexture, 
                       doorOpenTexture, passengersLabelTexture, finesLabelTexture, controlTexture);

        // ========== RENDEROVANJE 3D SCENE ==========
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, mode->width, mode->height);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);  // Plavo nebo (sky blue)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader3D);

        glm::mat4 shakeModel = model;
        shakeModel = glm::translate(shakeModel, glm::vec3(0.0f, busShakeOffset, 0.0f));

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)mode->width / (float)mode->height, 0.05f, 1000.0f);

        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(shakeModel));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uV"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uP"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shader3D, "uLightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(shader3D, "uLightColor"), 1, glm::value_ptr(lightColor));
        glUniform3fv(glGetUniformLocation(shader3D, "uViewPos"), 1, glm::value_ptr(cameraPos));
        glUniform1i(glGetUniformLocation(shader3D, "useTex"), false);
        glUniform1i(glGetUniformLocation(shader3D, "transparent"), true);
        glUniform1i(glGetUniformLocation(shader3D, "useCustomColor"), false);

        glm::mat4 worldModel = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(worldModel));
        
        glBindVertexArray(roadVAO);
        
        
        for (int i = 0; i < 4; ++i) {
            glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
        }
        
        glBindVertexArray(station3DVAO);
        
        float distanceToNextStation = (1.0f - busProgress) * STATION_DISTANCE;
        
        for (int stationIdx = 0; stationIdx < 5; stationIdx++) {
            float stationZ = -distanceToNextStation - (stationIdx * STATION_DISTANCE);
            
            glm::mat4 stationModel = glm::mat4(1.0f);
            stationModel = glm::translate(stationModel, glm::vec3(6.0f, 0.0f, stationZ));
            glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(stationModel));
            
            for (int i = 0; i < 9; ++i) {
                glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
            }
        }
        
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(shakeModel));

        glBindVertexArray(VAO3D);

        for (int i = 0; i < 11; ++i) {
            glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
        }

        // Animacija volana
        glm::mat4 wheelModel = shakeModel;
        glm::vec3 wheelCenter = glm::vec3(0.0f, -0.25f, -0.4f);
        wheelModel = glm::translate(wheelModel, wheelCenter);
        wheelModel = glm::rotate(wheelModel, glm::radians(wheelRotation), glm::vec3(0.0f, 0.0f, 1.0f));
        wheelModel = glm::translate(wheelModel, -wheelCenter);
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(wheelModel));
        glDrawArrays(GL_TRIANGLE_FAN, 11 * 4, 4);

        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(shakeModel));

        // Crtanje 2D displeja sa teksturom
        glUniform1i(glGetUniformLocation(shader3D, "useTex"), true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, displayTexture);
        glUniform1i(glGetUniformLocation(shader3D, "uTex"), 0);
        glDrawArrays(GL_TRIANGLE_FAN, 12 * 4, 4);
        glUniform1i(glGetUniformLocation(shader3D, "useTex"), false);

        // Crtanje ostatka kabine
        for (int i = 13; i < 20; ++i) {
            glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
        }

        // Animacija vrata
        glm::mat4 doorModel = shakeModel;
        doorModel = glm::translate(doorModel, glm::vec3(-doorOffset * 0.3f, 0.0f, doorOffset));
        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(doorModel));
        glDrawArrays(GL_TRIANGLE_FAN, 20 * 4, 4);

        glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(shakeModel));
        
        // Crtanje sedista
        for (int i = 21; i < 23; ++i) {
            glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
        }

        // Crtanje putnika (humanoidan oblik - poboljšan sa animacijom hodanja)
        glBindVertexArray(humanoidVAO);
        
        for (const auto& p : activePassengers) {
            glm::mat4 passengerModel = shakeModel;
            passengerModel = glm::translate(passengerModel, p.position);
            
            // ROTACIJA PREMA PRAVCU KRETANJA - putnici gledaju u pravcu hodanja
            if (p.isMoving) {
                glm::vec3 direction = glm::normalize(p.targetPosition - p.position);
                // Izračunaj ugao rotacije (atan2 za ugao između pravca i Z ose)
                float angle = atan2(direction.x, direction.z);
                // Rotiraj putnika oko Y ose (vertikalno) da gleda ka target poziciji
                passengerModel = glm::rotate(passengerModel, angle, glm::vec3(0.0f, 1.0f, 0.0f));
            } else {
                // Kada stoji, gleda ka unutrašnjosti autobusa (default rotacija)
                passengerModel = glm::rotate(passengerModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            }
            
            glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(passengerModel));
            
            // OMOGUCI useCustomColor ZA SVE DELOVE PUTNIKA (ne samo za torso)
            glUniform1i(glGetUniformLocation(shader3D, "useCustomColor"), 1);
            
            // Skin tone boja za glavu, vrat
            glm::vec3 skinColor = glm::vec3(1.0f, 0.85f, 0.7f);
            
            // Glava (6 strana) + vrat (1 strana) = 7 faces - SKIN TONE
            glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(skinColor));
            for (int i = 0; i < 7; ++i) {
                glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
            }
            
            // Trup - koristi SHIRT boju (custom boja po putniku) - 6 faces
            glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(p.shirtColor));
            for (int i = 7; i < 13; ++i) {
                glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
            }
            
            // Leva ruka - SHIRT boja (rukavi majice) - 4 strane
            glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(p.shirtColor));
            for (int i = 13; i < 17; ++i) {
                glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
            }
            
            // Desna ruka - SHIRT boja (rukavi majice) - 4 strane
            glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(p.shirtColor));
            for (int i = 17; i < 21; ++i) {
                glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
            }
            
            // ========== ANIMACIJA HODANJA - NOGE (SA ROTACIJOM) ==========
            // Leva noga - koristi pants boju (4 strane) + prirodna animacija hodanja
            glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(p.pantsColor));
            
            if (p.isMoving) {
                // Leva noga - rotira se oko kuka (gornjeg dela noge) - ugao rotacije
                float leftLegAngle = sin(p.walkAnimTime) * 25.0f;  // Max 25 stepeni napred/nazad
                glm::vec3 hipPivot = glm::vec3(-0.04f, -0.05f, 0.01f);  // Pozicija kuka (gornji deo leve noge)
                
                glm::mat4 leftLegModel = passengerModel;  // VEĆ ROTIRANO prema pravcu kretanja!
                leftLegModel = glm::translate(leftLegModel, hipPivot);  // Pomeri do kuka
                leftLegModel = glm::rotate(leftLegModel, glm::radians(leftLegAngle), glm::vec3(1.0f, 0.0f, 0.0f));  // Rotiraj oko X-ose
                leftLegModel = glm::translate(leftLegModel, -hipPivot);  // Vrati nazad
                glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(leftLegModel));
                
                for (int i = 21; i < 25; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
                
                // Desna noga - rotira se u suprotnu stranu (prirodno hodanje)
                float rightLegAngle = -sin(p.walkAnimTime) * 25.0f;  // Suprotna faza
                glm::vec3 hipPivotRight = glm::vec3(0.04f, -0.05f, 0.01f);  // Pozicija desnog kuka
                
                glm::mat4 rightLegModel = passengerModel;  // VEĆ ROTIRANO prema pravcu kretanja!
                rightLegModel = glm::translate(rightLegModel, hipPivotRight);  // Pomeri do kuka
                rightLegModel = glm::rotate(rightLegModel, glm::radians(rightLegAngle), glm::vec3(1.0f, 0.0f, 0.0f));  // Rotiraj oko X-ose
                rightLegModel = glm::translate(rightLegModel, -hipPivotRight);  // Vrati nazad
                glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(rightLegModel));
                
                for (int i = 25; i < 29; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
            } else {
                // Kada stoji, noge su statične (bez rotacije) - ALI SA rotacijom tela!
                glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(passengerModel));
                for (int i = 21; i < 25; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
                for (int i = 25; i < 29; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
            }
            
            // KOSA ili KAPICA - zavisi da li je kontrolor ili običan putnik
            if (p.isInspector) {
                // KAPICA - SAMO za kontrolora (zasebni VAO - 4 faces)
                // Vrati model matricu na osnovnu poziciju putnika
                glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(passengerModel));
                
                // Prebaci se na capVAO (zasebni buffer za kapicu)
                glBindVertexArray(capVAO);
                
                glm::vec3 capColor = glm::vec3(0.02f, 0.02f, 0.08f);  // Tamnoplava
                glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(capColor));
                
                // Crtaj sve 4 face kapice (0-3 iz capVAO buffer-a)
                for (int i = 0; i < 4; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
                
                // Vrati se na humanoidVAO za sledeće putnike
                glBindVertexArray(humanoidVAO);
            } else {
                // KOSA - za obične putnike (hairVAO - 5 faces)
                // Vrati model matricu na osnovnu poziciju putnika
                glUniformMatrix4fv(glGetUniformLocation(shader3D, "uM"), 1, GL_FALSE, glm::value_ptr(passengerModel));
                
                // Prebaci se na hairVAO (zasebni buffer za kosu)
                glBindVertexArray(hairVAO);
                
                // Koristi hair boju putnika (random)
                glUniform3fv(glGetUniformLocation(shader3D, "uCustomColor"), 1, glm::value_ptr(p.hairColor));
                
                // Crtaj sve 5 faces kose (0-4 iz hairVAO buffer-a)
                for (int i = 0; i < 5; ++i) {
                    glDrawArrays(GL_TRIANGLE_FAN, i * 4, 4);
                }
                
                // Vrati se na humanoidVAO za sledeće putnike
                glBindVertexArray(humanoidVAO);
            }
        }
        
        glUniform1i(glGetUniformLocation(shader3D, "isInspector"), 0);
        glUniform1i(glGetUniformLocation(shader3D, "useCustomColor"), false);

        // ========== RENDEROVANJE AUTHOR TEKSTA (2D OVERLAY) ==========
        // PRIVREMENO iskljuci depth test za 2D overlay
        GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        
        glUseProgram(shader2D);
        glBindVertexArray(VAO2D);
        
        // Identity matrix za 2D prostor
        float identityMatrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(glGetUniformLocation(shader2D, "uModel"), 1, GL_FALSE, identityMatrix);
        
        glBindTexture(GL_TEXTURE_2D, authorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glUniform1f(glGetUniformLocation(shader2D, "uAlpha"), 1.0f);
        
        setModelMatrix(shader2D, 0.7f, 0.8f, 0.25f, 0.15f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        // VRATI prethodno stanje depth testa
        if (depthTestWasEnabled) {
            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
    }

    // ========== CISCENJE ==========
    glDeleteVertexArrays(1, &VAO2D);
    glDeleteBuffers(1, &VBO2D);
    glDeleteBuffers(1, &EBO2D);
    glDeleteVertexArrays(1, &VAO3D);
    glDeleteBuffers(1, &VBO3D);
    glDeleteVertexArrays(1, &humanoidVAO);
    glDeleteBuffers(1, &humanoidVBO);
    glDeleteVertexArrays(1, &hairVAO);
    glDeleteBuffers(1, &hairVBO);
    glDeleteVertexArrays(1, &capVAO);
    glDeleteBuffers(1, &capVBO);
    glDeleteVertexArrays(1, &pathVAO);
    glDeleteBuffers(1, &pathVBO);
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteVertexArrays(1, &roadVAO);
    glDeleteBuffers(1, &roadVBO);
    glDeleteVertexArrays(1, &station3DVAO);
    glDeleteBuffers(1, &station3DVBO);
    glDeleteProgram(shader2D);
    glDeleteProgram(shader3D);

    glDeleteTextures(1, &busTexture);
    glDeleteTextures(1, &stationTexture);
    glDeleteTextures(1, &controlTexture);
    glDeleteTextures(1, &doorClosedTexture);
    glDeleteTextures(1, &doorOpenTexture);
    glDeleteTextures(1, &authorTexture);
    glDeleteTextures(1, &passengersLabelTexture);
    glDeleteTextures(1, &finesLabelTexture);
    glDeleteTextures(10, numberTextures);

    glDeleteFramebuffers(1, &displayFBO);
    glDeleteTextures(1, &displayTexture);
    glDeleteRenderbuffers(1, &displayRBO);

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n=== PROGRAM ZAVRSEN ===" << std::endl;
    return 0;
}