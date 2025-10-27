/*-
 * Copyright (c) 2013, 2014 Andrew Turner
 * Copyright (c) 2015,2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation.
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

#if !defined(_MACHINE_ARMREG_H_) && \
    !defined(_MACHINE_CPU_H_) && \
    !defined(_MACHINE_HYPERVISOR_H_)
#error Do not include this file directly
#endif

#ifndef _MACHINE__ARMREG_H_
#define	_MACHINE__ARMREG_H_

#define	__MRS_REG_ALT_NAME(op0, op1, crn, crm, op2)			\
    S##op0##_##op1##_C##crn##_C##crm##_##op2
#define	_MRS_REG_ALT_NAME(op0, op1, crn, crm, op2)			\
    __MRS_REG_ALT_NAME(op0, op1, crn, crm, op2)
#define	MRS_REG_ALT_NAME(reg)						\
    _MRS_REG_ALT_NAME(reg##_op0, reg##_op1, reg##_CRn, reg##_CRm, reg##_op2)


#define	READ_SPECIALREG(reg)						\
({	uint64_t _val;							\
	__asm __volatile("mrs	%0, " __STRING(reg) : "=&r" (_val));	\
	_val;								\
})
#define	WRITE_SPECIALREG(reg, _val)					\
	__asm __volatile("msr	" __STRING(reg) ", %0" : : "r"((uint64_t)_val))

#define	UL(x)	UINT64_C(x)

#endif /* !_MACHINE__ARMREG_H_ */
