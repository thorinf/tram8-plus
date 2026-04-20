#ifndef SYSEX_HANDLER_H
#define SYSEX_HANDLER_H

#include <stdint.h>

#define SYSEX_BUF_SIZE 24

typedef struct {
  uint8_t buf[SYSEX_BUF_SIZE];
  uint8_t len;
} SysexAccumulator;

static inline void sysex_acc_init(SysexAccumulator *acc) { acc->len = 0; }

// Feed a byte. Returns 1 when a complete message is ready in acc->buf.
// Returns -1 if the message was cancelled (non-realtime status byte).
// Returns 0 if still accumulating.
static inline int8_t sysex_acc_feed(SysexAccumulator *acc, uint8_t b) {
  if (b == 0xF7) {
    if (acc->len < SYSEX_BUF_SIZE) { acc->buf[acc->len++] = b; }
    return 1; // complete
  }

  if (b >= 0xF8) {
    return 0; // real-time, ignore during sysex accumulation
  }

  if (b >= 0x80) {
    acc->len = 0;
    return -1; // cancelled by another status byte
  }

  // data byte
  if (acc->len < SYSEX_BUF_SIZE) { acc->buf[acc->len++] = b; }
  return 0;
}

#endif
