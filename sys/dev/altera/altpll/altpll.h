/*-
 * Copyright (c) 2015 Ed Maste
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
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
 *
 * $FreeBSD$
 */

#ifndef _DEV_ALTPLL_H_
#define	_DEV_ALTPLL_H_

struct altpll_softc {
	/*
	 * Bus-related fields.
	 */
	device_t	 ap_dev;
	int		 ap_unit;

	/*
	 * The device node and memory-mapped I/O region.
	 */
	struct cdev	*ap_reg_cdev;
	struct resource	*ap_reg_res;
	int		 ap_reg_rid;

	/*
	 * PLL parameters.
	 */
	uint64_t	 ap_base_frequency;
};

/*
 * Altera PLL "register offsets."
 *
 * Communication with the ALTPLL_RECONFIG IP core happens over a proprietary
 * serial interface. A small perhipheral caches the parameters and streams them
 * to the PLL by trigging a write with a specified address bit high. From the
 * driver's perspective we just pretend to have a set of 32-bit registers.
 *
 * Address Bits  Description
 *          1-0  Always zero (word aligned)
 *          5-2  Counter type
 *          8-6  Counter parameter
 *            9  Set to 1 and write for transfer to PLL.
 *               Read returns 0x0d when the transfer is complete.
 */
#define	ALTPLL_OFF_TYPE_N		(0<<2)
#define	ALTPLL_OFF_TYPE_M		(1<<2)
#define	ALTPLL_OFF_TYPE_C0		(4<<2)
#define	ALTPLL_OFF_TYPE_C1		(5<<2)
#define	ALTPLL_OFF_PARAM_HIGH_COUNT	(0<<6)
#define	ALTPLL_OFF_PARAM_LOW_COUNT	(1<<6)
#define	ALTPLL_OFF_PARAM_BYPASS		(4<<6)
#define	ALTPLL_OFF_PARAM_ODD_COUNT	(5<<6)
#define	ALTPLL_OFF_TRANSFER		(1<<9)
#define	ALTPLL_TRANSFER_COMPLETE	0x0d

#define	ALTPLL_DEFAULT_FREQUENCY	50000000

/*
 * Driver setup routines from the bus attachment/teardown.
 */
int	altpll_attach(struct altpll_softc *sc);
void	altpll_detach(struct altpll_softc *sc);

/*
 * fdt_clock interface.
 */
int	altpll_set_frequency(device_t, uint64_t);

extern devclass_t	altpll_devclass;

#endif /* _DEV_ALTPLL_H_ */
