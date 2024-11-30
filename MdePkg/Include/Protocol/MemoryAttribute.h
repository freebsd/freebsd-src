/** @file

  EFI Memory Attribute Protocol provides retrieval and update service
  for memory attributes in EFI environment.

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2023, Google LLC. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFI_MEMORY_ATTRIBUTE_H_
#define EFI_MEMORY_ATTRIBUTE_H_

#define EFI_MEMORY_ATTRIBUTE_PROTOCOL_GUID \
  { \
    0xf4560cf6, 0x40ec, 0x4b4a, { 0xa1, 0x92, 0xbf, 0x1d, 0x57, 0xd0, 0xb1, 0x89 } \
  }

typedef struct _EFI_MEMORY_ATTRIBUTE_PROTOCOL EFI_MEMORY_ATTRIBUTE_PROTOCOL;

/**
  This function set given attributes of the memory region specified by
  BaseAddress and Length.

  The valid Attributes is EFI_MEMORY_RP, EFI_MEMORY_XP, and EFI_MEMORY_RO.

  @param  This              The EFI_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        The bit mask of attributes to set for the memory
                            region.

  @retval EFI_SUCCESS           The attributes were set for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of
                                attributes that cannot be set together.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.
                                The bit mask of attributes is not supported for
                                the memory resource range specified by
                                BaseAddress and Length.
  @retval EFI_OUT_OF_RESOURCES  Requested attributes cannot be applied due to
                                lack of system resources.
  @retval EFI_ACCESS_DENIED     Attributes for the requested memory region are
                                controlled by system firmware and cannot be
                                updated via the protocol.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SET_MEMORY_ATTRIBUTES)(
  IN  EFI_MEMORY_ATTRIBUTE_PROTOCOL       *This,
  IN  EFI_PHYSICAL_ADDRESS                BaseAddress,
  IN  UINT64                              Length,
  IN  UINT64                              Attributes
  );

/**
  This function clears given attributes of the memory region specified by
  BaseAddress and Length.

  The valid Attributes is EFI_MEMORY_RP, EFI_MEMORY_XP, and EFI_MEMORY_RO.

  @param  This              The EFI_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        The bit mask of attributes to clear for the memory
                            region.

  @retval EFI_SUCCESS           The attributes were cleared for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes specified an illegal combination of
                                attributes that cannot be cleared together.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.
                                The bit mask of attributes is not supported for
                                the memory resource range specified by
                                BaseAddress and Length.
  @retval EFI_OUT_OF_RESOURCES  Requested attributes cannot be applied due to
                                lack of system resources.
  @retval EFI_ACCESS_DENIED     Attributes for the requested memory region are
                                controlled by system firmware and cannot be
                                updated via the protocol.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_CLEAR_MEMORY_ATTRIBUTES)(
  IN  EFI_MEMORY_ATTRIBUTE_PROTOCOL       *This,
  IN  EFI_PHYSICAL_ADDRESS                BaseAddress,
  IN  UINT64                              Length,
  IN  UINT64                              Attributes
  );

/**
  This function retrieves the attributes of the memory region specified by
  BaseAddress and Length. If different attributes are obtained from different
  parts of the memory region, EFI_NO_MAPPING will be returned.

  @param  This              The EFI_MEMORY_ATTRIBUTE_PROTOCOL instance.
  @param  BaseAddress       The physical address that is the start address of
                            a memory region.
  @param  Length            The size in bytes of the memory region.
  @param  Attributes        Pointer to attributes returned.

  @retval EFI_SUCCESS           The attributes got for the memory region.
  @retval EFI_INVALID_PARAMETER Length is zero.
                                Attributes is NULL.
  @retval EFI_NO_MAPPING        Attributes are not consistent cross the memory
                                region.
  @retval EFI_UNSUPPORTED       The processor does not support one or more
                                bytes of the memory resource range specified
                                by BaseAddress and Length.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GET_MEMORY_ATTRIBUTES)(
  IN  EFI_MEMORY_ATTRIBUTE_PROTOCOL       *This,
  IN  EFI_PHYSICAL_ADDRESS                BaseAddress,
  IN  UINT64                              Length,
  OUT UINT64                              *Attributes
  );

///
/// EFI Memory Attribute Protocol provides services to retrieve or update
/// attribute of memory in the EFI environment.
///
struct _EFI_MEMORY_ATTRIBUTE_PROTOCOL {
  EFI_GET_MEMORY_ATTRIBUTES      GetMemoryAttributes;
  EFI_SET_MEMORY_ATTRIBUTES      SetMemoryAttributes;
  EFI_CLEAR_MEMORY_ATTRIBUTES    ClearMemoryAttributes;
};

extern EFI_GUID  gEfiMemoryAttributeProtocolGuid;

#endif
