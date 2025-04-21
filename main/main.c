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
#define ADC_ACCEL_PIN 26
#define ADC_BRAKE_PIN 27
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define UART_BAUD_RATE 9600


QueueHandle_t xQueueInput;
QueueHandle_t xQueueAction;
SemaphoreHandle_t xSemaphoreLed;

void btn_callback(uint gpio, uint32_t events) {
    uint32_t pin = gpio;
    xQueueSendFromISR(xQueueInput, &pin, 0);
}

void input_task(void *params) {
    uint32_t gpio;
    char msg[32];

    for(;;) {
        if (xQueueReceive(xQueueInput, &gpio, pdMS_TO_TICKS(100))) {
            // Mapeia botão para mensagem
            if (gpio == BTN_GEAR_UP_PIN) {
                strcpy(msg, "BTN_GEARUP");
            } else if (gpio == BTN_GEAR_DOWN_PIN) {
                strcpy(msg, "BTN_GEARDOWN");
            } else if (gpio == BTN_HANDBRAKE_PIN) { 
                strcpy(msg, "BTN_HANDBRAKE");
            } else if (gpio == BTN_IGNITION_PIN) { 
                strcpy(msg, "BTN_IGNITION");
                xSemaphoreGive(xSemaphoreLed);
            } else if (gpio == BTN_HORN_PIN) {
                strcpy(msg, "BTN_HORN");
            } else if (gpio == BTN_LIGHTS_PIN) {
                strcpy(msg, "BTN_LIGHTS");
            }

            xQueueSend(xQueueAction, msg, portMAX_DELAY);
        }

        // Leitura periódica dos pedais
        adc_select_input(0);  // Acelerador
        uint16_t accel = adc_read();
        adc_select_input(1);  // Freio
        uint16_t brake = adc_read();
        snprintf(msg, sizeof(msg), "PEDAL A:%u B:%u", accel, brake);
        xQueueSend(xQueueAction, msg, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void comunica_task(void *params) {
    char buffer[32];

    for(;;) {
        if (xQueueReceive(xQueueAction, buffer, portMAX_DELAY)) {
            uart_puts(uart0, buffer);
            uart_puts(uart0, "\r\n");
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

int main() {
    stdio_init_all();
    uart_init(uart0, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Configura botões com pull-ups e IRQ
    uint btn_pins[] = {BTN_GEAR_UP_PIN, BTN_GEAR_DOWN_PIN, BTN_HANDBRAKE_PIN, BTN_IGNITION_PIN, BTN_HORN_PIN, BTN_LIGHTS_PIN};
    for (int i = 0; i < 6; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
        gpio_set_irq_enabled_with_callback(btn_pins[i], GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    }

    gpio_init(LED_IND_PIN);
    gpio_set_dir(LED_IND_PIN, GPIO_OUT);
    gpio_put(LED_IND_PIN, 0);

    adc_init();
    adc_gpio_init(ADC_ACCEL_PIN);
    adc_gpio_init(ADC_BRAKE_PIN);

    // filas e semáforo
    xQueueInput   = xQueueCreate(10, sizeof(uint32_t));
    xQueueAction  = xQueueCreate(10, sizeof(char[32]));
    xSemaphoreLed = xSemaphoreCreateBinary();

    // tasks
    xTaskCreate(input_task, "InputTask", 1024, NULL, 2, NULL);
    xTaskCreate(comunica_task, "ComunicaTask", 1024, NULL, 1, NULL);
    xTaskCreate(led_task, "LedTask", 512, NULL, 1, NULL);

    // scheduler
    vTaskStartScheduler();

    while (1) {}
}