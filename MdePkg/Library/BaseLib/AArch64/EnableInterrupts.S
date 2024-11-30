#------------------------------------------------------------------------------
#
# EnableInterrupts() for AArch64
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# Portions copyright (c) 2011 - 2013, ARM Ltd. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.p2align 2
GCC_ASM_EXPORT(EnableInterrupts)

.set DAIF_WR_IRQ_BIT,   (1 << 1)

#/**
#  Enables CPU interrupts.
#
#**/
#VOID
#EFIAPI
#EnableInterrupts (
#  VOID
#  );
#
ASM_PFX(EnableInterrupts):
    AARCH64_BTI(c)
    msr  daifclr, #DAIF_WR_IRQ_BIT
    ret
