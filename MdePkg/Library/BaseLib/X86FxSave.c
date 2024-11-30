/** @file
  IA-32/x64 AsmFxSave()

  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Save the current floating point/SSE/SSE2 context to a buffer.

  Saves the current floating point/SSE/SSE2 state to the buffer specified by
  Buffer. Buffer must be aligned on a 16-byte boundary. This function is only
  available on IA-32 and x64.

  If Buffer is NULL, then ASSERT().
  If Buffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  Buffer  A pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
AsmFxSave (
  OUT     IA32_FX_BUFFER  *Buffer
  )
{
  ASSERT (Buffer != NULL);
  ASSERT (0 == ((UINTN)Buffer & 0xf));

  InternalX86FxSave (Buffer);

  //
  // Mark one flag at end of Buffer, it will be check by AsmFxRestor()
  //
  *(UINT32 *)(&Buffer->Buffer[sizeof (Buffer->Buffer) - 4]) = 0xAA5555AA;
}
