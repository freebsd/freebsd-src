/** @file
  Provides a service to retrieve a pointer to the SMM Services Table.
  Only available to SMM module types.

Copyright (c) 2009, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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
