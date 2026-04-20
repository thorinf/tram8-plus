#include "../../protocol/tram8_sysex.h"
#include "gpio.h"
#include "hardware_config.h"
#include "max5825_control.h"
#include "midi_learn.h"
#include "midi_mapper.h"
#include "midi_parser.h"
#include "twi_control.h"
#include "ui.h"
#include <avr/eeprom.h>
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
static uint8_t module_mode = MODE_VELOCITY;

static void handle_velocity(const MidiMsg* msg);
static void handle_cc(const MidiMsg* msg);
static void (*handle_midi_message)(const MidiMsg* msg) = handle_velocity;

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

ISR(TIMER2_COMP_vect) {
  timer_ticks++;
}

static inline uint8_t rb_pop(uint8_t* out) {
  if (rb_tail == rb_head)
    return 0;
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

static inline uint8_t pop_lsb(uint8_t* mask) {
  uint8_t value = *mask;
  uint8_t gate = 0;
  while (((value >> gate) & 1u) == 0u) {
    ++gate;
  }
  *mask = (uint8_t)(value & (value - 1));
  return gate;
}

static void set_mode(uint8_t mode) {
  module_mode = mode;
  handle_midi_message = (mode == MODE_CC) ? handle_cc : handle_velocity;
  for (uint8_t i = 0; i < NUM_GATES; ++i) {
    gate_set(i, 0);
    max5825_write(i, 0);
  }
}

static void handle_velocity(const MidiMsg* msg) {
  const uint8_t status = msg->status & 0xF0;
  const uint8_t channel = msg->status & 0x0F;
  const uint8_t note = msg->d1;
  const uint8_t velocity = msg->d2;

  if (learn_is_active()) {
    if (status == 0x90 && velocity > 0) {
      if (g_learn.gate == 0) {
        midi_mapper_set_channel(channel);
      }
      learn_on_note(note);
    }
    return;
  }

  if (channel != midi_mapper_get_channel()) {
    return;
  }

  uint8_t gate_candidates;

  switch (status) {
    case 0x90:
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
      gate_candidates = midi_mapper_get_gates(note);
      while (gate_candidates) {
        uint8_t gate_index = pop_lsb(&gate_candidates);
        gate_set(gate_index, 0);
        max5825_write(gate_index, 0);
      }
      break;
  }
}

static void handle_cc(const MidiMsg* msg) {
  const uint8_t status = msg->status & 0xF0;
  const uint8_t channel = msg->status & 0x0F;
  const uint8_t note = msg->d1;
  const uint8_t velocity = msg->d2;

  if (learn_is_active()) {
    if (status == 0x90 && velocity > 0) {
      if (g_learn.gate == 0) {
        midi_mapper_set_channel(channel);
      }
      learn_on_note(note);
    }
    return;
  }

  if (channel != midi_mapper_get_channel()) {
    return;
  }

  uint8_t gate_candidates;

  switch (status) {
    case 0x90:
      gate_candidates = midi_mapper_get_gates(note);
      while (gate_candidates) {
        uint8_t gate_index = pop_lsb(&gate_candidates);
        if (velocity > 0) {
          gate_set(gate_index, 1);
        } else {
          gate_set(gate_index, 0);
        }
      }
      break;
    case 0x80:
      gate_candidates = midi_mapper_get_gates(note);
      while (gate_candidates) {
        uint8_t gate_index = pop_lsb(&gate_candidates);
        gate_set(gate_index, 0);
      }
      break;
    case 0xB0:
      if (msg->d1 >= 69 && msg->d1 <= 76) {
        uint8_t dac_ch = msg->d1 - 69;
        max5825_write(dac_ch, (uint16_t)msg->d2 << 5);
      }
      break;
  }
}

static void handle_sysex_state(const uint8_t* buf, uint8_t len) {
  uint8_t gate_mask;
  uint16_t dac[TRAM8_NUM_GATES];

  if (tram8_parse_state(buf, len, &gate_mask, dac) != 0)
    return;

  for (uint8_t i = 0; i < TRAM8_NUM_GATES; i++) {
    gate_set(i, (gate_mask >> i) & 1);
    max5825_write(i, dac[i]);
  }
}

static void sysex_mode_loop(void) {
  uint8_t syx_buf[TRAM8_STATE_MSG_LEN];
  uint8_t syx_len = 0;
  uint8_t in_sysex = 0;

  for (;;) {
    uint8_t overflow;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      overflow = rb_overflow;
    }
    if (rb_tail == rb_head && overflow) {
      in_sysex = 0;
      syx_len = 0;
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        rb_overflow = 0;
      }
    }

    uint8_t byte;
    if (rb_pop(&byte)) {
      if (byte >= 0xF8)
        continue; // skip real-time

      if (byte == TRAM8_SYSEX_START) {
        in_sysex = 1;
        syx_len = 0;
        syx_buf[syx_len++] = byte;
        continue;
      }

      if (!in_sysex)
        continue;

      if (byte == TRAM8_SYSEX_END) {
        if (syx_len < TRAM8_STATE_MSG_LEN - 1) {
          in_sysex = 0;
          continue;
        }
        syx_buf[syx_len++] = byte;
        handle_sysex_state(syx_buf, syx_len);
        in_sysex = 0;
        syx_len = 0;
        continue;
      }

      if (byte >= 0x80) {
        in_sysex = 0;
        syx_len = 0;
        continue;
      }

      if (syx_len < TRAM8_STATE_MSG_LEN - 1) {
        syx_buf[syx_len++] = byte;
      } else {
        in_sysex = 0;
        syx_len = 0;
      }
    }

    uint8_t ticks;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      ticks = timer_ticks;
      timer_ticks = 0;
    }
    if (ticks) {
      button_update(&learn_button, ticks);
      if (learn_button.state == BUTTON_HELD)
        return;
    }
  }
}

static void play_mode_loop(void) {
  MidiParser parser;
  midi_parser_init(&parser);

  for (;;) {
    uint8_t overflow;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      overflow = rb_overflow;
    }
    if (rb_tail == rb_head && overflow) {
      midi_parser_force_desync(&parser);
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        rb_overflow = 0;
      }
    }

    uint8_t byte;
    if (rb_pop(&byte)) {
      MidiMsg msg;
      if (midi_parse(&parser, byte, &msg)) {
        handle_midi_message(&msg);
      }
    }

    uint8_t ticks;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      ticks = timer_ticks;
      timer_ticks = 0;
    }
    if (ticks) {
      button_update(&learn_button, ticks);

      if (learn_button.state == BUTTON_HELD) {
        return;
      }
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
    if (!ticks) {
      continue;
    }

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
          set_mode(MODE_VELOCITY);
          eeprom_update_byte((uint8_t*)EEPROM_MODE_ADDR, module_mode);
          break;
        case 2:
          set_mode(MODE_CC);
          eeprom_update_byte((uint8_t*)EEPROM_MODE_ADDR, module_mode);
          break;
        case 3:
          set_mode(MODE_SYSEX);
          eeprom_update_byte((uint8_t*)EEPROM_MODE_ADDR, module_mode);
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

  uint8_t mode = eeprom_read_byte((uint8_t*)EEPROM_MODE_ADDR);
  if (mode == MODE_CC) {
    set_mode(MODE_CC);
  } else if (mode == MODE_SYSEX) {
    set_mode(MODE_SYSEX);
  } else {
    set_mode(MODE_VELOCITY);
  }

  gate_wipe();

  USART_Init(MY_UBRR);
  Timer_Init();

  sei();

  for (;;) {
    if (module_mode == MODE_SYSEX) {
      sysex_mode_loop();
    } else {
      play_mode_loop();
    }
    menu_mode_loop();
  }
}
