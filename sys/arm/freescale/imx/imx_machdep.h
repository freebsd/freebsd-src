/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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

#ifndef	IMX_MACHDEP_H
#define	IMX_MACHDEP_H

#include <sys/types.h>

/* Common functions, implemented in imx_machdep.c. */

void imx_devmap_addentry(vm_paddr_t _pa, vm_size_t _sz);
void imx_wdog_cpu_reset(vm_offset_t _wdcr_phys)  __attribute__((__noreturn__));

/*
 * SoC identity.
 */
#define	IMXSOC_51	0x05000100
#define	IMXSOC_53	0x05000300
#define	IMXSOC_6S	0x06000010
#define	IMXSOC_6SL	0x06000011
#define	IMXSOC_6D	0x06000020
#define	IMXSOC_6DL	0x06000021
#define	IMXSOC_6Q	0x06000040
#define	IMXSOC_6QL	0x06000041
#define	IMXSOC_FAMSHIFT	24

u_int imx_soc_type(void);
u_int imx_soc_family(void);

/* From here down, routines are implemented in imxNN_machdep.c. */

void imx_devmap_init(void);

#endif

