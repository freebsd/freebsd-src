##------------------------------------------------------------------------------
#
# SpeculationBarrier() for RISCV64
#
# Copyright (c) 2023, Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##------------------------------------------------------------------------------

.text
.p2align 2

ASM_GLOBAL ASM_PFX(SpeculationBarrier)


#/**
#  Uses as a barrier to stop speculative execution.
#
#  Ensures that no later instruction will execute speculatively, until all prior
#  instructions have completed.
#
#**/
#VOID
#EFIAPI
#SpeculationBarrier (
#  VOID
#  );
#
ASM_PFX(SpeculationBarrier):
    fence rw,rw
    fence.i
    fence r,r
    ret
