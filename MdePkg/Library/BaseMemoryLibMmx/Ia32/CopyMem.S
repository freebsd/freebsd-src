#------------------------------------------------------------------------------
#
# Copyright (c) 2006, Intel Corporation. All rights reserved.<BR>
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
#   CopyMem.asm
#
# Abstract:
#
#   CopyMem function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalMemCopyMem)

#------------------------------------------------------------------------------
#  VOID *
#  EFIAPI
#  InternalMemCopyMem (
#    IN VOID   *Destination,
#    IN VOID   *Source,
#    IN UINTN  Count
#    );
#------------------------------------------------------------------------------
ASM_PFX(InternalMemCopyMem):
    push    %esi
    push    %edi
    movl    16(%esp), %esi              # esi <- Source
    movl    12(%esp), %edi              # edi <- Destination
    movl    20(%esp), %edx              # edx <- Count
    leal    -1(%esi,%edx,), %eax        # eax <- End of Source
    cmpl    %edi, %esi
    jae     L0
    cmpl    %edi, %eax                  # Overlapped?
    jae     L_CopyBackward              # Copy backward if overlapped
L0:
    xorl    %ecx, %ecx
    subl    %esi, %ecx
    andl    $7, %ecx                    # ecx + esi aligns on 8-byte boundary
    jz      L1
    cmpl    %edx, %ecx
    cmova   %edx, %ecx
    subl    %ecx, %edx                  # edx <- remaining bytes to copy
    rep
    movsb
L1:
    movl    %edx, %ecx
    andl    $7, %edx
    shrl    $3, %ecx                    # ecx <- # of Qwords to copy
    jz      L_CopyBytes
    pushl   %eax
    pushl   %eax
    movq    %mm0, (%esp)                # save mm0
L2:
    movq    (%esi), %mm0
    movq    %mm0, (%edi)
    addl    $8, %esi
    addl    $8, %edi
    loop    L2
    movq    (%esp), %mm0                # restore mm0
    popl    %ecx                        # stack cleanup
    popl    %ecx                        # stack cleanup
    jmp     L_CopyBytes
L_CopyBackward:
    movl    %eax, %esi                  # esi <- Last byte in Source
    leal    -1(%edi,%edx,), %edi        # edi <- Last byte in Destination
    std
L_CopyBytes:
    movl    %edx, %ecx
    rep
    movsb
    cld
    movl    12(%esp), %eax
    pop     %edi
    pop     %esi
    ret
