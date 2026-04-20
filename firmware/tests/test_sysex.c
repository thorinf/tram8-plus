#include "../../protocol/tram8_sysex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_gates_only(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {100, 200, 300, 400, 500, 600, 700, 800};

  uint8_t len = tram8_pack(buf, 0xA5, dac_in, TRAM8_FORM_GATES);
  assert(len == 6);
  assert(buf[0] == 0xF0);
  assert(buf[1] == 0x7D);
  assert(buf[2] == 0x10);
  assert(buf[5] == 0xF7);

  uint8_t gate_out;
  uint16_t dac_out[8] = {0};
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0xA5);
  assert(form == TRAM8_FORM_GATES);

  printf("gates_only passed\n");
}

static void test_coarse_roundtrip(void) {
  uint8_t buf[24];
  uint16_t dac_in[8];
  for (int i = 0; i < 8; i++)
    dac_in[i] = (uint16_t)(i * 16) << 7;

  uint8_t len = tram8_pack(buf, 0xFF, dac_in, TRAM8_FORM_COARSE);
  assert(len == 14);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0xFF);
  assert(form == TRAM8_FORM_COARSE);

  for (int i = 0; i < 8; i++) {
    uint16_t expected = dac_in[i] & 0x3F80;
    assert(dac_out[i] == expected);
  }

  printf("coarse_roundtrip passed\n");
}

static void test_full_roundtrip(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0x0000, 0x3FFC, 0x0248, 0x1158, 0x1E24, 0x2AF0, 0x0004, 0x2000};

  uint8_t len = tram8_pack(buf, 0xA5, dac_in, TRAM8_FORM_FULL);
  assert(len == 22);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0xA5);
  assert(form == TRAM8_FORM_FULL);

  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == dac_in[i]);

  printf("full_roundtrip passed\n");
}

static void test_gate8_bit7(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};

  tram8_pack(buf, 0x80, dac_in, TRAM8_FORM_GATES);
  assert(buf[3] == 0x00);
  assert(buf[4] == 0x01);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, TRAM8_LEN_GATES, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0x80);

  printf("gate8_bit7 passed\n");
}

static void test_all_data_bytes_7bit(void) {
  uint8_t buf[24];
  uint16_t dac_in[8];
  for (int i = 0; i < 8; i++)
    dac_in[i] = 0x3FFF;

  uint8_t len = tram8_pack(buf, 0x7F, dac_in, TRAM8_FORM_FULL);

  for (int i = 1; i < len - 1; i++)
    assert(buf[i] <= 0x7F);

  printf("all_data_bytes_7bit passed\n");
}

static void test_velocity_precision(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};
  dac_in[0] = 127 << 7;

  uint8_t len = tram8_pack(buf, 0x01, dac_in, TRAM8_FORM_COARSE);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(dac_out[0] == 127 << 7);

  printf("velocity_precision passed\n");
}

static void test_roundtrip_zeros(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};

  uint8_t len = tram8_pack(buf, 0x00, dac_in, TRAM8_FORM_FULL);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0x00);
  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == 0);

  printf("roundtrip_zeros passed\n");
}

static void test_roundtrip_all_ones(void) {
  uint8_t buf[24];
  uint16_t dac_in[8];
  for (int i = 0; i < 8; i++)
    dac_in[i] = 0x3FFF;

  uint8_t len = tram8_pack(buf, 0xFF, dac_in, TRAM8_FORM_FULL);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
  assert(gate_out == 0xFF);
  for (int i = 0; i < 8; i++)
    assert(dac_out[i] == 0x3FFF);

  printf("roundtrip_all_ones passed\n");
}

static void test_roundtrip_each_gate(void) {
  for (int g = 0; g < 8; g++) {
    uint8_t buf[24];
    uint8_t gate_mask = (uint8_t)(1 << g);
    uint16_t dac_in[8] = {0};

    tram8_pack(buf, gate_mask, dac_in, TRAM8_FORM_GATES);

    uint8_t gate_out;
    uint16_t dac_out[8];
    tram8_form_t form;
    assert(tram8_parse(buf, TRAM8_LEN_GATES, &gate_out, dac_out, &form) == 0);
    assert(gate_out == gate_mask);
  }

  printf("roundtrip_each_gate passed\n");
}

static void test_roundtrip_each_dac(void) {
  for (int ch = 0; ch < 8; ch++) {
    uint8_t buf[24];
    uint16_t dac_in[8] = {0};
    dac_in[ch] = 0x3FFC;

    uint8_t len = tram8_pack(buf, 0, dac_in, TRAM8_FORM_FULL);

    uint8_t gate_out;
    uint16_t dac_out[8];
    tram8_form_t form;
    assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);
    for (int i = 0; i < 8; i++)
      assert(dac_out[i] == dac_in[i]);
  }

  printf("roundtrip_each_dac passed\n");
}

static void test_parse_rejects_short(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};
  tram8_pack(buf, 0, dac_in, TRAM8_FORM_GATES);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, TRAM8_LEN_GATES - 1, &gate_out, dac_out, &form) == -1);

  printf("parse_rejects_short passed\n");
}

static void test_parse_rejects_bad_header(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};
  tram8_pack(buf, 0, dac_in, TRAM8_FORM_GATES);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;

  buf[1] = 0x00;
  assert(tram8_parse(buf, TRAM8_LEN_GATES, &gate_out, dac_out, &form) == -1);

  printf("parse_rejects_bad_header passed\n");
}

static void test_message_framing(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0};

  uint8_t len = tram8_pack(buf, 0, dac_in, TRAM8_FORM_GATES);

  assert(buf[0] == TRAM8_SYSEX_START);
  assert(buf[1] == TRAM8_MANUFACTURER_ID);
  assert(buf[2] == TRAM8_CMD_STATE);
  assert(buf[len - 1] == TRAM8_SYSEX_END);

  printf("message_framing passed\n");
}

static void test_14bit_to_12bit(void) {
  uint8_t buf[24];
  uint16_t dac_in[8] = {0x3FFC, 0x0000, 0x1F00, 0x007C, 0x2A54, 0x3F80, 0x0080, 0x1554};

  uint8_t len = tram8_pack(buf, 0xFF, dac_in, TRAM8_FORM_FULL);

  uint8_t gate_out;
  uint16_t dac_out[8];
  tram8_form_t form;
  assert(tram8_parse(buf, len, &gate_out, dac_out, &form) == 0);

  for (int i = 0; i < 8; i++) {
    uint16_t dac12 = dac_out[i] >> 2;
    uint16_t expected12 = dac_in[i] >> 2;
    assert(dac12 == expected12);
  }

  printf("14bit_to_12bit passed\n");
}

int main(void) {
  printf("Running SysEx protocol tests...\n");

  test_gates_only();
  test_coarse_roundtrip();
  test_full_roundtrip();
  test_gate8_bit7();
  test_all_data_bytes_7bit();
  test_velocity_precision();
  test_roundtrip_zeros();
  test_roundtrip_all_ones();
  test_roundtrip_each_gate();
  test_roundtrip_each_dac();
  test_parse_rejects_short();
  test_parse_rejects_bad_header();
  test_message_framing();
  test_14bit_to_12bit();

  printf("\nAll SysEx protocol tests passed!\n");
  return 0;
}
