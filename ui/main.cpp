#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "AppState.hpp"
#include "ImageTexture.hpp"
#include "LeftPanel.hpp"
#include "CenterPanel.hpp"
#include "RightPanel.hpp"
#include "CompareWindow.hpp"

void error_callback(int error, const char* description) {
    std::cerr << "Erro GLFW [" << error << "]: " << description << std::endl;
}

int main() {
    // GLFW callback de erro
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        std::cerr << "Falha ao inicializar a GLFW" << std::endl;
        return -1;
    }

    // OpenGL configuração
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criar jenela nativa
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Processador", nullptr, nullptr);
    if (!window) {
        std::cerr << "Falha ao criar a janela" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Contexto ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Estilo escuro
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ui::AppState state;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    state.textureId = texture;
    
    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags containerFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar;
        ImGui::Begin("Processador", nullptr, containerFlags);

        if (ImGui::BeginMenuBar()) {
            ImGui::Text("Processador de Imagens");
            ImGui::EndMenuBar();
        }

        float fullWidth = ImGui::GetContentRegionAvail().x;
        float fullHeight = ImGui::GetContentRegionAvail().y - 40.0f; 
        float sideWidth = fullWidth * 0.22f;
        float centerWidth = fullWidth - 2.0f * sideWidth - 16.0f;

        ui::RenderLeftPanel(state, sideWidth, fullHeight);
        ImGui::SameLine();
        ui::RenderCenterPanel(state, centerWidth, fullHeight);
        ImGui::SameLine();
        ui::RenderRightPanel(state, sideWidth, fullHeight);

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextWrapped("%s", state.statusMessage.c_str());

        if (ImGui::Button("Fechar Programa")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        
        ImGui::End();

        if (state.showCompareWindow) {
            ui::RenderCompareWindow(state);
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    ui::ReleaseCompareTextures(state);
    glDeleteTextures(1, &texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

