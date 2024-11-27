#include "Camera.h"
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace pt
{
    Camera::Camera(float fov, float aspect, float near, float far) : fov(fov), aspect(aspect), near(near),
                                                                     far(far)
    {
        updateMatrix();
    }

    void Camera::updateMatrix()
    {
        projMatrix = glm::perspective(glm::radians(fov), aspect, near, far);
        viewMatrix = glm::lookAt(position, position + direction, glm::vec3{0.0, 1.0, 0.0});
    }

    void Camera::moveForward(float deltaTime)
    {
        position += direction * CAM_MOV_SPEED * deltaTime;
    }

    void Camera::moveBackward(float deltaTime)
    {
        position -= direction * CAM_MOV_SPEED * deltaTime;
    }

    void Camera::moveLeft(float deltaTime)
    {
        position -= glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)) * CAM_MOV_SPEED * deltaTime;
    }

    void Camera::moveRight(float deltaTime)
    {
        position += glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)) * CAM_MOV_SPEED * deltaTime;
    }

    void Camera::moveUp(float deltaTime)
    {
        position += glm::vec3(0.0f, 1.0f, 0.0f) * CAM_MOV_SPEED * deltaTime * 0.25f;
    }

    void Camera::moveDown(float deltaTime)
    {
        position -= glm::vec3(0.0f, -1.0f, 0.0f) * CAM_MOV_SPEED * deltaTime * 0.25f;
    }

    void Camera::updateCamDirection(float deltax, float deltay)
    {
        // pitch and yaw delta already depends on framerate so we don't multiply it by deltaTime
        float pitchDelta = deltay * CAM_VIEW_SPEED;
        float yawDelta = deltax * CAM_VIEW_SPEED;

        glm::vec3 rightDirection = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::quat q = glm::normalize(glm::cross(glm::angleAxis(pitchDelta, rightDirection),
                                                glm::angleAxis(-yawDelta, glm::vec3(0.f, 1.0f, 0.0f))));
        direction = glm::rotate(q, direction);
    }
} // grass
