#include "aux.h"

void init_btns() {
    int btn_pins[] = {BTN_GEAR_UP_PIN, BTN_GEAR_DOWN_PIN, BTN_IGNITION_PIN, BTN_LIGHTS_PIN};
    for (int i = 0; i < 4; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }

    gpio_set_irq_enabled_with_callback(BTN_GEAR_UP_PIN, GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_GEAR_DOWN_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_IGNITION_PIN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_LIGHTS_PIN, GPIO_IRQ_EDGE_FALL, true);
}