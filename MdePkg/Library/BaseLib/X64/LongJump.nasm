;------------------------------------------------------------------------------
;
; Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
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
;   LongJump.Asm
;
; Abstract:
;
;   Implementation of _LongJump() on x64.
;
;------------------------------------------------------------------------------

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
; VOID
; EFIAPI
; InternalLongJump (
;   IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
;   IN      UINTN                     Value
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalLongJump)
ASM_PFX(InternalLongJump):
    mov     rbx, [rcx]
    mov     rsp, [rcx + 8]
    mov     rbp, [rcx + 0x10]
    mov     rdi, [rcx + 0x18]
    mov     rsi, [rcx + 0x20]
    mov     r12, [rcx + 0x28]
    mov     r13, [rcx + 0x30]
    mov     r14, [rcx + 0x38]
    mov     r15, [rcx + 0x40]
    ; load non-volatile fp registers
    ldmxcsr [rcx + 0x50]
    movdqu  xmm6,  [rcx + 0x58]
    movdqu  xmm7,  [rcx + 0x68]
    movdqu  xmm8,  [rcx + 0x78]
    movdqu  xmm9,  [rcx + 0x88]
    movdqu  xmm10, [rcx + 0x98]
    movdqu  xmm11, [rcx + 0xA8]
    movdqu  xmm12, [rcx + 0xB8]
    movdqu  xmm13, [rcx + 0xC8]
    movdqu  xmm14, [rcx + 0xD8]
    movdqu  xmm15, [rcx + 0xE8]
    mov     rax, rdx               ; set return value
    jmp     qword [rcx + 0x48]

