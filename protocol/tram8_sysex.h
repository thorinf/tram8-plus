#ifndef TRAM8_SYSEX_H
#define TRAM8_SYSEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * tram8+ SysEx protocol
 *
 * Header: F0 7D <cmd> <payload...> F7
 *   7D = non-commercial/educational manufacturer ID
 *
 * All data bytes are 7-bit (0x00-0x7F) per MIDI spec.
 */

#define TRAM8_SYSEX_START 0xF0
#define TRAM8_SYSEX_END 0xF7
#define TRAM8_MANUFACTURER_ID 0x7D

#define TRAM8_NUM_GATES 8
#define TRAM8_DAC_BITS 12
#define TRAM8_DAC_MAX ((1 << TRAM8_DAC_BITS) - 1)

/* --- v1 protocol (deprecated, kept for backwards compat) --- */

#define TRAM8_DEVICE_ID 0x01
#define TRAM8_CMD_STATE 0x01
#define TRAM8_CMD_GATE_OFF 0x02

/*
 * v1 state message: F0 7D 01 01 <15 packed bytes> F7 = 20 bytes always
 */

#define TRAM8_STATE_MSG_LEN 20

/* --- v2 protocol: variable-length state update --- */

/*
 * CMD 0x10: variable-length state update
 *
 * Header: F0 7D 10
 * Gate mask: <gate_lo> <gate_hi>     (8 bits split across 7-bit boundary)
 * Optional:  <dac0_hi> ... <dac7_hi> (top 7 bits of each DAC, bits 11:5)
 * Optional:  <lo_packed...>          (bottom 5 bits per DAC, bitpacked)
 * End: F7
 *
 * gate_lo = gate_mask & 0x7F  (bits 6:0)
 * gate_hi = gate_mask >> 7    (bit 7)
 *
 * Firmware determines form by payload length (bytes between cmd and F7):
 *
 *   Form 1 - gates only:      2 payload bytes  ->  6 bytes total
 *   Form 2 - gates + coarse:  10 payload bytes -> 14 bytes total
 *   Form 3 - gates + full:    16 payload bytes -> 20 bytes total
 *
 * Form 1: gates only (note off, DAC-off mode)
 *   F0 7D 10 GL GH F7
 *   Firmware sets gates, DACs unchanged.
 *
 * Form 2: gates + coarse DAC (velocity, CC — 7-bit sources)
 *   F0 7D 10 GL GH D0 D1 D2 D3 D4 D5 D6 D7 F7
 *   dac[i] = dac_hi[i] << 5   (128 levels, 0-4064)
 *
 * Form 3: gates + full 12-bit DAC (pitch mode)
 *   F0 7D 10 GL GH D0..D7 L0 L1 L2 L3 L4 L5 F7
 *   Low 5 bits packed LSB-first: 8×5 = 40 bits -> ceil(40/7) = 6 bytes
 *   dac[i] = (dac_hi[i] << 5) | low5[i]
 *
 * Typical sizes vs v1 (always 20 bytes):
 *   Note off (gates only):        5 bytes  (75% smaller)
 *   Velocity note on (coarse):   13 bytes  (35% smaller)
 *   Pitch note on (full):        19 bytes  ( 5% smaller)
 */

#define TRAM8_CMD_STATE_V2 0x10

#define TRAM8_V2_LEN_GATES 6
#define TRAM8_V2_LEN_COARSE 14
#define TRAM8_V2_LEN_FULL 20
#define TRAM8_V2_HEADER_LEN 3

static inline void tram8_pack_state(uint8_t* buf, uint8_t gate_mask, const uint16_t dac[8]) {
  buf[0] = TRAM8_SYSEX_START;
  buf[1] = TRAM8_MANUFACTURER_ID;
  buf[2] = TRAM8_DEVICE_ID;
  buf[3] = TRAM8_CMD_STATE;

  uint32_t acc = gate_mask;
  uint8_t bits = 8;
  uint8_t pos = 4;

  for (int i = 0; i < 8; i++) {
    acc |= (uint32_t)(dac[i] & 0xFFF) << bits;
    bits += 12;
    while (bits >= 7) {
      buf[pos++] = (uint8_t)(acc & 0x7F);
      acc >>= 7;
      bits -= 7;
    }
  }

  if (bits > 0)
    buf[pos++] = (uint8_t)(acc & 0x7F);

  buf[pos] = TRAM8_SYSEX_END;
}

static inline int tram8_parse_state(const uint8_t* buf, uint8_t len, uint8_t* gate_mask, uint16_t dac[8]) {
  if (len < TRAM8_STATE_MSG_LEN)
    return -1;
  if (buf[0] != TRAM8_SYSEX_START)
    return -1;
  if (buf[1] != TRAM8_MANUFACTURER_ID)
    return -1;
  if (buf[2] != TRAM8_DEVICE_ID)
    return -1;
  if (buf[3] != TRAM8_CMD_STATE)
    return -1;
  if (buf[TRAM8_STATE_MSG_LEN - 1] != TRAM8_SYSEX_END)
    return -1;

  uint32_t acc = 0;
  uint8_t bits = 0;
  uint8_t pos = 4;

  while (bits < 8) {
    acc |= (uint32_t)buf[pos++] << bits;
    bits += 7;
  }
  *gate_mask = (uint8_t)(acc & 0xFF);
  acc >>= 8;
  bits -= 8;

  for (int i = 0; i < 8; i++) {
    while (bits < 12) {
      acc |= (uint32_t)buf[pos++] << bits;
      bits += 7;
    }
    dac[i] = (uint16_t)(acc & 0xFFF);
    acc >>= 12;
    bits -= 12;
  }

  return 0;
}

/* --- v2 pack functions --- */

typedef enum { TRAM8_FORM_GATES, TRAM8_FORM_COARSE, TRAM8_FORM_FULL } tram8_form_t;

static inline uint8_t tram8_pack_v2(uint8_t* buf, uint8_t gate_mask, const uint16_t dac[8], tram8_form_t form) {
  buf[0] = TRAM8_SYSEX_START;
  buf[1] = TRAM8_MANUFACTURER_ID;
  buf[2] = TRAM8_CMD_STATE_V2;
  buf[3] = gate_mask & 0x7F;
  buf[4] = (gate_mask >> 7) & 0x01;

  if (form == TRAM8_FORM_GATES) {
    buf[5] = TRAM8_SYSEX_END;
    return TRAM8_V2_LEN_GATES;
  }

  for (int i = 0; i < 8; i++)
    buf[5 + i] = (uint8_t)((dac[i] >> 5) & 0x7F);

  if (form == TRAM8_FORM_COARSE) {
    buf[13] = TRAM8_SYSEX_END;
    return TRAM8_V2_LEN_COARSE;
  }

  uint32_t acc = 0;
  uint8_t bits = 0;
  uint8_t pos = 13;

  for (int i = 0; i < 8; i++) {
    acc |= (uint32_t)(dac[i] & 0x1F) << bits;
    bits += 5;
    while (bits >= 7) {
      buf[pos++] = (uint8_t)(acc & 0x7F);
      acc >>= 7;
      bits -= 7;
    }
  }
  if (bits > 0)
    buf[pos++] = (uint8_t)(acc & 0x7F);

  buf[pos] = TRAM8_SYSEX_END;
  return pos + 1;
}

static inline int
tram8_parse_v2(const uint8_t* buf, uint8_t len, uint8_t* gate_mask, uint16_t dac[8], tram8_form_t* form) {
  if (len < TRAM8_V2_LEN_GATES)
    return -1;
  if (buf[0] != TRAM8_SYSEX_START)
    return -1;
  if (buf[1] != TRAM8_MANUFACTURER_ID)
    return -1;
  if (buf[2] != TRAM8_CMD_STATE_V2)
    return -1;

  *gate_mask = (buf[3] & 0x7F) | ((buf[4] & 0x01) << 7);

  if (len == TRAM8_V2_LEN_GATES) {
    *form = TRAM8_FORM_GATES;
    return 0;
  }

  if (len < TRAM8_V2_LEN_COARSE)
    return -1;

  for (int i = 0; i < 8; i++)
    dac[i] = (uint16_t)(buf[5 + i] & 0x7F) << 5;

  if (len == TRAM8_V2_LEN_COARSE) {
    *form = TRAM8_FORM_COARSE;
    return 0;
  }

  if (len < TRAM8_V2_LEN_FULL)
    return -1;

  uint32_t acc = 0;
  uint8_t bits = 0;
  uint8_t pos = 13;

  for (int i = 0; i < 8; i++) {
    while (bits < 5) {
      acc |= (uint32_t)(buf[pos++] & 0x7F) << bits;
      bits += 7;
    }
    dac[i] |= (uint16_t)(acc & 0x1F);
    acc >>= 5;
    bits -= 5;
  }

  *form = TRAM8_FORM_FULL;
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif
