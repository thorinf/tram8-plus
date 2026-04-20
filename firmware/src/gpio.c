#include "gpio.h"

#include <avr/eeprom.h>
#include <avr/io.h>
#include <util/delay.h>

// Gate polarity: 1 = active high (v1.2+), 0 = active low (v1.1)
static uint8_t gate_active_high = 1;

static void detect_hardware_version(void) {
  VERSION_DDR &= ~(1 << VERSION_PIN); // Input
  VERSION_PORT |= (1 << VERSION_PIN); // Pull-up
  _delay_ms(10); // Wait for settling

  if (VERSION_PIN_REG & (1 << VERSION_PIN)) {
    // v1.1 - PB1 floating/high, has inverters, ACTIVE LOW
    gate_active_high = 0;
  } else {
    // v1.2+ - PB1 tied to GND, no inverters, ACTIVE HIGH
    gate_active_high = 1;
  }
}

void gpio_init(void) {
  detect_hardware_version();
  GATE_DDR_B |= (1 << GATE_PIN_0); // Set PB0 as output
  GATE_DDR_D |= 0xFE; // Set PD1 to PD7 as outputs

  DDRC |= (1 << LED_PIN);
  led_off();

  // This is to match original codebase, true case ought to be default
  while (!eeprom_is_ready())
    ;
  if (eeprom_read_byte((uint8_t*)EEPROM_BUTTON_FIX_ADDR) == 0xAA) {
    BUTTON_DDR &= (uint8_t)~(1 << BUTTON_PIN); // Set as input
    BUTTON_PORT |= (1 << BUTTON_PIN); // Enable pull-up
  } else {
    BUTTON_DDR |= (uint8_t)(1 << BUTTON_PIN);
  }

  CONTROL_DDR |= (1 << LDAC_PIN) | (1 << CLR_PIN);
  LDAC_PORT |= (1 << LDAC_PIN);
  CLR_PORT |= (1 << CLR_PIN);
}

void led_on(void) {
  LED_PORT &= ~(1 << LED_PIN);
}

void led_off(void) {
  LED_PORT |= (1 << LED_PIN);
}

uint8_t read_button(void) {
  return BUTTON_PIN_REG & (1 << BUTTON_PIN);
}

void gate_set(uint8_t gate_index, uint8_t state) {
  if (gate_index >= NUM_GATES) {
    return;
  }

  // Invert state for v1.1 hardware (active low)
  uint8_t hw_state = gate_active_high ? state : !state;

  volatile uint8_t* port_reg;
  uint8_t mask;

  if (gate_index == 0) {
    port_reg = &PORTB;
    mask = (uint8_t)(1 << PB0);
  } else {
    port_reg = &PORTD;
    mask = (uint8_t)(1 << (PD1 + gate_index - 1));
  }

  if (hw_state) {
    *port_reg |= mask;
  } else {
    *port_reg &= (uint8_t)~mask;
  }
}
