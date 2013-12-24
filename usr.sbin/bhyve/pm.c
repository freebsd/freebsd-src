/*-
 * Copyright (c) 2013 Advanced Computing Technologies LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include "inout.h"

#define	PM1A_EVT_ADDR	0x400
#define	PM1A_CNT_ADDR	0x404

/*
 * Reset Control register at I/O port 0xcf9.  Bit 2 forces a system
 * reset when it transitions from 0 to 1.  Bit 1 selects the type of
 * reset to attempt: 0 selects a "soft" reset, and 1 selects a "hard"
 * reset.
 */
static int
reset_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{
	static uint8_t reset_control;

	if (bytes != 1)
		return (-1);
	if (in)
		*eax = reset_control;
	else {
		reset_control = *eax;

		/* Treat hard and soft resets the same. */
		if (reset_control & 0x4)
			return (INOUT_RESET);
	}
	return (0);
}
INOUT_PORT(reset_reg, 0xCF9, IOPORT_F_INOUT, reset_handler);

/*
 * Power Management 1 Event Registers
 *
 * bhyve doesn't support any power management events currently, so the
 * status register always returns zero.  The enable register preserves
 * its value but has no effect.
 */
static int
pm1_status_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{

	if (bytes != 2)
		return (-1);
	if (in)
		*eax = 0;
	return (0);
}

static int
pm1_enable_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{
	static uint16_t pm1_enable;

	if (bytes != 2)
		return (-1);
	if (in)
		*eax = pm1_enable;
	else
		pm1_enable = *eax;
	return (0);
}
INOUT_PORT(pm1_status, PM1A_EVT_ADDR, IOPORT_F_INOUT, pm1_status_handler);
INOUT_PORT(pm1_enable, PM1A_EVT_ADDR + 2, IOPORT_F_INOUT, pm1_enable_handler);

/*
 * Power Management 1 Control Register
 *
 * This is mostly unimplemented except that we wish to handle writes that
 * set SPL_EN to handle S5 (soft power off).
 */
#define	PM1_SLP_TYP	0x1c00
#define	PM1_SLP_EN	0x2000
#define	PM1_ALWAYS_ZERO	0xc003

static int
pm1_control_handler(struct vmctx *ctx, int vcpu, int in, int port, int bytes,
    uint32_t *eax, void *arg)
{
	static uint16_t pm1_control;

	if (bytes != 2)
		return (-1);
	if (in)
		*eax = pm1_control;
	else {
		/*
		 * Various bits are write-only or reserved, so force them
		 * to zero in pm1_control.
		 */
		pm1_control = *eax & ~(PM1_SLP_EN | PM1_ALWAYS_ZERO);

		/*
		 * If SLP_EN is set, check for S5.  Bhyve's _S5_ method
		 * says that '5' should be stored in SLP_TYP for S5.
		 */
		if (*eax & PM1_SLP_EN) {
			if ((pm1_control & PM1_SLP_TYP) >> 10 == 5)
				return (INOUT_POWEROFF);
		}
	}
	return (0);
}
INOUT_PORT(pm1_control, PM1A_CNT_ADDR, IOPORT_F_INOUT, pm1_control_handler);
