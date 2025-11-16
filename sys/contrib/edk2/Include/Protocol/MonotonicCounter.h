/** @file
  Monotonic Counter Architectural Protocol as defined in PI SPEC VOLUME 2 DXE

  This code provides the services required to access the system's monotonic counter

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __ARCH_PROTOCOL_MONTONIC_COUNTER_H__
#define __ARCH_PROTOCOL_MONTONIC_COUNTER_H__

///
/// Global ID for the Monotonic Counter Architectural Protocol.
///
#define EFI_MONOTONIC_COUNTER_ARCH_PROTOCOL_GUID \
  {0x1da97072, 0xbddc, 0x4b30, {0x99, 0xf1, 0x72, 0xa0, 0xb5, 0x6f, 0xff, 0x2a} }

extern EFI_GUID  gEfiMonotonicCounterArchProtocolGuid;

#endif
