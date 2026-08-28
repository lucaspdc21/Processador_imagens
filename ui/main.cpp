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

#include "Image.hpp"
#include "ImageLoad.hpp"

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
    GLFWwindow* window = glfwCreateWindow(800, 600, "PhotoSell - ImGui Exemplo", nullptr, nullptr);
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
    
    char path[512] = "";
    bool carregou;
    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("oiii");                          
        std::string pathString(path);
        ImGui::InputText("Path:", path, sizeof(path));
        bool loadResult = ImGui::Button("Carregar Imagem");
        if(loadResult){
            core::Image loadImage(0,0,255);
            if(core::loadImage(pathString, loadImage)){
                std::cerr << "imagem carregada" << std::endl;
            }else{
            }
        }
        
        if (ImGui::Button("Fechar Programa")) {
            glfwSetWindowShouldClose(window, GLFW_TRUE); 
        }
        
        ImGui::End();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

