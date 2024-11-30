#------s------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2010, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------


       .text
       .align 2
       GCC_ASM_EXPORT(__aeabi_ulcmp)

ASM_PFX(__aeabi_ulcmp):
       stmfd   sp!, {r4, r5, r8}
       cmp     r3, r1
       mov     r8, r0
       mov     r9, r1
       mov     r4, r2
       mov     r5, r3
       bls     L16
L2:
       mvn     r0, #0
L1:
       ldmfd   sp!, {r4, r5, r8}
       bx      lr
L16:
       beq     L17
L4:
       cmp     r9, r5
       bhi     L7
       beq     L18
       cmp     r8, r4
L14:
       cmpeq   r9, r5
       moveq   r0, #0
       beq     L1
       b       L1
L18:
       cmp     r8, r4
       bls     L14
L7:
       mov     r0, #1
       b       L1
L17:
       cmp     r2, r0
       bhi     L2
       b       L4

