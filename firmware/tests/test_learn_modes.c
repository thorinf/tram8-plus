#include "../src/hardware_config.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t gates[NUM_GATES], led;
static uint16_t dac[NUM_GATES];
static uint8_t eeprom[512];
static unsigned mode_updates, map_updates;
static unsigned button_reads, menu_presses;
static uint8_t playing;

// Stub hardware boundaries; compile the actual menu, dispatcher, mapper and learn code.
#define MAX5825_CONTROL_H
#define TWI_CONTROL_H
static void twi_init(void) {}
static void max5825_init(void) {}
static void max5825_write(uint8_t gate, uint16_t value) {
  dac[gate] = value;
}
static void max5825_write_all(const uint16_t* values) {
  memcpy(dac, values, sizeof(dac));
}

#define main firmware_main
#include "../src/main.c"
#undef main

uint8_t UBRRH, UBRRL, UCSRB, UCSRC, TCCR2, OCR2, TCNT2, TIMSK, UDR;

void test_tick(void) {
  timer_ticks = 255;
}

uint8_t eeprom_read_byte(const uint8_t* address) {
  assert((uintptr_t)address < sizeof(eeprom));
  return eeprom[(uintptr_t)address];
}

void eeprom_read_block(void* dest, const void* address, size_t size) {
  assert((uintptr_t)address + size <= sizeof(eeprom));
  memcpy(dest, eeprom + (uintptr_t)address, size);
}

void eeprom_update_byte(uint8_t* address, uint8_t value) {
  assert((uintptr_t)address < sizeof(eeprom));
  if ((uintptr_t)address == EEPROM_MODE_ADDR) {
    ++mode_updates;
  } else if ((uintptr_t)address == EEPROM_CHANNEL_ADDR) {
    ++map_updates;
  }
  eeprom[(uintptr_t)address] = value;
}

void eeprom_update_block(const void* src, void* address, size_t size) {
  assert((uintptr_t)address == EEPROM_NOTEMAP_ADDR && size == NUM_GATES);
  ++map_updates;
  memcpy(eeprom + (uintptr_t)address, src, size);
}

void gpio_init(void) {}
void gate_set(uint8_t gate, uint8_t value) {
  assert(gate < NUM_GATES);
  gates[gate] = value;
}
void gate_set_mask(uint8_t mask) {
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    gate_set(gate, (mask >> gate) & 1);
  }
}
void led_on(void) {
  led = 1;
}
void led_off(void) {
  led = 0;
}

uint8_t read_button(void) {
  assert(++button_reads < 100);
  if (playing) {
    return rb_tail == rb_head;
  }
  unsigned step = button_reads - 1;
  if (step < 2) {
    return 0; // Release the hold that opened the menu.
  }
  step -= 2;
  if (step < menu_presses * 4) {
    return step % 4 < 2; // Debounced press, release, then idle.
  }
  return 1; // Hold to select the highlighted entry.
}

static void select_menu(uint8_t index) {
  playing = 0;
  button_reads = 0;
  menu_presses = index;
  learn_button.state = BUTTON_HELD;
  learn_button.ticks = 0;
  menu_mode_loop();
}

static void reset(uint8_t mode) {
  memset(eeprom, 0xFF, sizeof(eeprom));
  eeprom[EEPROM_CHANNEL_ADDR] = 9;
  eeprom[EEPROM_MODE_ADDR] = mode;
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    eeprom[EEPROM_NOTEMAP_ADDR + gate] = 60 + gate;
  }
  g_learn = (LearnState){0};
  led = 0;
  rb_head = rb_tail = rb_overflow = 0;
  mode_updates = map_updates = 0;
  midi_mapper_init();
  set_mode(mode);
}

static void assert_saved_map(void) {
  assert(midi_mapper_get_channel() == 9);
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    assert(midi_mapper_get_note_for_gate(gate) == 60 + gate);
  }
  assert(midi_mapper_get_gates(80) == 0);
}

static void test_cancel(uint8_t target, uint8_t partial) {
  reset(MODE_CC);
  select_menu(0);
  assert(learn_is_active() && led && gates[0]);
  assert(module_mode == MODE_CC && mode_updates == 0 && map_updates == 0);
  assert(midi_mapper_get_gates(60) == 0);

  if (partial) {
    MidiMsg note = {0x92, 80, 100, 3};
    handle_midi_message(&note);
    assert(learn_get_current_gate() == 1 && midi_mapper_get_channel() == 2);
  }
  select_menu(target);
  assert(!learn_is_active() && !led && module_mode == target);
  assert_saved_map();
  assert(eeprom[EEPROM_MODE_ADDR] == target && mode_updates == 1 && map_updates == 0);
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    assert(gates[gate] == 0 && dac[gate] == 0);
  }

  MidiMsg note = {0x99, 60, 100, 3};
  handle_midi_message(&note);
  assert(gates[0] && !learn_is_active());
  assert(dac[0] == (target == MODE_VELOCITY ? 100 << 5 : 0));
  note.status = 0x89;
  handle_midi_message(&note);
  assert(!gates[0]);
}

static void test_learn_from_sysex(void) {
  reset(MODE_SYSEX);
  select_menu(0);
  assert(module_mode == MODE_VELOCITY && learn_is_active());
  assert(eeprom[EEPROM_MODE_ADDR] == MODE_SYSEX && mode_updates == 0 && map_updates == 0);

  // Feed real MIDI bytes through the playback parser and learn handler.
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    rb[rb_head++] = 0x92;
    rb[rb_head++] = 80 + gate;
    rb[rb_head++] = 100;
  }
  playing = 1;
  button_reads = 0;
  play_mode_loop();
  assert(!learn_is_active() && !led && map_updates == 2 && mode_updates == 0);
  assert(eeprom[EEPROM_MODE_ADDR] == MODE_SYSEX && eeprom[EEPROM_CHANNEL_ADDR] == 2);
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    assert(midi_mapper_get_note_for_gate(gate) == 80 + gate);
    assert(eeprom[EEPROM_NOTEMAP_ADDR + gate] == 80 + gate);
    assert(!gates[gate] && dac[gate] == 0);
  }
  select_menu(MODE_SYSEX);
  assert(midi_mapper_get_channel() == 2 && midi_mapper_get_gates(80) == 1);
  assert(map_updates == 2 && mode_updates == 1);
}

int main(void) {
  for (uint8_t mode = MODE_VELOCITY; mode <= MODE_CC; ++mode) {
    test_cancel(mode, 0);
    test_cancel(mode, 1);
  }
  test_learn_from_sysex();
  puts("All learn mode transition tests passed!");
  return 0;
}
