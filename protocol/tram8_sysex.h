#ifndef TRAM8_SYSEX_H
#define TRAM8_SYSEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * tram8+ SysEx protocol
 *
 * All messages: F0 7D 01 <cmd> <payload...> F7
 *   7D = non-commercial/educational manufacturer ID
 *   01 = tram8 device ID
 *
 * All data bytes are 7-bit (0x00-0x7F) per MIDI spec.
 * 12-bit DAC values are split across 2 bytes:
 *   lo = bits 0-6, hi = bits 7-11
 */

#define TRAM8_SYSEX_START 0xF0
#define TRAM8_SYSEX_END 0xF7
#define TRAM8_MANUFACTURER_ID 0x7D
#define TRAM8_DEVICE_ID 0x01

#define TRAM8_CMD_STATE 0x01 /* full state update (gates + DACs) */
#define TRAM8_CMD_GATE_OFF 0x02 /* single gate off (legacy) */

#define TRAM8_NUM_GATES 8
#define TRAM8_DAC_BITS 12
#define TRAM8_DAC_MAX ((1 << TRAM8_DAC_BITS) - 1)

/*
 * State message layout (TRAM8_CMD_STATE):
 *
 *   F0 7D 01 01 <gate_lo> <gate_hi> <d0_lo> <d0_hi> ... <d7_lo> <d7_hi> F7
 *
 *   gate_lo:   bits 0-6 of gate mask (bit N = gate N, 1=on, 0=off)
 *   gate_hi:   bit 7 of gate mask (gate 7)
 *   dN_lo:     bits 0-6 of 12-bit DAC value
 *   dN_hi:     bits 7-11 of 12-bit DAC value
 *
 *   Total payload: 2 + 16 = 18 bytes
 *   Total message: 4 (header) + 18 (payload) + 1 (F7) = 23 bytes
 */

#define TRAM8_STATE_MSG_LEN 23

static inline uint8_t tram8_dac_lo(uint16_t dac) {
  return (uint8_t)(dac & 0x7F);
}

static inline uint8_t tram8_dac_hi(uint16_t dac) {
  return (uint8_t)((dac >> 7) & 0x1F);
}

static inline uint16_t tram8_dac_unpack(uint8_t lo, uint8_t hi) {
  return (uint16_t)((hi & 0x1F) << 7) | (lo & 0x7F);
}

static inline void tram8_pack_state(uint8_t* buf, uint8_t gate_mask, const uint16_t dac[8]) {
  buf[0] = TRAM8_SYSEX_START;
  buf[1] = TRAM8_MANUFACTURER_ID;
  buf[2] = TRAM8_DEVICE_ID;
  buf[3] = TRAM8_CMD_STATE;
  buf[4] = gate_mask & 0x7F;
  buf[5] = (gate_mask >> 7) & 0x01;
  for (int i = 0; i < 8; i++) {
    buf[6 + i * 2] = tram8_dac_lo(dac[i]);
    buf[6 + i * 2 + 1] = tram8_dac_hi(dac[i]);
  }
  buf[22] = TRAM8_SYSEX_END;
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
  if (buf[22] != TRAM8_SYSEX_END)
    return -1;

  *gate_mask = (uint8_t)((buf[5] & 0x01) << 7) | (buf[4] & 0x7F);
  for (int i = 0; i < 8; i++) {
    dac[i] = tram8_dac_unpack(buf[6 + i * 2], buf[6 + i * 2 + 1]);
  }
  return 0;
}

#ifdef __cplusplus
}
#endif

#endif
