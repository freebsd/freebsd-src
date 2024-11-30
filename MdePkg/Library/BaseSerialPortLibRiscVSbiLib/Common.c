/** @file
  Serial Port Library backed by SBI console.

  Common functionality shared by PrePiDxeSerialPortLibRiscVSbi and
  PrePiDxeSerialPortLibRiscVSbiRam implementations.

  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Common.h"

/**
  Return whether the DBCN extension is implemented.

  @retval TRUE                  Extension is implemented.
  @retval FALSE                 Extension is not implemented.

**/
BOOLEAN
SbiImplementsDbcn (
  VOID
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, 1, SBI_EXT_DBCN);
  if ((TranslateError (Ret.Error) == EFI_SUCCESS) &&
      (Ret.Value != 0))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Return whether the legacy console putchar extension is implemented.

  @retval TRUE                  Extension is implemented.
  @retval FALSE                 Extension is not implemented.

**/
BOOLEAN
SbiImplementsLegacyPutchar (
  VOID
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, 1, SBI_EXT_0_1_CONSOLE_PUTCHAR);
  if ((TranslateError (Ret.Error) == EFI_SUCCESS) &&
      (Ret.Value != 0))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  Write data from buffer to console via SBI legacy putchar extension.

  The number of bytes actually written to the SBI console is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.

  @param  Buffer           The pointer to the data buffer to be written.
  @param  NumberOfBytes    The number of bytes to written to the serial device.

  @retval >=0              The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the
                           write operation failed.

**/
UINTN
SbiLegacyPutchar (
  IN  UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  SBI_RET  Ret;
  UINTN    Index;

  for (Index = 0; Index < NumberOfBytes; Index++) {
    Ret =  SbiCall (SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, 1, Buffer[Index]);
    if ((INT64)Ret.Error < 0) {
      break;
    }
  }

  return Index;
}

/**
  Write data from buffer to console via SBI DBCN.

  The number of bytes actually written to the SBI console is returned.
  If the return value is less than NumberOfBytes, then the write operation failed.

  @param  Buffer           The pointer to the data buffer to be written.
  @param  NumberOfBytes    The number of bytes to written to the serial device.

  @retval >=0              The number of bytes written to the serial device.
                           If this value is less than NumberOfBytes, then the
                           write operation failed.

**/
UINTN
SbiDbcnWrite (
  IN  UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  SBI_RET  Ret;

  Ret = SbiCall (
          SBI_EXT_DBCN,
          SBI_EXT_DBCN_WRITE,
          3,
          NumberOfBytes,
          ((UINTN)Buffer),
          0
          );

  /*
   * May do partial writes. Don't bother decoding
   * Ret.Error as we're only interested in number of
   * bytes written to console.
   */
  return Ret.Value;
}
