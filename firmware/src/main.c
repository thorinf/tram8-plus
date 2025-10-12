#include "hardware_config.h"
#include "midi_parser.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define RB_SIZE 64
#define RB_MASK (RB_SIZE - 1)

static volatile uint8_t rb[RB_SIZE];
static volatile uint8_t rb_head = 0;
static volatile uint8_t rb_tail = 0;
static volatile uint8_t rb_overflow = 0;

void USART_Init(unsigned int ubrr) {
  UBRRH = (unsigned char)(ubrr >> 8);
  UBRRL = (unsigned char)ubrr;
  UCSRB = (1 << RXEN) | (1 << RXCIE);
  UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

ISR(USART_RXC_vect) {
  uint8_t byte = UDR;
  uint8_t head = rb_head;
  uint8_t next = (head + 1) & RB_MASK;

  if (next == rb_tail) {
    head = rb_tail = 0;
    next = 1;
    rb_overflow = 1;
  }
  rb[head] = byte;
  rb_head = next;
}

static inline uint8_t rb_pop(uint8_t *out) {
  if (rb_tail == rb_head) return 0;
  uint8_t tail = rb_tail;
  *out = rb[tail];
  rb_tail = (tail + 1) & RB_MASK;
  return 1;
}

static inline void gate_set(uint8_t gate_index, uint8_t state) {
  volatile uint8_t *port_reg;
  uint8_t mask;

  if (gate_index == 0) {
    port_reg = &GATE_PORT_B;
    mask = (uint8_t)(1 << GATE_PIN_0);
  } else {
    port_reg = &GATE_PORT_D;
    mask = (uint8_t)(1 << (GATE_PIN_1 + gate_index - 1));
  }

  if (state) {
    *port_reg |= mask;
  } else {
    *port_reg &= (uint8_t)~mask;
  }
}

static inline void gate_wipe(void) {
  for (uint8_t i = 0; i < NUM_GATES; i++) {
    gate_set(i, 1);
    _delay_ms(50);
    gate_set(i, 0);
  }
}

static void handle_midi_message(const MidiMsg *msg) {
  uint8_t status = msg->status & 0xF0;
  uint8_t note = msg->d1;
  uint8_t velocity = msg->d2;

  if ((status == 0x90 || status == 0x80) && note >= 24 && note <= 31) {
    uint8_t gate_index = note - 24; // 0-7
    uint8_t gate_state = (status == 0x90 && velocity > 0) ? 1 : 0;
    gate_set(gate_index, gate_state);
  }
}

int main(void) {
  GATE_DDR_B |= (1 << GATE_PIN_0); // Set PB0 as output
  GATE_DDR_D |= 0xFE;              // Set PD1 to PD7 as outputs

  gate_wipe();

  USART_Init(MY_UBRR);

  MidiParser parser;
  midi_parser_init(&parser);

  sei();

  for (;;) {
    uint8_t head = rb_head;
    if (rb_tail == rb_head && rb_overflow) {
      midi_parser_force_desync(&parser);
      rb_overflow = 0;
    }

    uint8_t byte;
    if (rb_pop(&byte)) {
      MidiMsg msg;
      if (midi_parse(&parser, byte, &msg)) { handle_midi_message(&msg); }
    }
  }
  return 0;
}
