
#include "BaseLibInternals.h"

;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
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
;   Thunk.asm
;
; Abstract:
;
;   Real mode thunk
;
;------------------------------------------------------------------------------

    .686p
    .model  flat,C

EXTERNDEF   C   m16Start:BYTE
EXTERNDEF   C   m16Size:WORD
EXTERNDEF   C   mThunk16Attr:WORD
EXTERNDEF   C   m16Gdt:WORD
EXTERNDEF   C   m16GdtrBase:WORD
EXTERNDEF   C   mTransition:WORD

;
; Here is the layout of the real mode stack. _ToUserCode() is responsible for
; loading all these registers from real mode stack.
;
IA32_REGS   STRUC   4t
_EDI        DD      ?
_ESI        DD      ?
_EBP        DD      ?
_ESP        DD      ?
_EBX        DD      ?
_EDX        DD      ?
_ECX        DD      ?
_EAX        DD      ?
_DS         DW      ?
_ES         DW      ?
_FS         DW      ?
_GS         DW      ?
_EFLAGS     DD      ?
_EIP        DD      ?
_CS         DW      ?
_SS         DW      ?
IA32_REGS   ENDS

    .const

;
; These are global constant to convey information to C code.
;
m16Size         DW      InternalAsmThunk16 - m16Start
mThunk16Attr    DW      _ThunkAttr - m16Start
m16Gdt          DW      _NullSegDesc - m16Start
m16GdtrBase     DW      _16GdtrBase - m16Start
mTransition     DW      _EntryPoint - m16Start

    .code

m16Start    LABEL   BYTE

SavedGdt    LABEL   FWORD
            DW      ?
            DD      ?
;------------------------------------------------------------------------------
; _BackFromUserCode() takes control in real mode after 'retf' has been executed
; by user code. It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_BackFromUserCode   PROC
    ;
    ; The order of saved registers on the stack matches the order they appears
    ; in IA32_REGS structure. This facilitates wrapper function to extract them
    ; into that structure.
    ;
    push    ss
    push    cs
    DB      66h
    call    @Base                       ; push eip
@Base:
    pushf                               ; pushfd actually
    cli                                 ; disable interrupts
    push    gs
    push    fs
    push    es
    push    ds
    pushaw                              ; pushad actually
    DB      66h, 0bah                   ; mov edx, imm32
_ThunkAttr  DD      ?
    test    dl, THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15
    jz      @1
    mov     eax, 15cd2401h              ; mov ax, 2401h & int 15h
    cli                                 ; disable interrupts
    jnc     @2
@1:
    test    dl, THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL
    jz      @2
    in      al, 92h
    or      al, 2
    out     92h, al                     ; deactivate A20M#
@2:
    xor     ax, ax                      ; xor eax, eax
    mov     eax, ss                     ; mov ax, ss
    DB      67h
    lea     bp, [esp + sizeof (IA32_REGS)]
    ;
    ; esi's in the following 2 instructions are indeed bp in 16-bit code. Fact
    ; is "esi" in 32-bit addressing mode has the same encoding of "bp" in 16-
    ; bit addressing mode.
    ;
    mov     word ptr (IA32_REGS ptr [esi - sizeof (IA32_REGS)])._ESP, bp
    mov     ebx, (IA32_REGS ptr [esi - sizeof (IA32_REGS)])._EIP
    shl     ax, 4                       ; shl eax, 4
    add     bp, ax                      ; add ebp, eax
    DB      66h, 0b8h                   ; mov eax, imm32
SavedCr4    DD      ?
    mov     cr4, eax
    DB      66h
    lgdt    fword ptr cs:[edi + (SavedGdt - @Base)]
    DB      66h, 0b8h                   ; mov eax, imm32
SavedCr0    DD      ?
    mov     cr0, eax
    DB      0b8h                        ; mov ax, imm16
SavedSs     DW      ?
    mov     ss, eax
    DB      66h, 0bch                   ; mov esp, imm32
SavedEsp    DD      ?
    DB      66h
    retf                                ; return to protected mode
_BackFromUserCode   ENDP

_EntryPoint DD      _ToUserCode - m16Start
            DW      8h
_16Idtr     FWORD   (1 SHL 10) - 1
_16Gdtr     LABEL   FWORD
            DW      GdtEnd - _NullSegDesc - 1
_16GdtrBase DD      _NullSegDesc

;------------------------------------------------------------------------------
; _ToUserCode() takes control in real mode before passing control to user code.
; It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_ToUserCode PROC
    mov     edx, ss
    mov     ss, ecx                     ; set new segment selectors
    mov     ds, ecx
    mov     es, ecx
    mov     fs, ecx
    mov     gs, ecx
    mov     cr0, eax                    ; real mode starts at next instruction
                                        ;  which (per SDM) *must* be a far JMP.
    DB      0eah
_RealAddr DW 0,0                       ; filled in by InternalAsmThunk16

    mov     cr4, ebp
    mov     ss, esi                     ; set up 16-bit stack segment
    xchg    sp, bx                      ; set up 16-bit stack pointer

;   mov     bp, [esp + sizeof(IA32_REGS)
    DB      67h
    mov     ebp, [esp + sizeof(IA32_REGS)] ; BackFromUserCode address from stack

;   mov     cs:[bp + (SavedSs - _BackFromUserCode)], dx
    mov     cs:[esi + (SavedSs - _BackFromUserCode)], edx

;   mov     cs:[bp + (SavedEsp - _BackFromUserCode)], ebx
    DB      2eh, 66h, 89h, 9eh
    DW      SavedEsp - _BackFromUserCode

;   lidt    cs:[bp + (_16Idtr - _BackFromUserCode)]
    DB      2eh, 66h, 0fh, 01h, 9eh
    DW      _16Idtr - _BackFromUserCode

    popaw                               ; popad actually
    pop     ds
    pop     es
    pop     fs
    pop     gs
    popf                                ; popfd
    DB      66h                         ; Use 32-bit addressing for "retf" below
    retf                                ; transfer control to user code
_ToUserCode ENDP

_NullSegDesc    DQ      0
_16CsDesc       LABEL   QWORD
                DW      -1
                DW      0
                DB      0
                DB      9bh
                DB      8fh             ; 16-bit segment, 4GB limit
                DB      0
_16DsDesc       LABEL   QWORD
                DW      -1
                DW      0
                DB      0
                DB      93h
                DB      8fh             ; 16-bit segment, 4GB limit
                DB      0
GdtEnd          LABEL   QWORD

;------------------------------------------------------------------------------
; IA32_REGISTER_SET *
; EFIAPI
; InternalAsmThunk16 (
;   IN      IA32_REGISTER_SET         *RegisterSet,
;   IN OUT  VOID                      *Transition
;   );
;------------------------------------------------------------------------------
InternalAsmThunk16  PROC    USES    ebp ebx esi edi ds  es  fs  gs
    mov     esi, [esp + 36]             ; esi <- RegSet, the 1st parameter
    movzx   edx, (IA32_REGS ptr [esi])._SS
    mov     edi, (IA32_REGS ptr [esi])._ESP
    add     edi, - (sizeof (IA32_REGS) + 4) ; reserve stack space
    mov     ebx, edi                    ; ebx <- stack offset
    imul    eax, edx, 16                ; eax <- edx * 16
    push    sizeof (IA32_REGS) / 4
    add     edi, eax                    ; edi <- linear address of 16-bit stack
    pop     ecx
    rep     movsd                       ; copy RegSet
    mov     eax, [esp + 40]             ; eax <- address of transition code
    mov     esi, edx                    ; esi <- 16-bit stack segment
    lea     edx, [eax + (SavedCr0 - m16Start)]
    mov     ecx, eax
    and     ecx, 0fh
    shl     eax, 12
    lea     ecx, [ecx + (_BackFromUserCode - m16Start)]
    mov     ax, cx
    stosd                               ; [edi] <- return address of user code
    add     eax, _RealAddr + 4 - _BackFromUserCode
    mov     dword ptr [edx + (_RealAddr - SavedCr0)], eax
    sgdt    fword ptr [edx + (SavedGdt - SavedCr0)]
    sidt    fword ptr [esp + 36]        ; save IDT stack in argument space
    mov     eax, cr0
    mov     [edx], eax                  ; save CR0 in SavedCr0
    and     eax, 7ffffffeh              ; clear PE, PG bits
    mov     ebp, cr4
    mov     [edx + (SavedCr4 - SavedCr0)], ebp
    and     ebp, NOT 30h                ; clear PAE, PSE bits
    push    10h
    pop     ecx                         ; ecx <- selector for data segments
    lgdt    fword ptr [edx + (_16Gdtr - SavedCr0)]
    pushfd                              ; Save df/if indeed
    call    fword ptr [edx + (_EntryPoint - SavedCr0)]
    popfd
    lidt    fword ptr [esp + 36]        ; restore protected mode IDTR
    lea     eax, [ebp - sizeof (IA32_REGS)] ; eax <- the address of IA32_REGS
    ret
InternalAsmThunk16  ENDP

    END
