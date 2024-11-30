/** @file
  The file provides the protocol to provide interface to accept memory.

  Copyright (c) 2021 - 2022, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef MEMORY_ACCEPT_H_
#define MEMORY_ACCEPT_H_

#define EDKII_MEMORY_ACCEPT_PROTOCOL_GUID \
  { 0x38c74800, 0x5590, 0x4db4, { 0xa0, 0xf3, 0x67, 0x5d, 0x9b, 0x8e, 0x80, 0x26 } };

typedef struct _EDKII_MEMORY_ACCEPT_PROTOCOL EDKII_MEMORY_ACCEPT_PROTOCOL;

/**
  @param This                   A pointer to a EDKII_MEMORY_ACCEPT_PROTOCOL.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_ACCEPT_MEMORY)(
  IN  EDKII_MEMORY_ACCEPT_PROTOCOL  *This,
  IN  EFI_PHYSICAL_ADDRESS          StartAddress,
  IN  UINTN                         Size
  );

///
/// The EDKII_MEMORY_ACCEPT_PROTOCOL provides the ability for memory services
/// to accept memory.
///
struct _EDKII_MEMORY_ACCEPT_PROTOCOL {
  EDKII_ACCEPT_MEMORY    AcceptMemory;
};

extern EFI_GUID  gEdkiiMemoryAcceptProtocolGuid;

#endif
