;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

  EXPORT  SetJump
  EXPORT  InternalLongJump

  AREA  BaseLib, CODE, READONLY

;/**
;  Saves the current CPU context that can be restored with a call to LongJump() and returns 0.;
;
;  Saves the current CPU context in the buffer specified by JumpBuffer and returns 0.  The initial
;  call to SetJump() must always return 0.  Subsequent calls to LongJump() cause a non-zero
;  value to be returned by SetJump().
;
;  If JumpBuffer is NULL, then ASSERT().
;  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().
;
;  @param  JumpBuffer    A pointer to CPU context buffer.
;
;**/
;
;UINTN
;EFIAPI
;SetJump (
;  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer  // R0
;  )
;
SetJump
  MOV  R3, R13
  STM  R0, {R3-R12,R14}
  EOR  R0, R0
  BX   LR

;/**
;  Restores the CPU context that was saved with SetJump().;
;
;  Restores the CPU context from the buffer specified by JumpBuffer.
;  This function never returns to the caller.
;  Instead is resumes execution based on the state of JumpBuffer.
;
;  @param  JumpBuffer    A pointer to CPU context buffer.
;  @param  Value         The value to return when the SetJump() context is restored.
;
;**/
;VOID
;EFIAPI
;InternalLongJump (
;  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,  // R0
;  IN      UINTN                     Value         // R1
;  );
;
InternalLongJump
  LDM   R0, {R3-R12,R14}
  MOV   R13, R3
  MOV   R0, R1
  BX    LR

  END
