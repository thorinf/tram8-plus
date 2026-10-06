#pragma once

void test_tick(void);
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(state) for (uint8_t once = (test_tick(), 1); once; once = 0)
