#include "engine.h"

#include <functional>

namespace engine
{
    void Engine::init()
    {
        // Controls init
        keysArePressed_ = new bool[512]{false};
        camera_.position = glm::vec3(0.0, 0.0, 3.0);
        initWindow();
        initImGui();
        renderer_.init(window_);
    }

    void Engine::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Path Tracing Engine", nullptr, nullptr);

        glfwSetWindowUserPointer(window_, this);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        auto keyCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->keyCallback(window, key);
        };
        auto mouseCallback = [](GLFWwindow* window, double xpos, double ypos)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->mouseCallback(window, static_cast<float>(xpos), static_cast<float>(ypos));
        };
        auto mouseButtonCallback = [](GLFWwindow* window, int button, int action, int mods)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->mouseButtonCallback(window, button, action);
        };
        glfwSetCursorPosCallback(window_, mouseCallback);
        glfwSetKeyCallback(window_, keyCallback);
        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    }

    void Engine::initImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
    }


    void Engine::keyCallback(GLFWwindow* window, int key)
    {
        keysArePressed_[key] = (glfwGetKey(window, key) == GLFW_PRESS);
    }

    void Engine::keyInput()
    {
        float deltaTime = io->DeltaTime;

        if (keysArePressed_['W'] && focused_)
        {
            camera_.moveForward(deltaTime);
        }
        if (keysArePressed_['S'] && focused_)
        {
            camera_.moveBackward(deltaTime);
        }
        if (keysArePressed_['A'] && focused_)
        {
            camera_.moveLeft(deltaTime);
        }
        if (keysArePressed_['D'] && focused_)
        {
            camera_.moveRight(deltaTime);
        }
        if (keysArePressed_['Q'] && focused_)
        {
            camera_.moveDown(deltaTime);
        }
        if (keysArePressed_[' '] && focused_)
        {
            camera_.moveUp(deltaTime);
        }
    }

    void Engine::mouseCallback(GLFWwindow* window, float xpos, float ypos)
    {
        if (!focused_)
            return;

        // the mouse was not focused the frame before
        if (isFirstMouseMove_)
        {
            lastMousePosition_.x = xpos;
            lastMousePosition_.y = ypos;
            isFirstMouseMove_ = false;
        }
        float xOffset = xpos - lastMousePosition_.x;
        float yOffset = lastMousePosition_.y - ypos;

        lastMousePosition_.x = xpos;
        lastMousePosition_.y = ypos;

        camera_.updateCamDirection(xOffset, yOffset);
    }

    void Engine::mouseButtonCallback(GLFWwindow* window, int button, int action)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            focused_ = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        {
            focused_ = false;
            isFirstMouseMove_ = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }


    void Engine::run()
    {
        while (!glfwWindowShouldClose(window_))
        {
            keyInput();
            glfwPollEvents();
            camera_.updateMatrix();

            renderer_.newImGuiFrame();
            ImGui::NewFrame();
            ImGui::ShowAboutWindow();
            ImGui::Render();
            renderer_.render(camera_);
        }
    }


    void Engine::cleanup()
    {
        renderer_.cleanup();
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
} // engine
