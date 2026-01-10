#include "../src/ui.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t simulated_reading = 0;

static uint8_t mock_button_read(void) { return simulated_reading; }

static void simulate_ticks(button_t *btn, uint16_t ticks) {
  for (uint16_t i = 0; i < ticks; i++) {
    button_update(btn, 1);
  }
}

static void test_idle_to_down(void) {
  button_t btn = {.state = BUTTON_IDLE, .ticks = 0, .get_reading = mock_button_read};

  simulated_reading = 0;
  assert(btn.state == BUTTON_IDLE);

  // Press button
  simulated_reading = 1;
  button_update(&btn, 1);
  assert(btn.state == BUTTON_DEBOUNCING);

  // Wait for debounce
  simulate_ticks(&btn, DEBOUNCE_TICKS);
  assert(btn.state == BUTTON_DOWN);

  printf("test_idle_to_down passed\n");
}

static void test_short_press(void) {
  button_t btn = {.state = BUTTON_IDLE, .ticks = 0, .get_reading = mock_button_read};

  // Press and debounce
  simulated_reading = 1;
  simulate_ticks(&btn, DEBOUNCE_TICKS + 1);
  assert(btn.state == BUTTON_DOWN);

  // Release - triggers PRESSED state
  simulated_reading = 0;
  button_update(&btn, 1);
  assert(btn.state == BUTTON_PRESSED);

  // Next update returns to IDLE
  button_update(&btn, 1);
  assert(btn.state == BUTTON_IDLE);

  printf("test_short_press passed\n");
}

static void test_long_press(void) {
  button_t btn = {.state = BUTTON_IDLE, .ticks = 0, .get_reading = mock_button_read};

  // Press and debounce
  simulated_reading = 1;
  simulate_ticks(&btn, DEBOUNCE_TICKS + 1);
  assert(btn.state == BUTTON_DOWN);

  // Hold until just before long press threshold
  simulate_ticks(&btn, LONG_PRESS_TICKS - DEBOUNCE_TICKS - 1);
  assert(btn.state == BUTTON_DOWN);

  // One more tick triggers HELD
  button_update(&btn, 1);
  assert(btn.state == BUTTON_HELD);

  // Next update goes to wait release
  button_update(&btn, 1);
  assert(btn.state == BUTTON_HELD_WAIT_RELEASE);

  // Still holding - stays in wait release
  button_update(&btn, 1);
  assert(btn.state == BUTTON_HELD_WAIT_RELEASE);

  // Release
  simulated_reading = 0;
  button_update(&btn, 1);
  assert(btn.state == BUTTON_IDLE);

  printf("test_long_press passed\n");
}

static void test_debounce_bounce(void) {
  button_t btn = {.state = BUTTON_IDLE, .ticks = 0, .get_reading = mock_button_read};

  // Start press
  simulated_reading = 1;
  button_update(&btn, 1);
  assert(btn.state == BUTTON_DEBOUNCING);

  // Button bounces during debounce (release before threshold)
  simulated_reading = 0;
  button_update(&btn, 1);

  // Should return to idle
  assert(btn.state == BUTTON_IDLE);

  printf("test_debounce_bounce passed\n");
}

int main(void) {
  printf("Running button tests...\n");

  test_idle_to_down();
  test_short_press();
  test_long_press();
  test_debounce_bounce();

  printf("All button tests passed!\n");
  return 0;
}
