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
 * DAC values are transmitted as 14-bit (two 7-bit bytes).
 * Firmware shifts >> 2 to get 12-bit for the hardware DAC.
 *
 * Gate mask: <gate_lo> <gate_hi>     (8 bits split across 7-bit boundary)
 * Optional:  <dac0_hi> ... <dac7_hi> (top 7 bits of each DAC, bits 13:7)
 * Optional:  <dac0_lo> ... <dac7_lo> (low 7 bits of each DAC, bits 6:0)
 *
 * gate_lo = gate_mask & 0x7F  (bits 6:0)
 * gate_hi = gate_mask >> 7    (bit 7)
 *
 * Firmware determines form by message length:
 *
 *   Form 1 - gates only:       6 bytes total
 *   Form 2 - gates + coarse:  14 bytes total
 *   Form 3 - gates + full:    22 bytes total
 *
 * Form 1: gates only (note off, DAC-off mode)
 *   F0 7D 10 GL GH F7
 *   Firmware sets gates, DACs unchanged.
 *
 * Form 2: gates + coarse DAC (velocity, CC — 7-bit sources)
 *   F0 7D 10 GL GH D0h D1h D2h D3h D4h D5h D6h D7h F7
 *   dac[i] = dac_hi[i] << 5   (128 levels, 7-bit precision)
 *
 * Form 3: gates + full 14-bit DAC (pitch mode)
 *   F0 7D 10 GL GH D0h..D7h D0l..D7l F7
 *   dac[i] = (dac_hi[i] << 5) | (dac_lo[i] >> 2)   (12-bit precision)
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
#define TRAM8_LEN_FULL 22
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
    buf[5 + i] = (uint8_t)((dac[i] >> 7) & 0x7F);

  if (form == TRAM8_FORM_COARSE) {
    buf[13] = TRAM8_SYSEX_END;
    return TRAM8_LEN_COARSE;
  }

  for (int i = 0; i < 8; i++)
    buf[13 + i] = (uint8_t)(dac[i] & 0x7F);

  buf[21] = TRAM8_SYSEX_END;
  return TRAM8_LEN_FULL;
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
    dac[i] = (uint16_t)(buf[5 + i] & 0x7F) << 7;

  if (len == TRAM8_LEN_COARSE) {
    *form = TRAM8_FORM_COARSE;
    return 0;
  }

  if (len < TRAM8_LEN_FULL)
    return -1;

  for (int i = 0; i < 8; i++)
    dac[i] |= (uint16_t)(buf[13 + i] & 0x7F);

  *form = TRAM8_FORM_FULL;
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif
