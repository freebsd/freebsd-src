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
;   Generates random number through CPU RdRand instruction on a 32-bit platform
;   to store a random value in the GCC __stack_check_guard stack cookie.
;   The first byte is 0'd to prevent string copy functions from clobbering
;   the stack cookie.
;
; Notes:
;
; If RdRand fails, the build time static stack cookie value will be used instead.
;
;------------------------------------------------------------------------------

SECTION .text

extern ASM_PFX(__stack_chk_guard)
extern ASM_PFX(_CModuleEntryPoint)
global ASM_PFX(_ModuleEntryPoint)

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; _ModuleEntryPoint (
;   Parameters are passed through
;   );
;------------------------------------------------------------------------------
global _ModuleEntryPoint
_ModuleEntryPoint:
  push ebx
  push ecx
  push edx

  mov eax, 1                        ; CPUID function 1
  cpuid
  test ecx, 0x40000000              ; Check if the RdRand bit (bit 30) is set in ECX
  jz c_entry                        ; If not set, jump to c_entry

  rdrand eax                        ; Use rdrand, getting a 32 bit value as on
                                    ; IA32, __stack_chk_guard is a 32 bit value.
                                    ; CF=1 if RN generated ok, otherwise CF=0
  jnc c_entry                       ; If the cmd fails, don't update __stack_chk_guard, we'll have to move forward
                                    ; with the static value provided at build time.

  lea ebx, [ASM_PFX(__stack_chk_guard)]      ; load the address of __stack_chk_guard into ebx

  xor ah, ah                        ; Zero a byte of the __stack_chk_guard value to protect against string functions
                                    ; (such as strcpy like functions) clobbering past the canary
  mov [ebx], eax                    ; Store our random value, with 0'd first byte to __stack_chk_guard

c_entry:
  pop edx
  pop ecx
  pop ebx
  jmp ASM_PFX(_CModuleEntryPoint)
