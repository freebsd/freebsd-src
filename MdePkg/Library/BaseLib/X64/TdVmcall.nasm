;------------------------------------------------------------------------------
;*
;* Copyright (c) 2020 - 2021, Intel Corporation. All rights reserved.<BR>
;* SPDX-License-Identifier: BSD-2-Clause-Patent
;*
;*
;------------------------------------------------------------------------------

DEFAULT REL
SECTION .text

%define TDVMCALL_EXPOSE_REGS_MASK       0xffcc
%define TDVMCALL                        0x0

%macro tdcall 0
    db 0x66,0x0f,0x01,0xcc
%endmacro

%macro tdcall_push_regs 0
    push rbp
    mov  rbp, rsp
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rsi
    push rdi
%endmacro

%macro tdcall_pop_regs 0
    pop rdi
    pop rsi
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15
    pop rbp
%endmacro

%define number_of_regs_pushed 8
%define number_of_parameters  4

;
; Keep these in sync for push_regs/pop_regs, code below
; uses them to find 5th or greater parameters
;
%define first_variable_on_stack_offset \
  ((number_of_regs_pushed * 8) + (number_of_parameters * 8) + 8)
%define second_variable_on_stack_offset \
  ((first_variable_on_stack_offset) + 8)

%macro tdcall_regs_preamble 2
    mov rax, %1

    xor rcx, rcx
    mov ecx, %2

    ; R10 = 0 (standard TDVMCALL)

    xor r10d, r10d

    ; Zero out unused (for standard TDVMCALL) registers to avoid leaking
    ; secrets to the VMM.

    xor ebx, ebx
    xor esi, esi
    xor edi, edi

    xor edx, edx
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
%endmacro

%macro tdcall_regs_postamble 0
    xor ebx, ebx
    xor esi, esi
    xor edi, edi

    xor ecx, ecx
    xor edx, edx
    xor r8d,  r8d
    xor r9d,  r9d
    xor r10d, r10d
    xor r11d, r11d
%endmacro

;------------------------------------------------------------------------------
; 0   => RAX = TDCALL leaf
; M   => RCX = TDVMCALL register behavior
; 1   => R10 = standard vs. vendor
; RDI => R11 = TDVMCALL function / nr
; RSI =  R12 = p1
; RDX => R13 = p2
; RCX => R14 = p3
; R8  => R15 = p4

;  UINT64
;  EFIAPI
;  TdVmCall (
;    UINT64  Leaf,  // Rcx
;    UINT64  P1,  // Rdx
;    UINT64  P2,  // R8
;    UINT64  P3,  // R9
;    UINT64  P4,  // rsp + 0x28
;    UINT64  *Val // rsp + 0x30
;    )
global ASM_PFX(TdVmCall)
ASM_PFX(TdVmCall):
       tdcall_push_regs

       mov r11, rcx
       mov r12, rdx
       mov r13, r8
       mov r14, r9
       mov r15, [rsp + first_variable_on_stack_offset ]

       tdcall_regs_preamble TDVMCALL, TDVMCALL_EXPOSE_REGS_MASK

       tdcall

       ; ignore return dataif TDCALL reports failure.
       test rax, rax
       jnz .no_return_data

       ; Propagate TDVMCALL success/failure to return value.
       mov rax, r10

       ; Retrieve the Val pointer.
       mov r9, [rsp + second_variable_on_stack_offset ]
       test r9, r9
       jz .no_return_data

       ; Propagate TDVMCALL output value to output param
       mov [r9], r11
.no_return_data:
       tdcall_regs_postamble

       tdcall_pop_regs

       ret
