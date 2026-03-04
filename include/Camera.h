#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class CameraMovement {
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down
};

constexpr float DEFAULT_YAW         = -90.0f;
constexpr float DEFAULT_PITCH       =   0.0f;
constexpr float DEFAULT_SPEED       =   5.0f;
constexpr float DEFAULT_SENSITIVITY =   0.1f;
constexpr float DEFAULT_FOV         =  45.0f;

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;

    float MovementSpeed;
    float MouseSensitivity;
    float Fov;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 up       = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw          = DEFAULT_YAW,
           float pitch        = DEFAULT_PITCH);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio,
                                   float nearPlane = 0.1f,
                                   float farPlane  = 1000.0f) const;

    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void processMouseScroll(float yoffset);

private:
    void updateCameraVectors();
};
