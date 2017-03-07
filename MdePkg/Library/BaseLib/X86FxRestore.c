/** @file
  IA-32/x64 AsmFxRestore()

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/




#include "BaseLibInternals.h"

/**
  Restores the current floating point/SSE/SSE2 context from a buffer.

  Restores the current floating point/SSE/SSE2 state from the buffer specified
  by Buffer. Buffer must be aligned on a 16-byte boundary. This function is
  only available on IA-32 and x64.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-byte boundary, then ASSERT().
  If Buffer was not saved with AsmFxSave(), then ASSERT().

  @param  Buffer  A pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
AsmFxRestore (
  IN      CONST IA32_FX_BUFFER      *Buffer
  )
{
  ASSERT (Buffer != NULL);
  ASSERT (0 == ((UINTN)Buffer & 0xf));

  //
  // Check the flag recorded by AsmFxSave()
  //
  ASSERT (0xAA5555AA == *(UINT32 *) (&Buffer->Buffer[sizeof (Buffer->Buffer) - 4]));

  InternalX86FxRestore (Buffer);
}
