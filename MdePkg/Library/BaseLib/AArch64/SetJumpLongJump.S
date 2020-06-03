#------------------------------------------------------------------------------
#
# Copyright (c) 2009-2013, ARM Ltd.  All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------
.text
.p2align 3

GCC_ASM_EXPORT(SetJump)
GCC_ASM_EXPORT(InternalLongJump)

#define GPR_LAYOUT                         \
        REG_PAIR (x19, x20,  0);           \
        REG_PAIR (x21, x22, 16);           \
        REG_PAIR (x23, x24, 32);           \
        REG_PAIR (x25, x26, 48);           \
        REG_PAIR (x27, x28, 64);           \
        REG_PAIR (x29, x30, 80);/*FP, LR*/ \
        REG_ONE  (x16,      96) /*IP0*/

#define FPR_LAYOUT                      \
        REG_PAIR ( d8,  d9, 112);       \
        REG_PAIR (d10, d11, 128);       \
        REG_PAIR (d12, d13, 144);       \
        REG_PAIR (d14, d15, 160);

#/**
#  Saves the current CPU context that can be restored with a call to LongJump() and returns 0.#
#
#  Saves the current CPU context in the buffer specified by JumpBuffer and returns 0.  The initial
#  call to SetJump() must always return 0.  Subsequent calls to LongJump() cause a non-zero
#  value to be returned by SetJump().
#
#  If JumpBuffer is NULL, then ASSERT().
#  For IPF CPUs, if JumpBuffer is not aligned on a 16-byte boundary, then ASSERT().
#
#  @param  JumpBuffer    A pointer to CPU context buffer.
#
#**/
#
#UINTN
#EFIAPI
#SetJump (
#  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer  // X0
#  );
#
ASM_PFX(SetJump):
        mov     x16, sp // use IP0 so save SP
#define REG_PAIR(REG1, REG2, OFFS)      stp REG1, REG2, [x0, OFFS]
#define REG_ONE(REG1, OFFS)             str REG1, [x0, OFFS]
        GPR_LAYOUT
        FPR_LAYOUT
#undef REG_PAIR
#undef REG_ONE
        mov     w0, #0
        ret

#/**
#  Restores the CPU context that was saved with SetJump().#
#
#  Restores the CPU context from the buffer specified by JumpBuffer.
#  This function never returns to the caller.
#  Instead is resumes execution based on the state of JumpBuffer.
#
#  @param  JumpBuffer    A pointer to CPU context buffer.
#  @param  Value         The value to return when the SetJump() context is restored.
#
#**/
#VOID
#EFIAPI
#InternalLongJump (
#  IN      BASE_LIBRARY_JUMP_BUFFER  *JumpBuffer,  // X0
#  IN      UINTN                     Value         // X1
#  );
#
ASM_PFX(InternalLongJump):
#define REG_PAIR(REG1, REG2, OFFS)      ldp REG1, REG2, [x0, OFFS]
#define REG_ONE(REG1, OFFS)             ldr REG1, [x0, OFFS]
        GPR_LAYOUT
        FPR_LAYOUT
#undef REG_PAIR
#undef REG_ONE
        mov     sp, x16
        cmp     w1, #0
        mov     w0, #1
        csel    w0, w1, w0, ne
        // use br not ret, as ret is guaranteed to mispredict
        br      x30

ASM_FUNCTION_REMOVE_IF_UNREFERENCED
