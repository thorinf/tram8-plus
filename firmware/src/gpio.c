#include "gpio.h"
#include <avr/io.h>

void gpio_init(void) {
  DDRB |= (1 << PB0); // Set PB0 as output
  DDRD |= 0xFE;       // Set PD1 to PD7 as outputs
}

void gate_set(uint8_t gate_index, uint8_t state) {
  if (gate_index >= 8) { return; }

  volatile uint8_t *port_reg;
  uint8_t mask;

  if (gate_index == 0) {
    port_reg = &PORTB;
    mask = (uint8_t)(1 << PB0);
  } else {
    port_reg = &PORTD;
    mask = (uint8_t)(1 << (PD1 + gate_index - 1));
  }

  if (state) {
    *port_reg |= mask;
  } else {
    *port_reg &= (uint8_t)~mask;
  }
}
