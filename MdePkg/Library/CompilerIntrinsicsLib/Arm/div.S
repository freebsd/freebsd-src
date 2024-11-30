#------------------------------------------------------------------------------
#
# Copyright (c) 2011, ARM. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.align 2
GCC_ASM_EXPORT(__aeabi_uidiv)
GCC_ASM_EXPORT(__aeabi_uidivmod)
GCC_ASM_EXPORT(__aeabi_idiv)
GCC_ASM_EXPORT(__aeabi_idivmod)

#    AREA  Math, CODE, READONLY

#
#UINT32
#EFIAPI
#__aeabi_uidivmode (
#  IN UINT32  Dividen
#  IN UINT32  Divisor
#  );
#

ASM_PFX(__aeabi_uidiv):
ASM_PFX(__aeabi_uidivmod):
  rsbs    r12, r1, r0, LSR #4
  mov     r2, #0
  bcc     ASM_PFX(__arm_div4)
  rsbs    r12, r1, r0, LSR #8
  bcc     ASM_PFX(__arm_div8)
  mov     r3, #0
  b       ASM_PFX(__arm_div_large)

#
#INT32
#EFIAPI
#__aeabi_idivmode (
#  IN INT32  Dividen
#  IN INT32  Divisor
#  );
#
ASM_PFX(__aeabi_idiv):
ASM_PFX(__aeabi_idivmod):
  orrs    r12, r0, r1
  bmi     ASM_PFX(__arm_div_negative)
  rsbs    r12, r1, r0, LSR #1
  mov     r2, #0
  bcc     ASM_PFX(__arm_div1)
  rsbs    r12, r1, r0, LSR #4
  bcc     ASM_PFX(__arm_div4)
  rsbs    r12, r1, r0, LSR #8
  bcc     ASM_PFX(__arm_div8)
  mov     r3, #0
  b       ASM_PFX(__arm_div_large)
ASM_PFX(__arm_div8):
  rsbs    r12, r1, r0, LSR #7
  subcs   r0, r0, r1, LSL #7
  adc     r2, r2, r2
  rsbs    r12, r1, r0,LSR #6
  subcs   r0, r0, r1, LSL #6
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #5
  subcs   r0, r0, r1, LSL #5
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #4
  subcs   r0, r0, r1, LSL #4
  adc     r2, r2, r2
ASM_PFX(__arm_div4):
  rsbs    r12, r1, r0, LSR #3
  subcs   r0, r0, r1, LSL #3
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #2
  subcs   r0, r0, r1, LSL #2
  adcs    r2, r2, r2
  rsbs    r12, r1, r0, LSR #1
  subcs   r0, r0, r1, LSL #1
  adc     r2, r2, r2
ASM_PFX(__arm_div1):
  subs    r1, r0, r1
  movcc   r1, r0
  adc     r0, r2, r2
  bx      r14
ASM_PFX(__arm_div_negative):
  ands    r2, r1, #0x80000000
  rsbmi   r1, r1, #0
  eors    r3, r2, r0, ASR #32
  rsbcs   r0, r0, #0
  rsbs    r12, r1, r0, LSR #4
  bcc     label1
  rsbs    r12, r1, r0, LSR #8
  bcc     label2
ASM_PFX(__arm_div_large):
  lsl     r1, r1, #6
  rsbs    r12, r1, r0, LSR #8
  orr     r2, r2, #0xfc000000
  bcc     label2
  lsl     r1, r1, #6
  rsbs    r12, r1, r0, LSR #8
  orr     r2, r2, #0x3f00000
  bcc     label2
  lsl     r1, r1, #6
  rsbs    r12, r1, r0, LSR #8
  orr     r2, r2, #0xfc000
  orrcs   r2, r2, #0x3f00
  lslcs   r1, r1, #6
  rsbs    r12, r1, #0
  bcs     ASM_PFX(__aeabi_idiv0)
label3:
  lsrcs   r1, r1, #6
label2:
  rsbs    r12, r1, r0, LSR #7
  subcs   r0, r0, r1, LSL #7
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #6
  subcs   r0, r0, r1, LSL #6
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #5
  subcs   r0, r0, r1, LSL #5
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #4
  subcs   r0, r0, r1, LSL #4
  adc     r2, r2, r2
label1:
  rsbs    r12, r1, r0, LSR #3
  subcs   r0, r0, r1, LSL #3
  adc     r2, r2, r2
  rsbs    r12, r1, r0, LSR #2
  subcs   r0, r0, r1, LSL #2
  adcs    r2, r2, r2
  bcs     label3
  rsbs    r12, r1, r0, LSR #1
  subcs   r0, r0, r1, LSL #1
  adc     r2, r2, r2
  subs    r1, r0, r1
  movcc   r1, r0
  adc     r0, r2, r2
  asrs    r3, r3, #31
  rsbmi   r0, r0, #0
  rsbcs   r1, r1, #0
  bx      r14

  @ What to do about division by zero?  For now, just return.
ASM_PFX(__aeabi_idiv0):
  bx      r14
