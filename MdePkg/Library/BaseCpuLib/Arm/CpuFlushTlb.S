#------------------------------------------------------------------------------ 
#
# CpuFlushTlb() for ARM
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#------------------------------------------------------------------------------

.text
.p2align 2
GCC_ASM_EXPORT(CpuFlushTlb)

#/**
#  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.
#
#  Flushes all the Translation Lookaside Buffers(TLB) entries in a CPU.
#
#**/
#VOID
#EFIAPI
#CpuFlushTlb (
#  VOID
#  )#
#
ASM_PFX(CpuFlushTlb):
    mov r0,#0
    mcr p15,0,r0,c8,c5,0        // Invalidate all the unlocked entried in TLB
    bx  LR
