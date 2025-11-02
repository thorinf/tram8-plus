#pragma once

#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR ((F_CPU / (16UL * BAUD)) - 1)

// Timer configuration
#define TIMER_PRESCALER 64
#define TIMER_FREQ_HZ 1000 // 1kHz = 1ms tick
#define TIMER_OCR ((F_CPU / TIMER_PRESCALER / TIMER_FREQ_HZ) - 1)
#define TIMER_TICK_MS (1000 / TIMER_FREQ_HZ)

// LED indicator
#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_PIN PC0

// Button input
#define BUTTON_PORT PORTC
#define BUTTON_DDR DDRC
#define BUTTON_PIN_REG PINC
#define BUTTON_PIN PC1

// Gate configuration
#define NUM_GATES 8
#define GATE_PORT_B PORTB
#define GATE_PORT_D PORTD
#define GATE_DDR_B DDRB
#define GATE_DDR_D DDRD
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1

// DAC control pins
#define LDAC_PIN PC2
#define CLR_PIN PC3
#define LDAC_PORT PORTC
#define CLR_PORT PORTC
#define CONTROL_DDR DDRC

// EEPROM configuration
#define EEPROM_BUTTON_FIX_ADDR 0x07
#define EEPROM_MIDIMAP_ADDR 0x101
