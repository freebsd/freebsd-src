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
#   MultU64x64.S
#
# Abstract:
#
#   Calculate the product of a 64-bit integer and another 64-bit integer
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMathMultU64x64)

#------------------------------------------------------------------------------
# UINT64
# EFIAPI
# InternalMathMultU64x64 (
#   IN      UINT64                    Multiplicand,
#   IN      UINT64                    Multiplier
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMathMultU64x64):
    push    %ebx                                     
    movl    8(%esp), %ebx             # ebx <- M1[0..31]                
    movl    16(%esp), %edx            # edx <- M2[0..31]                                   
    movl    %ebx, %ecx                                                  
    movl    %edx, %eax                
    imull   20(%esp), %ebx            # ebx <- M1[0..31] * M2[32..63]   
    imull   12(%esp), %edx            # edx <- M1[32..63] * M2[0..31]   
    addl    %edx, %ebx                # carries are abandoned           
    mull    %ecx                      # edx:eax <- M1[0..31] * M2[0..31]
    addl    %ebx, %edx                # carries are abandoned           
    pop     %ebx
    ret
