;------------------------------------------------------------------------------
;*
;*   Copyright (c) 2016 - 2017, Intel Corporation. All rights reserved.<BR>
;*   SPDX-License-Identifier: BSD-2-Clause-Patent
;*
;*
;------------------------------------------------------------------------------

    SECTION .rodata

;
; Float control word initial value:
; all exceptions masked, double-precision, round-to-nearest
;
mFpuControlWord: DW 0x27F
;
; Multimedia-extensions control word:
; all exceptions masked, round-to-nearest, flush to zero for masked underflow
;
mMmxControlWord: DD 0x1F80

    SECTION .text

;
; Initializes floating point units for requirement of UEFI specification.
;
; This function initializes floating-point control word to 0x027F (all exceptions
; masked,double-precision, round-to-nearest) and multimedia-extensions control word
; (if supported) to 0x1F80 (all exceptions masked, round-to-nearest, flush to zero
; for masked underflow).
;
global ASM_PFX(InitializeFloatingPointUnits)
ASM_PFX(InitializeFloatingPointUnits):

    push    ebx

    ;
    ; Initialize floating point units
    ;
    finit
    fldcw   [mFpuControlWord]

    ;
    ; Use CpuId instructuion (CPUID.01H:EDX.SSE[bit 25] = 1) to test
    ; whether the processor supports SSE instruction.
    ;
    mov     eax, 1
    cpuid
    bt      edx, 25
    jnc     Done

    ;
    ; Set OSFXSR bit 9 in CR4
    ;
    mov     eax, cr4
    or      eax, BIT9
    mov     cr4, eax

    ;
    ; The processor should support SSE instruction and we can use
    ; ldmxcsr instruction
    ;
    ldmxcsr [mMmxControlWord]
Done:
    pop     ebx

    ret

