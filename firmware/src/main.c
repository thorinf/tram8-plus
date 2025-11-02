#include "gpio.h"
#include "hardware_config.h"
#include "midi_parser.h"
#include "ui.h"
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
static volatile uint8_t timer_ticks = 0;

static button_t learn_button = {BUTTON_IDLE, 0, read_button};
static led_t learn_led = {LED_OFF, 0, 0, led_on, led_off};

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
    head = rb_tail = 0;
    next = 1;
    rb_overflow = 1;
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
  gpio_init();
  gate_wipe();

  USART_Init(MY_UBRR);
  Timer_Init();

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

    if (timer_ticks) {
      uint8_t ticks = timer_ticks;
      timer_ticks = 0;
      button_update(&learn_button, ticks);

      if (learn_button.state == BUTTON_HELD) {
        if (learn_led.state == LED_OFF) {
          learn_led.state = LED_ON;
        } else {
          learn_led.state = LED_OFF;
        }
      }

      led_update(&learn_led, ticks);
    }
  }
}
