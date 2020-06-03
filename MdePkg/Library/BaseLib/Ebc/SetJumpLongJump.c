/** @file
  Implementation of SetJump() and LongJump() on EBC.

  SetJump() and LongJump() are not currently supported for the EBC processor type.
  Implementation for EBC just returns 0 for SetJump(), and ASSERT() for LongJump().

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "BaseLibInternals.h"

/**
  Saves the current CPU context that can be restored with a call to LongJump() and returns 0.

  Saves the current CPU context in the buffer specified by JumpBuffer and returns 0.  The initial
  call to SetJump() must always return 0.  Subsequent calls to LongJump() cause a non-zero
  value to be returned by SetJump().

  If JumpBuffer is NULL, then ASSERT().
  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().

  @param  JumpBuffer    A pointer to CPU context buffer.

  @retval 0 Indicates a return from SetJump().

**/
RETURNS_TWICE
UINTN
EFIAPI
SetJump (
  OUT      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
  )
{
  InternalAssertJumpBuffer (JumpBuffer);
  return 0;
}

/**
  Restores the CPU context that was saved with SetJump().

  Restores the CPU context from the buffer specified by JumpBuffer.
  This function never returns to the caller.
  Instead it resumes execution based on the state of JumpBuffer.

  @param  JumpBuffer    A pointer to CPU context buffer.
  @param  Value         The value to return when the SetJump() context is restored.

**/
VOID
EFIAPI
InternalLongJump (
  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
  IN      UINTN                     Value
  )
{
  //
  // This function cannot work on EBC
  //
  ASSERT (FALSE);
}
