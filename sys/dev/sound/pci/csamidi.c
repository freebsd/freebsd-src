/*-
 * SPDX-License-Identifier: BSD-2-Clause

 * Copyright (c) 2015-2018 Tai-hwa Liang <avatar@FreeBSD.org>
 * All rights reserved
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_snd.h"
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/sound/pcm/sound.h>

#include <dev/sound/midi/midi.h>
#include <dev/sound/midi/mpu401.h>

#include <dev/sound/pci/csareg.h>
#include <dev/sound/pci/csavar.h>


/* device private data */

static uint32_t
csamidi_readio(struct resource *io, unsigned long offset)
{
	if (offset < BA0_AC97_RESET)
		return bus_space_read_4(rman_get_bustag(io), rman_get_bushandle(io), offset) & 0xffffffff;
	else
		return (0);
}

static void
csamidi_writeio(struct resource *io, unsigned long offset, uint32_t data)
{
	if (offset < BA0_AC97_RESET)
		bus_space_write_4(rman_get_bustag(io), rman_get_bushandle(io), offset, data);
}

static unsigned char
csamidi_mread(void *devinfo, int reg)
{
	struct resource *io = devinfo;
	unsigned int rc;
	unsigned int uart_stat;

	rc = 0;
	/* hacks to convert hardware status to MPU compatible ones */
	switch (reg) {
	case MPU_STATPORT:
		uart_stat = csamidi_readio(io, BA0_MIDSR);
		if (uart_stat & MIDSR_TBF)
			rc |= MPU_OUTPUTBUSY;	/* Tx buffer full */
		if (uart_stat & MIDSR_RBE)
			rc |= MPU_INPUTBUSY;
		break;
	case MPU_DATAPORT:
		rc = csamidi_readio(io, BA0_MIDRP);
		break;
	default:
		printf("csamidi_mread: unknown register %d\n", reg);
		break;
	}
	return (rc);
}

static void
csamidi_mwrite(void *devinfo, int reg, uint8_t b)
{
	struct resource *io = devinfo;
	unsigned int val;

	switch (reg) {
	case MPU_CMDPORT:
		switch (b)
		{
		case MPU_RESET:
			/* preserve current operation mode */
			val = csamidi_readio(io, BA0_MIDCR);
			/* reset the MIDI port */
			csamidi_writeio(io, BA0_MIDCR, MIDCR_MRST);
			csamidi_writeio(io, BA0_MIDCR, MIDCR_MLB);
			csamidi_writeio(io, BA0_MIDCR, 0x00);
			/* restore previous operation mode */
			csamidi_writeio(io, BA0_MIDCR, val);
			break;
		case MPU_UART:
			/* switch to UART mode, no-op */
		default:
			break;
		}
		break;
	case MPU_DATAPORT:
		/* put the MIDI databyte in the write port */
		csamidi_writeio(io, BA0_MIDWP, b);
		break;
	default:
		printf("csamidi_mwrite: unknown register %d\n", reg);
		break;
	}
}

const struct mpu401_ops csamidi_mpu_ops = {
	csamidi_mread,
	csamidi_mwrite,
};

void
csamidi_init(struct resource *io)
{
	/* reset the MIDI port */
	csamidi_writeio(io, BA0_MIDCR, MIDCR_MRST);
	/* MIDI transmit enable, no interrupt */
	csamidi_writeio(io, BA0_MIDCR, MIDCR_TXE | MIDCR_RXE);
	csamidi_writeio(io, BA0_HICR, HICR_IEV | HICR_CHGM);
}
