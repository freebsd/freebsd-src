/** @file
  This file declares Temporary RAM Done PPI.
  The PPI that provides a service to disable the use of Temporary RAM.

  Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
  This PPI is introduced in PI Version 1.2.1.

**/

#ifndef __TEMPORARY_RAM_DONE_H__
#define __TEMPORARY_RAM_DONE_H__

#define EFI_PEI_TEMPORARY_RAM_DONE_PPI_GUID \
  { 0xceab683c, 0xec56, 0x4a2d, { 0xa9, 0x6, 0x40, 0x53, 0xfa, 0x4e, 0x9c, 0x16 } }

/**
  TemporaryRamDone() disables the use of Temporary RAM. If present, this service is invoked
  by the PEI Foundation after the EFI_PEI_PERMANENT_MEMORY_INSTALLED_PPI is installed.

  @retval EFI_SUCCESS           Use of Temporary RAM was disabled.
  @retval EFI_INVALID_PARAMETER Temporary RAM could not be disabled.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_PEI_TEMPORARY_RAM_DONE)(
  VOID
  );

///
/// This is an optional PPI that may be produced by SEC or a PEIM. If present, it provide a service to
/// disable the use of Temporary RAM. This service may only be called by the PEI Foundation after the
/// transition from Temporary RAM to Permanent RAM is complete. This PPI provides an alternative
/// to the Temporary RAM Migration PPI for system architectures that allow Temporary RAM and
/// Permanent RAM to be enabled and accessed at the same time with no side effects.
///
typedef struct _EFI_PEI_TEMPORARY_RAM_DONE_PPI {
  EFI_PEI_TEMPORARY_RAM_DONE    TemporaryRamDone;
} EFI_PEI_TEMPORARY_RAM_DONE_PPI;

extern EFI_GUID  gEfiTemporaryRamDonePpiGuid;

#endif
