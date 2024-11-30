/** @file
  This file declares a mock of Hash2 Protocol.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MOCK_HASH2_H_
#define MOCK_HASH2_H_

#include <Library/GoogleTestLib.h>
#include <Library/FunctionMockLib.h>

extern "C" {
  #include <Uefi.h>
  #include <Protocol/Hash2.h>
}

struct MockHash2 {
  MOCK_INTERFACE_DECLARATION (MockHash2);

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    GetHashSize,
    (IN CONST EFI_HASH2_PROTOCOL  *This,
     IN CONST EFI_GUID            *HashAlgorithm,
     OUT UINTN                    *HashSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    Hash,
    (IN CONST EFI_HASH2_PROTOCOL  *This,
     IN CONST EFI_GUID            *HashAlgorithm,
     IN CONST UINT8               *Message,
     IN UINTN                     MessageSize,
     IN OUT EFI_HASH2_OUTPUT      *Hash)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    HashInit,
    (IN CONST EFI_HASH2_PROTOCOL  *This,
     IN CONST EFI_GUID            *HashAlgorithm)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    HashUpdate,
    (IN CONST EFI_HASH2_PROTOCOL  *This,
     IN CONST UINT8               *Message,
     IN UINTN                     MessageSize)
    );

  MOCK_FUNCTION_DECLARATION (
    EFI_STATUS,
    HashFinal,
    (IN CONST EFI_HASH2_PROTOCOL  *This,
     IN OUT EFI_HASH2_OUTPUT      *Hash)
    );
};

extern "C" {
  extern EFI_HASH2_PROTOCOL  *gHash2Protocol;
}

#endif // MOCK_HASH2_H_
