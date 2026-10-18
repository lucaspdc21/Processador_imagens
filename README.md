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
│   ├── Image.hpp            # classe Image (matriz de inteiros + metadados)
│   ├── ColorImage.hpp       # imagem RGB = 3 planos Image (cinza = R, G e B iguais)
│   ├── ColorSpace.hpp       # conversão RGB <-> HSI (pixel e imagem), planos H, S e I
│   ├── ImageColor.hpp       # cinza, sépia, chroma-key, equalização HSI, ajustes de cor...
│   ├── ImageLoad.hpp        # abrir / salvar arquivo
│   ├── ImageIntensity.hpp   # transformações de intensidade e esteganografia
│   ├── ImageQuantization.hpp# quantização (k bits)
│   ├── ImageResample.hpp    # reamostragem (vizinho / bilinear / bicúbica)
│   ├── ImageStats.hpp       # mín, máx, faixa dinâmica, contraste
│   └── ImageMetrics.hpp     # métricas espaciais e de armazenamento
├── src/                     # implementações do núcleo (um .cpp por header)
├── tests/
│   └── color_tests.cpp      # testes do núcleo de cor (alvo color_tests)
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

### Imagens coloridas

A imagem agora é carregada em cor (`ColorImage`: três planos `Image` R, G e B). Uma imagem em cinza é só o caso R = G = B,
então todo processamento escrito para cinza roda plano a plano (`applyToPlanes` / `mapPlanes` em `ImageColor.hpp`).
No painel esquerdo, **"Imagem colorida: aplicar em"** escolhe se as transformações de intensidade e os filtros atuam
em R, G e B separadamente ou só no componente I do HSI (preserva as cores). O negativo é sempre em RGB.

| Ferramenta | Onde |
|---|---|
| RGB <-> HSI (equações do Cap. 6.2.3) e exibição dos planos R, G, B, H, S, I | "Cor: sistemas RGB / HSI, cinza e sépia" |
| Cinza: média aritmética simples e média ponderada (pesos configuráveis, padrão ITU-R BT.601) | idem |
| Sépia: matriz clássica ou via HSI | idem |
| Negativo | "Transformações de intensidade" |
| Chroma-key (cor-chave, conta-gotas, distância D0, modo esfera/cubo, borda suave, imagem de fundo) | "Cor: ajustes e chroma-key" |
| Histogramas R, G, B e I (com mediana de I) | painel direito |
| Equalização de histograma colorida (só o I do HSI; ganho de saturação opcional) | "7. Equalização de histograma" |
| Ajuste de matiz, saturação e brilho (HSI) | "Cor: ajustes e chroma-key" |
| Ajuste de canal C/R, M/G, Y/B | idem |

Detalhes: a mensagem de esteganografia fica no LSB do canal R; o gradiente de bordas tem a opção de gradiente
vetorial de Di Zenzo; o PNG é salvo em RGB.

Os testes do núcleo de cor (tabela de validação do HSI, ida e volta, chroma-key etc.) rodam com:

```bash
cmake --build build --target color_tests && ./build/color_tests
```

### Fluxo básico

1. O usuário escolhe um arquivo (`LeftPanel`) → `AppState::loadFromPath` → `core::loadImage` (imagem colorida RGB).
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
4. **Restaurar imagem original** desfaz tudo.
5. **Salvar como PNG** grava o resultado.

---
