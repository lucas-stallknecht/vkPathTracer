#include "engine.h"

#include "path_tracing/trace_mesh.h"

namespace engine
{
    void Engine::init()
    {
        initWindow();
        initImGui();
        renderer_.init(window_);

        camera_.position = glm::vec3(0.0, 0.0, 1.5);

        path_tracing::TraceMeshBuilder builder;
        builder.setGeometry("./assets/models/grenade.obj");
        builder.setMaterial({
            .colorMap =  "./assets/textures/grenade_basecolor.png",
            .roughnessMap = "./assets/textures/grenade_roughness.png",
            .metallicMap = "./assets/textures/grenade_metallic.png"
        });
        path_tracing::Mesh grenade = builder.build(10);
        // builder.setGeometry("./assets/models/dragon.obj");
        // builder.setMaterial({
        //     .color = glm::vec3(0.850743, 0.463014, 0.359456),
        //     .roughness = 0.7
        // });
        // path_tracing::Mesh dragon = builder.build(32);
        const std::vector<path_tracing::Mesh> scene = {grenade};
        renderer_.uploadPathTracingScene(scene);
        renderer_.uploadSkybox("./assets/skyboxes/paris");
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
            renderer_.resetAccumulation();
        }
        if (keysArePressed_['S'] && focused_)
        {
            camera_.moveBackward(deltaTime);
            renderer_.resetAccumulation();
        }
        if (keysArePressed_['A'] && focused_)
        {
            camera_.moveLeft(deltaTime);
            renderer_.resetAccumulation();
        }
        if (keysArePressed_['D'] && focused_)
        {
            camera_.moveRight(deltaTime);
            renderer_.resetAccumulation();
        }
        if (keysArePressed_['Q'] && focused_)
        {
            camera_.moveDown(deltaTime);
            renderer_.resetAccumulation();
        }
        if (keysArePressed_[' '] && focused_)
        {
            camera_.moveUp(deltaTime);
            renderer_.resetAccumulation();
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

        if (abs(xOffset) > 0.0 || abs(yOffset) > 0.0)
            renderer_.resetAccumulation();

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
            {
                ImGui::Begin("Settings");
                ImGui::Text("%.1f ms/frame (%.1f FPS)", 1000.0 / io->Framerate, io->Framerate);
                if (ImGui::CollapsingHeader("Path tracing", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool useChange = renderer_.ptPushConstants_.useSkybox == 1;
                    bool visibleChange = renderer_.ptPushConstants_.skyboxVisible == 1;
                    if (ImGui::Checkbox("Use skybox lighting", &useChange))
                    {
                        renderer_.ptPushConstants_.useSkybox = !renderer_.ptPushConstants_.useSkybox;
                        renderer_.resetAccumulation();
                    };
                    if (ImGui::Checkbox("Show skybox", &visibleChange))
                    {
                        renderer_.ptPushConstants_.skyboxVisible = !renderer_.ptPushConstants_.skyboxVisible;
                        renderer_.resetAccumulation();
                    }
                }

                ImGui::End();
            }
            ImGui::EndFrame();
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
