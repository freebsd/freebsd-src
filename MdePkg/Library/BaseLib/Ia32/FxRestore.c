/** @file
  AsmFxRestore function

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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

  @param  Buffer  The pointer to a buffer to save the floating point/SSE/SSE2 context.

**/
VOID
EFIAPI
InternalX86FxRestore (
  IN CONST IA32_FX_BUFFER *Buffer
  )
{
  _asm {
    mov     eax, Buffer
    fxrstor [eax]
  }
}

