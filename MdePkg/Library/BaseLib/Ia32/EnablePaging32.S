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
#   EnablePaging32.S
#
# Abstract:
#
#   InternalX86EnablePaging32 function
#
# Notes:
#
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalX86EnablePaging32)

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# InternalX86EnablePaging32 (
#   IN      SWITCH_STACK_ENTRY_POINT  EntryPoint,
#   IN      VOID                      *Context1,    OPTIONAL
#   IN      VOID                      *Context2,    OPTIONAL
#   IN      VOID                      *NewStack
#   );
#------------------------------------------------------------------------------
ASM_PFX(InternalX86EnablePaging32):
    movl    4(%esp), %ebx
    movl    8(%esp), %ecx
    movl    12(%esp), %edx
    pushfl
    pop     %edi                        # save flags in edi
    cli
    movl    %cr0, %eax
    btsl    $31, %eax
    movl    16(%esp), %esp
    movl    %eax, %cr0
    push    %edi
    popfl                               # restore flags
    push    %edx
    push    %ecx
    call    *%ebx
    jmp     .
