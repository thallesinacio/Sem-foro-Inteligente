#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/clocks.h"
#include <string.h>
#include "hardware/pio.h"
#include "pio_matrix.pio.h"


#define led_red 13
#define led_green 11
#define led_blue 12
#define BUTTON_A 5
#define BUZZER_A 21

// Display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
char cor[10];
char expr[15];

// Matriz de led's
PIO pio;
uint sm;
uint32_t VALOR_LED;
unsigned char R, G, B;
#define NUM_PIXELS 25
#define OUT_PIN 7
int idx;
int red, green, blue;


double matriz[4][25] = {
    { // apaga (índice 0)
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0, 0.0
    },
    { // X (índice 1)
        0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.1, 0.0,
        0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0,
        0.1, 0.0, 0.0, 0.0, 0.1
    },
    { // V (índice 2)
        0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 
        0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 
        0.0, 0.1, 0.0, 0.0, 0.0
    },
    {
        0.0, 0.1, 0.1, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1,
        0.1, 0.0, 0.0, 0.0, 0.1, 0.1, 0.0, 0.0, 0.0, 0.1,
        0.0, 0.1, 0.1, 0.1, 0.0
    }
};

// Handles das tarefas
TaskHandle_t handle_normal = NULL;
TaskHandle_t handle_noturno = NULL;
TaskHandle_t handle_buzzers = NULL;
TaskHandle_t handle_display = NULL;
TaskHandle_t handle_matriz = NULL;

// Variável de debouncing
static volatile uint32_t last_time = 0;

// Flag das tarefas
bool modo;
bool verde = false, vermelho = true, amarelo = false, amarelo_noturno = false;

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6

// Interrupções
void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_time > 600000){
        last_time = current_time; 
        if(gpio == botaoB){
            reset_usb_boot(0, 0);
        } else if (gpio == BUTTON_A) { // Alterna entre as Tasks principais (modo normal e noturno)
            modo = !modo;
            if (modo) {
                vTaskSuspend(handle_normal);
                //----------------
                vTaskSuspend(handle_display);
                vTaskSuspend(handle_matriz);
                vTaskSuspend(handle_buzzers);
                //----------------
                vTaskResume(handle_noturno);
                //----------------
                vTaskResume(handle_matriz);
                vTaskResume(handle_buzzers);
                vTaskResume(handle_display);
                amarelo_noturno = true;
                verde = amarelo = vermelho = false;
            } else {
                vTaskSuspend(handle_noturno);
                //----------------
                vTaskSuspend(handle_display);
                vTaskSuspend(handle_matriz);
                vTaskSuspend(handle_buzzers);
                //----------------
                vTaskResume(handle_normal);
                //----------------
                vTaskResume(handle_display);
                vTaskResume(handle_matriz);
                vTaskResume(handle_buzzers);
                amarelo_noturno = false;
            }
        }
    }
}

void led_on(int pino) {
    gpio_init(pino);
    gpio_set_dir(pino,GPIO_OUT);
}

void button_on(int pino) {
    gpio_init(pino);
    gpio_set_dir(pino,GPIO_IN);
    gpio_pull_up(pino);
}

// Task 1
void vModo_normal() {

    modo = false;
    led_on(BUZZER_A);
    led_on(led_red);
    led_on(led_green);

    red = 0.0;
    green = 0.0;
    blue = 0.0;

        while(true) {
            // Vermelho
            amarelo = false;
            vermelho = true;
            idx = 1;
            strcpy(cor, "vermelho");
            strcpy(expr, "PARE");
            red = 1;
            green = 0;
            gpio_put(led_green,false);
            gpio_put(led_red,true);
            vTaskDelay(pdMS_TO_TICKS(2000));
            // Amarelo
            vermelho = false;
            amarelo = true;
            green = 1;
            idx = 3;
            strcpy(cor, "amarelo");
            strcpy(expr, "ATENCAO");
            gpio_put(led_green,true);
            vTaskDelay(pdMS_TO_TICKS(2000));
            // Verde
            amarelo = false;
            verde = true;
            red = 0;
            idx = 2;
            strcpy(cor, "verde");
            strcpy(expr, "SIGA");
            gpio_put(led_red,false);
            vTaskDelay(pdMS_TO_TICKS(2000));
            // Amarelo
            verde = false;
            amarelo = true;
            red = 1;
            idx = 3;
            strcpy(cor, "amarelo");
            strcpy(expr, "ATENCAO");
            gpio_put(led_red,true);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
}

// Task 2
void vModo_noturno () {

    led_on(BUZZER_A);
    led_on(led_red);
    led_on(led_green);

    red = 0.0;
    green = 0.0;
    blue = 0.0;
    
        while(true) {
            strcpy(cor, "amarelo");
            strcpy(expr, "MODO NOTURNO");
            idx = 3;
            red = 1;
            green = 1;
            gpio_put(led_red,true);
            gpio_put(led_green,true);
            vTaskDelay(pdMS_TO_TICKS(2000));
            gpio_put(led_red,false);
            gpio_put(led_green,false);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
}

// Ativação do buzzer
void buzz(uint8_t BUZZER_PIN, uint16_t freq, uint16_t duration) {
    int period = 1000000 / freq;
    int pulse = period / 2;
    int cycles = freq * duration / 1000;
    for (int j = 0; j < cycles; j++) {
        gpio_put(BUZZER_PIN, 1);
        sleep_us(pulse);
        gpio_put(BUZZER_PIN, 0);
        sleep_us(pulse);
    }
}

// Ativação do buzzer por um dado tempo 
void buzz_for_duration(uint8_t BUZZER_PIN, uint16_t freq, uint16_t duration, uint16_t total_time_ms) {
    uint16_t elapsed_time = 0;

    while (elapsed_time < total_time_ms) {
        buzz(BUZZER_PIN, freq, duration);
        elapsed_time += duration;
        sleep_ms(1); // Espera 1ms entre cada chamada de buzz
    }
}

// Task para manuseio do buzzer
void vBuzzers () {

    led_on(led_red);
    led_on(led_green);

        while(true) {
            if(vermelho) {
                buzz(BUZZER_A, 600, 500); // 500 ms ligado e
                vTaskDelay(pdMS_TO_TICKS(1500));//            1,5 seeg desligado
            } else if (amarelo) {
                for (int i = 0; i < 5; i++) {  
                    buzz(BUZZER_A, 800, 100);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            } else if(verde) {
                buzz(BUZZER_A, 1000, 1000); // beep curto por 1 seg
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else if (amarelo_noturno) {
                buzz(BUZZER_A, 500, 400);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
}

// Task para manuseio do display
void vDisplayTask(void *params) {
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    static ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while (true) {
        desenha_molde_completo(&ssd);
        ssd1306_draw_string_escala(&ssd, cor, 40, 18, 0.8);
        ssd1306_draw_string_escala(&ssd, expr, 10, 36, 1.2);
        ssd1306_send_data(&ssd);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

//rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double b, double r, double g)
{
  //unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

//rotina para acionar a matrix de leds - ws2812b
void desenho_pio(int pattern_idx, uint32_t valor_led, PIO pio, uint sm) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        double intensity = matriz[pattern_idx][24-i];
        valor_led = matrix_rgb(intensity * blue, intensity * red, intensity * green);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}
// Task para manuseio da matriz dee leds
void vMatriz() {

    double r = 0.0, b = 0.0 , g = 0.0;
   bool ok;
   ok = set_sys_clock_khz(128000, false);
   pio = pio0;

   uint offset = pio_add_program(pio, &work_iii_program);
   uint sm = pio_claim_unused_sm(pio, true);
   work_iii_program_init(pio, sm, offset, OUT_PIN);

    while(true) {
        desenho_pio(idx, VALOR_LED, pio, sm);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main()
{
    stdio_init_all();

    button_on(BUTTON_A);
    button_on(botaoB);


    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    xTaskCreate(vModo_noturno, "Modo Noturno", 1024, NULL, 1, &handle_noturno);
    vTaskSuspend(handle_noturno);
    sleep_ms(20);

    xTaskCreate(vModo_normal, "Modo Normal", 1024, NULL, 1, &handle_normal);
    xTaskCreate(vBuzzers, "Buzzers Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &handle_buzzers);
    xTaskCreate(vDisplayTask, "Display", 1024, NULL, 1, &handle_display);
    xTaskCreate(vMatriz, "Matriz", 1024, NULL, 1, &handle_matriz);

    sleep_ms(2000); // Sleep inicial para evitar "concorrência" entre as tarefas

    vTaskStartScheduler();

    panic_unsupported();

}
