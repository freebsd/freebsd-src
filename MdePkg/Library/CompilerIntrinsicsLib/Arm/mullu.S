#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------
.text

GCC_ASM_EXPORT(__ARM_ll_mullu)
GCC_ASM_EXPORT(__aeabi_lmul)
#
#INT64
#EFIAPI
#__aeabi_lmul (
#  IN INT64   Multiplicand
#  IN INT32   Multiplier
#  );
#
ASM_PFX(__ARM_ll_mullu):
  mov     r3, #0
# Make upper part of INT64 Multiplier 0 and use __aeabi_lmul

#
#INT64
#EFIAPI
#__aeabi_lmul (
#  IN INT64   Multiplicand
#  IN INT64   Multiplier
#  );
#
ASM_PFX(__aeabi_lmul):
  stmdb   sp!, {lr}
  mov     lr, r0
  umull   r0, ip, r2, lr
  mla     r1, r2, r1, ip
  mla     r1, r3, lr, r1
  ldmia   sp!, {pc}
