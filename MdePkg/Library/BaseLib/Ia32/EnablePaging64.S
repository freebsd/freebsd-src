#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
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
#   EnablePaging64.S
#
# Abstract:
#
#   InternalX86EnablePaging64 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalX86EnablePaging64)

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# InternalX86EnablePaging64 (
#   IN      UINT16                    CodeSelector,
#   IN      UINT64                    EntryPoint,
#   IN      UINT64                    Context1,    OPTIONAL
#   IN      UINT64                    Context2,    OPTIONAL
#   IN      UINT64                    NewStack
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalX86EnablePaging64):
    cli
    movl    $LongStart, (%esp)          # offset for far retf, seg is the 1st arg
    movl    %cr4, %eax
    orb     $0x20, %al
    movl    %eax, %cr4                  # enable PAE
    movl    $0xc0000080, %ecx
    rdmsr
    orb     $1, %ah                     # set LME
    wrmsr
    movl    %cr0, %eax
    btsl    $31, %eax                   # set PG
    movl    %eax, %cr0                  # enable paging
    lret                                # topmost 2 dwords hold the address
LongStart:                              # long mode starts here
    .byte   0x67, 0x48                  # 32-bit address size, 64-bit operand size
    movl    (%esp), %ebx                # mov rbx, [esp]
    .byte   0x67, 0x48
    movl    8(%esp), %ecx               # mov rcx, [esp + 8]
    .byte   0x67, 0x48
    movl    0x10(%esp), %edx            # mov rdx, [esp + 10h]
    .byte   0x67, 0x48
    movl    0x18(%esp), %esp            # mov rsp, [esp + 18h]
    .byte   0x48
    addl    $-0x20, %esp                # add rsp, -20h
    call    *%ebx                       # call rbx
    jmp     .                           # no one should get here
