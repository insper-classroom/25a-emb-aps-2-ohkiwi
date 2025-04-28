#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "aux.h"

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

void encoder_task(void *p) {
    int last_clk = gpio_get(ENCODER_CLK_PIN);
    int counter = 0;
    int last_sent = 0;
    const int MAX_JUMP = 4;      // passos máximos permitidos por pulso
    uart_packet_t pkt;

    while (1) {
        int clk = gpio_get(ENCODER_CLK_PIN);
        int dt  = gpio_get(ENCODER_DT_PIN);

        if (last_clk == 1 && clk == 0) {
            // determina incremento bruto
            int delta = (dt != clk) ? 2 : -2;
            int tentative = counter + delta;
            // limita salto máximo relativo ao último enviado
            int diff = tentative - last_sent;
            if (diff > MAX_JUMP)       tentative = last_sent + MAX_JUMP;
            else if (diff < -MAX_JUMP) tentative = last_sent - MAX_JUMP;
            counter = tentative;
            // limita faixa total
            if (counter > 50) counter = 50;
            if (counter < -50) counter = -50;
            // envia se mudou
            if (counter != last_sent) {
                pkt.id = ENCODER_CLK_PIN;
                pkt.value = (int16_t)(counter + 50);
                xQueueSend(xQueueUART, &pkt, portMAX_DELAY);
                last_sent = counter;
            }
        }
        last_clk = clk;
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
    gpio_put(LED_IND_PIN, 1);

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

    // scheduler
    vTaskStartScheduler();

    while (1) {}
}