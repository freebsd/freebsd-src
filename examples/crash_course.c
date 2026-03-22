/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cbor.h"

// Part 1: Begin
void item_examples() {
  // A cbor_item_t can contain any CBOR data type
  cbor_item_t* float_item = cbor_build_float4(3.14f);
  cbor_item_t* string_item = cbor_build_string("Hello World!");
  cbor_item_t* array_item = cbor_new_indefinite_array();

  // They can be inspected
  assert(cbor_is_float(float_item));
  assert(cbor_typeof(string_item) == CBOR_TYPE_STRING);
  assert(cbor_array_is_indefinite(array_item));
  assert(cbor_array_size(array_item) == 0);

  // The data can be accessed
  assert(cbor_float_get_float4(float_item) == 3.14f);
  assert(memcmp(cbor_string_handle(string_item), "Hello World!",
                cbor_string_length(string_item)) == 0);

  // And they can be modified
  assert(cbor_array_push(array_item, float_item));
  assert(cbor_array_push(array_item, string_item));
  assert(cbor_array_size(array_item) == 2);

  // At the end of their lifetime, items must be freed
  cbor_decref(&float_item);
  cbor_decref(&string_item);
  cbor_decref(&array_item);
}
// Part 1: End

// Part 2: Begin
void encode_decode() {
  cbor_item_t* item = cbor_build_uint8(42);

  // Serialize the item to a buffer (it will be allocated by libcbor)
  unsigned char* buffer;
  size_t buffer_size;
  cbor_serialize_alloc(item, &buffer, &buffer_size);
  assert(buffer_size == 2);
  assert(buffer[0] == 0x18);  // Encoding byte for uint8
  assert(buffer[1] == 42);    // The value itself

  // And deserialize bytes back to an item
  struct cbor_load_result result;
  cbor_item_t* decoded_item = cbor_load(buffer, buffer_size, &result);
  assert(result.error.code == CBOR_ERR_NONE);
  assert(cbor_isa_uint(decoded_item));
  assert(cbor_get_uint8(decoded_item) == 42);

  // Free the allocated buffer and items
  free(buffer);
  cbor_decref(&decoded_item);
  cbor_decref(&item);
}
// Part 2: End

// Part 3: Begin
void reference_counting() {
  // cbor_item_t is a reference counted pointer under the hood
  cbor_item_t* item = cbor_build_uint8(42);

  // Reference count starts at 1
  assert(cbor_refcount(item) == 1);

  // Most operations have reference semantics
  cbor_item_t* array_item = cbor_new_definite_array(1);
  assert(cbor_array_push(array_item, item));
  assert(cbor_refcount(item) == 2);  // item and array_item reference it
  cbor_item_t* first_array_element = cbor_array_get(array_item, 0);
  assert(first_array_element == item);  // same item under the hood
  assert(cbor_refcount(item) ==
         3);  // and now first_array_element also points to it

  // To release the reference, use cbor_decref
  cbor_decref(&first_array_element);

  // When reference count reaches 0, the item is freed
  assert(cbor_refcount(array_item) == 1);
  cbor_decref(&array_item);
  assert(array_item == NULL);
  assert(cbor_refcount(item) == 1);

  // Be careful, loops leak memory!

  // Deep copy copies the whole item tree
  cbor_item_t* item_copy = cbor_copy(item);
  assert(cbor_refcount(item) == 1);
  assert(cbor_refcount(item_copy) == 1);
  assert(item_copy != item);
  cbor_decref(&item);
  cbor_decref(&item_copy);
}
// Part 3: End

// Part 4: Begin
void moving_values() {
  {
    // Move the "42" into an array.
    cbor_item_t* array_item = cbor_new_definite_array(1);
    // The line below leaks memory!
    assert(cbor_array_push(array_item, cbor_build_uint8(42)));
    cbor_item_t* first_array_element = cbor_array_get(array_item, 0);
    assert(cbor_refcount(first_array_element) == 3);  // Should be 2!
    cbor_decref(&first_array_element);
    cbor_decref(&array_item);
    assert(cbor_refcount(first_array_element) == 1);  // Shouldn't exist!
    // Clean up
    cbor_decref(&first_array_element);
  }

  {
    // A correct way to move values is to decref them in the caller scope.
    cbor_item_t* array_item = cbor_new_definite_array(1);
    cbor_item_t* item = cbor_build_uint8(42);
    assert(cbor_array_push(array_item, item));
    assert(cbor_refcount(item) == 2);
    // "Give up" the item
    cbor_decref(&item);
    cbor_decref(&array_item);
    // item is a dangling pointer at this point
  }

  {
    // cbor_move avoids the need to decref and the dangling pointer
    cbor_item_t* array_item = cbor_new_definite_array(1);
    assert(cbor_array_push(array_item, cbor_move(cbor_build_uint8(42))));
    cbor_item_t* first_array_element = cbor_array_get(array_item, 0);
    assert(cbor_refcount(first_array_element) == 2);
    cbor_decref(&first_array_element);
    cbor_decref(&array_item);
  }
}
// Part 4: End

// Part 5: Begin
// Refcount can be managed in conjunction with ownership
static cbor_item_t* global_item = NULL;

// This function takes shared ownership of the item
void borrow_item(cbor_item_t* item) {
  global_item = item;
  // Mark the extra reference
  cbor_incref(item);
}

void return_item() {
  cbor_decref(&global_item);
  global_item = NULL;
}

void reference_ownership() {
  cbor_item_t* item = cbor_build_uint8(42);

  // Lend the item
  borrow_item(item);
  assert(cbor_refcount(item) == 2);
  cbor_decref(&item);

  // Release the shared ownership. return_item will deallocate the item.
  return_item();
}
// Part 5: End

int main(void) {
  item_examples();
  encode_decode();
  reference_counting();
  moving_values();
  reference_ownership();
  return 0;
}
