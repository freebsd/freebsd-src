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

/* Used to generate mcontext_t offsets */

#include <sys/types.h>
#include <sys/assym.h>
#include <sys/ucontext.h>

#include <stddef.h>

ASSYM(_MC_VERSION, _MC_VERSION);
ASSYM(_MC_VERSION_KSE, _MC_VERSION_KSE);
ASSYM(_MC_FP_VALID, _MC_FP_VALID);

ASSYM(_MC_VERS, offsetof(mcontext_t, mc_vers));
ASSYM(_MC_FLAGS, offsetof(mcontext_t, mc_flags));

ASSYM(_MC_R0,  offsetof(mcontext_t, mc_frame[0]));
ASSYM(_MC_R1,  offsetof(mcontext_t, mc_frame[1]));
ASSYM(_MC_R2,  offsetof(mcontext_t, mc_frame[2]));
ASSYM(_MC_R3,  offsetof(mcontext_t, mc_frame[3]));
ASSYM(_MC_R4,  offsetof(mcontext_t, mc_frame[4]));
ASSYM(_MC_R5,  offsetof(mcontext_t, mc_frame[5]));
ASSYM(_MC_R6,  offsetof(mcontext_t, mc_frame[6]));
ASSYM(_MC_R7,  offsetof(mcontext_t, mc_frame[7]));
ASSYM(_MC_R8,  offsetof(mcontext_t, mc_frame[8]));
ASSYM(_MC_R9,  offsetof(mcontext_t, mc_frame[9]));
ASSYM(_MC_R10, offsetof(mcontext_t, mc_frame[10]));
ASSYM(_MC_R11, offsetof(mcontext_t, mc_frame[11]));
ASSYM(_MC_R12, offsetof(mcontext_t, mc_frame[12]));
ASSYM(_MC_R13, offsetof(mcontext_t, mc_frame[13]));
ASSYM(_MC_R14, offsetof(mcontext_t, mc_frame[14]));
ASSYM(_MC_R15, offsetof(mcontext_t, mc_frame[15]));
ASSYM(_MC_R16, offsetof(mcontext_t, mc_frame[16]));
ASSYM(_MC_R17, offsetof(mcontext_t, mc_frame[17]));
ASSYM(_MC_R18, offsetof(mcontext_t, mc_frame[18]));
ASSYM(_MC_R19, offsetof(mcontext_t, mc_frame[19]));
ASSYM(_MC_R20, offsetof(mcontext_t, mc_frame[20]));
ASSYM(_MC_R21, offsetof(mcontext_t, mc_frame[21]));
ASSYM(_MC_R22, offsetof(mcontext_t, mc_frame[22]));
ASSYM(_MC_R23, offsetof(mcontext_t, mc_frame[23]));
ASSYM(_MC_R24, offsetof(mcontext_t, mc_frame[24]));
ASSYM(_MC_R25, offsetof(mcontext_t, mc_frame[25]));
ASSYM(_MC_R26, offsetof(mcontext_t, mc_frame[26]));
ASSYM(_MC_R27, offsetof(mcontext_t, mc_frame[27]));
ASSYM(_MC_R28, offsetof(mcontext_t, mc_frame[28]));
ASSYM(_MC_R29, offsetof(mcontext_t, mc_frame[29]));
ASSYM(_MC_R30, offsetof(mcontext_t, mc_frame[30]));
ASSYM(_MC_R31, offsetof(mcontext_t, mc_frame[31]));
ASSYM(_MC_LR,  offsetof(mcontext_t, mc_frame[32]));
ASSYM(_MC_CR,  offsetof(mcontext_t, mc_frame[33]));
ASSYM(_MC_XER, offsetof(mcontext_t, mc_frame[34]));
ASSYM(_MC_CTR, offsetof(mcontext_t, mc_frame[35]));

ASSYM(_MC_FPSCR, offsetof(mcontext_t, mc_fpreg[32]));
ASSYM(_MC_F0,  offsetof(mcontext_t, mc_fpreg[0]));
ASSYM(_MC_F1,  offsetof(mcontext_t, mc_fpreg[1]));
ASSYM(_MC_F2,  offsetof(mcontext_t, mc_fpreg[2]));
ASSYM(_MC_F3,  offsetof(mcontext_t, mc_fpreg[3]));
ASSYM(_MC_F4,  offsetof(mcontext_t, mc_fpreg[4]));
ASSYM(_MC_F5,  offsetof(mcontext_t, mc_fpreg[5]));
ASSYM(_MC_F6,  offsetof(mcontext_t, mc_fpreg[6]));
ASSYM(_MC_F7,  offsetof(mcontext_t, mc_fpreg[7]));
ASSYM(_MC_F8,  offsetof(mcontext_t, mc_fpreg[8]));
ASSYM(_MC_F9,  offsetof(mcontext_t, mc_fpreg[9]));
ASSYM(_MC_F10, offsetof(mcontext_t, mc_fpreg[10]));
ASSYM(_MC_F11, offsetof(mcontext_t, mc_fpreg[11]));
ASSYM(_MC_F12, offsetof(mcontext_t, mc_fpreg[12]));
ASSYM(_MC_F13, offsetof(mcontext_t, mc_fpreg[13]));
ASSYM(_MC_F14, offsetof(mcontext_t, mc_fpreg[14]));
ASSYM(_MC_F15, offsetof(mcontext_t, mc_fpreg[15]));
ASSYM(_MC_F16, offsetof(mcontext_t, mc_fpreg[16]));
ASSYM(_MC_F17, offsetof(mcontext_t, mc_fpreg[17]));
ASSYM(_MC_F18, offsetof(mcontext_t, mc_fpreg[18]));
ASSYM(_MC_F19, offsetof(mcontext_t, mc_fpreg[19]));
ASSYM(_MC_F20, offsetof(mcontext_t, mc_fpreg[20]));
ASSYM(_MC_F21, offsetof(mcontext_t, mc_fpreg[21]));
ASSYM(_MC_F22, offsetof(mcontext_t, mc_fpreg[22]));
ASSYM(_MC_F23, offsetof(mcontext_t, mc_fpreg[23]));
ASSYM(_MC_F24, offsetof(mcontext_t, mc_fpreg[24]));
ASSYM(_MC_F25, offsetof(mcontext_t, mc_fpreg[25]));
ASSYM(_MC_F26, offsetof(mcontext_t, mc_fpreg[26]));
ASSYM(_MC_F27, offsetof(mcontext_t, mc_fpreg[27]));
ASSYM(_MC_F28, offsetof(mcontext_t, mc_fpreg[28]));
ASSYM(_MC_F29, offsetof(mcontext_t, mc_fpreg[29]));
ASSYM(_MC_F30, offsetof(mcontext_t, mc_fpreg[30]));
ASSYM(_MC_F31, offsetof(mcontext_t, mc_fpreg[31]));
