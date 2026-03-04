#include "Camera.h"

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Position(position)
    , Front(glm::vec3(0.0f, 0.0f, -1.0f))
    , WorldUp(up)
    , Yaw(yaw)
    , Pitch(pitch)
    , MovementSpeed(DEFAULT_SPEED)
    , MouseSensitivity(DEFAULT_SENSITIVITY)
    , Fov(DEFAULT_FOV)
{
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float nearPlane, float farPlane) const {
    return glm::perspective(glm::radians(Fov), aspectRatio, nearPlane, farPlane);
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime) {
    float velocity = MovementSpeed * deltaTime;

    switch (direction) {
        case CameraMovement::Forward:  Position += Front   * velocity; break;
        case CameraMovement::Backward: Position -= Front   * velocity; break;
        case CameraMovement::Left:     Position -= Right   * velocity; break;
        case CameraMovement::Right:    Position += Right   * velocity; break;
        case CameraMovement::Up:       Position += WorldUp * velocity; break;
        case CameraMovement::Down:     Position -= WorldUp * velocity; break;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch) {
        if (Pitch >  89.0f) Pitch =  89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset) {
    MovementSpeed += yoffset * 0.5f;
    if (MovementSpeed <  0.5f) MovementSpeed =  0.5f;
    if (MovementSpeed > 50.0f) MovementSpeed = 50.0f;
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));

    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}
