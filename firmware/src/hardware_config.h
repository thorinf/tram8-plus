#pragma once

#define F_CPU 16000000UL
#define BAUD 31250UL
#define MY_UBRR ((F_CPU / (16UL * BAUD)) - 1)

// Gate configuration
#define NUM_GATES 8
#define GATE_PORT_B PORTB
#define GATE_PORT_D PORTD
#define GATE_DDR_B DDRB
#define GATE_DDR_D DDRD
#define GATE_PIN_0 PB0
#define GATE_PIN_1 PD1
