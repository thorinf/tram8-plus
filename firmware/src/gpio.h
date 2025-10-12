#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

#ifndef NUM_GATES
#define NUM_GATES 8
#elif NUM_GATES != 8
#error "gpio.h assumes NUM_GATES == 8"
#endif

void gpio_init(void);
void gate_set(uint8_t gate_index, uint8_t state);

#endif
