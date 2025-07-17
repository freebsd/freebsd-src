/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "cbor.h"

unsigned char buffer[512];

static void test_embedded_map_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(1, cbor_encode_map_start(1, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xA1}), 1);
}

static void test_map_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(5, cbor_encode_map_start(1000000, buffer, 512));
  assert_memory_equal(buffer, ((unsigned char[]){0xBA, 0x00, 0x0F, 0x42, 0x40}),
                      5);
}

static void test_indef_map_start(void **_CBOR_UNUSED(_state)) {
  assert_size_equal(1, cbor_encode_indef_map_start(buffer, 512));
  assert_size_equal(0, cbor_encode_indef_map_start(buffer, 0));
  assert_memory_equal(buffer, ((unsigned char[]){0xBF}), 1);
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_embedded_map_start),
                                     cmocka_unit_test(test_map_start),
                                     cmocka_unit_test(test_indef_map_start)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
