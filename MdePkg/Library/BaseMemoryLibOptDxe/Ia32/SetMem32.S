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
#   SetMem32.S
#
# Abstract:
#
#   SetMem32 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemSetMem32)

#------------------------------------------------------------------------------
#  VOID *
#  InternalMemSetMem32 (
#    IN VOID   *Buffer,
#    IN UINTN  Count,
#    IN UINT32 Value
#    )
#------------------------------------------------------------------------------
ASM_PFX(InternalMemSetMem32):
    push    %edi
    movl    16(%esp),%eax
    movl    8(%esp),%edi
    movl    12(%esp),%ecx
    rep
    stosl
    movl    8(%esp),%eax
    pop     %edi
    ret
