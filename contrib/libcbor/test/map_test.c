/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

cbor_item_t *map;
struct cbor_load_result res;

unsigned char empty_map[] = {0xA0};

static void test_empty_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(empty_map, 1, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_size(map) == 0);
  assert_true(res.read == 1);
  assert_size_equal(cbor_map_allocated(map), 0);
  cbor_decref(&map);
  assert_null(map);
}

unsigned char simple_map[] = {0xA2, 0x01, 0x02, 0x03, 0x04};

/* {1: 2, 3: 4} */
static void test_simple_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(simple_map, 5, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_definite(map));
  assert_true(cbor_map_size(map) == 2);
  assert_true(res.read == 5);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_uint8(handle[0].key, 1);
  assert_uint8(handle[0].value, 2);
  assert_uint8(handle[1].key, 3);
  assert_uint8(handle[1].value, 4);
  cbor_decref(&map);
  assert_null(map);
}

unsigned char simple_indef_map[] = {0xBF, 0x01, 0x02, 0x03, 0x04, 0xFF};

/* {_ 1: 2, 3: 4} */
static void test_indef_simple_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(simple_indef_map, 6, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_indefinite(map));
  assert_true(cbor_map_size(map) == 2);
  assert_true(res.read == 6);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_uint8(handle[0].key, 1);
  assert_uint8(handle[0].value, 2);
  assert_uint8(handle[1].key, 3);
  assert_uint8(handle[1].value, 4);
  cbor_decref(&map);
  assert_null(map);
}

//{
//	"glossary": {
//		"title": "example glossary"
//	}
//}
unsigned char def_nested_map[] = {
    0xA1, 0x68, 0x67, 0x6C, 0x6F, 0x73, 0x73, 0x61, 0x72, 0x79, 0xA1, 0x65,
    0x74, 0x69, 0x74, 0x6C, 0x65, 0x70, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C,
    0x65, 0x20, 0x67, 0x6C, 0x6F, 0x73, 0x73, 0x61, 0x72, 0x79};

static void test_def_nested_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(def_nested_map, 34, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_definite(map));
  assert_true(cbor_map_size(map) == 1);
  assert_true(res.read == 34);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_true(cbor_typeof(handle[0].key) == CBOR_TYPE_STRING);
  assert_true(cbor_typeof(handle[0].value) == CBOR_TYPE_MAP);
  struct cbor_pair *inner_handle = cbor_map_handle(handle[0].value);
  assert_true(cbor_typeof(inner_handle[0].key) == CBOR_TYPE_STRING);
  assert_true(cbor_typeof(inner_handle[0].value) == CBOR_TYPE_STRING);
  assert_memory_equal(cbor_string_handle(inner_handle[0].value),
                      "example glossary", strlen("example glossary"));
  cbor_decref(&map);
  assert_null(map);
}

unsigned char streamed_key_map[] = {0xA1, 0x7F, 0x61, 0x61,
                                    0x61, 0x62, 0xFF, 0xA0};

/* '{ (_"a" "b"): {}}' */
static void test_streamed_key_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(streamed_key_map, 8, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_definite(map));
  assert_true(cbor_map_size(map) == 1);
  assert_true(res.read == 8);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_true(cbor_typeof(handle[0].key) == CBOR_TYPE_STRING);
  assert_true(cbor_string_is_indefinite(handle[0].key));
  assert_size_equal(cbor_string_chunk_count(handle[0].key), 2);
  assert_true(cbor_isa_map(handle[0].value));
  assert_size_equal(cbor_map_size(handle[0].value), 0);
  cbor_decref(&map);
  assert_null(map);
}

unsigned char streamed_kv_map[] = {0xA1, 0x7F, 0x61, 0x61, 0x61, 0x62, 0xFF,
                                   0x7F, 0x61, 0x63, 0x61, 0x64, 0xFF};

/* '{ (_"a" "b"): (_"c", "d")}' */
static void test_streamed_kv_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(streamed_kv_map, 13, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_definite(map));
  assert_size_equal(cbor_map_size(map), 1);
  assert_size_equal(res.read, 13);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_true(cbor_typeof(handle[0].key) == CBOR_TYPE_STRING);
  assert_true(cbor_string_is_indefinite(handle[0].key));
  assert_size_equal(cbor_string_chunk_count(handle[0].key), 2);
  assert_true(cbor_typeof(handle[0].value) == CBOR_TYPE_STRING);
  assert_true(cbor_string_is_indefinite(handle[0].value));
  assert_size_equal(cbor_string_chunk_count(handle[0].value), 2);
  assert_memory_equal(
      cbor_string_handle(cbor_string_chunks_handle(handle[0].value)[1]), "d",
      1);
  cbor_decref(&map);
  assert_null(map);
}

unsigned char streamed_streamed_kv_map[] = {0xBF, 0x7F, 0x61, 0x61, 0x61,
                                            0x62, 0xFF, 0x7F, 0x61, 0x63,
                                            0x61, 0x64, 0xFF, 0xFF};

/* '{_ (_"a" "b"): (_"c", "d")}' */
static void test_streamed_streamed_kv_map(void **_CBOR_UNUSED(_state)) {
  map = cbor_load(streamed_streamed_kv_map, 14, &res);
  assert_non_null(map);
  assert_true(cbor_typeof(map) == CBOR_TYPE_MAP);
  assert_true(cbor_isa_map(map));
  assert_true(cbor_map_is_indefinite(map));
  assert_size_equal(cbor_map_size(map), 1);
  assert_size_equal(res.read, 14);
  struct cbor_pair *handle = cbor_map_handle(map);
  assert_true(cbor_typeof(handle[0].key) == CBOR_TYPE_STRING);
  assert_true(cbor_string_is_indefinite(handle[0].key));
  assert_size_equal(cbor_string_chunk_count(handle[0].key), 2);
  assert_true(cbor_typeof(handle[0].value) == CBOR_TYPE_STRING);
  assert_true(cbor_string_is_indefinite(handle[0].value));
  assert_size_equal(cbor_string_chunk_count(handle[0].value), 2);
  assert_memory_equal(
      cbor_string_handle(cbor_string_chunks_handle(handle[0].value)[1]), "d",
      1);
  cbor_decref(&map);
  assert_null(map);
}

static void test_map_add_full(void **_CBOR_UNUSED(_state)) {
  map = cbor_new_definite_map(0);
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_false(cbor_map_add(map, (struct cbor_pair){.key = one, .value = two}));

  cbor_decref(&map);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_map_add_too_big_to_realloc(void **_CBOR_UNUSED(_state)) {
  map = cbor_new_indefinite_map();
  struct _cbor_map_metadata *metadata =
      (struct _cbor_map_metadata *)&map->metadata;
  // Pretend we already have a huge memory block
  metadata->allocated = SIZE_MAX;
  metadata->end_ptr = SIZE_MAX;
  cbor_item_t *one = cbor_build_uint8(1);
  cbor_item_t *two = cbor_build_uint8(2);

  assert_false(cbor_map_add(map, (struct cbor_pair){.key = one, .value = two}));

  metadata->allocated = 0;
  metadata->end_ptr = 0;
  cbor_decref(&map);
  cbor_decref(&one);
  cbor_decref(&two);
}

static void test_map_creation(void **_CBOR_UNUSED(_state)) {
  WITH_FAILING_MALLOC({ assert_null(cbor_new_definite_map(42)); });
  WITH_MOCK_MALLOC({ assert_null(cbor_new_definite_map(42)); }, 2, MALLOC,
                   MALLOC_FAIL);

  WITH_FAILING_MALLOC({ assert_null(cbor_new_indefinite_map()); });
}

static void test_map_add(void **_CBOR_UNUSED(_state)) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *map = cbor_new_indefinite_map();
        cbor_item_t *key = cbor_build_uint8(0);
        cbor_item_t *value = cbor_build_bool(true);

        assert_false(
            cbor_map_add(map, (struct cbor_pair){.key = key, .value = value}));
        assert_size_equal(cbor_map_allocated(map), 0);
        assert_null(map->data);

        cbor_decref(&map);
        cbor_decref(&key);
        cbor_decref(&value);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

static unsigned char test_indef_map[] = {0xBF, 0x01, 0x02, 0x03, 0x04, 0xFF};
static void test_indef_map_decode(void **_CBOR_UNUSED(_state)) {
  WITH_MOCK_MALLOC(
      {
        cbor_item_t *map;
        struct cbor_load_result res;
        map = cbor_load(test_indef_map, 6, &res);

        assert_null(map);
        assert_size_equal(res.error.code, CBOR_ERR_MEMERROR);
      },
      4, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_empty_map),
      cmocka_unit_test(test_simple_map),
      cmocka_unit_test(test_indef_simple_map),
      cmocka_unit_test(test_def_nested_map),
      cmocka_unit_test(test_streamed_key_map),
      cmocka_unit_test(test_streamed_kv_map),
      cmocka_unit_test(test_streamed_streamed_kv_map),
      cmocka_unit_test(test_map_add_full),
      cmocka_unit_test(test_map_add_too_big_to_realloc),
      cmocka_unit_test(test_map_creation),
      cmocka_unit_test(test_map_add),
      cmocka_unit_test(test_indef_map_decode),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
