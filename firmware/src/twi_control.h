#ifndef TWI_CONTROL_H
#define TWI_CONTROL_H

#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifndef TWI_FREQ
#define TWI_FREQ 400000UL
#endif

static inline void twi_init(void) {
  TWSR = 0x00;
  TWBR = (uint8_t)(((F_CPU / TWI_FREQ) - 16) / 2);
  TWCR = (1 << TWEN);
}

static inline void twi_start(void) {
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
}

static inline void twi_stop(void) {
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  // No need to wait for stop condition to complete
}

static inline void twi_write(uint8_t data) {
  TWDR = data;
  TWCR = (1 << TWINT) | (1 << TWEN);
  while (!(TWCR & (1 << TWINT)))
    ;
}

#endif
