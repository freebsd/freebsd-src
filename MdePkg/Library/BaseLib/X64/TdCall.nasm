;------------------------------------------------------------------------------
;*
;* Copyright (c) 2020 - 2021, Intel Corporation. All rights reserved.<BR>
;* SPDX-License-Identifier: BSD-2-Clause-Patent
;*
;*
;------------------------------------------------------------------------------

DEFAULT REL
SECTION .text

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

;  TdCall (
;    UINT64  Leaf,    // Rcx
;    UINT64  P1,      // Rdx
;    UINT64  P2,      // R8
;    UINT64  P3,      // R9
;    UINT64  Results, // rsp + 0x28
;    )
global ASM_PFX(TdCall)
ASM_PFX(TdCall):
       tdcall_push_regs

       mov rax, rcx
       mov rcx, rdx
       mov rdx, r8
       mov r8, r9

       tdcall

       ; exit if tdcall reports failure.
       test rax, rax
       jnz .exit

       ; test if caller wanted results
       mov r12, [rsp + first_variable_on_stack_offset ]
       test r12, r12
       jz .exit
       mov [r12 + 0 ], rcx
       mov [r12 + 8 ], rdx
       mov [r12 + 16], r8
       mov [r12 + 24], r9
       mov [r12 + 32], r10
       mov [r12 + 40], r11
.exit:
       tdcall_pop_regs
       ret
