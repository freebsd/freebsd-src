/*-
 * Copyright (c) 2000
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <i386/isa/isa_device.h>
#include <dev/lnc/if_lncvar.h>
#include <dev/lnc/if_lncreg.h>

int ne2100_probe __P((lnc_softc_t *, unsigned));
int bicc_probe __P((lnc_softc_t *, unsigned));
int depca_probe __P((lnc_softc_t *, unsigned));
int lance_probe __P((lnc_softc_t *));
int pcnet_probe __P((lnc_softc_t *));
int lnc_probe __P((struct isa_device *));
int lnc_attach __P((struct isa_device *));

static int dec_macaddr_extract __P((u_char[], lnc_softc_t *));
static ointhand2_t lncintr;

extern lnc_softc_t lnc_softc[];
extern int lnc_attach_sc __P((lnc_softc_t *, int));
extern void lncintr_sc __P((lnc_softc_t *));

int
ne2100_probe(lnc_softc_t *sc, unsigned iobase)
{
	int i;

	sc->rap = iobase + PCNET_RAP;
	sc->rdp = iobase + PCNET_RDP;

	sc->nic.ic = pcnet_probe(sc);
	if ((sc->nic.ic > 0) && (sc->nic.ic < PCnet_PCI)) {
		sc->nic.ident = NE2100;
		sc->nic.mem_mode = DMA_FIXED;

		/* XXX - For now just use the defines */
		sc->nrdre = NRDRE;
		sc->ntdre = NTDRE;

		/* Extract MAC address from PROM */
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			sc->arpcom.ac_enaddr[i] = inb(iobase + i);
		return (NE2100_IOSIZE);
	} else {
		return (0);
	}
}

int
bicc_probe(lnc_softc_t *sc, unsigned iobase)
{
	int i;

	/*
	 * There isn't any way to determine if a NIC is a BICC. Basically, if
	 * the lance probe succeeds using the i/o addresses of the BICC then
	 * we assume it's a BICC.
	 *
	 */

	sc->rap = iobase + BICC_RAP;
	sc->rdp = iobase + BICC_RDP;

	/* I think all these cards us the Am7990 */

	if ((sc->nic.ic = lance_probe(sc))) {
		sc->nic.ident = BICC;
		sc->nic.mem_mode = DMA_FIXED;

		/* XXX - For now just use the defines */
		sc->nrdre = NRDRE;
		sc->ntdre = NTDRE;

		/* Extract MAC address from PROM */
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			sc->arpcom.ac_enaddr[i] = inb(iobase + (i * 2));

		return (BICC_IOSIZE);
	} else {
		return (0);
	}
}

/*
 * I don't have data sheets for the dec cards but it looks like the mac
 * address is contained in a 32 byte ring. Each time you read from the port
 * you get the next byte in the ring. The mac address is stored after a
 * signature so keep searching for the signature first.
 */
static int
dec_macaddr_extract(u_char ring[], lnc_softc_t * sc)
{
	const unsigned char signature[] = {0xff, 0x00, 0x55, 0xaa, 0xff, 0x00, 0x55, 0xaa};

	int i, j, rindex;

	for (i = 0; i < sizeof ring; i++) {
		for (j = 0, rindex = i; j < sizeof signature; j++) {
			if (ring[rindex] != signature[j])
				break;
			if (++rindex > sizeof ring)
				rindex = 0;
		}
		if (j == sizeof signature) {
			for (j = 0, rindex = i; j < ETHER_ADDR_LEN; j++) {
				sc->arpcom.ac_enaddr[j] = ring[rindex];
				if (++rindex > sizeof ring)
					rindex = 0;
			}
			return (1);
		}
	}
	return (0);
}

int
depca_probe(lnc_softc_t *sc, unsigned iobase)
{
	int i;
	unsigned char maddr_ring[DEPCA_ADDR_ROM_SIZE];

	sc->rap = iobase + DEPCA_RAP;
	sc->rdp = iobase + DEPCA_RDP;

	if ((sc->nic.ic = lance_probe(sc))) {
		sc->nic.ident = DEPCA;
		sc->nic.mem_mode = SHMEM;

		/* Extract MAC address from PROM */
		for (i = 0; i < DEPCA_ADDR_ROM_SIZE; i++)
			maddr_ring[i] = inb(iobase + DEPCA_ADP);
		if (dec_macaddr_extract(maddr_ring, sc)) {
			return (DEPCA_IOSIZE);
		}
	}
	return (0);
}

int
lance_probe(lnc_softc_t *sc)
{
	write_csr(sc, CSR0, STOP);

	if ((inw(sc->rdp) & STOP) && !(read_csr(sc, CSR3))) {
		/*
		 * Check to see if it's a C-LANCE. For the LANCE the INEA bit
		 * cannot be set while the STOP bit is. This restriction is
		 * removed for the C-LANCE.
		 */
		write_csr(sc, CSR0, INEA);
		if (read_csr(sc, CSR0) & INEA)
			return (C_LANCE);
		else
			return (LANCE);
	} else
		return (UNKNOWN);
}

int
pcnet_probe(lnc_softc_t *sc)
{
	u_long chip_id;
	int type;

	/*
	 * The PCnet family don't reset the RAP register on reset so we'll
	 * have to write during the probe :-) It does have an ID register
	 * though so the probe is just a matter of reading it.
	 */

	if ((type = lance_probe(sc))) {
		chip_id = read_csr(sc, CSR89);
		chip_id <<= 16;
		chip_id |= read_csr(sc, CSR88);
		if (chip_id & AMD_MASK) {
			chip_id >>= 12;
			switch (chip_id & PART_MASK) {
			case Am79C960:
				return (PCnet_ISA);
			case Am79C961:
				return (PCnet_ISAplus);
			case Am79C961A:
				return (PCnet_ISA_II);
			case Am79C965:
				return (PCnet_32);
			case Am79C970:
				return (PCnet_PCI);
			case Am79C970A:
				return (PCnet_PCI_II);
			case Am79C971:
				return (PCnet_FAST);
			case Am79C972:
				return (PCnet_FASTplus);
			case Am79C978:
				return (PCnet_Home);
			default:
				break;
			}
		}
	}
	return (type);
}

int
lnc_probe(struct isa_device * isa_dev)
{
	int nports;
	int unit = isa_dev->id_unit;
	lnc_softc_t *sc = &lnc_softc[unit];
	unsigned iobase = isa_dev->id_iobase;

#ifdef DIAGNOSTIC
	int vsw;
	vsw = inw(isa_dev->id_iobase + PCNET_VSW);
	printf("Vendor Specific Word = %x\n", vsw);
#endif

	nports = bicc_probe(sc, iobase);
	if (nports == 0)
		nports = ne2100_probe(sc, iobase);
	if (nports == 0)
		nports = depca_probe(sc, iobase);
#ifdef PC98
	if (nports == 0)
		nports = cnet98s_probe(sc, iobase);
#endif
	return (nports);
}

int
lnc_attach(struct isa_device * isa_dev)
{
	int unit = isa_dev->id_unit;
	lnc_softc_t *sc = &lnc_softc[unit];
	int result;

	isa_dev->id_ointr = lncintr;
	result = lnc_attach_sc (sc, unit);
	if (result == 0)
		return (0);

#ifndef PC98
	/*
	 * XXX - is it safe to call isa_dmacascade() after if_attach() 
	 *       and ether_ifattach() have been called in lnc_attach() ???
	 */
	if ((sc->nic.mem_mode != SHMEM) &&
	    (sc->nic.ic < PCnet_32))
		isa_dmacascade(isa_dev->id_drq);
#endif

	return result;
}

static void
lncintr(int unit)
{
	lnc_softc_t *sc = &lnc_softc[unit];
	lncintr_sc (sc);
}
