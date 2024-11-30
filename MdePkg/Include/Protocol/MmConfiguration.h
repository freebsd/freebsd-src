/** @file
  EFI MM Configuration Protocol as defined in the PI 1.5 specification.

  This protocol is used to:
  1) report the portions of MMRAM regions which cannot be used for the MMRAM heap.
  2) register the MM Foundation entry point with the processor code. The entry
     point will be invoked by the MM processor entry code.

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _MM_CONFIGURATION_H_
#define _MM_CONFIGURATION_H_

#include <Pi/PiMmCis.h>

#define EFI_MM_CONFIGURATION_PROTOCOL_GUID \
  { \
    0x26eeb3de, 0xb689, 0x492e, {0x80, 0xf0, 0xbe, 0x8b, 0xd7, 0xda, 0x4b, 0xa7 }  \
  }

typedef struct _EFI_MM_CONFIGURATION_PROTOCOL EFI_MM_CONFIGURATION_PROTOCOL;

/**
  Register the MM Foundation entry point.

  This function registers the MM Foundation entry point with the processor code. This entry point
  will be invoked by the MM Processor entry code.

  @param[in] This                The EFI_MM_CONFIGURATION_PROTOCOL instance.
  @param[in] MmEntryPoint        MM Foundation entry point.

  @retval EFI_SUCCESS            Success to register MM Entry Point.
  @retval EFI_INVALID_PARAMETER  MmEntryPoint is NULL.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_MM_REGISTER_MM_ENTRY)(
  IN CONST EFI_MM_CONFIGURATION_PROTOCOL  *This,
  IN EFI_MM_ENTRY_POINT                   MmEntryPoint
  );

///
/// The EFI MM Configuration Protocol is a mandatory protocol published by a DXE CPU driver to
/// indicate which areas within MMRAM are reserved for use by the CPU for any purpose,
/// such as stack, save state or MM entry point.
///
/// The RegistermmEntry() function allows the MM IPL DXE driver to register the MM
/// Foundation entry point with the MM entry vector code.
///
struct _EFI_MM_CONFIGURATION_PROTOCOL {
  ///
  /// A pointer to an array MMRAM ranges used by the initial MM entry code.
  ///
  EFI_MM_RESERVED_MMRAM_REGION    *MmramReservedRegions;
  EFI_MM_REGISTER_MM_ENTRY        RegisterMmEntry;
};

extern EFI_GUID  gEfiMmConfigurationProtocolGuid;

#endif
