#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "neopixel.c"

#define SDA_PIN 14
#define SCL_PIN 15
#define BUTTON_PIN_A 5
#define BUTTON_PIN_B 6
#define LED_PIN 7
#define LED_COUNT 25
#define MATRIX_SIZE 5

// Estrutura para representar a cor de um objeto
typedef struct {
    int r;
    int g;
    int b;
} Color;

// Matriz para armazenar as cores dos objetos (associando cor a cada célula)
Color led_colors[MATRIX_SIZE][MATRIX_SIZE] = {0};  // Inicializa com cor preta (0, 0, 0)

// Estrutura para representar as dimensões de um objeto
typedef struct {
    int width;
    int height;
} Object;

Object obj;  // Instância do objeto que será colocado na matriz
int led_matrix[MATRIX_SIZE][MATRIX_SIZE] = {0}; // Matriz de LEDs, representa a área visual

int colors[6][3] = {
    {32, 32, 32},   // Branco
    {32, 0, 0},   // Vermelho
    {0, 32, 0},   // Verde
    {0, 0, 32},    // Azul
    {32, 0, 32},    // Roxo
    {0, 32, 32},    // Ciano
};
int current_color_index = 0;

ssd1306_t disp; // Instância do display OLED

// Função para configurar o display OLED
void setupDisplay() {
    i2c_init(i2c1, 400000);  // Inicializa a comunicação I2C
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);  // Configura os pinos SDA e SCL para I2C
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);  // Habilita os resistores de pull-up para SDA e SCL
    gpio_pull_up(SCL_PIN);

    disp.external_vcc = false;  // Define a alimentação do display (não usa VCC externa)
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);  // Inicializa o display OLED
    ssd1306_clear(&disp);  // Limpa o display
    ssd1306_show(&disp);  // Atualiza o display
}

// Função para desenhar strings no display OLED
void drawStringOnDisplay(char* str1, char* str2) {
    static char last_str1[16] = "";
    static char last_str2[16] = "";

    // Atualiza a tela apenas se o conteúdo mudou
    if (strcmp(str1, last_str1) != 0 || strcmp(str2, last_str2) != 0) {
        ssd1306_clear(&disp);  // Limpa o display
        ssd1306_draw_string(&disp, 0, 0, 1, str1);  // Desenha a primeira string
        ssd1306_draw_string(&disp, 0, 10, 1, str2);  // Desenha a segunda string
        ssd1306_show(&disp);  // Atualiza o display

        // Atualiza as strings exibidas
        snprintf(last_str1, sizeof(last_str1), "%s", str1);
        snprintf(last_str2, sizeof(last_str2), "%s", str2);
    }
}

volatile int click_count = 0; // Contador para cliques dos botões

uint32_t last_interrupt_time_A = 0; // Armazena o último tempo de interrupção para o botão A
uint32_t last_interrupt_time_B = 0; // Armazena o último tempo de interrupção para o botão B

// Função de callback para o botão A
void button_callback(uint gpio, uint32_t events) {
    uint32_t interrupt_time = to_ms_since_boot(get_absolute_time());
    
    // Verifica se o botão A foi pressionado e aplica debounce
    if (gpio == BUTTON_PIN_A && interrupt_time - last_interrupt_time_A > 500) {
        click_count++;  // Incrementa o contador de cliques
        if (click_count > 5) {
            click_count = 1;  // Reseta o contador caso ultrapasse o tamanho máximo
        }
        last_interrupt_time_A = interrupt_time;  // Atualiza o tempo da última interrupção
    }

    // Verifica se o botão B foi pressionado, mas não faz nada com o evento
    if (gpio == BUTTON_PIN_B && interrupt_time - last_interrupt_time_B > 500) {
        last_interrupt_time_B = interrupt_time;  // Atualiza o tempo da última interrupção
    }
}

// Função para configurar os botões
void setupButton() {
    gpio_init(BUTTON_PIN_B);
    gpio_set_dir(BUTTON_PIN_B, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_B);  // Habilita o pull-up interno para o botão B
    gpio_set_irq_enabled_with_callback(BUTTON_PIN_B, GPIO_IRQ_EDGE_FALL, true, &button_callback);
    
    gpio_init(BUTTON_PIN_A);
    gpio_set_dir(BUTTON_PIN_A, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_A);  // Habilita o pull-up interno para o botão A
}

// Função para verificar se o espaço na matriz está disponível para o objeto
int isSpaceAvailable(int x, int y, Object obj) {
    for (int k = 0; k < obj.height; ++k) {
        for (int l = 0; l < obj.width; ++l) {
            if (led_matrix[x + k][y + l] != 0) { // Se algum espaço estiver ocupado, retorna falso
                return 0;
            }
        }
    }
    return 1; // Retorna verdadeiro se o espaço estiver livre
}

int placeObjectInMatrix(Object obj) {
    // Obtém a cor para o novo objeto
    int r = colors[current_color_index][0];  // Cor vermelha
    int g = colors[current_color_index][1];  // Cor verde
    int b = colors[current_color_index][2];  // Cor azul

    for (int i = 0; i <= MATRIX_SIZE - obj.height; ++i) {
        for (int j = 0; j <= MATRIX_SIZE - obj.width; ++j) {
            if (isSpaceAvailable(i, j, obj)) {
                // Coloca o objeto na matriz
                for (int k = 0; k < obj.height; ++k) {
                    for (int l = 0; l < obj.width; ++l) {
                        led_matrix[i + k][j + l] = 1;  // Marca o espaço na matriz de LEDs
                        led_colors[i + k][j + l].r = r; // Armazena a cor vermelha do objeto
                        led_colors[i + k][j + l].g = g; // Armazena a cor verde do objeto
                        led_colors[i + k][j + l].b = b; // Armazena a cor azul do objeto
                    }
                }
                return 1; // Retorna sucesso ao colocar o objeto
            }
        }
    }
    return 0; // Retorna falha se não for possível colocar o objeto
}


// Função para imprimir a matriz no display OLED
void printMatrix() {
    ssd1306_clear(&disp); // Limpa o display
    ssd1306_draw_string(&disp, 0, 0, 1, "AREA TOTAL:"); // Exibe título
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            char buf[2];
            snprintf(buf, sizeof(buf), "%d", led_matrix[i][j]);  // Converte o valor para string
            ssd1306_draw_string(&disp, j * 10, i * 10 + 10, 1, buf);  // Desenha o número na posição correta
        }
    }
    ssd1306_show(&disp); // Atualiza o display
}

// Função para verificar se a matriz está cheia
int isMatrixFull() {
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            if (led_matrix[i][j] == 0) {  // Se houver espaço vazio, retorna falso
                return 0;
            }
        }
    }
    return 1; // Retorna verdadeiro se a matriz estiver cheia
}

// Função para selecionar o comprimento do objeto
void selectHeight() {
    int navigating = 1;  // Flag de navegação
 
    while (navigating) {
        char buf[16];
        snprintf(buf, sizeof(buf), "comprimento: %d", click_count);  // Mostra o comprimento atual
        drawStringOnDisplay("Aperte B e defina", buf);
        sleep_ms(200);  // Pausa para evitar leituras muito rápidas

        // Se o Botão A for pressionado, confirma o comprimento
        if (gpio_get(BUTTON_PIN_A) == 0) {
            obj.height = click_count;  // Define o comprimento do objeto
            navigating = 0;  // Sai do loop de navegação
            drawStringOnDisplay("Comprimento", "confirmado!");
            sleep_ms(1000);  // Pausa antes de continuar
        }

        // Se o Botão B for pressionado, navega para o próximo comprimento
        if (gpio_get(BUTTON_PIN_B) == 0) {
            click_count++;  // Incrementa o contador de cliques
            if (click_count > 5) {
                click_count = 1;  // Reseta o contador ao ultrapassar o tamanho máximo
            }
            sleep_ms(600);  // Debounce para o Botão B
        }
    }
}

// Função para mudar a cor do próximo objeto
void changeObjectColor() {
    // Incrementa o índice da cor e faz a rotação
    current_color_index = (current_color_index + 1) % 5;
}

// Função para atualizar a exibição dos LEDs NeoPixel com ajuste de brilho
void updateNeoPixelDisplay() {
    for (int i = 0; i < MATRIX_SIZE; ++i) {
        for (int j = 0; j < MATRIX_SIZE; ++j) {
            int led_index;
            if (i % 2 == 0) {
                led_index = (4 - i) * MATRIX_SIZE + (4 - j);  // Linhas ímpares são invertidas
            } else {
                led_index = (4 - i) * MATRIX_SIZE + j;  // Linhas pares são diretas
            }

            // Acende o LED com a cor do objeto se ele estiver na matriz
            if (led_matrix[i][j] == 1) {
                npSetPixelColor(led_index, led_colors[i][j].r, led_colors[i][j].g, led_colors[i][j].b);  // LED aceso com a cor correspondente
            } else {
                npSetPixelColor(led_index, 0, 0, 0);  // LED apagado
            }
        }
    }

    // Atualiza a exibição dos LEDs
    npWrite();
}

int main() {
    stdio_init_all();  // Inicializa a comunicação serial
    setupDisplay();  // Configura o display OLED
    setupButton();  // Configura os botões

    // Inicialização da matriz de LEDs NeoPixel
    printf("Preparando NeoPixel...");
    npInit(LED_PIN, LED_COUNT);

    drawStringOnDisplay("INICIANDO", "Box System");
    sleep_ms(1500);  // Pausa de inicialização

    while (1) {  // Loop principal
        drawStringOnDisplay("Selecione a largura", "do objeto.");
        sleep_ms(1000);
        
        int navigating = 1;  // Flag de navegação
        while (navigating) {
            char buf[16];
            snprintf(buf, sizeof(buf), "largura: %d", click_count);  // Mostra a largura atual
            drawStringOnDisplay("Aperte B e defina", buf);
            sleep_ms(200);

            if (gpio_get(BUTTON_PIN_A) == 0) {
                obj.width = click_count;  // Define a largura do objeto
                navigating = 0;  // Sai do loop de navegação
                drawStringOnDisplay("Largura", "confirmada!");
                sleep_ms(800);  // Pausa antes de continuar
            }

            if (gpio_get(BUTTON_PIN_B) == 0) {
                click_count++;  // Incrementa o contador de cliques
                if (click_count > 5) {
                    click_count = 1;  // Reseta o contador ao ultrapassar o tamanho máximo
                }
                sleep_ms(600);  // Debounce para o Botão B
            }
        }

        selectHeight();  // Chama a função para selecionar o comprimento do objeto

        // Tenta colocar o objeto na matriz
        if (placeObjectInMatrix(obj)) {
            drawStringOnDisplay("Objeto colocado", "na area!");
            changeObjectColor();  // Muda a cor do objeto
            updateNeoPixelDisplay();  // Atualiza os LEDs com a nova alocação
        } else {
            drawStringOnDisplay("Nao foi possivel", "colocar o objeto.");
        }
        
        sleep_ms(1000);  // Pausa antes de continuar

        printMatrix();  // Imprime a matriz no display
        sleep_ms(3000);  // Pausa para visualização

        if (isMatrixFull()) {  // Verifica se a matriz está cheia
            drawStringOnDisplay("A area esta", "preenchida!");
            sleep_ms(2000);
            drawStringOnDisplay("Fim do", "Box System...");
            break;  // Sai do loop se a matriz estiver cheia
        }
        drawStringOnDisplay("Deseja adicionar", "outro objeto? A(s)/B(n)");
        sleep_ms(1000);
        int repeat = 1;

        while (repeat) {
            if (gpio_get(BUTTON_PIN_A) == 0) { // Botão A é pressionado (sim)
                drawStringOnDisplay("Adicionando", "novo objeto...");
                sleep_ms(1000); 
                repeat = 0; // Sai do loop de repetição
            }

            if (gpio_get(BUTTON_PIN_B) == 0) { // Botão B é pressionado (não)
                drawStringOnDisplay("Fim do", "Box System!");
                sleep_ms(1000); 
                return 0; // Sai do programa
            }

            sleep_ms(200); // Debounce entre as verificações
        }
    }
}
