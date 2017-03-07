#------------------------------------------------------------------------------
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
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
#   DisablePaging64.S
#
# Abstract:
#
#   AsmDisablePaging64 function
#
# Notes:
#
#------------------------------------------------------------------------------

    

#------------------------------------------------------------------------------
# VOID
# EFIAPI
# InternalX86DisablePaging64 (
#   IN      UINT16                    Cs,
#   IN      UINT32                    EntryPoint,
#   IN      UINT32                    Context1,  OPTIONAL
#   IN      UINT32                    Context2,  OPTIONAL
#   IN      UINT32                    NewStack
#   );
#------------------------------------------------------------------------------

ASM_GLOBAL ASM_PFX(InternalX86DisablePaging64)
ASM_PFX(InternalX86DisablePaging64):
    cli    
    lea    L1(%rip), %rsi                 # rsi <- The start address of transition code
    mov    0x28(%rsp), %edi               # rdi <- New stack
    lea    _mTransitionEnd(%rip), %rax    # rax <- end of transition code
    sub    %rsi, %rax                     # rax <- The size of transition piece code
    add    $4, %rax                       # round rax up to the next 4 byte boundary
    and    $0xfc, %al
    sub    %rax, %rdi                     # rdi <- use stack to hold transition code 
    mov    %edi, %r10d                    # r10 <- The start address of transicition code below 4G
    push   %rcx                           # save rcx to stack
    mov    %rax, %rcx                     # rcx <- The size of transition piece code
    rep
    movsb                                 # copy transition code to (new stack - 64byte) below 4G
    pop    %rcx                           # restore rcx
   
    mov    %r8d, %esi 
    mov    %r9d, %edi 
    mov    %r10d, %eax
    sub    $4, %eax
    push   %rcx                           # push Cs to stack
    push   %r10                           # push address of transition code on stack  
    .byte  0x48, 0xcb                     # retq: Use far return to load CS register from stack
                                          # (Use raw byte code since some GNU assemblers generates incorrect code for "retq")  
L1:
    mov    %eax,%esp                      # set up new stack
    mov    %cr0,%rax
    btr    $0x1f,%eax                     # clear CR0.PG
    mov    %rax,%cr0                      # disable paging

    mov    %edx,%ebx                      # save EntryPoint to ebx, for rdmsr will overwrite edx
    mov    $0xc0000080,%ecx
    rdmsr  
    and    $0xfe,%ah                      # clear LME
    wrmsr  
    mov    %cr4,%rax
    and    $0xdf,%al                      # clear PAE
    mov    %rax,%cr4
    push   %rdi                           # push Context2
    push   %rsi                           # push Context1
    callq  *%rbx                          # transfer control to EntryPoint
    jmp    .                              # no one should get here

_mTransitionEnd :
