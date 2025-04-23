;------------------------------------------------------------------------------
;
; Copyright (c) Microsoft Corporation.
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   DynamicCookie.nasm
;
; Abstract:
;
;   Generates random number through CPU RdRand instruction on 64-bit platform
;   to store a random value in the GCC __stack_check_guard stack cookie.
;   The first byte is 0'd to prevent string copy functions from clobbering
;   the stack cookie.
;
; Notes:
;
; If RdRand fails, the build time static stack cookie value will be used instead.
;
;------------------------------------------------------------------------------

DEFAULT REL
SECTION .text

extern ASM_PFX(__stack_chk_guard)
extern ASM_PFX(_CModuleEntryPoint)

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; _ModuleEntryPoint (
;   Parameters are passed through. TODO: Make sure there are only two args on X64
;   );
;------------------------------------------------------------------------------
global ASM_PFX(_ModuleEntryPoint)
ASM_PFX(_ModuleEntryPoint):
  push rbx
  push rcx
  push rdx

  mov eax, 1                        ; Set eax to 1 to get feature information
  cpuid                             ; Call cpuid
  test ecx, 0x40000000              ; Test the rdrand bit (bit 30) in ecx
  jz c_entry                        ; If rdrand is not supported, jump to c_entry

  rdrand rax                        ; Call rdrand functionality here, getting a 64 bit value as on
                                    ; X64, __stack_chk_guard is a 64 bit value.
                                    ; CF=1 if RN generated ok, otherwise CF=0
  jnc c_entry                       ; If the cmd fails, don't, update __stack_chk_guard, we'll have to move forward
                                    ; with the static value provided at build time.

  lea rbx, [rel ASM_PFX(__stack_chk_guard)]  ; load the address of __stack_check_guard into rbx

  xor ah, ah                        ; Zero a byte of the __stack_chk_guard value to protect against string functions
                                    ; (such as strcpy like functions) clobbering past the canary
  mov [rbx], rax                    ; Store our random value, with 0'd first byte to __stack_chk_guard

c_entry:
  pop rdx
  pop rcx
  pop rbx
  jmp ASM_PFX(_CModuleEntryPoint)
