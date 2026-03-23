/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "assertions.h"
#include "cbor.h"
#include "test_allocator.h"

cbor_item_t *item, *copy, *tmp;

static void test_uints(void** _state _CBOR_UNUSED) {
  item = cbor_build_uint8(10);
  assert_uint8(copy = cbor_copy(item), 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_uint16(10);
  assert_uint16(copy = cbor_copy(item), 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_uint32(10);
  assert_uint32(copy = cbor_copy(item), 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_uint64(10);
  assert_uint64(copy = cbor_copy(item), 10);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_negints(void** _state _CBOR_UNUSED) {
  item = cbor_build_negint8(10);
  assert_true(cbor_get_uint8(copy = cbor_copy(item)) == 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_negint16(10);
  assert_true(cbor_get_uint16(copy = cbor_copy(item)) == 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_negint32(10);
  assert_true(cbor_get_uint32(copy = cbor_copy(item)) == 10);
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_negint64(10);
  assert_true(cbor_get_uint64(copy = cbor_copy(item)) == 10);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_def_bytestring(void** _state _CBOR_UNUSED) {
  item = cbor_build_bytestring((cbor_data) "abc", 3);
  assert_memory_equal(cbor_bytestring_handle(copy = cbor_copy(item)),
                      cbor_bytestring_handle(item), 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_indef_bytestring(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));
  copy = cbor_copy(item);

  assert_size_equal(cbor_bytestring_chunk_count(item),
                    cbor_bytestring_chunk_count(copy));

  assert_memory_equal(
      cbor_bytestring_handle(cbor_bytestring_chunks_handle(copy)[0]), "abc", 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_def_string(void** _state _CBOR_UNUSED) {
  item = cbor_build_string("abc");
  assert_memory_equal(cbor_string_handle(copy = cbor_copy(item)),
                      cbor_string_handle(item), 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_indef_string(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));
  copy = cbor_copy(item);

  assert_size_equal(cbor_string_chunk_count(item),
                    cbor_string_chunk_count(copy));

  assert_memory_equal(cbor_string_handle(cbor_string_chunks_handle(copy)[0]),
                      "abc", 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_def_array(void** _state _CBOR_UNUSED) {
  item = cbor_new_definite_array(1);
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  assert_uint8(tmp = cbor_array_get(copy = cbor_copy(item), 0), 42);
  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_indef_array(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  assert_uint8(tmp = cbor_array_get(copy = cbor_copy(item), 0), 42);
  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_def_map(void** _state _CBOR_UNUSED) {
  item = cbor_new_definite_map(1);
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  assert_uint8(cbor_map_handle(copy = cbor_copy(item))[0].key, 42);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_indef_map(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  assert_uint8(cbor_map_handle(copy = cbor_copy(item))[0].key, 42);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_tag(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(10, cbor_move(cbor_build_uint8(42)));

  assert_uint8(cbor_move(cbor_tag_item(copy = cbor_copy(item))), 42);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_ctrls(void** _state _CBOR_UNUSED) {
  item = cbor_new_null();
  assert_true(cbor_is_null(copy = cbor_copy(item)));
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_floats(void** _state _CBOR_UNUSED) {
  item = cbor_build_float2(3.14f);
  assert_true(cbor_float_get_float2(copy = cbor_copy(item)) ==
              cbor_float_get_float2(item));
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_float4(3.14f);
  assert_true(cbor_float_get_float4(copy = cbor_copy(item)) ==
              cbor_float_get_float4(item));
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_float8(3.14);
  assert_true(cbor_float_get_float8(copy = cbor_copy(item)) ==
              cbor_float_get_float8(item));
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_uints(void** _state _CBOR_UNUSED) {
  item = cbor_build_uint8(10);
  assert_uint8(copy = cbor_copy_definite(item), 10);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_negints(void** _state _CBOR_UNUSED) {
  item = cbor_build_negint16(10);
  assert_true(cbor_get_uint16(copy = cbor_copy_definite(item)) == 10);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_bytestring(void** _state _CBOR_UNUSED) {
  item = cbor_build_bytestring((cbor_data) "abc", 3);
  assert_memory_equal(cbor_bytestring_handle(copy = cbor_copy_definite(item)),
                      cbor_bytestring_handle(item), 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_indef_bytestring(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));

  assert_memory_equal(cbor_bytestring_handle(copy = cbor_copy_definite(item)),
                      "abc", 3);
  assert_true(cbor_isa_bytestring(copy));
  assert_true(cbor_bytestring_is_definite(copy));
  assert_size_equal(cbor_bytestring_length(copy), 3);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_bytestring_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy_definite(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_string(void** _state _CBOR_UNUSED) {
  item = cbor_build_string("abc");
  assert_memory_equal(cbor_string_handle(copy = cbor_copy_definite(item)),
                      cbor_string_handle(item), 3);
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_indef_string(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));

  assert_memory_equal(cbor_string_handle(copy = cbor_copy_definite(item)),
                      "abc", 3);
  assert_true(cbor_isa_string(copy));
  assert_true(cbor_string_is_definite(copy));
  assert_size_equal(cbor_string_length(copy), 3);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_string_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy_definite(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_array(void** _state _CBOR_UNUSED) {
  item = cbor_new_definite_array(1);
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_array(copy));
  assert_true(cbor_array_is_definite(copy));
  assert_size_equal(cbor_array_size(copy), 1);
  assert_uint8(tmp = cbor_array_get(copy, 0), 42);

  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_definite_indef_array(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_array(copy));
  assert_true(cbor_array_is_definite(copy));
  assert_uint8(tmp = cbor_array_get(copy, 0), 42);

  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_definite_indef_array_nested(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  cbor_item_t* nested_array = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(nested_array)));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_array(copy));
  assert_true(cbor_array_is_definite(copy));
  assert_size_equal(cbor_array_size(copy), 1);

  tmp = cbor_array_get(copy, 0);
  assert_true(cbor_isa_array(tmp));
  assert_true(cbor_array_is_definite(tmp));
  assert_size_equal(cbor_array_size(tmp), 0);

  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_definite_array_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy_definite(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_array_item_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy_definite(item)); }, 3,
                   // New array, new array data, item copy
                   MALLOC, MALLOC, MALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_map(void** _state _CBOR_UNUSED) {
  item = cbor_new_definite_map(1);
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_map(copy));
  assert_true(cbor_map_is_definite(copy));
  assert_size_equal(cbor_map_size(copy), 1);
  assert_uint8(cbor_map_handle(copy)[0].key, 42);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_indef_map(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_map(copy));
  assert_true(cbor_map_is_definite(copy));
  assert_size_equal(cbor_map_size(copy), 1);
  assert_uint8(cbor_map_handle(copy)[0].key, 42);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_indef_map_nested(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  cbor_item_t* key = cbor_new_indefinite_array();
  cbor_item_t* value = cbor_new_indefinite_array();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(key),
                                     .value = cbor_move(value),
                                 }));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_map(copy));
  assert_true(cbor_map_is_definite(copy));
  assert_size_equal(cbor_map_size(copy), 1);

  assert_true(cbor_isa_array(cbor_map_handle(copy)[0].key));
  assert_true(cbor_array_is_definite(cbor_map_handle(copy)[0].key));
  assert_size_equal(cbor_array_size(cbor_map_handle(copy)[0].key), 0);

  assert_true(cbor_isa_array(cbor_map_handle(copy)[0].value));
  assert_true(cbor_array_is_definite(cbor_map_handle(copy)[0].value));
  assert_size_equal(cbor_array_size(cbor_map_handle(copy)[0].value), 0);

  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_map_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy_definite(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_map_key_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy_definite(item)); }, 3,
                   // New map, map data, key copy
                   MALLOC, MALLOC, MALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_map_value_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(cbor_map_add(item, (struct cbor_pair){
                                     .key = cbor_move(cbor_build_uint8(42)),
                                     .value = cbor_move(cbor_build_uint8(43)),
                                 }));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy_definite(item)); }, 4,
                   // New map, map data, key copy, value copy
                   MALLOC, MALLOC, MALLOC, MALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_tag(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(10, cbor_move(cbor_build_uint8(42)));

  copy = cbor_copy_definite(item);
  assert_uint8(tmp = cbor_tag_item(copy), 42);

  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_definite_tag_nested(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(10, cbor_move(cbor_new_indefinite_array()));

  copy = cbor_copy_definite(item);
  assert_true(cbor_isa_tag(copy));

  tmp = cbor_tag_item(copy);
  assert_true(cbor_isa_array(tmp));
  assert_true(cbor_array_is_definite(tmp));
  assert_size_equal(cbor_array_size(tmp), 0);

  cbor_decref(&item);
  cbor_decref(&copy);
  cbor_decref(&tmp);
}

static void test_definite_tag_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(10, cbor_move(cbor_build_uint8(42)));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy_definite(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_definite_ctrls(void** _state _CBOR_UNUSED) {
  item = cbor_new_null();
  assert_true(cbor_is_null(copy = cbor_copy_definite(item)));
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_definite_floats(void** _state _CBOR_UNUSED) {
  item = cbor_build_float2(3.14f);
  assert_true(cbor_float_get_float2(copy = cbor_copy_definite(item)) ==
              cbor_float_get_float2(item));
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_float4(3.14f);
  assert_true(cbor_float_get_float4(copy = cbor_copy_definite(item)) ==
              cbor_float_get_float4(item));
  cbor_decref(&item);
  cbor_decref(&copy);

  item = cbor_build_float8(3.14);
  assert_true(cbor_float_get_float8(copy = cbor_copy_definite(item)) ==
              cbor_float_get_float8(item));
  cbor_decref(&item);
  cbor_decref(&copy);
}

static void test_alloc_failure_simple(void** _state _CBOR_UNUSED) {
  item = cbor_build_uint8(10);

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_bytestring_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_bytestring_chunk_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 2, MALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_bytestring_chunk_append_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 5,
                   // New indef string, cbor_indefinite_string_data, chunk item,
                   // chunk data, extend cbor_indefinite_string_data.chunks
                   MALLOC, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_bytestring_second_chunk_alloc_failure(
    void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_bytestring();
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "abc", 3))));
  assert_true(cbor_bytestring_add_chunk(
      item, cbor_move(cbor_build_bytestring((cbor_data) "def", 3))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 6,
                   // New indef string, cbor_indefinite_string_data, chunk item,
                   // chunk data, extend cbor_indefinite_string_data.chunks,
                   // second chunk item
                   MALLOC, MALLOC, MALLOC, MALLOC, REALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_string_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_string_chunk_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 2, MALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_string_chunk_append_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 5,
                   // New indef string, cbor_indefinite_string_data, chunk item,
                   // chunk data, extend cbor_indefinite_string_data.chunks
                   MALLOC, MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_string_second_chunk_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_string();
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("abc"))));
  assert_true(cbor_string_add_chunk(item, cbor_move(cbor_build_string("def"))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 6,
                   // New indef string, cbor_indefinite_string_data, chunk item,
                   // chunk data, extend cbor_indefinite_string_data.chunks,
                   // second chunk item
                   MALLOC, MALLOC, MALLOC, MALLOC, REALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_array_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_array_item_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 2,
                   // New array, item copy
                   MALLOC, MALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_array_push_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 3,
                   // New array, item copy, array reallocation
                   MALLOC, MALLOC, REALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_array_second_item_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_array();
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(42))));
  assert_true(cbor_array_push(item, cbor_move(cbor_build_uint8(43))));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 4,
                   // New array, item copy, array reallocation, second item copy
                   MALLOC, MALLOC, REALLOC, MALLOC_FAIL);

  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_map_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(
      cbor_map_add(item, (struct cbor_pair){cbor_move(cbor_build_uint8(42)),
                                            cbor_move(cbor_build_bool(true))}));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_map_key_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(
      cbor_map_add(item, (struct cbor_pair){cbor_move(cbor_build_uint8(42)),
                                            cbor_move(cbor_build_bool(true))}));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 2,
                   // New map, key copy
                   MALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_map_value_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(
      cbor_map_add(item, (struct cbor_pair){cbor_move(cbor_build_uint8(42)),
                                            cbor_move(cbor_build_bool(true))}));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 3,
                   // New map, key copy, value copy
                   MALLOC, MALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_map_add_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(
      cbor_map_add(item, (struct cbor_pair){cbor_move(cbor_build_uint8(42)),
                                            cbor_move(cbor_build_bool(true))}));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 4,
                   // New map, key copy, value copy, add
                   MALLOC, MALLOC, MALLOC, REALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_map_second_key_failure(void** _state _CBOR_UNUSED) {
  item = cbor_new_indefinite_map();
  assert_true(
      cbor_map_add(item, (struct cbor_pair){cbor_move(cbor_build_uint8(42)),
                                            cbor_move(cbor_build_bool(true))}));
  assert_true(cbor_map_add(
      item, (struct cbor_pair){cbor_move(cbor_build_uint8(43)),
                               cbor_move(cbor_build_bool(false))}));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 5,
                   // New map, key copy, value copy, add, second key copy
                   MALLOC, MALLOC, MALLOC, REALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_tag_item_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(1, cbor_move(cbor_build_uint8(42)));

  WITH_FAILING_MALLOC({ assert_null(cbor_copy(item)); });
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

static void test_tag_alloc_failure(void** _state _CBOR_UNUSED) {
  item = cbor_build_tag(1, cbor_move(cbor_build_uint8(42)));

  WITH_MOCK_MALLOC({ assert_null(cbor_copy(item)); }, 2,
                   // Item copy, tag
                   MALLOC, MALLOC_FAIL);
  assert_size_equal(cbor_refcount(item), 1);

  cbor_decref(&item);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_uints),
      cmocka_unit_test(test_negints),
      cmocka_unit_test(test_def_bytestring),
      cmocka_unit_test(test_indef_bytestring),
      cmocka_unit_test(test_def_string),
      cmocka_unit_test(test_indef_string),
      cmocka_unit_test(test_def_array),
      cmocka_unit_test(test_indef_array),
      cmocka_unit_test(test_def_map),
      cmocka_unit_test(test_indef_map),
      cmocka_unit_test(test_tag),
      cmocka_unit_test(test_ctrls),
      cmocka_unit_test(test_floats),
      cmocka_unit_test(test_alloc_failure_simple),
      cmocka_unit_test(test_bytestring_alloc_failure),
      cmocka_unit_test(test_bytestring_chunk_alloc_failure),
      cmocka_unit_test(test_bytestring_chunk_append_failure),
      cmocka_unit_test(test_bytestring_second_chunk_alloc_failure),
      cmocka_unit_test(test_string_alloc_failure),
      cmocka_unit_test(test_string_chunk_alloc_failure),
      cmocka_unit_test(test_string_chunk_append_failure),
      cmocka_unit_test(test_string_second_chunk_alloc_failure),
      cmocka_unit_test(test_array_alloc_failure),
      cmocka_unit_test(test_array_item_alloc_failure),
      cmocka_unit_test(test_array_push_failure),
      cmocka_unit_test(test_array_second_item_alloc_failure),
      cmocka_unit_test(test_map_alloc_failure),
      cmocka_unit_test(test_map_key_alloc_failure),
      cmocka_unit_test(test_map_value_alloc_failure),
      cmocka_unit_test(test_map_add_failure),
      cmocka_unit_test(test_map_second_key_failure),
      cmocka_unit_test(test_tag_item_alloc_failure),
      cmocka_unit_test(test_tag_alloc_failure),
      cmocka_unit_test(test_definite_uints),
      cmocka_unit_test(test_definite_negints),
      cmocka_unit_test(test_definite_bytestring),
      cmocka_unit_test(test_definite_bytestring_alloc_failure),
      cmocka_unit_test(test_definite_indef_bytestring),
      cmocka_unit_test(test_definite_string),
      cmocka_unit_test(test_definite_indef_string),
      cmocka_unit_test(test_definite_string_alloc_failure),
      cmocka_unit_test(test_definite_array),
      cmocka_unit_test(test_definite_indef_array),
      cmocka_unit_test(test_definite_indef_array_nested),
      cmocka_unit_test(test_definite_array_alloc_failure),
      cmocka_unit_test(test_definite_array_item_alloc_failure),
      cmocka_unit_test(test_definite_map),
      cmocka_unit_test(test_definite_indef_map),
      cmocka_unit_test(test_definite_indef_map_nested),
      cmocka_unit_test(test_definite_map_alloc_failure),
      cmocka_unit_test(test_definite_map_key_alloc_failure),
      cmocka_unit_test(test_definite_map_value_alloc_failure),
      cmocka_unit_test(test_definite_tag),
      cmocka_unit_test(test_definite_tag_nested),
      cmocka_unit_test(test_definite_tag_alloc_failure),
      cmocka_unit_test(test_definite_ctrls),
      cmocka_unit_test(test_definite_floats),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
