/** @file
  Variable Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  This provides the services required to get and set environment variables. This
  protocol must be produced by a runtime DXE driver and may be consumed only by
  the DXE Foundation. The DXE driver that produces this protocol must be a runtime
  driver. This driver is responsible for initializing the GetVariable(),
  GetNextVariableName(), and SetVariable() fields of the UEFI Runtime Services Table.

  After the three fields of the UEFI Runtime Services Table have been initialized,
  the driver must install the EFI_VARIABLE_ARCH_PROTOCOL_GUID on a new handle with
  a NULL interface pointer. The installation of this protocol informs the DXE Foundation
  that the read-only and the volatile environment variable related services are
  now available and that the DXE Foundation must update the 32-bit CRC of the UEFI
  Runtime Services Table. The full complement of environment variable services are
  not available until both this protocol and EFI_VARIABLE_WRITE_ARCH_PROTOCOL are
  installed. DXE drivers that require read-only access or read/write access to volatile
  environment variables must have this architectural protocol in their dependency
  expressions. DXE drivers that require write access to nonvolatile environment
  variables must have the EFI_VARIABLE_WRITE_ARCH_PROTOCOL in their dependency
  expressions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_VARIABLE_ARCH_H__
#define __ARCH_PROTOCOL_VARIABLE_ARCH_H__

///
/// Global ID for the Variable Architectural Protocol
///
#define EFI_VARIABLE_ARCH_PROTOCOL_GUID \
  { 0x1e5668e2, 0x8481, 0x11d4, {0xbc, 0xf1, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } }

extern EFI_GUID  gEfiVariableArchProtocolGuid;

#endif
