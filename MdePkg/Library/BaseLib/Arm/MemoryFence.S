##------------------------------------------------------------------------------
#
# MemoryFence() for AArch64
#
# Copyright (c) 2013, ARM Ltd. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##------------------------------------------------------------------------------

.text
.p2align 2

GCC_ASM_EXPORT(MemoryFence)


#/**
#  Used to serialize load and store operations.
#
#  All loads and stores that proceed calls to this function are guaranteed to be
#  globally visible when this function returns.
#
#**/
#VOID
#EFIAPI
#MemoryFence (
#  VOID
#  );
#
ASM_PFX(MemoryFence):
    // System wide Data Memory Barrier.
    dmb
    bx   lr
