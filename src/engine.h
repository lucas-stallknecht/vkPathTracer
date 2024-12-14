#pragma once
#include "constants.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <array>

#include "core/camera.h"
#include "renderer/renderer.h"

namespace engine
{
    class Engine
    {
    public:
        void init();
        void run();
        void cleanup();

    private:
        void initWindow();
        void initImGui();
        void keyCallback(GLFWwindow* window, int key);
        void mouseCallback(GLFWwindow* window, float xpos, float ypos);
        void mouseButtonCallback(GLFWwindow* window, int button, int action);
        void keyInput();

        GLFWwindow* window_ = nullptr;
        ImGuiIO* io = nullptr;
        core::Camera camera_ = {35.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT)};
        renderer::Renderer renderer_{};

        // Controls
        bool focused_ = false;
        std::array<bool, 512> keysArePressed_{};
        bool isFirstMouseMove_ = true;
        glm::vec2 lastMousePosition_ = glm::vec2();
    };
} // engine
