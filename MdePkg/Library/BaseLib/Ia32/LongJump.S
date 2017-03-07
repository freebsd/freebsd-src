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
#   LongJump.S
#
# Abstract:
#
#   Implementation of _LongJump() on IA-32.
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalLongJump)

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# InternalLongJump (
#   IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,
#   IN      UINTN                     Value
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalLongJump):
    pop     %eax                        # skip return address
    pop     %edx                        # edx <- JumpBuffer
    pop     %eax                        # eax <- Value
    movl    (%edx), %ebx
    movl    4(%edx), %esi
    movl    8(%edx), %edi
    movl    12(%edx), %ebp
    movl    16(%edx), %esp
    jmp     *20(%edx)                   # restore "eip"
