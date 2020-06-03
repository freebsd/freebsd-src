/** @file
  Provides a service to retrieve a pointer to the SMM Services Table.
  Only available to SMM module types.

Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SMM_SERVICES_TABLE_LIB_H__
#define __SMM_SERVICES_TABLE_LIB_H__

#include <PiSmm.h>

///
/// Cache pointer to the SMM Services Table
///
extern EFI_SMM_SYSTEM_TABLE2   *gSmst;

/**
  This function allows the caller to determine if the driver is executing in
  System Management Mode(SMM).

  This function returns TRUE if the driver is executing in SMM and FALSE if the
  driver is not executing in SMM.

  @retval  TRUE  The driver is executing in System Management Mode (SMM).
  @retval  FALSE The driver is not executing in System Management Mode (SMM).

**/
BOOLEAN
EFIAPI
InSmm (
  VOID
  );

#endif
