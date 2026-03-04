// Request dedicated NVIDIA/AMD GPU over integrated graphics
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <filesystem>
#include <cstring>

#include "Shader.h"
#include "Camera.h"
#include "Renderer.h"
#include "GaussianData.h"
#include "PLYLoader.h"
#include "PointCloudRenderer.h"
#include "SplatRenderer.h"

// Window settings
constexpr int   WINDOW_WIDTH  = 1280;
constexpr int   WINDOW_HEIGHT = 720;
const char*     WINDOW_TITLE  = "GaussianClouds";

// Global state for GLFW callbacks
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float  lastX          = WINDOW_WIDTH / 2.0f;
float  lastY          = WINDOW_HEIGHT / 2.0f;
bool   firstMouse     = true;
float  deltaTime      = 0.0f;
float  lastFrame      = 0.0f;
bool   mouseCaptured  = false;

// Rendering state (controlled by UI)
float  clearColor[3]  = { 0.1f, 0.1f, 0.1f };
bool   wireframeMode  = false;
bool   depthTestEnabled = true;

// Point cloud state
GaussianData   gaussianData;
PLYLoadResult  plyLoadResult;
bool           showPointCloud = true;
bool           showTriangle   = true;
float          pointSize      = 2.0f;
float          pointOpacity   = 1.0f;
float          pcRotation[3]  = { 180.0f, 0.0f, 0.0f }; // X=180 for COLMAP->OpenGL default
char           plyPathBuffer[512] = "";

// Splat renderer state
bool           useSplatRenderer = true;
float          splatScale       = 1.0f;

// Forward declarations
void loadPLYFile(const std::string& path, PointCloudRenderer& pcRenderer, SplatRenderer& spRenderer);

// --- GLFW Callbacks ---

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!mouseCaptured) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed: y goes bottom-to-top
    lastX = xpos;
    lastY = ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    camera.processMouseScroll(static_cast<float>(yoffset));
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            mouseCaptured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else if (action == GLFW_RELEASE) {
            mouseCaptured = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;
        if (mouseCaptured) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void processInput(GLFWwindow* window) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Forward, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Backward, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Left, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Right, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Up, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::Down, deltaTime);
}

// Pointers for drag-and-drop callback to access renderers
PointCloudRenderer* g_pointCloudRendererPtr = nullptr;
SplatRenderer*      g_splatRendererPtr      = nullptr;

void loadPLYFile(const std::string& path, PointCloudRenderer& pcRenderer, SplatRenderer& spRenderer) {
    plyLoadResult = PLYLoader::load(path, gaussianData);
    if (plyLoadResult.success) {
        pcRenderer.upload(gaussianData);
        spRenderer.upload(gaussianData);
        // Copy path into the UI buffer
        std::strncpy(plyPathBuffer, path.c_str(), sizeof(plyPathBuffer) - 1);
        plyPathBuffer[sizeof(plyPathBuffer) - 1] = '\0';
    }
}

void dropCallback(GLFWwindow* window, int count, const char** paths) {
    for (int i = 0; i < count; ++i) {
        std::string path(paths[i]);
        // Accept .ply files
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".ply") {
            if (g_pointCloudRendererPtr && g_splatRendererPtr) {
                loadPLYFile(path, *g_pointCloudRendererPtr, *g_splatRendererPtr);
            }
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    // Set working directory to exe location so shaders are found
    std::filesystem::path exeDir = std::filesystem::path(argv[0]).parent_path();
    if (!exeDir.empty()) {
        std::filesystem::current_path(exeDir);
    }
    std::cout << "Working directory: " << std::filesystem::current_path().string() << std::endl;

    // Initialize GLFW
    glfwSetErrorCallback([](int error, const char* desc) {
        std::cerr << "GLFW Error (" << error << "): " << desc << std::endl;
    });

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Request OpenGL 4.5 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL function pointers via GLAD
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetDropCallback(window, dropCallback);

    // --- Dear ImGui setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    // Disable VSync for maximum framerate
    glfwSwapInterval(0);

    // OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Create shaders and renderers
    Shader basicShader("shaders/basic.vert", "shaders/basic.frag");
    Renderer renderer;
    renderer.init();

    Shader pointCloudShader("shaders/pointcloud.vert", "shaders/pointcloud.frag");
    PointCloudRenderer pointCloudRenderer;
    g_pointCloudRendererPtr = &pointCloudRenderer;

    Shader splatShader("shaders/splat.vert", "shaders/splat.frag");
    SplatRenderer splatRenderer;
    g_splatRendererPtr = &splatRenderer;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Delta time
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Input
        processInput(window);

        // Apply rendering state from UI
        glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
        else                  glDisable(GL_DEPTH_TEST);

        // Clear
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Common transforms
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix(aspect);

        // Draw triangle
        if (showTriangle) {
            basicShader.use();
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), currentFrame, glm::vec3(0.0f, 1.0f, 0.0f));
            basicShader.setMat4("model", model);
            basicShader.setMat4("view", view);
            basicShader.setMat4("projection", projection);
            renderer.draw(basicShader);
        }

        // Draw point cloud / splats
        if (showPointCloud && gaussianData.count > 0) {
            glm::mat4 pcModel = glm::mat4(1.0f);
            pcModel = glm::rotate(pcModel, glm::radians(pcRotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
            pcModel = glm::rotate(pcModel, glm::radians(pcRotation[1]), glm::vec3(0.0f, 1.0f, 0.0f));
            pcModel = glm::rotate(pcModel, glm::radians(pcRotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));

            if (useSplatRenderer && splatRenderer.getInstanceCount() > 0) {
                glDepthMask(GL_FALSE);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // premultiplied alpha
                glm::vec2 vp(static_cast<float>(width), static_cast<float>(height));
                splatRenderer.draw(splatShader, view * pcModel, projection, vp, splatScale);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  // restore
                glDepthMask(GL_TRUE);
            } else if (pointCloudRenderer.getVertexCount() > 0) {
                // Point cloud path
                pointCloudShader.use();
                pointCloudShader.setMat4("model", pcModel);
                pointCloudShader.setMat4("view", view);
                pointCloudShader.setMat4("projection", projection);
                pointCloudShader.setFloat("pointSize", pointSize);
                pointCloudShader.setFloat("opacity", pointOpacity);
                pointCloudRenderer.draw(pointCloudShader);
            }
        }

        // --- ImGui frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Settings panel
        ImGui::Begin("Settings");

        // Camera section
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                        camera.Position.x, camera.Position.y, camera.Position.z);
            ImGui::Text("Yaw: %.1f  Pitch: %.1f", camera.Yaw, camera.Pitch);
            ImGui::SliderFloat("FOV", &camera.Fov, 10.0f, 120.0f, "%.1f");
            ImGui::SliderFloat("Speed", &camera.MovementSpeed, 0.5f, 50.0f, "%.1f");
            ImGui::SliderFloat("Sensitivity", &camera.MouseSensitivity, 0.01f, 1.0f, "%.3f");
        }

        // Rendering section
        if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Clear Color", clearColor);
            ImGui::Checkbox("Wireframe", &wireframeMode);
            ImGui::Checkbox("Depth Test", &depthTestEnabled);
        }

        // Info section
        if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("FPS: %.1f (%.3f ms)", io.Framerate, 1000.0f / io.Framerate);
            if (useSplatRenderer && splatRenderer.getInstanceCount() > 0) {
                ImGui::Text("GPU Sort (%s): %.2f ms",
                    splatRenderer.useRadixSort ? "Radix" : "Bitonic",
                    splatRenderer.getLastSortTimeMs());
                ImGui::Text("Drawn: %zu | Visible: %zu | Total: %zu",
                    splatRenderer.getVisibleCount(),
                    splatRenderer.getTotalVisible(),
                    splatRenderer.getInstanceCount());
            }
            ImGui::Text("OpenGL: %s", glGetString(GL_VERSION));
            ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
        }

        // Gaussian Splatting section
        if (ImGui::CollapsingHeader("Gaussian Splatting", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("PLY Path", plyPathBuffer, sizeof(plyPathBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                std::string path(plyPathBuffer);
                if (!path.empty()) {
                    loadPLYFile(path, pointCloudRenderer, splatRenderer);
                }
            }
            ImGui::TextDisabled("(Or drag-and-drop a .ply file onto the window)");

            // Load status
            if (plyLoadResult.success) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Loaded successfully!");
            } else if (!plyLoadResult.error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", plyLoadResult.error.c_str());
            }

            // Statistics (only shown when data is loaded)
            if (gaussianData.count > 0) {
                ImGui::Separator();
                ImGui::Text("Gaussians: %zu", gaussianData.count);
                if (plyLoadResult.fileSizeBytes > 0) {
                    double mb = plyLoadResult.fileSizeBytes / (1024.0 * 1024.0);
                    ImGui::Text("File Size: %.1f MB", mb);
                }
                ImGui::Text("Load Time: %.1f ms", plyLoadResult.loadTimeMs);
                ImGui::Text("Centroid: %.2f, %.2f, %.2f",
                            gaussianData.centroid.x, gaussianData.centroid.y, gaussianData.centroid.z);
                ImGui::Text("BBox Min: %.2f, %.2f, %.2f",
                            gaussianData.bboxMin.x, gaussianData.bboxMin.y, gaussianData.bboxMin.z);
                ImGui::Text("BBox Max: %.2f, %.2f, %.2f",
                            gaussianData.bboxMax.x, gaussianData.bboxMax.y, gaussianData.bboxMax.z);

                ImGui::Separator();
                ImGui::Checkbox("Use Splat Renderer", &useSplatRenderer);
                if (useSplatRenderer) {
                    ImGui::Checkbox("Use Radix Sort", &splatRenderer.useRadixSort);
                    ImGui::SliderFloat("Splat Scale", &splatScale, 0.01f, 10.0f, "%.2f");
                    ImGui::SliderInt("Max Splats", &splatRenderer.maxVisibleSplats, 5000, 2000000);
                } else {
                    ImGui::SliderFloat("Point Size", &pointSize, 0.5f, 20.0f, "%.1f");
                    ImGui::SliderFloat("Point Opacity", &pointOpacity, 0.0f, 1.0f, "%.2f");
                }
                ImGui::SliderFloat("Rotate X", &pcRotation[0], -180.0f, 180.0f, "%.1f");
                ImGui::SliderFloat("Rotate Y", &pcRotation[1], -180.0f, 180.0f, "%.1f");
                ImGui::SliderFloat("Rotate Z", &pcRotation[2], -180.0f, 180.0f, "%.1f");
                if (ImGui::Button("Reset Rotation")) {
                    pcRotation[0] = 180.0f; pcRotation[1] = 0.0f; pcRotation[2] = 0.0f;
                }

                if (ImGui::Button("Center Camera on Cloud")) {
                    glm::vec3 extent = gaussianData.bboxMax - gaussianData.bboxMin;
                    float maxExtent = glm::max(extent.x, glm::max(extent.y, extent.z));
                    float distance = maxExtent * 1.5f;
                    camera.Position = gaussianData.centroid + glm::vec3(0.0f, 0.0f, distance);
                    camera.Yaw = -90.0f;
                    camera.Pitch = 0.0f;
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Show Triangle", &showTriangle);
            ImGui::Checkbox("Show Point Cloud", &showPointCloud);
        }

        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap and poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    renderer.cleanup();
    pointCloudRenderer.cleanup();
    splatRenderer.cleanup();
    g_pointCloudRendererPtr = nullptr;
    g_splatRendererPtr = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
