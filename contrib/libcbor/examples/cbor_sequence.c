/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>

#include "cbor.h"

void write_cbor_sequence(const char* filename) {
  FILE* file = fopen(filename, "wb");
  if (!file) {
    fprintf(stderr, "Error: Could not open file %s for writing\n", filename);
    return;
  }

  // Create example CBOR items
  cbor_item_t* int_item = cbor_build_uint32(42);
  cbor_item_t* string_item = cbor_build_string("Hello, CBOR!");
  cbor_item_t* array_item = cbor_new_definite_array(2);
  assert(cbor_array_push(array_item, cbor_build_uint8(1)));
  assert(cbor_array_push(array_item, cbor_build_uint8(2)));

  // Serialize and write items to the file
  unsigned char* buffer;
  size_t buffer_size;

  cbor_serialize_alloc(int_item, &buffer, &buffer_size);
  fwrite(buffer, 1, buffer_size, file);
  free(buffer);
  cbor_decref(&int_item);

  cbor_serialize_alloc(string_item, &buffer, &buffer_size);
  fwrite(buffer, 1, buffer_size, file);
  free(buffer);
  cbor_decref(&string_item);

  cbor_serialize_alloc(array_item, &buffer, &buffer_size);
  fwrite(buffer, 1, buffer_size, file);
  free(buffer);
  cbor_decref(&array_item);

  fclose(file);
  printf("CBOR sequence written to %s\n", filename);
}

void read_cbor_sequence(const char* filename) {
  FILE* file = fopen(filename, "rb");
  if (!file) {
    fprintf(stderr, "Error: Could not open file %s\n", filename);
    return;
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  unsigned char* buffer = malloc(file_size);
  if (!buffer) {
    fprintf(stderr, "Error: Could not allocate memory\n");
    fclose(file);
    return;
  }

  fread(buffer, 1, file_size, file);
  fclose(file);

  struct cbor_load_result result;
  size_t offset = 0;

  while (offset < file_size) {
    cbor_item_t* item = cbor_load(buffer + offset, file_size - offset, &result);
    if (result.error.code != CBOR_ERR_NONE) {
      fprintf(stderr, "Error: Failed to parse CBOR item at offset %zu\n",
              offset);
      break;
    }

    cbor_describe(item, stdout);
    printf("\n");

    offset += result.read;
    cbor_decref(&item);
  }

  free(buffer);
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <r|w> <file>\n", argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "w") == 0) {
    write_cbor_sequence(argv[2]);
  } else if (strcmp(argv[1], "r") == 0) {
    read_cbor_sequence(argv[2]);
  } else {
    fprintf(stderr,
            "Error: First argument must be 'r' (read) or 'w' (write)\n");
    return 1;
  }

  return 0;
}
