#include "../../protocol/tram8_sysex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_roundtrip_zeros(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint8_t gate_mask = 0x00;
  uint16_t dac_in[8] = {0};

  tram8_pack_state(buf, gate_mask, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
  assert(gate_out == 0x00);
  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == 0);

  printf("roundtrip_zeros passed\n");
}

static void test_roundtrip_all_ones(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint8_t gate_mask = 0xFF;
  uint16_t dac_in[8];
  for (int i = 0; i < 8; i++)
    dac_in[i] = TRAM8_DAC_MAX;

  tram8_pack_state(buf, gate_mask, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
  assert(gate_out == 0xFF);
  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == TRAM8_DAC_MAX);

  printf("roundtrip_all_ones passed\n");
}

static void test_roundtrip_gate7(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint8_t gate_mask = 0x80;
  uint16_t dac_in[8] = {0};

  tram8_pack_state(buf, gate_mask, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
  assert(gate_out == 0x80);

  printf("roundtrip_gate7 passed\n");
}

static void test_roundtrip_mixed(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint8_t gate_mask = 0xA5;
  uint16_t dac_in[8] = {0, 0xFFF, 0x123, 0x456, 0x789, 0xABC, 0x001, 0x800};

  tram8_pack_state(buf, gate_mask, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
  assert(gate_out == 0xA5);
  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == dac_in[i]);

  printf("roundtrip_mixed passed\n");
}

static void test_all_data_bytes_7bit(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint16_t dac_in[8];
  for (int i = 0; i < 8; i++)
    dac_in[i] = TRAM8_DAC_MAX;

  tram8_pack_state(buf, 0xFF, dac_in);

  for (int i = 4; i < TRAM8_STATE_MSG_LEN - 1; i++)
    assert(buf[i] <= 0x7F);

  printf("all_data_bytes_7bit passed\n");
}

static void test_message_framing(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint16_t dac_in[8] = {0};

  tram8_pack_state(buf, 0, dac_in);

  assert(buf[0] == TRAM8_SYSEX_START);
  assert(buf[1] == TRAM8_MANUFACTURER_ID);
  assert(buf[2] == TRAM8_DEVICE_ID);
  assert(buf[3] == TRAM8_CMD_STATE);
  assert(buf[TRAM8_STATE_MSG_LEN - 1] == TRAM8_SYSEX_END);

  printf("message_framing passed\n");
}

static void test_parse_rejects_short(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint16_t dac_in[8] = {0};
  tram8_pack_state(buf, 0, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN - 1, &gate_out, dac_out) == -1);

  printf("parse_rejects_short passed\n");
}

static void test_parse_rejects_bad_header(void) {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  uint16_t dac_in[8] = {0};
  tram8_pack_state(buf, 0, dac_in);

  uint8_t gate_out;
  uint16_t dac_out[8];

  buf[1] = 0x00;
  assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == -1);

  printf("parse_rejects_bad_header passed\n");
}

static void test_roundtrip_each_gate(void) {
  for (int g = 0; g < 8; g++) {
    uint8_t buf[TRAM8_STATE_MSG_LEN];
    uint8_t gate_mask = (uint8_t)(1 << g);
    uint16_t dac_in[8] = {0};

    tram8_pack_state(buf, gate_mask, dac_in);

    uint8_t gate_out;
    uint16_t dac_out[8];
    assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
    assert(gate_out == gate_mask);
  }

  printf("roundtrip_each_gate passed\n");
}

static void test_roundtrip_each_dac(void) {
  for (int ch = 0; ch < 8; ch++) {
    uint8_t buf[TRAM8_STATE_MSG_LEN];
    uint16_t dac_in[8] = {0};
    dac_in[ch] = TRAM8_DAC_MAX;

    tram8_pack_state(buf, 0, dac_in);

    uint8_t gate_out;
    uint16_t dac_out[8];
    assert(tram8_parse_state(buf, TRAM8_STATE_MSG_LEN, &gate_out, dac_out) == 0);
    for (int i = 0; i < 8; i++)
      assert(dac_out[i] == dac_in[i]);
  }

  printf("roundtrip_each_dac passed\n");
}

int main(void) {
  printf("Running SysEx protocol tests...\n");

  test_roundtrip_zeros();
  test_roundtrip_all_ones();
  test_roundtrip_gate7();
  test_roundtrip_mixed();
  test_all_data_bytes_7bit();
  test_message_framing();
  test_parse_rejects_short();
  test_parse_rejects_bad_header();
  test_roundtrip_each_gate();
  test_roundtrip_each_dac();

  printf("All SysEx protocol tests passed!\n");
  return 0;
}
