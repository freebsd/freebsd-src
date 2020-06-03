
#include "BaseLibInternals.h"

;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   Thunk.asm
;
; Abstract:
;
;   Real mode thunk
;
;------------------------------------------------------------------------------

global ASM_PFX(m16Size)
global ASM_PFX(mThunk16Attr)
global ASM_PFX(m16Gdt)
global ASM_PFX(m16GdtrBase)
global ASM_PFX(mTransition)
global ASM_PFX(m16Start)

struc IA32_REGS

  ._EDI:       resd      1
  ._ESI:       resd      1
  ._EBP:       resd      1
  ._ESP:       resd      1
  ._EBX:       resd      1
  ._EDX:       resd      1
  ._ECX:       resd      1
  ._EAX:       resd      1
  ._DS:        resw      1
  ._ES:        resw      1
  ._FS:        resw      1
  ._GS:        resw      1
  ._EFLAGS:    resq      1
  ._EIP:       resd      1
  ._CS:        resw      1
  ._SS:        resw      1
  .size:

endstruc

SECTION .data

;
; These are global constant to convey information to C code.
;
ASM_PFX(m16Size)         DW      ASM_PFX(InternalAsmThunk16) - ASM_PFX(m16Start)
ASM_PFX(mThunk16Attr)    DW      _BackFromUserCode.ThunkAttrEnd - 4 - ASM_PFX(m16Start)
ASM_PFX(m16Gdt)          DW      _NullSeg - ASM_PFX(m16Start)
ASM_PFX(m16GdtrBase)     DW      _16GdtrBase - ASM_PFX(m16Start)
ASM_PFX(mTransition)     DW      _EntryPoint - ASM_PFX(m16Start)

SECTION .text

ASM_PFX(m16Start):

SavedGdt:
            dw  0
            dq  0

;------------------------------------------------------------------------------
; _BackFromUserCode() takes control in real mode after 'retf' has been executed
; by user code. It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_BackFromUserCode:
    ;
    ; The order of saved registers on the stack matches the order they appears
    ; in IA32_REGS structure. This facilitates wrapper function to extract them
    ; into that structure.
    ;
BITS    16
    push    ss
    push    cs
    ;
    ; Note: We can't use o32 on the next instruction because of a bug
    ; in NASM 2.09.04 through 2.10rc1.
    ;
    call    dword .Base                 ; push eip
.Base:
    push    dword 0                     ; reserved high order 32 bits of EFlags
    pushfd
    cli                                 ; disable interrupts
    push    gs
    push    fs
    push    es
    push    ds
    pushad
    mov     edx, strict dword 0
.ThunkAttrEnd:
    test    dl, THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15
    jz      .1
    mov     ax, 2401h
    int     15h
    cli                                 ; disable interrupts
    jnc     .2
.1:
    test    dl, THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL
    jz      .2
    in      al, 92h
    or      al, 2
    out     92h, al                     ; deactivate A20M#
.2:
    xor     eax, eax
    mov     ax, ss
    lea     ebp, [esp + IA32_REGS.size]
    mov     [bp - IA32_REGS.size + IA32_REGS._ESP], ebp
    mov     ebx, [bp - IA32_REGS.size + IA32_REGS._EIP]
    shl     eax, 4                      ; shl eax, 4
    add     ebp, eax                    ; add ebp, eax
    mov     eax, cs
    shl     eax, 4
    lea     eax, [eax + ebx + (.X64JmpEnd - .Base)]
    mov     [cs:bx + (.X64JmpEnd - 6 - .Base)], eax
    mov     eax, strict dword 0
.SavedCr4End:
    mov     cr4, eax
o32 lgdt [cs:bx + (SavedGdt - .Base)]
    mov     ecx, 0c0000080h
    rdmsr
    or      ah, 1
    wrmsr
    mov     eax, strict dword 0
.SavedCr0End:
    mov     cr0, eax
    jmp     0:strict dword 0
.X64JmpEnd:
BITS    64
    nop
    mov rsp, strict qword 0
.SavedSpEnd:
    nop
    ret

_EntryPoint:
        DD      _ToUserCode - ASM_PFX(m16Start)
        DW      CODE16
_16Gdtr:
        DW      GDT_SIZE - 1
_16GdtrBase:
        DQ      0
_16Idtr:
        DW      (1 << 10) - 1
        DD      0

;------------------------------------------------------------------------------
; _ToUserCode() takes control in real mode before passing control to user code.
; It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_ToUserCode:
BITS    16
    mov     ss, dx                      ; set new segment selectors
    mov     ds, dx
    mov     es, dx
    mov     fs, dx
    mov     gs, dx
    mov     ecx, 0c0000080h
    mov     cr0, eax                    ; real mode starts at next instruction
    rdmsr
    and     ah, ~1
    wrmsr
    mov     cr4, ebp
    mov     ss, si                      ; set up 16-bit stack segment
    mov     esp, ebx                    ; set up 16-bit stack pointer
    call    dword .Base                 ; push eip
.Base:
    pop     ebp                         ; ebp <- address of .Base
    push    word [dword esp + IA32_REGS.size + 2]
    lea     ax, [bp + (.RealMode - .Base)]
    push    ax
    retf                                ; execution begins at next instruction
.RealMode:

o32 lidt    [cs:bp + (_16Idtr - .Base)]

    popad
    pop     ds
    pop     es
    pop     fs
    pop     gs
    popfd
    lea     esp, [esp + 4]        ; skip high order 32 bits of EFlags

o32 retf                                ; transfer control to user code

ALIGN   8

CODE16  equ _16Code - $
DATA16  equ _16Data - $
DATA32  equ _32Data - $

_NullSeg    DQ      0
_16Code:
            DW      -1
            DW      0
            DB      0
            DB      9bh
            DB      8fh                 ; 16-bit segment, 4GB limit
            DB      0
_16Data:
            DW      -1
            DW      0
            DB      0
            DB      93h
            DB      8fh                 ; 16-bit segment, 4GB limit
            DB      0
_32Data:
            DW      -1
            DW      0
            DB      0
            DB      93h
            DB      0cfh                ; 16-bit segment, 4GB limit
            DB      0

GDT_SIZE equ $ - _NullSeg

;------------------------------------------------------------------------------
; IA32_REGISTER_SET *
; EFIAPI
; InternalAsmThunk16 (
;   IN      IA32_REGISTER_SET         *RegisterSet,
;   IN OUT  VOID                      *Transition
;   );
;------------------------------------------------------------------------------
global ASM_PFX(InternalAsmThunk16)
ASM_PFX(InternalAsmThunk16):
BITS    64
    push    rbp
    push    rbx
    push    rsi
    push    rdi

    mov     ebx, ds
    push    rbx          ; Save ds segment register on the stack
    mov     ebx, es
    push    rbx          ; Save es segment register on the stack
    mov     ebx, ss
    push    rbx          ; Save ss segment register on the stack

    push    fs
    push    gs
    mov     rsi, rcx
    movzx   r8d, word [rsi + IA32_REGS._SS]
    mov     edi, [rsi + IA32_REGS._ESP]
    lea     rdi, [edi - (IA32_REGS.size + 4)]
    imul    eax, r8d, 16                ; eax <- r8d(stack segment) * 16
    mov     ebx, edi                    ; ebx <- stack for 16-bit code
    push    IA32_REGS.size / 4
    add     edi, eax                    ; edi <- linear address of 16-bit stack
    pop     rcx
    rep     movsd                       ; copy RegSet
    lea     ecx, [rdx + (_BackFromUserCode.SavedCr4End - ASM_PFX(m16Start))]
    mov     eax, edx                    ; eax <- transition code address
    and     edx, 0fh
    shl     eax, 12                     ; segment address in high order 16 bits
    lea     ax, [rdx + (_BackFromUserCode - ASM_PFX(m16Start))]  ; offset address
    stosd                               ; [edi] <- return address of user code

    sgdt    [rsp + 60h]       ; save GDT stack in argument space
    movzx   r10, word [rsp + 60h]   ; r10 <- GDT limit
    lea     r11, [rcx + (ASM_PFX(InternalAsmThunk16) - _BackFromUserCode.SavedCr4End) + 0xf]
    and     r11, ~0xf            ; r11 <- 16-byte aligned shadowed GDT table in real mode buffer

    mov     [rcx + (SavedGdt - _BackFromUserCode.SavedCr4End)], r10w      ; save the limit of shadowed GDT table
    mov     [rcx + (SavedGdt - _BackFromUserCode.SavedCr4End) + 2], r11  ; save the base address of shadowed GDT table

    mov     rsi, [rsp + 62h]  ; rsi <- the original GDT base address
    xchg    rcx, r10                    ; save rcx to r10 and initialize rcx to be the limit of GDT table
    inc     rcx                         ; rcx <- the size of memory to copy
    xchg    rdi, r11                    ; save rdi to r11 and initialize rdi to the base address of shadowed GDT table
    rep     movsb                       ; perform memory copy to shadow GDT table
    mov     rcx, r10                    ; restore the orignal rcx before memory copy
    mov     rdi, r11                    ; restore the original rdi before memory copy

    sidt    [rsp + 50h]       ; save IDT stack in argument space
    mov     rax, cr0
    mov     [rcx + (_BackFromUserCode.SavedCr0End - 4 - _BackFromUserCode.SavedCr4End)], eax
    and     eax, 7ffffffeh              ; clear PE, PG bits
    mov     rbp, cr4
    mov     [rcx - 4], ebp              ; save CR4 in _BackFromUserCode.SavedCr4End - 4
    and     ebp, ~30h                ; clear PAE, PSE bits
    mov     esi, r8d                    ; esi <- 16-bit stack segment
    push    DATA32
    pop     rdx                         ; rdx <- 32-bit data segment selector
    lgdt    [rcx + (_16Gdtr - _BackFromUserCode.SavedCr4End)]
    mov     ss, edx
    pushfq
    lea     edx, [rdx + DATA16 - DATA32]
    lea     r8, [REL .RetFromRealMode]
    push    r8
    mov     r8d, cs
    mov     [rcx + (_BackFromUserCode.X64JmpEnd - 2 - _BackFromUserCode.SavedCr4End)], r8w
    mov     [rcx + (_BackFromUserCode.SavedSpEnd - 8 - _BackFromUserCode.SavedCr4End)], rsp
    jmp     dword far [rcx + (_EntryPoint - _BackFromUserCode.SavedCr4End)]
.RetFromRealMode:
    popfq
    lgdt    [rsp + 60h]       ; restore protected mode GDTR
    lidt    [rsp + 50h]       ; restore protected mode IDTR
    lea     eax, [rbp - IA32_REGS.size]
    pop     gs
    pop     fs
    pop     rbx
    mov     ss, ebx
    pop     rbx
    mov     es, ebx
    pop     rbx
    mov     ds, ebx

    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp

    ret
