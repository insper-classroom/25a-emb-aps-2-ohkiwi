#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#define BTN_GEAR_UP_PIN 2
#define BTN_GEAR_DOWN_PIN 3
#define BTN_HANDBRAKE_PIN 4
#define BTN_IGNITION_PIN 5
#define BTN_HORN_PIN 6
#define BTN_LIGHTS_PIN 7
#define LED_IND_PIN 8
#define ENCODER_CLK_PIN 16
#define ENCODER_DT_PIN 17
#define ADC_ACCEL_PIN 26
#define ADC_BRAKE_PIN 27
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BAUD_RATE 115200


// Pacote
# define EOP 0xFF

typedef struct {
    uint8_t id;
    int16_t value;
} uart_packet_t;

QueueHandle_t xQueueInput;
QueueHandle_t xQueueUART;
SemaphoreHandle_t xSemaphoreLed;

void btn_callback(uint gpio, uint32_t events) {
    uint32_t pin = gpio;
    xQueueSendFromISR(xQueueInput, &pin, 0);
}

void input_task(void *params) {
    uint32_t gpio;
    uart_packet_t pkt;

    for(;;) {
        if (xQueueReceive(xQueueInput, &gpio, pdMS_TO_TICKS(100))) {
            // Mapeia botão para mensagem
            pkt.value = (int16_t) 1;
            pkt.id = gpio;

            xQueueSend(xQueueUART, &pkt, portMAX_DELAY);
        }

        // Leitura periódica dos pedais
        adc_select_input(0);  // Acelerador
        pkt.id = ADC_ACCEL_PIN;
        pkt.value = (int16_t) adc_read();
        xQueueSend(xQueueUART, &pkt, portMAX_DELAY);

        adc_select_input(1);  // Freio
        pkt.id = ADC_BRAKE_PIN;
        pkt.value = (int16_t) adc_read();
        xQueueSend(xQueueUART, &pkt, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void comunica_task(void *params) {
    uart_packet_t pkt;
    uint8_t buffer[4];

    for(;;) {
        if (xQueueReceive(xQueueUART, &pkt, portMAX_DELAY)) {
            buffer[0] = pkt.id;
            buffer[1] = pkt.value & 0xFF;
            buffer[2] = (pkt.value >> 8) & 0xFF;
            buffer[3] = EOP;

            uart_write_blocking(uart0, buffer, sizeof(buffer));
        }
    }
}

void led_task(void *params) {
    for(;;) {
        if (xSemaphoreTake(xSemaphoreLed, portMAX_DELAY) == pdTRUE) {
            gpio_put(LED_IND_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_put(LED_IND_PIN, 0);
        }
    }
}

void init_btns() {
    int btn_pins[] = {BTN_GEAR_UP_PIN, BTN_GEAR_DOWN_PIN, BTN_HANDBRAKE_PIN, BTN_IGNITION_PIN, BTN_HORN_PIN, BTN_LIGHTS_PIN};
    for (int i = 0; i < 6; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }

    gpio_set_irq_enabled_with_callback(BTN_GEAR_UP_PIN, GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_GEAR_DOWN_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_HANDBRAKE_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_IGNITION_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_HORN_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_LIGHTS_PIN, GPIO_IRQ_EDGE_FALL, true);
}

void encoder_task(void *p) {
    int last_clk_state = gpio_get(ENCODER_CLK_PIN);
    int counter = 0;
    int last_sent = 0;
    uart_packet_t pkt;

    while (1) {
        int clk_state = gpio_get(ENCODER_CLK_PIN);
        int dt_state = gpio_get(ENCODER_DT_PIN);

        if (last_clk_state == 1 && clk_state == 0) {
            if (dt_state != clk_state) counter++;
            else counter--;

            // Limita entre -50 e +50 (900 graus)
            if (counter > 50) counter = 50;
            if (counter < -50) counter = -50;

            // Só envia se mudou
            if (counter != last_sent) {
                pkt.id = ENCODER_CLK_PIN;
                pkt.value = counter + 50; // Transmitir como 0-100
                xQueueSend(xQueueUART, &pkt, portMAX_DELAY);
                last_sent = counter;
            }
        }

        last_clk_state = clk_state;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

int main() {
    stdio_init_all();
    init_btns();

    uart_init(uart0, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_init(LED_IND_PIN);
    gpio_set_dir(LED_IND_PIN, GPIO_OUT);
    gpio_put(LED_IND_PIN, 0);

    adc_init();
    adc_gpio_init(ADC_ACCEL_PIN);
    adc_gpio_init(ADC_BRAKE_PIN);

    gpio_init(ENCODER_CLK_PIN);
    gpio_set_dir(ENCODER_CLK_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_CLK_PIN);

    gpio_init(ENCODER_DT_PIN);
    gpio_set_dir(ENCODER_DT_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_DT_PIN);

    // filas e semáforo
    xQueueInput = xQueueCreate(32, sizeof(uint32_t));
    xQueueUART = xQueueCreate(32, sizeof(uart_packet_t));
    xSemaphoreLed = xSemaphoreCreateBinary();

    // tasks
    xTaskCreate(input_task, "InputTask", 1024, NULL, 2, NULL);
    xTaskCreate(encoder_task, "EncoderTask", 1024, NULL, 1, NULL);
    xTaskCreate(comunica_task, "ComunicaTask", 1024, NULL, 1, NULL);
    xTaskCreate(led_task, "LedTask", 512, NULL, 1, NULL);

    // scheduler
    vTaskStartScheduler();

    while (1) {}
}