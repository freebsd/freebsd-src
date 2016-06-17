/*-
 * Copyright (c) 2016 Michael Zhilin <mizhka@gmail.com>
 *
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
 */

/*
 * $FreeBSD$
 */

#ifndef _MIPS_BROADCOM_BCM_SOCINFO_H_
#define _MIPS_BROADCOM_BCM_SOCINFO_H_

#include <machine/cpuregs.h>

struct bcm_socinfo {
	uint32_t id;
	uint32_t cpurate;	/* in MHz */
	uint32_t uartrate;	/* in Hz */
};

struct bcm_socinfo*	bcm_get_socinfo_by_socid(uint32_t key);
struct bcm_socinfo*	bcm_get_socinfo(void);

#define	BCM_SOCADDR				0x18000000
#define		BCM_REG_CHIPC_ID		0x0
#define		BCM_REG_CHIPC_UART		0x300
#define		BCM_REG_CHIPC_PMUWD_OFFS	0x634
#define	BCM_SOCREG(reg)							\
		MIPS_PHYS_TO_KSEG1((BCM_SOCADDR + (reg)))
#define BCM_READ_REG32(reg)						\
	*((volatile uint32_t *)BCM_SOCREG(reg))
#define BCM_WRITE_REG32(reg, value)					\
	do {								\
		writel((void*)BCM_SOCREG((reg)),value);			\
	} while (0);

#endif /* _MIPS_BROADCOM_BCM_SOCINFO_H_ */
