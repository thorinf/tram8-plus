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
 * Payload is bitpacked LSB-first into 7-bit bytes.
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
 *   F0 7D 01 01 <15 packed bytes> F7
 *
 *   Bitstream (104 bits, LSB-first into 7-bit bytes):
 *     [7:0]   gate_mask  (8 bits, bit N = gate N)
 *     [19:8]  dac[0]     (12 bits)
 *     [31:20] dac[1]     (12 bits)
 *     ...
 *     [103:92] dac[7]    (12 bits)
 *
 *   ceil(104/7) = 15 data bytes
 *   Total message: 4 (header) + 15 (payload) + 1 (F7) = 20 bytes
 */

#define TRAM8_STATE_MSG_LEN 20

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

#ifdef __cplusplus
}
#endif

#endif
