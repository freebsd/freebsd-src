#------------------------------------------------------------------------------
#
# CpuFlushTlb() for ARM
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# Portions copyright (c) 2011 - 2013, ARM Ltd. All rights reserved.<BR>
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
  AARCH64_BTI(c)
  tlbi  vmalle1                 // Invalidate Inst TLB and Data TLB
  dsb   sy
  isb
  ret
