/** @file
  Serial Port Library backed by SBI console.

  Common functionality shared by PrePiDxeSerialPortLibRiscVSbi and
  PrePiDxeSerialPortLibRiscVSbiRam implementations.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SERIAL_PORT_SBI_COMMON_H_
#define SERIAL_PORT_SBI_COMMON_H_

#include <Base.h>
#include <Library/SerialPortLib.h>
#include <Library/BaseRiscVSbiLib.h>

BOOLEAN
SbiImplementsDbcn (
  VOID
  );

BOOLEAN
SbiImplementsLegacyPutchar (
  VOID
  );

UINTN
SbiLegacyPutchar (
  IN  UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  );

UINTN
SbiDbcnWrite (
  IN  UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  );

#endif /* SERIAL_PORT_SBI_COMMON_H_ */
