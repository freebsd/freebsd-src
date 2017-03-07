/** @file
  Implementation of _LongJump() on IA-32.

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
  Restores the CPU context that was saved with SetJump().

  Restores the CPU context from the buffer specified by JumpBuffer.
  This function never returns to the caller.
  Instead is resumes execution based on the state of JumpBuffer.

  @param  JumpBuffer    A pointer to CPU context buffer.
  @param  Value         The value to return when the SetJump() context is restored.

**/
__declspec (naked)
VOID
EFIAPI
InternalLongJump (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
  IN      UINTN                     Value
  )
{
  _asm {
    pop     eax                         ; skip return address
    pop     edx                         ; edx <- JumpBuffer
    pop     eax                         ; eax <- Value
    mov     ebx, [edx]
    mov     esi, [edx + 4]
    mov     edi, [edx + 8]
    mov     ebp, [edx + 12]
    mov     esp, [edx + 16]
    jmp     dword ptr [edx + 20]
  }
}

