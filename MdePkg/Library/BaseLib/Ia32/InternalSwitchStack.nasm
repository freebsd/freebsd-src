;------------------------------------------------------------------------------
;
; Copyright (c) 2016, Intel Corporation. All rights reserved.<BR>
; Portions copyright (c) 2011, Apple Inc. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   InternalSwitchStack.nasm
;
; Abstract:
;
;   Implementation of a stack switch on IA-32.
;
;------------------------------------------------------------------------------

    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalSwitchStack (
;   IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
;   IN      VOID                      *Context1,   OPTIONAL
;   IN      VOID                      *Context2,   OPTIONAL
;   IN      VOID                      *NewStack
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalSwitchStack)
ASM_PFX(InternalSwitchStack):
  push  ebp
  mov   ebp, esp

  mov   esp, [ebp + 20]    ; switch stack
  sub   esp, 8
  mov   eax, [ebp + 16]
  mov   [esp + 4], eax
  mov   eax, [ebp + 12]
  mov   [esp], eax
  push  0                  ; keeps gdb from unwinding stack
  jmp   dword [ebp + 8]    ; call and never return
