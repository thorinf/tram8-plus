#ifndef MAX5825_CONTROL_H
#define MAX5825_CONTROL_H

#include "twi_control.h"
#include <util/delay.h>

#define MAX5825_ADDR 0x20
#define MAX5825_REG_REF 0x20
#define MAX5825_REG_CODEn_LOADall 0xA0
#define MAX5825_REG_CODEn_LOADn 0xB0

static inline void max5825_init(void) {
  _delay_us(250); // Wait for DAC power-up calibration (200us typ)
  twi_start();
  twi_write(MAX5825_ADDR);
  twi_write(MAX5825_REG_REF | 0b101); // Setup command for reference voltage
  twi_write(0x00);
  twi_write(0x00);
  twi_stop();

  twi_start();
  twi_write(MAX5825_ADDR); // Device address with write bit
  twi_write(MAX5825_REG_CODEn_LOADall);
  twi_write(0x00);
  twi_write(0x00);
  twi_stop();
}

static inline void max5825_write(uint8_t channel, uint16_t value) {
  twi_start();
  twi_write(MAX5825_ADDR);
  twi_write(MAX5825_REG_CODEn_LOADn | (channel & 0x0F));

  twi_write((uint8_t)(value >> 4));
  twi_write((uint8_t)((value & 0x0F) << 4));
  twi_stop();
}

#endif
