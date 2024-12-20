#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace core
{
    class Camera
    {
        static constexpr float CAM_MOV_SPEED = 4.3f;
        static constexpr float CAM_VIEW_SPEED = 0.0007f;

    public:
        Camera(float fov, float aspect, float nearPlane = 0.1f, float farPlane = 100.0f);
        void updateMatrix();
        void moveForward(float deltaTime);
        void moveBackward(float deltaTime);
        void moveLeft(float deltaTime);
        void moveRight(float deltaTime);
        void moveUp(float deltaTime);
        void moveDown(float deltaTime);
        void updateCamDirection(float xoffset, float yoffset);

        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec3 position = glm::vec3();
        glm::vec3 direction = glm::vec3(0.0, 0.0, -1.0);
        float fov;
        float aspect;
        float nearPlane, farPlane;
    };
} // grass
