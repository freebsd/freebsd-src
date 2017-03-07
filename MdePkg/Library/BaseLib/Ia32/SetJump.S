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
#   SetJump.S
#
# Abstract:
#
#   Implementation of SetJump() on IA-32.
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(SetJump), ASM_PFX(InternalAssertJumpBuffer)

#------------------------------------------------------------------------------
# UINTN
# EFIAPI
# SetJump (
#   OUT     BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer
#   );
#------------------------------------------------------------------------------
ASM_PFX(SetJump):
    pushl   0x4(%esp)
    call    ASM_PFX(InternalAssertJumpBuffer)               # To validate JumpBuffer
    pop     %ecx
    pop     %ecx                                            # ecx <- return address
    movl    (%esp), %edx
    movl    %ebx, (%edx)
    movl    %esi, 4(%edx)
    movl    %edi, 8(%edx)
    movl    %ebp, 12(%edx)
    movl    %esp, 16(%edx)
    movl    %ecx, 20(%edx)                                  # eip value to restore in LongJump
    xorl    %eax, %eax
    jmp     *%ecx
