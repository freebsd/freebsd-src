/** @file MockHash2.cpp
  Google Test mock for Hash2 Protocol

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <GoogleTest/Protocol/MockHash2.h>

MOCK_INTERFACE_DEFINITION (MockHash2);
MOCK_FUNCTION_DEFINITION (MockHash2, GetHashSize, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHash2, Hash, 5, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHash2, HashInit, 2, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHash2, HashUpdate, 3, EFIAPI);
MOCK_FUNCTION_DEFINITION (MockHash2, HashFinal, 2, EFIAPI);

EFI_HASH2_PROTOCOL  HASH2_PROTOCOL_INSTANCE = {
  GetHashSize, // EFI_HASH2_GET_HASH_SIZE
  Hash,        // EFI_HASH2_HASH
  HashInit,    // EFI_HASH2_HASH_INIT
  HashUpdate,  // EFI_HASH2_HASH_UPDATE
  HashFinal    // EFI_HASH2_HASH_FINAL
};

extern "C" {
  EFI_HASH2_PROTOCOL  *gHash2Protocol = &HASH2_PROTOCOL_INSTANCE;
}
