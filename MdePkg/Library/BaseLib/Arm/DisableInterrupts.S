#------------------------------------------------------------------------------ 
#
# DisableInterrupts() for ARM
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
GCC_ASM_EXPORT(DisableInterrupts)

#/**
#  Disables CPU interrupts.
#
#**/
#VOID
#EFIAPI
#DisableInterrupts (
#  VOID
#  );
#
ASM_PFX(DisableInterrupts):
    mrs  R0,CPSR
    orr  R0,R0,#0x80    @Disable IRQ interrupts
    msr  CPSR_c,R0
    bx   LR
