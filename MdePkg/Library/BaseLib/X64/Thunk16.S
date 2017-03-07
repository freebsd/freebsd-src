#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
# Module Name:
#
#   Thunk16.S
#
# Abstract:
#
#   Real mode thunk
#
#------------------------------------------------------------------------------

#include <Library/BaseLib.h>

ASM_GLOBAL ASM_PFX(m16Start)
ASM_GLOBAL ASM_PFX(m16Size)
ASM_GLOBAL ASM_PFX(mThunk16Attr)
ASM_GLOBAL ASM_PFX(m16Gdt)
ASM_GLOBAL ASM_PFX(m16GdtrBase)
ASM_GLOBAL ASM_PFX(mTransition)
ASM_GLOBAL ASM_PFX(InternalAsmThunk16)

# define the structure of IA32_REGS
.set  _EDI, 0       #size 4
.set  _ESI, 4       #size 4
.set  _EBP, 8       #size 4
.set  _ESP, 12      #size 4
.set  _EBX, 16      #size 4
.set  _EDX, 20      #size 4
.set  _ECX, 24      #size 4
.set  _EAX, 28      #size 4
.set  _DS,  32      #size 2
.set  _ES,  34      #size 2
.set  _FS,  36      #size 2
.set  _GS,  38      #size 2
.set  _EFLAGS, 40   #size 8
.set  _EIP, 48      #size 4
.set  _CS, 52       #size 2
.set  _SS, 54       #size 2
.set  IA32_REGS_SIZE, 56

    .data
    
.set Lm16Size, ASM_PFX(InternalAsmThunk16) - ASM_PFX(m16Start)
ASM_PFX(m16Size):         .word      Lm16Size
.set  LmThunk16Attr, L_ThunkAttr - ASM_PFX(m16Start)
ASM_PFX(mThunk16Attr):    .word      LmThunk16Attr
.set Lm16Gdt, ASM_PFX(NullSeg) - ASM_PFX(m16Start)
ASM_PFX(m16Gdt):          .word      Lm16Gdt
.set Lm16GdtrBase, _16GdtrBase - ASM_PFX(m16Start)
ASM_PFX(m16GdtrBase):     .word      Lm16GdtrBase
.set LmTransition, _EntryPoint - ASM_PFX(m16Start)
ASM_PFX(mTransition):     .word      LmTransition

    .text

ASM_PFX(m16Start):

SavedGdt:    .space 10

#------------------------------------------------------------------------------
# _BackFromUserCode() takes control in real mode after 'retf' has been executed
# by user code. It will be shadowed to somewhere in memory below 1MB.
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(BackFromUserCode)
ASM_PFX(BackFromUserCode):
    #
    # The order of saved registers on the stack matches the order they appears
    # in IA32_REGS structure. This facilitates wrapper function to extract them
    # into that structure.
    #
    # Some instructions for manipulation of segment registers have to be written
    # in opcode since 64-bit MASM prevents accesses to those registers.
    #
    .byte 0x16                          # push ss
    .byte 0xe                           # push cs
    .byte 0x66
    call    L_Base                       # push eip
L_Base: 
    .byte 0x66
    pushq   $0                          # reserved high order 32 bits of EFlags
    .byte 0x66, 0x9c                    # pushfd actually
    cli                                 # disable interrupts
    push    %gs
    push    %fs
    .byte 6                             # push es
    .byte 0x1e                          # push ds
    .byte 0x66,0x60                     # pushad
    .byte 0x66,0xba                     # mov edx, imm32
L_ThunkAttr:  .space  4
    testb   $THUNK_ATTRIBUTE_DISABLE_A20_MASK_INT_15, %dl
    jz      L_1
    movl    $0x15cd2401,%eax            # mov ax, 2401h & int 15h
    cli                                 # disable interrupts
    jnc     L_2
L_1: 
    testb   $THUNK_ATTRIBUTE_DISABLE_A20_MASK_KBD_CTRL, %dl
    jz      L_2
    inb     $0x92,%al
    orb     $2,%al
    outb    %al, $0x92                   # deactivate A20M#
L_2: 
    xorw    %ax, %ax                     # xor eax, eax
    movl    %ss, %eax                    # mov ax, ss
    lea     IA32_REGS_SIZE(%esp), %bp
    #
    # rsi in the following 2 instructions is indeed bp in 16-bit code
    #
    movw    %bp, (_ESP - IA32_REGS_SIZE)(%rsi)
    .byte 0x66
    movl    (_EIP - IA32_REGS_SIZE)(%rsi), %ebx
    shlw    $4,%ax                      # shl eax, 4
    addw    %ax,%bp                     # add ebp, eax
    movw    %cs,%ax
    shlw    $4,%ax
    lea     (L_64BitCode - L_Base)(%ebx, %eax), %ax
    .byte 0x66,0x2e,0x89,0x87           # mov cs:[bx + (L_64Eip - L_Base)], eax
    .word   L_64Eip - L_Base
    .byte 0x66,0xb8                     # mov eax, imm32
L_SavedCr4: .space      4
    movq    %rax, %cr4
    #
    # rdi in the instruction below is indeed bx in 16-bit code
    #
    .byte 0x66,0x2e                     # 2eh is "cs:" segment override
    lgdt    (SavedGdt - L_Base)(%rdi)
    .byte 0x66
    movl    $0xc0000080,%ecx
    rdmsr
    orb     $1,%ah
    wrmsr
    .byte 0x66,0xb8                     # mov eax, imm32
L_SavedCr0: .space      4
    movq    %rax, %cr0
    .byte 0x66,0xea                     # jmp far cs:L_64Bit
L_64Eip:    .space      4
L_SavedCs:  .space      2
L_64BitCode:
    .byte   0x90
    .byte   0x48,0xbc                  # mov rsp, imm64
L_SavedSp:  .space      8              # restore stack
    nop
    ret

_EntryPoint: .long      ASM_PFX(ToUserCode) - ASM_PFX(m16Start)
             .word      CODE16
_16Gdtr:     .word      GDT_SIZE - 1
_16GdtrBase: .quad      0
_16Idtr:     .word      0x3ff
             .long      0

#------------------------------------------------------------------------------
# _ToUserCode() takes control in real mode before passing control to user code.
# It will be shadowed to somewhere in memory below 1MB.
#------------------------------------------------------------------------------
ASM_GLOBAL ASM_PFX(ToUserCode)
ASM_PFX(ToUserCode):
    movl    %edx,%ss                    # set new segment selectors
    movl    %edx,%ds
    movl    %edx,%es
    movl    %edx,%fs
    movl    %edx,%gs
    .byte 0x66
    movl    $0xc0000080,%ecx
    movq    %rax, %cr0
    rdmsr
    andb    $0xfe, %ah                  # $0b11111110
    wrmsr
    movq    %rbp, %cr4
    movl    %esi,%ss                    # set up 16-bit stack segment
    movw    %bx,%sp                     # set up 16-bit stack pointer
    .byte 0x66                          # make the following call 32-bit
    call    L_Base1                       # push eip
L_Base1: 
    popw    %bp                         # ebp <- address of L_Base1
    pushq   (IA32_REGS_SIZE + 2)(%esp)
    lea     0x0c(%rsi), %eax
    pushq   %rax
    lret                                # execution begins at next instruction
L_RealMode: 
    .byte 0x66,0x2e                     # CS and operand size override
    lidt    (_16Idtr - L_Base1)(%rsi)
    .byte 0x66,0x61                     # popad
    .byte 0x1f                          # pop ds
    .byte 0x7                           # pop es
    .byte 0x0f, 0xa1                    # pop fs
    .byte 0x0f, 0xa9                    # pop gs
    .byte 0x66, 0x9d                    # popfd
    leaw    4(%esp),%sp                 # skip high order 32 bits of EFlags
    .byte 0x66                          # make the following retf 32-bit
    lret                                # transfer control to user code

.set  CODE16,  ASM_PFX(_16Code) - .
.set  DATA16,  ASM_PFX(_16Data) - .
.set  DATA32,  ASM_PFX(_32Data) - .

ASM_PFX(NullSeg):   .quad      0
ASM_PFX(_16Code):
            .word -1
            .word 0
            .byte 0
            .byte 0x9b
            .byte 0x8f                  # 16-bit segment, 4GB limit
            .byte 0
ASM_PFX(_16Data):
            .word -1
            .word 0
            .byte 0
            .byte 0x93
            .byte 0x8f                  # 16-bit segment, 4GB limit
            .byte 0
ASM_PFX(_32Data):
            .word -1
            .word 0
            .byte 0
            .byte 0x93
            .byte 0xcf                  # 16-bit segment, 4GB limit
            .byte 0

.set  GDT_SIZE, . - ASM_PFX(NullSeg)

#------------------------------------------------------------------------------
# IA32_REGISTER_SET *
# EFIAPI
# InternalAsmThunk16 (
#   IN      IA32_REGISTER_SET         *RegisterSet,
#   IN OUT  VOID                      *Transition
#   );
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalAsmThunk16)
ASM_PFX(InternalAsmThunk16):
    pushq   %rbp
    pushq   %rbx
    pushq   %rsi
    pushq   %rdi
    
    movl    %ds, %ebx
    pushq   %rbx      # Save ds segment register on the stack
    movl    %es, %ebx
    pushq   %rbx      # Save es segment register on the stack
    movl    %ss, %ebx
    pushq   %rbx      # Save ss segment register on the stack

    .byte   0x0f, 0xa0                  #push   fs
    .byte   0x0f, 0xa8                  #push   gs
    movq    %rcx, %rsi
    movzwl  _SS(%rsi), %r8d
    movl    _ESP(%rsi), %edi
    lea     -(IA32_REGS_SIZE + 4)(%edi), %rdi
    imul    $16, %r8d, %eax 
    movl    %edi,%ebx                   # ebx <- stack for 16-bit code
    pushq   $(IA32_REGS_SIZE / 4)
    addl    %eax,%edi                   # edi <- linear address of 16-bit stack
    popq    %rcx
    rep
    movsl                               # copy RegSet
    lea     (L_SavedCr4 - ASM_PFX(m16Start))(%rdx), %ecx
    movl    %edx,%eax                   # eax <- transition code address
    andl    $0xf,%edx
    shll    $12,%eax                    # segment address in high order 16 bits
    .set LBackFromUserCodeDelta, ASM_PFX(BackFromUserCode) - ASM_PFX(m16Start) 
    lea     (LBackFromUserCodeDelta)(%rdx), %ax
    stosl                               # [edi] <- return address of user code
    sgdt    0x60(%rsp)                  # save GDT stack in argument space
    movzwq  0x60(%rsp), %r10            # r10 <- GDT limit 
    lea     ((ASM_PFX(InternalAsmThunk16) - L_SavedCr4) + 0xf)(%rcx), %r11 
    andq    $0xfffffffffffffff0, %r11   # r11 <- 16-byte aligned shadowed GDT table in real mode buffer      
    
    movw    %r10w, (SavedGdt - L_SavedCr4)(%rcx)       # save the limit of shadowed GDT table
    movq    %r11, (SavedGdt - L_SavedCr4 + 0x2)(%rcx)  # save the base address of shadowed GDT table
    
    movq    0x62(%rsp) ,%rsi            # rsi <- the original GDT base address
    xchg   %r10, %rcx                   # save rcx to r10 and initialize rcx to be the limit of GDT table 
    incq   %rcx                         # rcx <- the size of memory to copy
    xchg   %r11, %rdi                   # save rdi to r11 and initialize rdi to the base address of shadowed GDT table
    rep
    movsb                               # perform memory copy to shadow GDT table
    movq   %r10, %rcx                   # restore the orignal rcx before memory copy
    movq   %r11, %rdi                   # restore the original rdi before memory copy
       
    sidt    0x50(%rsp)
    movq    %cr0, %rax
    .set LSavedCrDelta, L_SavedCr0 - L_SavedCr4
    movl    %eax, (LSavedCrDelta)(%rcx)
    andl    $0x7ffffffe,%eax            # clear PE, PG bits
    movq    %cr4, %rbp
    movl    %ebp, (%rcx)                # save CR4 in SavedCr4
    andl    $0xffffffcf,%ebp            # clear PAE, PSE bits
    movl    %r8d, %esi                  # esi <- 16-bit stack segment
    .byte      0x6a, DATA32
    popq    %rdx
    lgdt    (_16Gdtr - L_SavedCr4)(%rcx)
    movl    %edx,%ss
    pushfq
    lea     -8(%rdx), %edx
    lea     L_RetFromRealMode(%rip), %r8
    pushq   %r8
    movl    %cs, %r8d
    movw    %r8w, (L_SavedCs - L_SavedCr4)(%rcx)
    movq    %rsp, (L_SavedSp - L_SavedCr4)(%rcx)
    .byte   0xff, 0x69                  #  jmp (_EntryPoint - L_SavedCr4)(%rcx)
    .set    Ltemp1, _EntryPoint - L_SavedCr4
    .byte   Ltemp1
L_RetFromRealMode: 
    popfq
    lgdt    0x60(%rsp)                  # restore protected mode GDTR
    lidt    0x50(%rsp)                  # restore protected mode IDTR
    lea     -IA32_REGS_SIZE(%rbp), %eax
    .byte 0x0f, 0xa9                    # pop gs
    .byte 0x0f, 0xa1                    # pop fs
    
    popq     %rbx
    movl     %ebx, %ss
    popq     %rbx
    movl     %ebx, %es
    popq     %rbx
    movl     %ebx, %ds
    
    popq    %rdi
    popq    %rsi
    popq    %rbx
    popq    %rbp

    ret
