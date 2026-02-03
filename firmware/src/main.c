#include "gpio.h"
#include "hardware_config.h"
#include "max5825_control.h"
#include "midi_learn.h"
#include "midi_mapper.h"
#include "midi_parser.h"
#include "twi_control.h"
#include "ui.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/atomic.h>
#include <util/delay.h>

#define RB_SIZE 64
#define RB_MASK (RB_SIZE - 1)

static volatile uint8_t rb[RB_SIZE];
static volatile uint8_t rb_head = 0;
static volatile uint8_t rb_tail = 0;
static volatile uint8_t rb_overflow = 0;
static volatile uint8_t timer_ticks = 0;

static button_t learn_button = {BUTTON_IDLE, 0, read_button};

void USART_Init(unsigned int ubrr) {
  UBRRH = (unsigned char)(ubrr >> 8);
  UBRRL = (unsigned char)ubrr;
  UCSRB = (1 << RXEN) | (1 << RXCIE);
  UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);
}

void Timer_Init(void) {
  TCCR2 = (1 << WGM21) | (1 << CS22);
  OCR2 = TIMER_OCR;
  TCNT2 = 0;
  TIMSK |= (1 << OCIE2);
}

ISR(USART_RXC_vect) {
  uint8_t byte = UDR;
  uint8_t head = rb_head;
  uint8_t next = (head + 1) & RB_MASK;

  if (next == rb_tail) {
    rb_overflow = 1;
    return;
  }
  rb[head] = byte;
  rb_head = next;
}

ISR(TIMER2_COMP_vect) { timer_ticks++; }

static inline uint8_t rb_pop(uint8_t *out) {
  if (rb_tail == rb_head) return 0;
  uint8_t tail = rb_tail;
  *out = rb[tail];
  rb_tail = (tail + 1) & RB_MASK;
  return 1;
}

static void gate_wipe(void) {
  for (uint8_t i = 0; i < NUM_GATES; i++) {
    gate_set(i, 1);
    _delay_ms(50);
    gate_set(i, 0);
  }
}

static inline uint8_t pop_lsb(uint8_t *mask) {
  uint8_t value = *mask;
  uint8_t gate = 0;
  while (((value >> gate) & 1u) == 0u) {
    ++gate;
  }
  *mask = (uint8_t)(value & (value - 1));
  return gate;
}

static void handle_midi_message(const MidiMsg *msg) {
  const uint8_t status = msg->status & 0xF0;
  const uint8_t channel = msg->status & 0x0F;
  const uint8_t note = msg->d1;
  const uint8_t velocity = msg->d2;

  uint8_t gate_candidates;

  switch (status) {
  case 0x90:
    if (learn_is_active()) {
      if (velocity > 0) {
        if (g_learn.gate == 0) { midi_mapper_set_channel(channel); }
        learn_on_note(note);
      }
      return;
    }

    if (channel != midi_mapper_get_channel()) { return; }

    gate_candidates = midi_mapper_get_gates(note);
    while (gate_candidates) {
      uint8_t gate_index = pop_lsb(&gate_candidates);
      if (velocity > 0) {
        gate_set(gate_index, 1);
        max5825_write(gate_index, (uint16_t)velocity << 5);
      } else {
        gate_set(gate_index, 0);
        max5825_write(gate_index, 0);
      }
    }
    break;
  case 0x80:
    if (learn_is_active()) { return; }

    if (channel != midi_mapper_get_channel()) { return; }

    gate_candidates = midi_mapper_get_gates(note);
    while (gate_candidates) {
      uint8_t gate_index = pop_lsb(&gate_candidates);
      gate_set(gate_index, 0);
      max5825_write(gate_index, 0);
    }
    break;
  case 0xB0:
    break;
  default:
    break;
  }
}

static void play_mode_loop(void) {
  MidiParser parser;
  midi_parser_init(&parser);

  for (;;) {
    uint8_t overflow;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { overflow = rb_overflow; }
    if (rb_tail == rb_head && overflow) {
      midi_parser_force_desync(&parser);
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { rb_overflow = 0; }
    }

    uint8_t byte;
    if (rb_pop(&byte)) {
      MidiMsg msg;
      if (midi_parse(&parser, byte, &msg)) { handle_midi_message(&msg); }
    }

    uint8_t ticks;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      ticks = timer_ticks;
      timer_ticks = 0;
    }
    if (ticks) {
      button_update(&learn_button, ticks);

      if (learn_button.state == BUTTON_HELD) { return; }
    }
  }
}

static void menu_mode_loop(void) {
  uint8_t menu_index = 0;
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    gate_set(gate, gate == menu_index);
  }

  for (;;) {
    uint8_t ticks;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      ticks = timer_ticks;
      timer_ticks = 0;
    }
    if (!ticks) { continue; }

    button_update(&learn_button, ticks);

    if (learn_button.state == BUTTON_PRESSED) {
      menu_index = (uint8_t)((menu_index + 1) % 4);
      for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
        gate_set(gate, gate == menu_index);
      }
    }

    if (learn_button.state == BUTTON_HELD) {
      switch (menu_index) {
      case 0:
        learn_begin();
        return;
      case 1:
        break;
      case 2:
        break;
      case 3:
        break;
      default:
        break;
      }

      for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
        gate_set(gate, 0);
      }
      return;
    }
  }
}

int main(void) {
  gpio_init();
  twi_init();
  max5825_init();
  midi_mapper_init();
  gate_wipe();

  USART_Init(MY_UBRR);
  Timer_Init();

  sei();

  for (;;) {
    play_mode_loop();
    menu_mode_loop();
  }
}
