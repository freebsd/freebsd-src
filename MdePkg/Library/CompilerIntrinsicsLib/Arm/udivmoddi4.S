#------------------------------------------------------------------------------
#
# Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

#include <AsmMacroLib.h>

  .syntax unified

ASM_FUNC(__udivmoddi4)
  stmfd  sp!, {r4, r5, r6, r7, lr}
  add  r7, sp, #12
  stmfd  sp!, {r10, r11}
  sub  sp, sp, #20
  stmia  sp, {r2-r3}
  ldr  r6, [sp, #48]
  orrs  r2, r2, r3
  mov  r10, r0
  mov  r11, r1
  beq  L2
  subs  ip, r1, #0
  bne  L4
  cmp  r3, #0
  bne  L6
  cmp  r6, #0
  beq  L8
  mov  r1, r2
  bl  ASM_PFX(__umodsi3)
  mov  r1, #0
  stmia  r6, {r0-r1}
L8:
  ldr  r1, [sp, #0]
  mov  r0, r10
  b  L45
L6:
  cmp  r6, #0
  movne  r1, #0
  stmiane  r6, {r0-r1}
  b  L2
L4:
  ldr  r1, [sp, #0]
  cmp  r1, #0
  bne  L12
  ldr  r2, [sp, #4]
  cmp  r2, #0
  bne  L14
  cmp  r6, #0
  beq  L16
  mov  r1, r2
  mov  r0, r11
  bl  ASM_PFX(__umodsi3)
  mov  r1, #0
  stmia  r6, {r0-r1}
L16:
  ldr  r1, [sp, #4]
  mov  r0, r11
L45:
  bl  ASM_PFX(__udivsi3)
L46:
  mov  r10, r0
  mov  r11, #0
  b  L10
L14:
  subs  r1, r0, #0
  bne  L18
  cmp  r6, #0
  beq  L16
  ldr  r1, [sp, #4]
  mov  r0, r11
  bl  ASM_PFX(__umodsi3)
  mov  r4, r10
  mov  r5, r0
  stmia  r6, {r4-r5}
  b  L16
L18:
  sub  r3, r2, #1
  tst  r2, r3
  bne  L22
  cmp  r6, #0
  movne  r4, r0
  andne  r5, ip, r3
  stmiane  r6, {r4-r5}
L24:
  rsb  r3, r2, #0
  and  r3, r2, r3
  clz  r3, r3
  rsb  r3, r3, #31
  mov  r0, ip, lsr r3
  b  L46
L22:
  clz  r2, r2
  clz  r3, ip
  rsb  r3, r3, r2
  cmp  r3, #30
  bhi  L48
  rsb  r2, r3, #31
  add  lr, r3, #1
  mov  r3, r1, asl r2
  str  r3, [sp, #12]
  mov  r3, r1, lsr lr
  ldr  r0, [sp, #0]
  mov  r5, ip, lsr lr
  orr  r4, r3, ip, asl r2
  str  r0, [sp, #8]
  b  L29
L12:
  ldr  r3, [sp, #4]
  cmp  r3, #0
  bne  L30
  sub  r3, r1, #1
  tst  r1, r3
  bne  L32
  cmp  r6, #0
  andne  r3, r3, r0
  movne  r2, r3
  movne  r3, #0
  stmiane  r6, {r2-r3}
L34:
  cmp  r1, #1
  beq  L10
  rsb  r3, r1, #0
  and  r3, r1, r3
  clz  r3, r3
  rsb  r0, r3, #31
  mov  r1, ip, lsr r0
  rsb  r3, r0, #32
  mov  r0, r10, lsr r0
  orr  ip, r0, ip, asl r3
  str  r1, [sp, #12]
  str  ip, [sp, #8]
  ldrd  r10, [sp, #8]
  b  L10
L32:
  clz  r2, r1
  clz  r3, ip
  rsb  r3, r3, r2
  rsb  r4, r3, #31
  mov  r2, r0, asl r4
  mvn  r1, r3
  and  r2, r2, r1, asr #31
  add  lr, r3, #33
  str  r2, [sp, #8]
  add  r2, r3, #1
  mov  r3, r3, asr #31
  and  r0, r3, r0, asl r1
  mov  r3, r10, lsr r2
  orr  r3, r3, ip, asl r4
  and  r3, r3, r1, asr #31
  orr  r0, r0, r3
  mov  r3, ip, lsr lr
  str  r0, [sp, #12]
  mov  r0, r10, lsr lr
  and  r5, r3, r2, asr #31
  rsb  r3, lr, #31
  mov  r3, r3, asr #31
  orr  r0, r0, ip, asl r1
  and  r3, r3, ip, lsr r2
  and  r0, r0, r2, asr #31
  orr  r4, r3, r0
  b  L29
L30:
  clz  r2, r3
  clz  r3, ip
  rsb  r3, r3, r2
  cmp  r3, #31
  bls  L37
L48:
  cmp  r6, #0
  stmiane  r6, {r10-r11}
  b  L2
L37:
  rsb  r1, r3, #31
  mov  r0, r0, asl r1
  add  lr, r3, #1
  mov  r2, #0
  str  r0, [sp, #12]
  mov  r0, r10, lsr lr
  str  r2, [sp, #8]
  sub  r2, r3, #31
  and  r0, r0, r2, asr #31
  mov  r3, ip, lsr lr
  orr  r4, r0, ip, asl r1
  and  r5, r3, r2, asr #31
L29:
  mov  ip, #0
  mov  r10, ip
  b  L40
L41:
  ldr  r1, [sp, #12]
  ldr  r2, [sp, #8]
  mov  r3, r4, lsr #31
  orr  r5, r3, r5, asl #1
  mov  r3, r1, lsr #31
  orr  r4, r3, r4, asl #1
  mov  r3, r2, lsr #31
  orr  r0, r3, r1, asl #1
  orr  r1, ip, r2, asl #1
  ldmia  sp, {r2-r3}
  str  r0, [sp, #12]
  subs  r2, r2, r4
  sbc  r3, r3, r5
  str  r1, [sp, #8]
  subs  r0, r2, #1
  sbc  r1, r3, #0
  mov  r2, r1, asr #31
  ldmia  sp, {r0-r1}
  mov  r3, r2
  and  ip, r2, #1
  and  r3, r3, r1
  and  r2, r2, r0
  subs  r4, r4, r2
  sbc  r5, r5, r3
  add  r10, r10, #1
L40:
  cmp  r10, lr
  bne  L41
  ldrd  r0, [sp, #8]
  adds  r0, r0, r0
  adc  r1, r1, r1
  cmp  r6, #0
  orr  r10, r0, ip
  mov  r11, r1
  stmiane  r6, {r4-r5}
  b  L10
L2:
  mov  r10, #0
  mov  r11, #0
L10:
  mov  r0, r10
  mov  r1, r11
  sub  sp, r7, #20
  ldmfd  sp!, {r10, r11}
  ldmfd  sp!, {r4, r5, r6, r7, pc}
