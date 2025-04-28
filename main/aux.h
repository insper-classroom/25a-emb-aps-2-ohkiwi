#ifndef VOLANTE_H
#define VOLANTE_H

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

// Pinos GPIO
#define BTN_GEAR_UP_PIN    2
#define BTN_GEAR_DOWN_PIN  3
#define BTN_IGNITION_PIN   4
#define BTN_LIGHTS_PIN     5
#define LED_IND_PIN        7
#define ENCODER_CLK_PIN   15
#define ENCODER_DT_PIN    14
#define ADC_ACCEL_PIN     26
#define ADC_BRAKE_PIN     27
#define UART_TX_PIN        0
#define UART_RX_PIN        1
#define UART_BAUD_RATE  115200

#define EOP 0xFF

// Estrutura do pacote UART
typedef struct {
    uint8_t  id;
    int16_t  value;
} uart_packet_t;

// FreeRTOS
QueueHandle_t    xQueueInput;
QueueHandle_t    xQueueUART;
SemaphoreHandle_t xSemaphoreLed;


void btn_callback(uint gpio, uint32_t events);
void init_btns(void);

void input_task(void *params);
void comunica_task(void *params);
void encoder_task(void *params);

#endif // VOLANTE_H
