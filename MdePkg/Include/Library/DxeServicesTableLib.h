/** @file
  Provides a service to retrieve a pointer to the DXE Services Table.
  Only available to DXE module types.

  This library does not contain any functions or macros.  It simply exports a global
  pointer to the DXE Services Table as defined in the Platform Initialization Driver
  Execution Environment Core Interface Specification.  The library constructor must
  initialize this global pointer to the DX Services Table, so it is available at the
  module's entry point.  Since there is overhead in looking up the pointer to the DXE
  Services Table, only those modules that actually require access to the DXE Services
  Table should use this library.  This will typically be DXE Drivers that require GCD
  or Dispatcher services.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DXE_SERVICES_TABLE_LIB_H__
#define __DXE_SERVICES_TABLE_LIB_H__

///
/// Cache copy of the DXE Services Table
///
extern EFI_DXE_SERVICES  *gDS;

#endif
