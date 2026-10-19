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
│   ├── ImageMetrics.hpp     # métricas espaciais e de armazenamento
│   ├── ImageWarp.hpp        # matriz afim, interpolação (vizinho/bilinear) e warp inverso
│   ├── ImageScale.hpp       # escala (sx, sy independentes) + suavização antes de reduzir
│   ├── ImageRotate.hpp      # rotação com pivô (manter tamanho / expandir)
│   ├── FrequencyTransform.hpp # DFT ingênua, FFT radix-2 própria, FFT da biblioteca, inversa
│   ├── FrequencyFilters.hpp # filtros pré-definidos H(u,v): LP/HP/BR/BP/notch (ideal, Butterworth, gaussiano)
│   ├── FrequencyMask.hpp    # máscara H editável: pincel suave, simetria conjugada, desfazer/refazer
│   └── FrequencySession.hpp # roteiro completo: padding, centralização, F em cache, espectro, IDFT
├── src/                     # implementações do núcleo (um .cpp por header)
├── tests/                   # testes automatizados
│   ├── color_tests.cpp      # testes do núcleo de cor (alvo color_tests)
│   └── test_*.cpp           # demais testes, sem dependências externas (alvo run_tests)
├── ui/
│   ├── main.cpp             # janela GLFW, loop principal, layout em 3 painéis
│   ├── AppState.cpp/.hpp    # estado compartilhado + ações (aplicar, salvar, restaurar)
│   ├── ImageTexture.cpp     # envio da imagem para textura OpenGL
│   ├── LeftPanel.cpp        # menus de métodos e parâmetros
│   ├── CenterPanel.cpp      # exibição da imagem
│   ├── RightPanel.cpp       # informações da imagem
│   ├── SpatialWindow.cpp    # janela de escala e rotação (prévia lado a lado, pivô por clique)
│   ├── FrequencyWindow.cpp  # janela de filtragem em frequência (worker, pincel, pré-definidos)
│   ├── RgbaTexture.cpp      # textura RGBA com atualização parcial (espectro)
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


## Operações espaciais: escala e rotação

Painel esquerdo → **Operações espaciais** → *Abrir escala e rotação*. A janela mostra **entrada | resultado**
lado a lado (zoom por vizinho mais próximo, sem suavizar a exibição). O resultado só altera a imagem de trabalho
quando você clica em **Aplicar resultado à imagem de trabalho**.

* **Escala**: `sx` e `sy` independentes (com *manter proporção*), vizinho mais próximo ou bilinear, e a caixa
  *suavizar antes de reduzir* (filtro de média de tamanho ímpar ≈ `ceil(1/s)` por eixo; desligada por padrão).
* **Rotação**: ângulo em graus (decimais e negativos; **positivo = anti-horário visual**), vizinho ou bilinear,
  **pivô** digitado, pelo botão *Centro da imagem* ou **clicando na imagem de entrada** (marcador vermelho; o
  pivô também é marcado no resultado), tela de saída *manter tamanho* ou *expandir* e cor de fundo.
* **Imagem colorida**: a mesma transformação (mesmo tamanho de saída e mesma interpolação) é aplicada em R, G e B e os
  três planos formam o resultado; em cinza (R = G = B) o resultado continua em cinza.
* Escala e rotação compartilham o mesmo núcleo, `warpInverse` (`ImageWarp.cpp`), por **mapeamento inverso**:
  percorre os pixels da saída, calcula a posição na entrada com a matriz inversa e interpola (nunca deixa buracos).
  Nenhuma rotina pronta de reamostragem é usada.

### Convenções

* Pixel em linha `y`, coluna `x` (0-based); o **centro do pixel `(x, y)` fica na coordenada contínua `(x, y)`**;
  a imagem ocupa `[-0,5; W-0,5] x [-0,5; H-0,5]`; eixo `y` para baixo.
  (`Image::pixel(x, y)` do projeto usa `x` = linha; nas funções de warp `x` é sempre a coluna.)
* Escala: `Wo = max(1, round(sx*W))`, escala efetiva `Wo/W`, `xs = (xo + 0,5)/sx_ef - 0,5` (não desloca a imagem e
  alcança a última linha/coluna).
* Rotação: `p' = R (p - c) + c - o` e, na implementação, `p = Rᵀ (p' + o - c) + c`, com
  `R = [[cosθ, senθ], [-senθ, cosθ]]`. O sinal está numa única constante, `kCounterClockwiseSign`
  (`ImageRotate.hpp`).
* Em *expandir*, a tela é o retângulo envolvente dos 4 cantos do retângulo contínuo: o resultado é o mesmo para
  qualquer pivô. O pivô só altera a imagem em *manter tamanho*.
* Domínio válido: `-0,5 <= xs <= W-0,5` (idem `ys`); fora dele usa-se o fundo. Dentro, o bilinear usa *clamp* nos
  vizinhos, então as bordas não escurecem. Saídas acima de **100 megapixels** são recusadas com aviso.

---

## Filtragem no domínio da frequência

Painel esquerdo → **Filtragem no domínio da frequência** → *Abrir filtragem em frequência*. A janela mostra
**entrada | espectro (editável) | imagem filtrada**. Fluxo (Seção 4.7.3 do plano):

1. Escolha o **algoritmo** e se usa **preenchimento**, e clique em **Calcular DFT**. A imagem é preenchida com zeros
   até `P x Q` (canto superior esquerdo), multiplicada por `(-1)^(x+y)` (centraliza a transformada) e transformada
   **uma única vez**; `F(u,v)` fica em cache (`complex128`).
2. O **espectro centralizado** é exibido como `log(1 + |F|)` normalizado por `S_max` (também há *original*,
   *máscara H* e *fase*).
3. **Edite** o espectro com o pincel (preto remove, branco restaura, cinza atenua; raio, dureza, opacidade, disco
   duro ou pincel suave gaussiano) ou com a ferramenta **notch por clique**, e/ou aplique **filtros pré-definidos**
   (passa/rejeita baixa, alta, faixa e notch; ideal, Butterworth, gaussiano). Tudo escreve na mesma máscara real
   `H(u,v)` em `[0,1]` (multiplicativa, preserva a fase), com **desfazer/refazer**, *resetar* e *inverter*.
4. **Aplicar (IDFT)** calcula `G = H·F`, a inversa, a parte real, `(-1)^(x+y)` e o recorte `[0:M, 0:N]`. A saída pode
   ser *normalizada min-max* (padrão) ou *recortada em [0,255]*; *Usar resultado como imagem de trabalho* o aplica.

**Imagem colorida**: o espectro é o da intensidade `I = (R+G+B)/3` (a mesma do HSI) e a filtragem atua sobre ela.
*Usar resultado como imagem de trabalho* substitui só o plano I (`applyToPlanes` com `IntensityHSI`), então matiz e
saturação são preservados; em imagem cinza isso equivale a trocar a imagem pelo resultado.

### Algoritmos selecionáveis

| Algoritmo | Código | Custo | Observações |
|---|---|---|---|
| **DFT própria (ingênua separável)** | `FrequencyTransform.cpp` | `O(MN(M+N))` | linhas e depois colunas; tabela `W[(u·x) mod M]` com soma incremental do índice; multi-thread |
| **FFT própria (radix-2)** | `FrequencyTransform.cpp` | `O(MN log MN)` | "duplicação sucessiva" (Seção 4.11.3), iterativa com inversão de bits; `P` e `Q` viram potências de 2 |
| **FFT da biblioteca** | pocketfft | `O(MN log MN)` | alternativa para imagens grandes; só este modo usa biblioteca |

A inversa usa **um único algoritmo direto**: `f = conj(DFT{conj(F)}) / (MN)` (Seção 4.11.2). A GUI estima o tempo da
DFT ingênua, avisa (e sugere a FFT) acima de `512 x 512` após o preenchimento, roda a DFT/IDFT em **outro thread** com
barra de progresso e botão *Cancelar*, e mostra os tempos na barra de status. Limite: `P x Q <= 4096 x 4096`.

### Detalhes e convenções

* Preenchimento: `P = 2M, Q = 2N` (DFT ingênua / biblioteca) ou a próxima potência de 2 `>= 2M-1` (FFT própria).
  Sem preenchimento, `P = M, Q = N` arredondados para par (ou potência de 2), com *wraparound*.
* **Simetria conjugada**: cada pincelada é espelhada em `((P-u) mod P, (Q-v) mod Q)` combinando os alfas antes de
  aplicar (`a = 1 - (1-a1)(1-a2)`), então `H` fica exatamente simétrica e a imagem resultante é real
  (a GUI mostra `max|Im|/max|Re|`, ~1e-16).
* Pincel suave: `sigma = raio·(1 - 0,9·dureza)`, `a = exp(-d²/(2σ²))`. Com dureza alta (>= 0,75) acrescenta-se um
  núcleo plano de raio `raio - 3σ`, de modo que **dureza 1 vira quase um disco** (como pede o plano); abaixo disso é
  exatamente a fórmula do plano.
* O espectro exibido durante a edição é `|H·F|`; a textura é atualizada só na região pintada.
* Filtros **ideais** produzem *ringing* (resposta espacial tipo sinc); Butterworth de baixa ordem e gaussiano, não.

### FFT da biblioteca (pocketfft)

O CMake baixa o [pocketfft](https://github.com/mreineck/pocketfft) (header-only, BSD) com `FetchContent`, como já
faz com a GLFW. Sem internet, o programa compila normalmente e esse modo aparece desabilitado na lista. Também é
possível usar uma cópia local: `cmake -S . -B build -DPOCKETFFT_INCLUDE_DIR=/caminho/com/pocketfft_hdronly.hpp`, ou
desligar com `-DPROCESSADOR_USE_POCKETFFT=OFF`. Nenhuma função de biblioteca é usada nos modos "própria".

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

### Testes automatizados

```bash
cmake -S . -B build
cmake --build build --target run_tests
```

Compila e executa todos os `tests/test_*.cpp` (`test_spatial.cpp`: escala 1,0 e rotação 0° exatas, escala ×2 por
replicação, bilinear em imagem constante, rotação de 90/180/270° contra a rotação "à mão", ida e volta, pivô fixo,
bilinear contra implementação de referência independente etc.) e `test_frequency.cpp` (exemplos do livro
`{1,2,4,4}` e `{1,2,3,4}`; DFT ingênua e FFT contra um oráculo de força bruta em vários tamanhos, inclusive não
quadrados; `IDFT(DFT(f)) = f`; `F(0,0) = MN·média`; simetria conjugada; translação não altera `|F|`; `H ≡ 1` devolve a
imagem; pincelada simétrica mantém o resultado real; desfazer/refazer; passa-baixa borra e passa-alta realça;
fórmulas dos filtros; remoção de padrão periódico por notch). Com a biblioteca compilada, os testes também a
comparam com os algoritmos próprios.
