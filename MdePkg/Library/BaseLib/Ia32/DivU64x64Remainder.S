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
#   DivU64x64Remainder.S
#
# Abstract:
#
#   Calculate the quotient of a 64-bit integer by a 64-bit integer and returns
#   both the quotient and the remainder
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMathDivRemU64x32), ASM_PFX(InternalMathDivRemU64x64)

#------------------------------------------------------------------------------
# UINT64
# EFIAPI
# InternalMathDivRemU64x64 (
#   IN      UINT64                    Dividend,
#   IN      UINT64                    Divisor,
#   OUT     UINT64                    *Remainder    OPTIONAL
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalMathDivRemU64x64):
    movl    16(%esp), %ecx              # ecx <- divisor[32..63]
    testl   %ecx, %ecx
    jnz     Hard                        # call _@DivRemU64x64 if Divisor > 2^32
    movl    20(%esp), %ecx
    jecxz   L1
    andl     $0, 4(%ecx)                # zero high dword of remainder
    movl    %ecx, 16(%esp)              # set up stack frame to match DivRemU64x32
L1:
    jmp     ASM_PFX(InternalMathDivRemU64x32)
Hard:
    push    %ebx
    push    %esi
    push    %edi
    mov     20(%esp), %edx
    mov     16(%esp), %eax              # edx:eax <- dividend
    movl    %edx, %edi
    movl    %eax, %esi                  # edi:esi <- dividend
    mov     24(%esp), %ebx              # ecx:ebx <- divisor
L2:
    shrl    %edx
    rcrl    $1, %eax
    shrdl   $1, %ecx, %ebx
    shrl    %ecx
    jnz     L2
    divl    %ebx
    movl    %eax, %ebx                  # ebx <- quotient                     
    movl    28(%esp), %ecx              # ecx <- high dword of divisor        
    mull    24(%esp)                    # edx:eax <- quotient * divisor[0..31]
    imull   %ebx, %ecx                  # ecx <- quotient * divisor[32..63]   
    addl    %ecx, %edx                  # edx <- (quotient * divisor)[32..63] 
    mov     32(%esp), %ecx              # ecx <- addr for Remainder           
    jc      TooLarge                    # product > 2^64                      
    cmpl    %edx, %edi                  # compare high 32 bits                
    ja      Correct                                                           
    jb      TooLarge                    # product > dividend                  
    cmpl    %eax, %esi                                                        
    jae     Correct                     # product <= dividend                 
TooLarge:
    decl    %ebx                        # adjust quotient by -1              
    jecxz   Return                      # return if Remainder == NULL        
    sub     24(%esp), %eax                                                   
    sbb     28(%esp), %edx              # edx:eax <- (quotient - 1) * divisor
Correct:
    jecxz   Return
    subl    %eax, %esi
    sbbl    %edx, %edi                  # edi:esi <- remainder
    movl    %esi, (%ecx)
    movl    %edi, 4(%ecx)
Return:
    movl    %ebx, %eax                  # eax <- quotient         
    xorl    %edx, %edx                  # quotient is 32 bits long
    pop     %edi
    pop     %esi
    pop     %ebx
    ret
