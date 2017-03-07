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
#   CopyMem.S
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
    cmpl    %esi, %edi
    je      L_CopyDone
    cmpl    $0, %edx
    je      L_CopyDone
    leal    -1(%esi, %edx), %eax        # eax <- End of Source
    cmpl    %edi, %esi
    jae     L_CopyBytes
    cmpl    %edi, %eax
    jb      L_CopyBytes                 # Copy backward if overlapped
    movl    %eax, %esi                  # esi <- End of Source
    leal    -1(%edi, %edx), %edi        # edi <- End of Destination
    std
L_CopyBytes:
    movl    %edx, %ecx
    rep
    movsb                               # Copy bytes backward
    cld
L_CopyDone:
    movl    12(%esp), %eax              # eax <- Destination as return value
    pop     %edi
    pop     %esi
    ret
