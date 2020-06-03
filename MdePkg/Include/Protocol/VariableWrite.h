/** @file
  Variable Write Architectural Protocol as defined in PI Specification VOLUME 2 DXE

  This provides the services required to set nonvolatile environment variables.
  This protocol must be produced by a runtime DXE driver and may be consumed only
  by the DXE Foundation.

  The DXE driver that produces this protocol must be a runtime driver. This driver
  may update the SetVariable() field of the UEFI Runtime Services Table.

  After the UEFI Runtime Services Table has been initialized, the driver must
  install the EFI_VARIABLE_WRITE_ARCH_PROTOCOL_GUID on a new handle with a NULL
  interface pointer. The installation of this protocol informs the DXE Foundation
  that the write services for nonvolatile environment variables are now available
  and that the DXE Foundation must update the 32-bit CRC of the UEFI Runtime Services
  Table. The full complement of environment variable services are not available
  until both this protocol and EFI_VARIABLE_ARCH_PROTOCOL are installed. DXE drivers
  that require read-only access or read/write access to volatile environment variables
  must have the EFI_VARIABLE_WRITE_ARCH_PROTOCOL in their dependency expressions.
  DXE drivers that require write access to nonvolatile environment variables must
  have this architectural protocol in their dependency expressions.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_VARIABLE_WRITE_ARCH_H__
#define __ARCH_PROTOCOL_VARIABLE_WRITE_ARCH_H__

///
/// Global ID for the Variable Write Architectural Protocol
///
#define EFI_VARIABLE_WRITE_ARCH_PROTOCOL_GUID \
  { 0x6441f818, 0x6362, 0x4e44, {0xb5, 0x70, 0x7d, 0xba, 0x31, 0xdd, 0x24, 0x53 } }

extern EFI_GUID gEfiVariableWriteArchProtocolGuid;

#endif
