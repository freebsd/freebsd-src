/** @file
  EFI MM Configuration PPI as defined in PI 1.5 specification.

  This PPI is used to:
  1) report the portions of MMRAM regions which cannot be used for the MMRAM heap.
  2) register the MM Foundation entry point with the processor code. The entry
     point will be invoked by the MM processor entry code.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef MM_CONFIGURATION_PPI_H_
#define MM_CONFIGURATION_PPI_H_

#include <Pi/PiMultiPhase.h>

#define EFI_PEI_MM_CONFIGURATION_PPI_GUID \
  { \
    0xc109319, 0xc149, 0x450e, { 0xa3, 0xe3, 0xb9, 0xba, 0xdd, 0x9d, 0xc3, 0xa4 }  \
  }

typedef struct _EFI_PEI_MM_CONFIGURATION_PPI EFI_PEI_MM_CONFIGURATION_PPI;

/**
  This function registers the MM Foundation entry point with the processor code. This entry point will be
  invoked by the MM Processor entry code as defined in PI specification.

  @param[in]  This            The EFI_PEI_MM_CONFIGURATION_PPI instance.
  @param[in]  MmEntryPoint    MM Foundation entry point.

  @retval EFI_SUCCESS         The entry-point was successfully registered.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_MM_REGISTER_MM_ENTRY)(
  IN CONST EFI_PEI_MM_CONFIGURATION_PPI *This,
  IN EFI_MM_ENTRY_POINT                 MmEntryPoint
  );

///
/// This PPI is a PPI published by a CPU PEIM to indicate which areas within MMRAM are reserved for use by
/// the CPU for any purpose, such as stack, save state or MM entry point. If a platform chooses to let a CPU
/// PEIM do MMRAM relocation, this PPI must be produced by this CPU PEIM.
///
/// The MmramReservedRegions points to an array of one or more EFI_MM_RESERVED_MMRAM_REGION structures, with
/// the last structure having the MmramReservedSize set to 0. An empty array would contain only the last
/// structure.
///
/// The RegisterMmEntry() function allows the MM IPL PEIM to register the MM Foundation entry point with the
/// MM entry vector code.
///
struct _EFI_PEI_MM_CONFIGURATION_PPI {
  EFI_MM_RESERVED_MMRAM_REGION    *MmramReservedRegions;
  EFI_PEI_MM_REGISTER_MM_ENTRY    RegisterMmEntry;
};

extern EFI_GUID  gEfiPeiMmConfigurationPpi;

#endif
