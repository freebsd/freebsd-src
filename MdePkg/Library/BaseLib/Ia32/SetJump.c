/** @file
  Implementation of SetJump() on IA-32.

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
  Worker function that checks ASSERT condition for JumpBuffer

  Checks ASSERT condition for JumpBuffer.

  If JumpBuffer is NULL, then ASSERT().
  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  JumpBuffer    A pointer to CPU context buffer.

**/
VOID
EFIAPI
InternalAssertJumpBuffer (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  );

/**
  Saves the current CPU context that can be restored with a call to LongJump()
  and returns 0.

  Saves the current CPU context in the buffer specified by JumpBuffer and
  returns 0. The initial call to SetJump() must always return 0. Subsequent
  calls to LongJump() cause a non-zero value to be returned by SetJump().

  If JumpBuffer is NULL, then ASSERT().
  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  JumpBuffer  A pointer to CPU context buffer.

  @retval 0 Indicates a return from SetJump().

**/
_declspec (naked)
UINTN
EFIAPI
SetJump (
  OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  )
{
  _asm {
    push    [esp + 4]
    call    InternalAssertJumpBuffer
    pop     ecx
    pop     ecx
    mov     edx, [esp]
    mov     [edx], ebx
    mov     [edx + 4], esi
    mov     [edx + 8], edi
    mov     [edx + 12], ebp
    mov     [edx + 16], esp
    mov     [edx + 20], ecx
    xor     eax, eax
    jmp     ecx
  }
}

