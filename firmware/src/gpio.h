#ifndef GPIO_H
#define GPIO_H

#include "hardware_config.h"
#include <avr/io.h>
#include <stdint.h>

#if NUM_GATES != 8
#error "gpio.h assumes NUM_GATES == 8"
#endif

void gpio_init(void);
void led_init(void);
void button_init(void);
void gate_set(uint8_t gate_index, uint8_t state);
void led_on(void);
void led_off(void);
uint8_t read_button(void);

#endif
