/*-
 * Copyright (c) 2001 by Thomas Moestl <tmm@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * OpenFirmware bus support code that is (hopefully) independent from the used
 * hardware.
 * Maybe this should go into dev/ofw/; there may however be sparc specific
 * bits left.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <ofw/openfirm.h>

#include <machine/ofw_bus.h>


/*
 * Route an interrupt using the firmware. This takes an interrupt map and mask,
 * as retrieved from the firmware (this must be done by the caller, since it
 * is not bus-independent).
 * regs points to a "reg" property as returned by the firmware. regsz ist the
 * the size of one reg element, physz is the size of the physical address member
 * at the start of each reg (this is matched against the interrupt map).
 * The interrupt map has entries of the size (physsz + 12), the 12 being the
 * size of two u_int32_t that hold the interrupt number to compare against, the
 * node the map belongs to and the interrupt that the child interrupt is mapped
 * to (if the map entry matches).
 * The first nregs registers are checked against the map; in some cases (e.g.
 * PCI), only the first must be checked.
 * The mask consists of a mask wich must be and-ed to the checked physical
 * address part of the ofw reg and to the interrupt number before checking
 * against the map.
 * regm should point to a buffer of physsz size (this is not malloc'ed because
 * malloc cannot be called in all situations).
 */
u_int32_t
ofw_bus_route_intr(int intr, void *regs, int regsz, int physsz, int nregs,
    void *imap, int nimap, void *imapmsk, char *regm)
{
	u_int8_t *mptr;
	u_int32_t mintr, cintr;
	int r, i;

	cintr = -1;
	bcopy((u_int8_t *)imapmsk + physsz, &mintr, sizeof(mintr));
	mintr &= intr;
	for (r = 0; r < nregs; r++) {
		for (i = 0; i < physsz; i++) {
			regm[i] = ((u_int8_t *)regs)[r * regsz + i] &
			    ((u_int8_t *)imapmsk)[i];
		}
		for (i = 0; i < nimap; i++) {
			mptr = (u_int8_t *)imap + i * (physsz + 12);
			if (bcmp(regm, mptr, physsz) == 0 &&
			    bcmp(&mintr, mptr + physsz, sizeof(mintr)) == 0) {
				bcopy(mptr + physsz + 8, &cintr,
				    sizeof(cintr));
				break;
			}
		}
	}
	return (cintr);
}
