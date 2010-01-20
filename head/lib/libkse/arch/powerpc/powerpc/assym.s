/*
 * Copyright (c) 2004 Peter Grehan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Struct offsets for version 0x1 of the mcontext struct.
 * Generated with
 *   cc -c assym.c
 *   ${SYSSRC}/kern/genassym.sh assym.o > assym_syms.s
 *   hand-edit output
 */
#define _MC_VERSION     0x1
#define _MC_VERSION_KSE 0xee
#define _MC_FP_VALID    0x1

#define _MC_VERS        0x0
#define _MC_FLAGS       0x4

#define _MC_R0	0x298
#define _MC_R1  0x21c
#define _MC_R2  0x220
#define _MC_R3  0x224
#define _MC_R4  0x228
#define _MC_R5  0x22c
#define _MC_R6  0x230
#define _MC_R7  0x234
#define _MC_R8  0x238
#define _MC_R9  0x23c
#define _MC_R10 0x240
#define _MC_R11 0x244
#define _MC_R12 0x248
#define _MC_R13 0x24c
#define _MC_R14 0x250
#define _MC_R15 0x254
#define _MC_R16 0x258
#define _MC_R17 0x25c
#define _MC_R18 0x260
#define _MC_R19 0x264
#define _MC_R20 0x268
#define _MC_R21 0x26c
#define _MC_R22 0x270
#define _MC_R23 0x274
#define _MC_R24 0x278
#define _MC_R25 0x27c
#define _MC_R26 0x280
#define _MC_R27 0x284
#define _MC_R28 0x288
#define _MC_R29 0x28c
#define _MC_R30 0x290
#define _MC_R31 0x294
#define _MC_LR  0x298
#define _MC_CR  0x29c
#define _MC_XER 0x2a0
#define _MC_CTR 0x2a4

#define _MC_FPSCR       0x3c0
#define _MC_F0  0x2c0
#define _MC_F1  0x2c8
#define _MC_F2  0x2d0
#define _MC_F3  0x2d8
#define _MC_F4  0x2e0
#define _MC_F5  0x2e8
#define _MC_F6  0x2f0
#define _MC_F7  0x2f8
#define _MC_F8  0x300
#define _MC_F9  0x308
#define _MC_F10 0x310
#define _MC_F11 0x318
#define _MC_F12 0x320
#define _MC_F13 0x328
#define _MC_F14 0x330
#define _MC_F15 0x338
#define _MC_F16 0x340
#define _MC_F17 0x348
#define _MC_F18 0x350
#define _MC_F19 0x358
#define _MC_F20 0x360
#define _MC_F21 0x368
#define _MC_F22 0x370
#define _MC_F23 0x378
#define _MC_F24 0x380
#define _MC_F25 0x388
#define _MC_F26 0x390
#define _MC_F27 0x398
#define _MC_F28 0x3a0
#define _MC_F29 0x3a8
#define _MC_F30 0x3b0
#define _MC_F31 0x3b8

