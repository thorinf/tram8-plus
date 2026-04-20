#ifndef TRAM8_SYSEX_H
#define TRAM8_SYSEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * tram8+ SysEx protocol
 *
 * Header: F0 7D 10
 *   7D = non-commercial/educational manufacturer ID
 *   10 = state update command
 *
 * All data bytes are 7-bit (0x00-0x7F) per MIDI spec.
 * DAC values are 12-bit (0-4095) on the wire.
 *
 * Gate mask: <gate_lo> <gate_hi>     (8 bits split across 7-bit boundary)
 * Optional:  <dac0_hi> ... <dac7_hi> (top 7 bits of each DAC, bits 11:5)
 * Optional:  <lo_packed...>          (low 5 bits per DAC, bitpacked into 6 bytes)
 *
 * gate_lo = gate_mask & 0x7F  (bits 6:0)
 * gate_hi = gate_mask >> 7    (bit 7)
 *
 * Firmware determines form by message length:
 *
 *   Form 1 - gates only:       6 bytes total
 *   Form 2 - gates + coarse:  14 bytes total
 *   Form 3 - gates + full:    20 bytes total
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
 *   Low 5 bits packed LSB-first: 8x5 = 40 bits -> ceil(40/7) = 6 bytes
 *   dac[i] = (dac_hi[i] << 5) | low5[i]
 */

#define TRAM8_SYSEX_START 0xF0
#define TRAM8_SYSEX_END 0xF7
#define TRAM8_MANUFACTURER_ID 0x7D
#define TRAM8_CMD_STATE 0x10

#define TRAM8_NUM_GATES 8
#define TRAM8_DAC_BITS 12
#define TRAM8_DAC_MAX ((1 << TRAM8_DAC_BITS) - 1)

#define TRAM8_LEN_GATES 6
#define TRAM8_LEN_COARSE 14
#define TRAM8_LEN_FULL 20
#define TRAM8_HEADER_LEN 3

typedef enum { TRAM8_FORM_GATES, TRAM8_FORM_COARSE, TRAM8_FORM_FULL } tram8_form_t;

static inline uint8_t tram8_pack(uint8_t* buf, uint8_t gate_mask, const uint16_t dac[8], tram8_form_t form) {
  buf[0] = TRAM8_SYSEX_START;
  buf[1] = TRAM8_MANUFACTURER_ID;
  buf[2] = TRAM8_CMD_STATE;
  buf[3] = gate_mask & 0x7F;
  buf[4] = (gate_mask >> 7) & 0x01;

  if (form == TRAM8_FORM_GATES) {
    buf[5] = TRAM8_SYSEX_END;
    return TRAM8_LEN_GATES;
  }

  for (int i = 0; i < 8; i++)
    buf[5 + i] = (uint8_t)((dac[i] >> 5) & 0x7F);

  if (form == TRAM8_FORM_COARSE) {
    buf[13] = TRAM8_SYSEX_END;
    return TRAM8_LEN_COARSE;
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
tram8_parse(const uint8_t* buf, uint8_t len, uint8_t* gate_mask, uint16_t dac[8], tram8_form_t* form) {
  if (len < TRAM8_LEN_GATES)
    return -1;
  if (buf[0] != TRAM8_SYSEX_START)
    return -1;
  if (buf[1] != TRAM8_MANUFACTURER_ID)
    return -1;
  if (buf[2] != TRAM8_CMD_STATE)
    return -1;

  *gate_mask = (buf[3] & 0x7F) | ((buf[4] & 0x01) << 7);

  if (len == TRAM8_LEN_GATES) {
    *form = TRAM8_FORM_GATES;
    return 0;
  }

  if (len < TRAM8_LEN_COARSE)
    return -1;

  for (int i = 0; i < 8; i++)
    dac[i] = (uint16_t)(buf[5 + i] & 0x7F) << 5;

  if (len == TRAM8_LEN_COARSE) {
    *form = TRAM8_FORM_COARSE;
    return 0;
  }

  if (len < TRAM8_LEN_FULL)
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
