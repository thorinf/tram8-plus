#include "../src/note_stack.h"

#include <assert.h>
#include <stdio.h>

static void test_empty(void) {
  note_stack_t s = {0};
  assert(note_stack_empty(&s));
  printf("test_empty passed\n");
}

static void test_push_pop(void) {
  note_stack_t s = {0};
  note_stack_push(&s, 60, 100);
  assert(!note_stack_empty(&s));
  assert(note_stack_top_note(&s) == 60);
  assert(note_stack_top_vel(&s) == 100);

  note_stack_push(&s, 64, 80);
  assert(note_stack_top_note(&s) == 64);
  assert(note_stack_top_vel(&s) == 80);

  note_stack_remove(&s, 64);
  assert(note_stack_top_note(&s) == 60);

  note_stack_remove(&s, 60);
  assert(note_stack_empty(&s));
  printf("test_push_pop passed\n");
}

static void test_dedup_on_push(void) {
  note_stack_t s = {0};
  note_stack_push(&s, 60, 100);
  note_stack_push(&s, 64, 80);
  note_stack_push(&s, 60, 90);

  assert(s.count == 2);
  assert(note_stack_top_note(&s) == 60);
  assert(note_stack_top_vel(&s) == 90);
  printf("test_dedup_on_push passed\n");
}

static void test_overflow_keeps_newest(void) {
  note_stack_t s = {0};
  for (uint8_t i = 0; i < NOTE_STACK_MAX + 3; ++i) {
    note_stack_push(&s, (uint8_t)(40 + i), (uint8_t)(50 + i));
  }
  assert(s.count == NOTE_STACK_MAX);
  assert(note_stack_top_note(&s) == 40 + NOTE_STACK_MAX + 2);
  printf("test_overflow_keeps_newest passed\n");
}

static void test_remove_middle(void) {
  note_stack_t s = {0};
  note_stack_push(&s, 60, 100);
  note_stack_push(&s, 64, 80);
  note_stack_push(&s, 67, 60);

  assert(note_stack_remove(&s, 64) == 1);
  assert(s.count == 2);
  assert(note_stack_top_note(&s) == 67);

  note_stack_remove(&s, 67);
  assert(note_stack_top_note(&s) == 60);
  printf("test_remove_middle passed\n");
}

static void test_rollback_on_top_release(void) {
  note_stack_t s = {0};
  note_stack_push(&s, 60, 40);
  note_stack_push(&s, 64, 90);
  assert(note_stack_top_vel(&s) == 90);

  note_stack_remove(&s, 64);
  assert(note_stack_top_vel(&s) == 40);
  printf("test_rollback_on_top_release passed\n");
}

static void test_clear(void) {
  note_stack_t s = {0};
  note_stack_push(&s, 60, 100);
  note_stack_push(&s, 64, 80);
  note_stack_clear(&s);
  assert(note_stack_empty(&s));
  printf("test_clear passed\n");
}

int main(void) {
  test_empty();
  test_push_pop();
  test_dedup_on_push();
  test_overflow_keeps_newest();
  test_remove_middle();
  test_rollback_on_top_release();
  test_clear();
  printf("\nAll tests passed!\n");
  return 0;
}
