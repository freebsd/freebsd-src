
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

EXTERNDEF   m16Start:BYTE
EXTERNDEF   m16Size:WORD
EXTERNDEF   mThunk16Attr:WORD
EXTERNDEF   m16Gdt:WORD
EXTERNDEF   m16GdtrBase:WORD
EXTERNDEF   mTransition:WORD

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
_EFLAGS     DQ      ?
_EIP        DD      ?
_CS         DW      ?
_SS         DW      ?
IA32_REGS   ENDS

    .const

m16Size         DW      InternalAsmThunk16 - m16Start
mThunk16Attr    DW      _ThunkAttr - m16Start
m16Gdt          DW      _NullSeg - m16Start
m16GdtrBase     DW      _16GdtrBase - m16Start
mTransition     DW      _EntryPoint - m16Start

    .code

m16Start    LABEL   BYTE

SavedGdt    LABEL   FWORD
            DW      ?
            DQ      ?

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
    ; Some instructions for manipulation of segment registers have to be written
    ; in opcode since 64-bit MASM prevents accesses to those registers.
    ;
    DB      16h                         ; push ss
    DB      0eh                         ; push cs
    DB      66h
    call    @Base                       ; push eip
@Base:
    DB      66h
    push    0                           ; reserved high order 32 bits of EFlags
    pushf                               ; pushfd actually
    cli                                 ; disable interrupts
    push    gs
    push    fs
    DB      6                           ; push es
    DB      1eh                         ; push ds
    DB      66h, 60h                    ; pushad
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
    lea     bp, [esp + sizeof (IA32_REGS)]
    ;
    ; rsi in the following 2 instructions is indeed bp in 16-bit code
    ;
    mov     word ptr (IA32_REGS ptr [rsi - sizeof (IA32_REGS)])._ESP, bp
    DB      66h
    mov     ebx, (IA32_REGS ptr [rsi - sizeof (IA32_REGS)])._EIP
    shl     ax, 4                       ; shl eax, 4
    add     bp, ax                      ; add ebp, eax
    mov     ax, cs
    shl     ax, 4
    lea     ax, [eax + ebx + (@64BitCode - @Base)]
    DB      66h, 2eh, 89h, 87h          ; mov cs:[bx + (@64Eip - @Base)], eax
    DW      @64Eip - @Base
    DB      66h, 0b8h                   ; mov eax, imm32
SavedCr4    DD      ?
    mov     cr4, rax
    ;
    ; rdi in the instruction below is indeed bx in 16-bit code
    ;
    DB      66h, 2eh                    ; 2eh is "cs:" segment override
    lgdt    fword ptr [rdi + (SavedGdt - @Base)]
    DB      66h
    mov     ecx, 0c0000080h
    rdmsr
    or      ah, 1
    wrmsr
    DB      66h, 0b8h                   ; mov eax, imm32
SavedCr0    DD      ?
    mov     cr0, rax
    DB      66h, 0eah                   ; jmp far cs:@64Bit
@64Eip      DD      ?
SavedCs     DW      ?
@64BitCode:
    db      090h 
    db      048h, 0bch                 ; mov rsp, imm64
SavedSp     DQ   ?                     ; restore stack
    nop
    ret
_BackFromUserCode   ENDP

_EntryPoint DD      _ToUserCode - m16Start
            DW      CODE16
_16Gdtr     LABEL   FWORD
            DW      GDT_SIZE - 1
_16GdtrBase DQ      _NullSeg
_16Idtr     FWORD   (1 SHL 10) - 1

;------------------------------------------------------------------------------
; _ToUserCode() takes control in real mode before passing control to user code.
; It will be shadowed to somewhere in memory below 1MB.
;------------------------------------------------------------------------------
_ToUserCode PROC
    mov     ss, edx                     ; set new segment selectors
    mov     ds, edx
    mov     es, edx
    mov     fs, edx
    mov     gs, edx
    DB      66h
    mov     ecx, 0c0000080h
    mov     cr0, rax                    ; real mode starts at next instruction
    rdmsr
    and     ah, NOT 1
    wrmsr
    mov     cr4, rbp
    mov     ss, esi                     ; set up 16-bit stack segment
    mov     sp, bx                      ; set up 16-bit stack pointer
    DB      66h                         ; make the following call 32-bit
    call    @Base                       ; push eip
@Base:
    pop     bp                          ; ebp <- address of @Base
    push    [esp + sizeof (IA32_REGS) + 2]
    lea     eax, [rsi + (@RealMode - @Base)]    ; rsi is "bp" in 16-bit code
    push    rax
    retf                                ; execution begins at next instruction
@RealMode:
    DB      66h, 2eh                    ; CS and operand size override
    lidt    fword ptr [rsi + (_16Idtr - @Base)]
    DB      66h, 61h                    ; popad
    DB      1fh                         ; pop ds
    DB      07h                         ; pop es
    pop     fs
    pop     gs
    popf                                ; popfd
    lea     sp, [esp + 4]               ; skip high order 32 bits of EFlags
    DB      66h                         ; make the following retf 32-bit
    retf                                ; transfer control to user code
_ToUserCode ENDP

CODE16  = _16Code - $
DATA16  = _16Data - $
DATA32  = _32Data - $

_NullSeg    DQ      0
_16Code     LABEL   QWORD
            DW      -1
            DW      0
            DB      0
            DB      9bh
            DB      8fh                 ; 16-bit segment, 4GB limit
            DB      0
_16Data     LABEL   QWORD
            DW      -1
            DW      0
            DB      0
            DB      93h
            DB      8fh                 ; 16-bit segment, 4GB limit
            DB      0
_32Data     LABEL   QWORD
            DW      -1
            DW      0
            DB      0
            DB      93h
            DB      0cfh                ; 16-bit segment, 4GB limit
            DB      0

GDT_SIZE = $ - _NullSeg

;------------------------------------------------------------------------------
; IA32_REGISTER_SET *
; EFIAPI
; InternalAsmThunk16 (
;   IN      IA32_REGISTER_SET         *RegisterSet,
;   IN OUT  VOID                      *Transition
;   );
;------------------------------------------------------------------------------
InternalAsmThunk16  PROC    USES    rbp rbx rsi rdi
    mov     rbx, ds
    push    rbx          ; Save ds segment register on the stack
    mov     rbx, es
    push    rbx          ; Save es segment register on the stack
    mov     rbx, ss
    push    rbx          ; Save ss segment register on the stack
    
    push    fs
    push    gs
    mov     rsi, rcx
    movzx   r8d, (IA32_REGS ptr [rsi])._SS
    mov     edi, (IA32_REGS ptr [rsi])._ESP
    lea     rdi, [edi - (sizeof (IA32_REGS) + 4)]
    imul    eax, r8d, 16                ; eax <- r8d(stack segment) * 16
    mov     ebx, edi                    ; ebx <- stack for 16-bit code
    push    sizeof (IA32_REGS) / 4
    add     edi, eax                    ; edi <- linear address of 16-bit stack
    pop     rcx
    rep     movsd                       ; copy RegSet
    lea     ecx, [rdx + (SavedCr4 - m16Start)]
    mov     eax, edx                    ; eax <- transition code address
    and     edx, 0fh
    shl     eax, 12                     ; segment address in high order 16 bits
    lea     ax, [rdx + (_BackFromUserCode - m16Start)]  ; offset address
    stosd                               ; [edi] <- return address of user code
  
    sgdt    fword ptr [rsp + 60h]       ; save GDT stack in argument space
    movzx   r10, word ptr [rsp + 60h]   ; r10 <- GDT limit 
    lea     r11, [rcx + (InternalAsmThunk16 - SavedCr4) + 0xf]
    and     r11, 0xfffffff0             ; r11 <- 16-byte aligned shadowed GDT table in real mode buffer
    
    mov     word ptr [rcx + (SavedGdt - SavedCr4)], r10w      ; save the limit of shadowed GDT table
    mov     qword ptr [rcx + (SavedGdt - SavedCr4) + 2], r11  ; save the base address of shadowed GDT table
    
    mov     rsi, qword ptr [rsp + 62h]  ; rsi <- the original GDT base address
    xchg    rcx, r10                    ; save rcx to r10 and initialize rcx to be the limit of GDT table
    inc     rcx                         ; rcx <- the size of memory to copy
    xchg    rdi, r11                    ; save rdi to r11 and initialize rdi to the base address of shadowed GDT table
    rep     movsb                       ; perform memory copy to shadow GDT table
    mov     rcx, r10                    ; restore the orignal rcx before memory copy
    mov     rdi, r11                    ; restore the original rdi before memory copy
    
    sidt    fword ptr [rsp + 50h]       ; save IDT stack in argument space
    mov     rax, cr0
    mov     [rcx + (SavedCr0 - SavedCr4)], eax
    and     eax, 7ffffffeh              ; clear PE, PG bits
    mov     rbp, cr4
    mov     [rcx], ebp                  ; save CR4 in SavedCr4
    and     ebp, NOT 30h                ; clear PAE, PSE bits
    mov     esi, r8d                    ; esi <- 16-bit stack segment
    DB      6ah, DATA32                 ; push DATA32
    pop     rdx                         ; rdx <- 32-bit data segment selector
    lgdt    fword ptr [rcx + (_16Gdtr - SavedCr4)]
    mov     ss, edx
    pushfq
    lea     edx, [rdx + DATA16 - DATA32]
    lea     r8, @RetFromRealMode
    push    r8
    mov     r8d, cs
    mov     [rcx + (SavedCs - SavedCr4)], r8w
    mov     [rcx + (SavedSp - SavedCr4)], rsp
    jmp     fword ptr [rcx + (_EntryPoint - SavedCr4)]
@RetFromRealMode:
    popfq
    lgdt    fword ptr [rsp + 60h]       ; restore protected mode GDTR
    lidt    fword ptr [rsp + 50h]       ; restore protected mode IDTR
    lea     eax, [rbp - sizeof (IA32_REGS)]
    pop     gs
    pop     fs
    pop     rbx
    mov     ss, rbx
    pop     rbx
    mov     es, rbx
    pop     rbx
    mov     ds, rbx
    ret
InternalAsmThunk16  ENDP

    END
