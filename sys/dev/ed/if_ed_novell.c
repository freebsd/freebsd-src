/*-
 * Copyright (c) 2005, M. Warner Losh
 * All rights reserved.
 * Copyright (c) 1995, David Greenman 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include "opt_ed.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <sys/bus.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <dev/ed/if_edreg.h>
#include <dev/ed/if_edvar.h>

/*
 * Probe and vendor-specific initialization routine for NE1000/2000 boards
 */
int
ed_probe_Novell_generic(device_t dev, int flags)
{
	struct ed_softc *sc = device_get_softc(dev);
	u_int   memsize, n;
	u_char  romdata[16], tmp;
	static char test_pattern[32] = "THIS is A memory TEST pattern";
	char    test_buffer[32];

	/* XXX - do Novell-specific probe here */

	/* Reset the board */
	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) {
		ed_asic_outb(sc, ED_NOVELL_RESET, 0);
		DELAY(200);
	}
	tmp = ed_asic_inb(sc, ED_NOVELL_RESET);

	/*
	 * I don't know if this is necessary; probably cruft leftover from
	 * Clarkson packet driver code. Doesn't do a thing on the boards I've
	 * tested. -DG [note that an outb(0x84, 0) seems to work here, and is
	 * non-invasive...but some boards don't seem to reset and I don't have
	 * complete documentation on what the 'right' thing to do is...so we
	 * do the invasive thing for now. Yuck.]
	 */
	ed_asic_outb(sc, ED_NOVELL_RESET, tmp);
	DELAY(5000);

	/*
	 * This is needed because some NE clones apparently don't reset the
	 * NIC properly (or the NIC chip doesn't reset fully on power-up) XXX
	 * - this makes the probe invasive! ...Done against my better
	 * judgement. -DLG
	 */
	ed_nic_outb(sc, ED_P0_CR, ED_CR_RD2 | ED_CR_STP);

	DELAY(5000);

	/* Make sure that we really have an 8390 based board */
	if (!ed_probe_generic8390(sc))
		return (ENXIO);

	sc->vendor = ED_VENDOR_NOVELL;
	sc->mem_shared = 0;
	sc->cr_proto = ED_CR_RD2;

	/*
	 * Test the ability to read and write to the NIC memory. This has the
	 * side affect of determining if this is an NE1000 or an NE2000.
	 */

	/*
	 * This prevents packets from being stored in the NIC memory when the
	 * readmem routine turns on the start bit in the CR.
	 */
	ed_nic_outb(sc, ED_P0_RCR, ED_RCR_MON);

	/* Temporarily initialize DCR for byte operations */
	ed_nic_outb(sc, ED_P0_DCR, ED_DCR_FT1 | ED_DCR_LS);

	ed_nic_outb(sc, ED_P0_PSTART, 8192 / ED_PAGE_SIZE);
	ed_nic_outb(sc, ED_P0_PSTOP, 16384 / ED_PAGE_SIZE);

	sc->isa16bit = 0;

	/*
	 * Write a test pattern in byte mode. If this fails, then there
	 * probably isn't any memory at 8k - which likely means that the board
	 * is an NE2000.
	 */
	ed_pio_writemem(sc, test_pattern, 8192, sizeof(test_pattern));
	ed_pio_readmem(sc, 8192, test_buffer, sizeof(test_pattern));

	if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
		sc->type = ED_TYPE_NE1000;
		sc->type_str = "NE1000";
	} else {

		/* neither an NE1000 nor a Linksys - try NE2000 */
		ed_nic_outb(sc, ED_P0_DCR, ED_DCR_WTS | ED_DCR_FT1 | ED_DCR_LS);
		ed_nic_outb(sc, ED_P0_PSTART, 16384 / ED_PAGE_SIZE);
		ed_nic_outb(sc, ED_P0_PSTOP, 32768 / ED_PAGE_SIZE);

		sc->isa16bit = 1;

		/*
		 * Write a test pattern in word mode. If this also fails, then
		 * we don't know what this board is.
		 */
		ed_pio_writemem(sc, test_pattern, 16384, sizeof(test_pattern));
		ed_pio_readmem(sc, 16384, test_buffer, sizeof(test_pattern));
		if (bcmp(test_pattern, test_buffer, sizeof(test_pattern)) == 0) {
			sc->type = ED_TYPE_NE2000;
			sc->type_str = "NE2000";
		} else {
			return (ENXIO);
		}
	}


	/* 8k of memory plus an additional 8k if 16bit */
	memsize = 8192 + sc->isa16bit * 8192;
	sc->mem_size = memsize;

	/* NIC memory doesn't start at zero on an NE board */
	/* The start address is tied to the bus width */
	sc->mem_start = (char *) 8192 + sc->isa16bit * 8192;
	sc->mem_end = sc->mem_start + memsize;
	sc->tx_page_start = memsize / ED_PAGE_SIZE;

	if (ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) {
		int     x, i, msize = 0;
		long    mstart = 0;
		char    pbuf0[ED_PAGE_SIZE], pbuf[ED_PAGE_SIZE], tbuf[ED_PAGE_SIZE];

		for (i = 0; i < ED_PAGE_SIZE; i++)
			pbuf0[i] = 0;

		/* Clear all the memory. */
		for (x = 1; x < 256; x++)
			ed_pio_writemem(sc, pbuf0, x * 256, ED_PAGE_SIZE);

		/* Search for the start of RAM. */
		for (x = 1; x < 256; x++) {
			ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
			if (bcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ed_pio_writemem(sc, pbuf, x * 256, ED_PAGE_SIZE);
				ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
				if (bcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0) {
					mstart = x * ED_PAGE_SIZE;
					msize = ED_PAGE_SIZE;
					break;
				}
			}
		}

		if (mstart == 0) {
			device_printf(dev, "Cannot find start of RAM.\n");
			return (ENXIO);
		}
		/* Search for the start of RAM. */
		for (x = (mstart / ED_PAGE_SIZE) + 1; x < 256; x++) {
			ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
			if (bcmp(pbuf0, tbuf, ED_PAGE_SIZE) == 0) {
				for (i = 0; i < ED_PAGE_SIZE; i++)
					pbuf[i] = 255 - x;
				ed_pio_writemem(sc, pbuf, x * 256, ED_PAGE_SIZE);
				ed_pio_readmem(sc, x * 256, tbuf, ED_PAGE_SIZE);
				if (bcmp(pbuf, tbuf, ED_PAGE_SIZE) == 0)
					msize += ED_PAGE_SIZE;
				else {
					break;
				}
			} else {
				break;
			}
		}

		if (msize == 0) {
			device_printf(dev, "Cannot find any RAM, start : %ld, x = %d.\n", mstart, x);
			return (ENXIO);
		}
		device_printf(dev, "RAM start at %ld, size : %d.\n", mstart, msize);

		sc->mem_size = msize;
		sc->mem_start = (caddr_t)(uintptr_t) mstart;
		sc->mem_end = (caddr_t)(uintptr_t) (msize + mstart);
		sc->tx_page_start = mstart / ED_PAGE_SIZE;
	}

	/*
	 * Use one xmit buffer if < 16k, two buffers otherwise (if not told
	 * otherwise).
	 */
	if ((memsize < 16384) || (flags & ED_FLAGS_NO_MULTI_BUFFERING))
		sc->txb_cnt = 1;
	else
		sc->txb_cnt = 2;

	sc->rec_page_start = sc->tx_page_start + sc->txb_cnt * ED_TXBUF_SIZE;
	sc->rec_page_stop = sc->tx_page_start + memsize / ED_PAGE_SIZE;

	sc->mem_ring = sc->mem_start + sc->txb_cnt * ED_PAGE_SIZE * ED_TXBUF_SIZE;

	ed_pio_readmem(sc, 0, romdata, 16);
	for (n = 0; n < ETHER_ADDR_LEN; n++)
		sc->arpcom.ac_enaddr[n] = romdata[n * (sc->isa16bit + 1)];

	if ((ED_FLAGS_GETTYPE(flags) == ED_FLAGS_GWETHER) &&
	    (sc->arpcom.ac_enaddr[2] == 0x86)) {
		sc->type_str = "Gateway AT";
	}

	/* clear any pending interrupts that might have occurred above */
	ed_nic_outb(sc, ED_P0_ISR, 0xff);

	return (0);
}

int
ed_probe_Novell(device_t dev, int port_rid, int flags)
{
	struct ed_softc *sc = device_get_softc(dev);
	int	error;

	error = ed_alloc_port(dev, port_rid, ED_NOVELL_IO_PORTS);
	if (error)
		return (error);

	sc->asic_offset = ED_NOVELL_ASIC_OFFSET;
	sc->nic_offset  = ED_NOVELL_NIC_OFFSET;

	return ed_probe_Novell_generic(dev, flags);
}
