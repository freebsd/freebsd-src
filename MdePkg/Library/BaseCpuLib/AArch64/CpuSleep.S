#------------------------------------------------------------------------------
#
# CpuSleep() for AArch64
#
# Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
# Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
# Portions copyright (c) 2011 - 2013, ARM LTD. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.align 3
GCC_ASM_EXPORT(CpuSleep)

#/**
#  Places the CPU in a sleep state until an interrupt is received.
#
#  Places the CPU in a sleep state until an interrupt is received. If interrupts
#  are disabled prior to calling this function, then the CPU will be placed in a
#  sleep state indefinitely.
#
#**/
#VOID
#EFIAPI
#CpuSleep (
#  VOID
#  );
#

ASM_PFX(CpuSleep):
    AARCH64_BTI(c)
    wfi
    ret
