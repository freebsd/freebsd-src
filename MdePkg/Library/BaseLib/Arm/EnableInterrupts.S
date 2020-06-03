#------------------------------------------------------------------------------
#
# EnableInterrupts() for ARM
#
# Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.p2align 2
GCC_ASM_EXPORT(EnableInterrupts)


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
    mrs  R0,CPSR
    bic  R0,R0,#0x80    @Enable IRQ interrupts
    msr  CPSR_c,R0
    bx   LR
