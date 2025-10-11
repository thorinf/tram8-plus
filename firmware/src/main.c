#include "hardware_config.h"
#include "midi_parser.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

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

int main(void) {
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
      if (midi_parse(&parser, byte, &msg)) {
        // TODO: Handle complete MIDI message
      }
    }
  }
  return 0;
}
