# processador

Processador de imagens com interface gráfica, desenvolvido em C++17 para a disciplina de Visualização de Imagens (Trabalho 1).
Todos os algoritmos de processamento são escritos manualmente sobre a matriz da imagem; bibliotecas de terceiros são usadas apenas para ler/gravar arquivos e para a interface.

---

## Arquitetura

O projeto é dividido em duas camadas, sem dependência da camada de interface dentro do núcleo de processamento:

```
┌───────────────────────────────────────────────┐
│  ui/  (executável processador_gui)              │
│  ImGui + GLFW + OpenGL + tinyfiledialogs      │
│  Painéis, estado da aplicação, texturas       │
└───────────────────┬───────────────────────────┘
                    │ chama
┌───────────────────▼───────────────────────────┐
│  core/  (biblioteca estática "core")          │
│  include/ + src/                              │
│  Image + algoritmos (sem dependência de GUI)  │
│  stb_image / stb_image_write (I/O de arquivo) │
└───────────────────────────────────────────────┘
```

### Estrutura de pastas

```
Processador/
├── CMakeLists.txt
├── include/                 # headers do núcleo
│   ├── Image.hpp            # classe Image (1 ou 3 canais, float, vetor contíguo + metadados)
│   ├── ImageLoad.hpp        # abrir / salvar arquivo (PNG, BMP, JPG)
│   ├── ImageIntensity.hpp   # transformações de intensidade e esteganografia
│   ├── ImageQuantization.hpp# quantização (k bits)
│   ├── ImageResample.hpp    # reamostragem (vizinho / bilinear / bicúbica)
│   ├── ImageStats.hpp       # mín, máx, faixa dinâmica, contraste
│   └── ImageMetrics.hpp     # métricas espaciais e de armazenamento
├── src/                     # implementações do núcleo (um .cpp por header)
├── ui/
│   ├── main.cpp             # janela GLFW, loop principal, layout em 3 painéis
│   ├── AppState.cpp/.hpp    # estado compartilhado + ações (aplicar, salvar, restaurar)
│   ├── ImageTexture.cpp     # envio da imagem para textura OpenGL
│   ├── LeftPanel.cpp        # menus de métodos e parâmetros
│   ├── CenterPanel.cpp      # exibição da imagem
│   ├── RightPanel.cpp       # informações da imagem
│   └── CompareWindow.cpp    # janela que compara todas as transformações
└── third_party/
    ├── imgui/               # Dear ImGui (+ backends GLFW/OpenGL3)
    ├── stb/                 # stb_image.h e stb_image_write.h
    └── tinyfiledialogs/     # diálogos de abrir/salvar arquivo
```

### Fluxo básico

1. O usuário escolhe um arquivo (`LeftPanel`) → `AppState::loadFromPath` → `core::loadImage`.
2. A imagem é guardada em `originalImage` (intocada) e `workingImage` (a que é editada).
3. Cada botão chama um método de `AppState` que aplica a função do `core` sobre `workingImage`.
4. `RefreshTexture` envia `workingImage` para o OpenGL e o `CenterPanel` desenha.
5. "Restaurar imagem original" copia `originalImage` de volta para `workingImage`.

---


## Como executar

### Requisitos

- CMake 3.15 ou superior
- Compilador com suporte a C++17 (GCC, Clang ou MSVC)
- Conexão com a internet na primeira configuração (o CMake baixa a GLFW 3.4 via `FetchContent`)

**Linux (Debian/Ubuntu):** instale as dependências do X11/OpenGL e, para os diálogos de arquivo, o `zenity`:

```bash
sudo apt install build-essential cmake git libgl1-mesa-dev \
     libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev zenity
```

> O `CMakeLists.txt` compila apenas com suporte a X11 (Wayland desativado). Em sessões Wayland ele roda via XWayland.

**Windows:** MinGW-w64.

### Compilar

Na raiz do projeto:

```bash
cmake -S . -B build
cmake --build build --config Release
```

### Testes do núcleo

Os testes não precisam de interface nem de GLFW:

```bash
cmake -S . -B build -DBUILD_GUI=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

### Rodar

```bash
# Linux / macOS
./build/processador_gui

# Windows (Visual Studio)
build\Release\processador_gui.exe
```

### Usando o programa

1. Clique em **Selecionar imagem** e escolha um arquivo.
2. Ajuste os parâmetros no painel esquerdo e clique no botão "Aplicar ..." da transformação desejada. As transformações são cumulativas.
3. Use **Comparar todas as transformações** para ver o resultado de cada uma lado a lado.
4. **Desfazer (Ctrl+Z)** volta a última operação; **Restaurar imagem original** volta ao arquivo aberto (e também pode ser desfeito).
5. **Salvar imagem** grava o resultado no formato da extensão escolhida (PNG, BMP ou JPG). JPG tem perdas e destrói mensagens escondidas por esteganografia; use PNG ou BMP.

Observações: imagens coloridas são mantidas em RGB (3 canais) e as transformações valem para cada canal; o canal alfa de PNGs é descartado ao abrir. "Normalizar exibição" só altera a exibição, não os dados.

---
