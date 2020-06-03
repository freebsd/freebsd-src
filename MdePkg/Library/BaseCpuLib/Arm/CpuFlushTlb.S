#------------------------------------------------------------------------------
#
# CpuFlushTlb() for ARM
#
# Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
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
