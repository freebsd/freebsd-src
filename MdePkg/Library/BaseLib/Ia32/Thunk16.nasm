
#include "BaseLibInternals.h"

;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
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
  ._EFLAGS:    resd      1
  ._EIP:       resd      1
  ._CS:        resw      1
  ._SS:        resw      1
  .size:

endstruc

;; .const

SECTION .data

;
; These are global constant to convey information to C code.
;
ASM_PFX(m16Size)         DW      ASM_PFX(InternalAsmThunk16) - ASM_PFX(m16Start)
ASM_PFX(mThunk16Attr)    DW      _BackFromUserCode.ThunkAttrEnd - 4 - ASM_PFX(m16Start)
ASM_PFX(m16Gdt)          DW      _NullSegDesc - ASM_PFX(m16Start)
ASM_PFX(m16GdtrBase)     DW      _16GdtrBase - ASM_PFX(m16Start)
ASM_PFX(mTransition)     DW      _EntryPoint - ASM_PFX(m16Start)

SECTION .text

ASM_PFX(m16Start):

SavedGdt:
            dw  0
            dd  0

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
    mov     bx, [bp - IA32_REGS.size + IA32_REGS._EIP]
    shl     eax, 4                      ; shl eax, 4
    add     ebp, eax                    ; add ebp, eax
    mov     eax, strict dword 0
.SavedCr4End:
    mov     cr4, eax
o32 lgdt [cs:bx + (SavedGdt - .Base)]
    mov     eax, strict dword 0
.SavedCr0End:
    mov     cr0, eax
    mov     ax, strict word 0
.SavedSsEnd:
    mov     ss, eax
    mov     esp, strict dword 0
.SavedEspEnd:
o32 retf                                ; return to protected mode

_EntryPoint:
        DD      _ToUserCode - ASM_PFX(m16Start)
        DW      8h
_16Idtr:
        DW      (1 << 10) - 1
        DD      0
_16Gdtr:
        DW      GdtEnd - _NullSegDesc - 1
_16GdtrBase:
        DD      0

;------------------------------------------------------------------------------
; _ToUserCode() takes control in real mode before passing control to user code.
; It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_ToUserCode:
BITS    16
    mov     dx, ss
    mov     ss, cx                      ; set new segment selectors
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx
    mov     cr0, eax                    ; real mode starts at next instruction
                                        ;  which (per SDM) *must* be a far JMP.
    jmp     0:strict word 0
.RealAddrEnd:
    mov     cr4, ebp
    mov     ss, si                      ; set up 16-bit stack segment
    xchg    esp, ebx                    ; set up 16-bit stack pointer
    mov     bp, [esp + IA32_REGS.size]
    mov     [cs:bp + (_BackFromUserCode.SavedSsEnd - 2 - _BackFromUserCode)], dx
    mov     [cs:bp + (_BackFromUserCode.SavedEspEnd - 4 - _BackFromUserCode)], ebx
    lidt    [cs:bp + (_16Idtr - _BackFromUserCode)]

    popad
    pop     ds
    pop     es
    pop     fs
    pop     gs
    popfd

o32 retf                                ; transfer control to user code

ALIGN   16
_NullSegDesc    DQ      0
_16CsDesc:
                DW      -1
                DW      0
                DB      0
                DB      9bh
                DB      8fh             ; 16-bit segment, 4GB limit
                DB      0
_16DsDesc:
                DW      -1
                DW      0
                DB      0
                DB      93h
                DB      8fh             ; 16-bit segment, 4GB limit
                DB      0
GdtEnd:

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
BITS    32
    push    ebp
    push    ebx
    push    esi
    push    edi
    push    ds
    push    es
    push    fs
    push    gs
    mov     esi, [esp + 36]             ; esi <- RegSet, the 1st parameter
    movzx   edx, word [esi + IA32_REGS._SS]
    mov     edi, [esi + IA32_REGS._ESP]
    add     edi, - (IA32_REGS.size + 4) ; reserve stack space
    mov     ebx, edi                    ; ebx <- stack offset
    imul    eax, edx, 16                ; eax <- edx * 16
    push    IA32_REGS.size / 4
    add     edi, eax                    ; edi <- linear address of 16-bit stack
    pop     ecx
    rep     movsd                       ; copy RegSet
    mov     eax, [esp + 40]             ; eax <- address of transition code
    mov     esi, edx                    ; esi <- 16-bit stack segment
    lea     edx, [eax + (_BackFromUserCode.SavedCr0End - ASM_PFX(m16Start))]
    mov     ecx, eax
    and     ecx, 0fh
    shl     eax, 12
    lea     ecx, [ecx + (_BackFromUserCode - ASM_PFX(m16Start))]
    mov     ax, cx
    stosd                               ; [edi] <- return address of user code
    add     eax, _ToUserCode.RealAddrEnd - _BackFromUserCode
    mov     [edx + (_ToUserCode.RealAddrEnd - 4 - _BackFromUserCode.SavedCr0End)], eax
    sgdt    [edx + (SavedGdt - _BackFromUserCode.SavedCr0End)]
    sidt    [esp + 36]        ; save IDT stack in argument space
    mov     eax, cr0
    mov     [edx - 4], eax                  ; save CR0 in _BackFromUserCode.SavedCr0End - 4
    and     eax, 7ffffffeh              ; clear PE, PG bits
    mov     ebp, cr4
    mov     [edx + (_BackFromUserCode.SavedCr4End - 4 - _BackFromUserCode.SavedCr0End)], ebp
    and     ebp, ~30h                ; clear PAE, PSE bits
    push    10h
    pop     ecx                         ; ecx <- selector for data segments
    lgdt    [edx + (_16Gdtr - _BackFromUserCode.SavedCr0End)]
    pushfd                              ; Save df/if indeed
    call    dword far [edx + (_EntryPoint - _BackFromUserCode.SavedCr0End)]
    popfd
    lidt    [esp + 36]        ; restore protected mode IDTR
    lea     eax, [ebp - IA32_REGS.size] ; eax <- the address of IA32_REGS
    pop     gs
    pop     fs
    pop     es
    pop     ds
    pop     edi
    pop     esi
    pop     ebx
    pop     ebp
    ret
