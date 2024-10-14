/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Andrew Turner
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef	_PSCI_SMCCC_H_
#define	_PSCI_SMCCC_H_

#define	SMCCC_VERSION_MAJOR(ver)	(((ver) >> 16) & 0x7fff)
#define	SMCCC_VERSION_MINOR(ver)	((ver) & 0xffff)

#define	SMCCC_FUNC_ID(type, call_conv, range, func)	\
	(((type) << 31) |				\
	 ((call_conv) << 30) |				\
	 (((range) & 0x3f) << 24) |				\
	 ((func) & 0xffff))

#define	SMCCC_YIELDING_CALL	0
#define	SMCCC_FAST_CALL		1

#define	SMCCC_32BIT_CALL	0
#define	SMCCC_64BIT_CALL	1

#define	SMCCC_ARM_ARCH_CALLS		0
#define	SMCCC_CPU_SERVICE_CALLS		1
#define	SMCCC_SIP_SERVICE_CALLS		2
#define	SMCCC_OEM_SERVICE_CALLS		3
#define	SMCCC_STD_SECURE_SERVICE_CALLS	4
#define	SMCCC_STD_HYP_SERVICE_CALLS	5
#define	SMCCC_VENDOR_HYP_SERVICE_CALLS	6

struct arm_smccc_res {
	register_t a0;
	register_t a1;
	register_t a2;
	register_t a3;
};

/*
 * Arm Architecture Calls.
 * These are documented in the document ARM DEN 0070A.
 */
#define	SMCCC_VERSION							\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL, 0, 0)
#define	SMCCC_ARCH_FEATURES						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL, 0, 1)
#define	SMCCC_ARCH_WORKAROUND_1						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL, 0, 0x8000)
#define	SMCCC_ARCH_WORKAROUND_2						\
    SMCCC_FUNC_ID(SMCCC_FAST_CALL, SMCCC_32BIT_CALL, 0, 0x7fff)

/* The return values from ARM DEN 0070A. */
#define	SMCCC_RET_SUCCESS		0
#define	SMCCC_RET_NOT_SUPPORTED		-1
#define	SMCCC_RET_NOT_REQUIRED		-2

void smccc_init(void);
uint32_t smccc_get_version(void);
int32_t smccc_arch_features(uint32_t);
int smccc_arch_workaround_1(void);
int smccc_arch_workaround_2(int);

int arm_smccc_smc(register_t, register_t, register_t, register_t, register_t,
    register_t, register_t, register_t, struct arm_smccc_res *res);
int arm_smccc_hvc(register_t, register_t, register_t, register_t, register_t,
    register_t, register_t, register_t, struct arm_smccc_res *res);

#define	arm_smccc_invoke_1(func, a0, res)				\
    func(a0,  0,  0,  0,  0,  0,  0,  0, res)
#define	arm_smccc_invoke_2(func, a0, a1, res)				\
    func(a0, a1,  0,  0,  0,  0,  0,  0, res)
#define	arm_smccc_invoke_3(func, a0, a1, a2, res)			\
    func(a0, a1, a2,  0,  0,  0,  0,  0, res)
#define	arm_smccc_invoke_4(func, a0, a1, a2, a3, res)			\
    func(a0, a1, a2, a3,  0,  0,  0,  0, res)
#define	arm_smccc_invoke_5(func, a0, a1, a2, a3, a4, res)		\
    func(a0, a1, a2, a3, a4,  0,  0,  0, res)
#define	arm_smccc_invoke_6(func, a0, a1, a2, a3, a4, a5, res)		\
    func(a0, a1, a2, a3, a4, a5,  0,  0, res)
#define	arm_smccc_invoke_7(func, a0, a1, a2, a3, a4, a5, a6, res)	\
    func(a0, a1, a2, a3, a4, a5, a6,  0, res)
#define	arm_smccc_invoke_8(func, a0, a1, a2, a3, a4, a5, a6, a7, res)	\
    func(a0, a1, a2, a3, a4, a5, a6, a7, res)

#define	_arm_smccc_invoke_macro(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) \
    NAME
#define	_arm_smccc_invoke(func, a0, ...)				\
    _arm_smccc_invoke_macro(__VA_ARGS__, arm_smccc_invoke_8,		\
      arm_smccc_invoke_7, arm_smccc_invoke_6, arm_smccc_invoke_5,	\
      arm_smccc_invoke_4, arm_smccc_invoke_3, arm_smccc_invoke_2,	\
      arm_smccc_invoke_1)(func, a0, __VA_ARGS__)

#define	arm_smccc_invoke_hvc(a0, ...)					\
    _arm_smccc_invoke(arm_smccc_hvc, a0, __VA_ARGS__)
#define	arm_smccc_invoke_smc(a0, ...)					\
    _arm_smccc_invoke(arm_smccc_smc, a0, __VA_ARGS__)
#define	arm_smccc_invoke(a0, ...)					\
    _arm_smccc_invoke(psci_callfn, a0, __VA_ARGS__)

struct arm_smccc_1_2_regs {
	register_t a0;
	register_t a1;
	register_t a2;
	register_t a3;
	register_t a4;
	register_t a5;
	register_t a6;
	register_t a7;
	register_t a8;
	register_t a9;
	register_t a10;
	register_t a11;
	register_t a12;
	register_t a13;
	register_t a14;
	register_t a15;
	register_t a16;
	register_t a17;
};

int arm_smccc_1_2_hvc(const struct arm_smccc_1_2_regs *args,
    struct arm_smccc_1_2_regs *res);
int arm_smccc_1_2_smc(const struct arm_smccc_1_2_regs *args,
    struct arm_smccc_1_2_regs *res);
#endif /* _PSCI_SMCCC_H_ */
