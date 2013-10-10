/*	$NecBSD: dp83932subr.c,v 1.5.6.2 1999/10/09 05:47:23 kmatsuda Exp $	*/
/*	$NetBSD$	*/
  
/*-
 * Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Kouichi Matsuda for
 *      NetBSD/pc98.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Routines of NEC PC-9801-83, 84, 103, 104, PC-9801N-25 and PC-9801N-J02, J02R 
 * Ethernet interface for NetBSD/pc98, ported by Kouichi Matsuda.
 *
 * These cards use National Semiconductor DP83934AVQB as Ethernet Controller
 * and National Semiconductor NS46C46 as (64 * 16 bits) Microwire Serial EEPROM.
 */

/*
 * Modified for FreeBSD(98) 4.0 from NetBSD/pc98 1.4.2 by Motomichi Matsuzaki.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/snc/dp83932reg.h>
#include <dev/snc/dp83932var.h>
#include <dev/snc/if_sncreg.h>
#include <dev/snc/dp83932subr.h>

static __inline u_int16_t snc_nec16_select_bank
	(struct snc_softc *, u_int32_t, u_int32_t);

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
int
sncsetup(struct snc_softc *sc, u_int8_t *lladdr)
{
	u_int32_t p, pp;
	int	i;
	int	offset;

	/*
	 * Put the pup in reset mode (sncinit() will fix it later),
	 * stop the timer, disable all interrupts and clear any interrupts.
	 */
	NIC_PUT(sc, SNCR_CR, CR_STP);
	wbflush();
	NIC_PUT(sc, SNCR_CR, CR_RST);
	wbflush();
	NIC_PUT(sc, SNCR_IMR, 0);
	wbflush();
	NIC_PUT(sc, SNCR_ISR, ISR_ALL);
	wbflush();

	/*
	 * because the SONIC is basically 16bit device it 'concatenates'
	 * a higher buffer address to a 16 bit offset--this will cause wrap
	 * around problems near the end of 64k !!
	 */
	p = pp = 0;

	for (i = 0; i < NRRA; i++) {
		sc->v_rra[i] = SONIC_GETDMA(p);
		p += RXRSRC_SIZE(sc);
	}
	sc->v_rea = SONIC_GETDMA(p);

	p = SOALIGN(sc, p);

	sc->v_cda = SONIC_GETDMA(p);
	p += CDA_SIZE(sc);

	p = SOALIGN(sc, p);

	for (i = 0; i < NTDA; i++) {
		struct mtd *mtdp = &sc->mtda[i];
		mtdp->mtd_vtxp = SONIC_GETDMA(p);
		p += TXP_SIZE(sc);
	}

	p = SOALIGN(sc, p);

	if ((p - pp) > PAGE_SIZE) {
		device_printf (sc->sc_dev, "sizeof RRA (%ld) + CDA (%ld) +"
		    "TDA (%ld) > PAGE_SIZE (%d). Punt!\n",
		    (u_long)sc->v_cda - (u_long)sc->v_rra[0],
		    (u_long)sc->mtda[0].mtd_vtxp - (u_long)sc->v_cda,
		    (u_long)p - (u_long)sc->mtda[0].mtd_vtxp,
		    PAGE_SIZE);
		return(1);
	}

	p = pp + PAGE_SIZE;
	pp = p;

	sc->sc_nrda = PAGE_SIZE / RXPKT_SIZE(sc);
	sc->v_rda = SONIC_GETDMA(p);

	p = pp + PAGE_SIZE;

	for (i = 0; i < NRBA; i++) {
		sc->rbuf[i] = p;
		p += PAGE_SIZE;
	}

	pp = p;
	offset = TXBSIZE;
	for (i = 0; i < NTDA; i++) {
		struct mtd *mtdp = &sc->mtda[i];

		mtdp->mtd_vbuf = SONIC_GETDMA(p);
		offset += TXBSIZE;
		if (offset < PAGE_SIZE) {
			p += TXBSIZE;
		} else {
			p = pp + PAGE_SIZE;
			pp = p;
			offset = TXBSIZE;
		}
	}

	return (0);
}

/*
 * miscellaneous NEC/SONIC detect functions.
 */

/*
 * check if a specified irq is acceptable.
 */
u_int8_t
snc_nec16_validate_irq(int irq)
{
	const u_int8_t encoded_irq[16] = {
	    -1, -1, -1, 0, -1, 1, 2, -1, -1, 3, 4, -1, 5, 6, -1, -1
	};

	return encoded_irq[irq];
}

/*
 * specify irq to board.
 */
int
snc_nec16_register_irq(struct snc_softc *sc, int irq)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t encoded_irq;

	encoded_irq = snc_nec16_validate_irq(irq);
	if (encoded_irq == (u_int8_t) -1) {
		printf("snc_nec16_register_irq: unsupported irq (%d)\n", irq);
		return 0;
	}

	/* select SNECR_IRQSEL register */
	bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_IRQSEL);
	/* write encoded irq value */
	bus_space_write_1(iot, ioh, SNEC_CTRLB, encoded_irq);

	return 1;
}

/*
 * check if a specified memory base address is acceptable.
 */
int
snc_nec16_validate_mem(int maddr)
{

	/* Check on Normal mode with max range, only */
	if ((maddr & ~0x1E000) != 0xC0000) {
		printf("snc_nec16_validate_mem: "
		    "unsupported window base (0x%x)\n", maddr);
		return 0;
	}

	return 1;
}

/*
 * specify memory base address to board and map to first bank.
 */
int
snc_nec16_register_mem(struct snc_softc *sc, int maddr)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (snc_nec16_validate_mem(maddr) == 0)
		return 0;

	/* select SNECR_MEMSEL register */
	bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMSEL);
	/* write encoded memory base select value */
	bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_MEMSEL_PHYS2EN(maddr));

	/*
	 * set current bank to 0 (bottom) and map
	 */
	/* select SNECR_MEMBS register */
	bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
	/* select new bank */
	bus_space_write_1(iot, ioh, SNEC_CTRLB,
	    SNECR_MEMBS_B2EB(0) | SNECR_MEMBS_BSEN);
	/* set current bank to 0 */
	sc->curbank = 0;

	return 1;
}

int
snc_nec16_check_memory(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_space_tag_t memt, bus_space_handle_t memh)
{
	u_int16_t val;
	int i, j;

	val = 0;
	for (i = 0; i < SNEC_NBANK; i++) {
		/* select new bank */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_MEMBS_B2EB(i) | SNECR_MEMBS_BSEN);

		/* write test pattern */
		for (j = 0; j < SNEC_NMEMS / 2; j++) {
			bus_space_write_2(memt, memh, j * 2, val + j);
		}
		val += 0x1000;
	}

	val = 0;
	for (i = 0; i < SNEC_NBANK; i++) {
		/* select new bank */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_MEMBS_B2EB(i) | SNECR_MEMBS_BSEN);

		/* read test pattern */
		for (j = 0; j < SNEC_NMEMS / 2; j++) {
			if (bus_space_read_2(memt, memh, j * 2) != val + j)
				break;
		}

		if (j < SNEC_NMEMS / 2) {
			printf("snc_nec16_check_memory: "
			    "memory check failed at 0x%04x%04x"
			    "val 0x%04x != expected 0x%04x\n", i, j,
			    bus_space_read_2(memt, memh, j * 2),
			    val + j);
			return 0;
		}
		val += 0x1000;
	}

	/* zero clear mem */
	for (i = 0; i < SNEC_NBANK; i++) {
		/* select new bank */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_MEMBS_B2EB(i) | SNECR_MEMBS_BSEN);

		bus_space_set_region_4(memt, memh, 0, 0, SNEC_NMEMS >> 2);
	}

	/* again read test if these are 0 */
	for (i = 0; i < SNEC_NBANK; i++) {
		/* select new bank */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_MEMBS_B2EB(i) | SNECR_MEMBS_BSEN);

		/* check if cleared */
		for (j = 0; j < SNEC_NMEMS; j += 2) {
			if (bus_space_read_2(memt, memh, j) != 0)
				break;
		}

		if (j != SNEC_NMEMS) {
			printf("snc_nec16_check_memory: "
			    "memory zero clear failed at 0x%04x%04x\n", i, j);
			return 0;
		}
	}

	return 1;
}

int
snc_nec16_detectsubr(bus_space_tag_t iot, bus_space_handle_t ioh,
    bus_space_tag_t memt, bus_space_handle_t memh, int irq, int maddr,
    u_int8_t type)
{
	u_int16_t cr;
	u_int8_t ident;
	int rv = 0;

	if (snc_nec16_validate_irq(irq) == (u_int8_t) -1)
		return 0;
	/* XXX: maddr already checked */
	if (snc_nec16_validate_mem(maddr) == 0)
		return 0;

	bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_IDENT);
	ident = bus_space_read_1(iot, ioh, SNEC_CTRLB);
	if (ident == 0xff || ident == 0x00) {
		/* not found */
		return 0;
	}

	switch (type) {
	case SNEC_TYPE_LEGACY:
		rv = (ident == SNECR_IDENT_LEGACY_CBUS);
		break;
	case SNEC_TYPE_PNP:
		rv = ((ident == SNECR_IDENT_PNP_CBUS) ||
		    (ident == SNECR_IDENT_PNP_PCMCIABUS));
		break;
	default:
		break;
	}

	if (rv == 0) {
		printf("snc_nec16_detectsubr: parent bus mismatch\n");
		return 0;
	}

	/* select SONIC register SNCR_CR */
	bus_space_write_1(iot, ioh, SNEC_ADDR, SNCR_CR);
	bus_space_write_2(iot, ioh, SNEC_CTRL, CR_RXDIS | CR_STP | CR_RST);
	DELAY(400);

	cr = bus_space_read_2(iot, ioh, SNEC_CTRL);
	if (cr != (CR_RXDIS | CR_STP | CR_RST)) {
#ifdef DIAGNOSTIC
		printf("snc_nec16_detectsubr: card reset failed, cr = 0x%04x\n",
		    cr);
#endif
		return 0;
	}

	if (snc_nec16_check_memory(iot, ioh, memt, memh) == 0)
		return 0;

	return 1;
}

/* XXX */
#define	SNC_VENDOR_NEC		0x00004c
#define	SNC_NEC_SERIES_LEGACY_CBUS	0xa5
#define	SNC_NEC_SERIES_PNP_PCMCIA	0xd5
#define	SNC_NEC_SERIES_PNP_PCMCIA2	0x6d	/* XXX */
#define	SNC_NEC_SERIES_PNP_CBUS		0x0d
#define	SNC_NEC_SERIES_PNP_CBUS2	0x3d

u_int8_t *
snc_nec16_detect_type(u_int8_t *myea)
{
	u_int32_t vendor = (myea[0] << 16) | (myea[1] << 8) | myea[2];
	u_int8_t series = myea[3];
	u_int8_t type = myea[4] & 0x80;
	u_int8_t *typestr;

	switch (vendor) {
	case SNC_VENDOR_NEC:
		switch (series) {
		case SNC_NEC_SERIES_LEGACY_CBUS:
			if (type)
				typestr = "NEC PC-9801-84";
			else
				typestr = "NEC PC-9801-83";
			break;
		case SNC_NEC_SERIES_PNP_CBUS:
		case SNC_NEC_SERIES_PNP_CBUS2:
			if (type)
				typestr = "NEC PC-9801-104";
			else
				typestr = "NEC PC-9801-103";
			break;
		case SNC_NEC_SERIES_PNP_PCMCIA:
		case SNC_NEC_SERIES_PNP_PCMCIA2:
			/* XXX: right ? */
			if (type)
				typestr = "NEC PC-9801N-J02R";
			else
				typestr = "NEC PC-9801N-J02";
			break;
		default:
			typestr = "NEC unknown (PC-9801N-25?)";
			break;
		}
		break;
	default:
		typestr = "unknown (3rd vendor?)";
		break;
	}

	return typestr;
}

int
snc_nec16_get_enaddr(bus_space_tag_t iot, bus_space_handle_t ioh,
    u_int8_t *myea)
{
	u_int8_t eeprom[SNEC_EEPROM_SIZE];
	u_int8_t rom_sum, sum = 0x00;
	int i;

	snc_nec16_read_eeprom(iot, ioh, eeprom);

	for (i = SNEC_EEPROM_KEY0; i < SNEC_EEPROM_CKSUM; i++) {
		sum = sum ^ eeprom[i];
	}

	rom_sum = eeprom[SNEC_EEPROM_CKSUM];

	if (sum != rom_sum) {
		printf("snc_nec16_get_enaddr: "
		    "checksum mismatch; calculated %02x != read %02x",
		    sum, rom_sum);
		return 0;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		myea[i] = eeprom[SNEC_EEPROM_SA0 + i];

	return 1;
}

/*
 * read from NEC/SONIC NIC register.
 */
u_int16_t
snc_nec16_nic_get(struct snc_softc *sc, u_int8_t reg)
{
	u_int16_t val;

	/* select SONIC register */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SNEC_ADDR, reg);
	val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, SNEC_CTRL);

	return val;
}

/*
 * write to NEC/SONIC NIC register.
 */
void
snc_nec16_nic_put(struct snc_softc *sc, u_int8_t reg, u_int16_t val)
{

	/* select SONIC register */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SNEC_ADDR, reg);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, SNEC_CTRL, val);
}


/*
 * select memory bank and map
 * where exists specified (internal buffer memory) offset.
 */
static __inline u_int16_t
snc_nec16_select_bank(struct snc_softc *sc, u_int32_t base, u_int32_t offset)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t bank;
	u_int16_t noffset;

	/* bitmode is fixed to 16 bit. */
	bank = (base + offset * 2) >> 13;
	noffset = (base + offset * 2) & (SNEC_NMEMS - 1);

#ifdef SNCDEBUG
	if (noffset % 2) {
		device_printf(sc->sc_dev, "noffset is odd (0x%04x)\n",
			      noffset);
	}
#endif	/* SNCDEBUG */

	if (sc->curbank != bank) {
		/* select SNECR_MEMBS register */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_MEMBS);
		/* select new bank */
		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_MEMBS_B2EB(bank) | SNECR_MEMBS_BSEN);
		/* update current bank */
		sc->curbank = bank;
	}

	return noffset;
}

/*
 * write to SONIC descriptors.
 */
void
snc_nec16_writetodesc(struct snc_softc *sc, u_int32_t base, u_int32_t offset,
    u_int16_t val)
{
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	u_int16_t noffset;

	noffset = snc_nec16_select_bank(sc, base, offset);

	bus_space_write_2(memt, memh, noffset, val);
}

/*
 * read from SONIC descriptors.
 */
u_int16_t
snc_nec16_readfromdesc(struct snc_softc *sc, u_int32_t base, u_int32_t offset)
{
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	u_int16_t noffset;

	noffset = snc_nec16_select_bank(sc, base, offset);

	return bus_space_read_2(memt, memh, noffset);
}

/*
 * read from SONIC data buffer.
 */
void
snc_nec16_copyfrombuf(struct snc_softc *sc, void *dst, u_int32_t offset,
    size_t size)
{
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	u_int16_t noffset;
	u_int8_t* bptr = dst;

	noffset = snc_nec16_select_bank(sc, offset, 0);

	/* XXX: should check if offset + size < 0x2000. */

	bus_space_barrier(memt, memh, noffset, size,
			  BUS_SPACE_BARRIER_READ);

	if (size > 3)  {
		if (noffset & 3)  {
			size_t asize = 4 - (noffset & 3);

			bus_space_read_region_1(memt, memh, noffset,
			    bptr, asize);
			bptr += asize;
			noffset += asize;
			size -= asize;
		}
		bus_space_read_region_4(memt, memh, noffset,
		    (u_int32_t *) bptr, size >> 2);
		bptr += size & ~3;
		noffset += size & ~3;
		size &= 3;
	}
	if (size)
		bus_space_read_region_1(memt, memh, noffset, bptr, size);
}

/*
 * write to SONIC data buffer.
 */
void
snc_nec16_copytobuf(struct snc_softc *sc, void *src, u_int32_t offset,
    size_t size)
{
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	u_int16_t noffset, onoffset;
	size_t osize = size;
	u_int8_t* bptr = src;

	noffset = snc_nec16_select_bank(sc, offset, 0);
	onoffset = noffset;

	/* XXX: should check if offset + size < 0x2000. */

	if (size > 3)  {
		if (noffset & 3)  {
			size_t asize = 4 - (noffset & 3);

			bus_space_write_region_1(memt, memh, noffset,
			    bptr, asize);
			bptr += asize;
			noffset += asize;
			size -= asize;
		}
		bus_space_write_region_4(memt, memh, noffset,
		    (u_int32_t *)bptr, size >> 2);
		bptr += size & ~3;
		noffset += size & ~3;
		size -= size & ~3;
	}
	if (size)
		bus_space_write_region_1(memt, memh, noffset, bptr, size);

	bus_space_barrier(memt, memh, onoffset, osize,
			  BUS_SPACE_BARRIER_WRITE);
}

/*
 * write (fill) 0 to SONIC data buffer.
 */
void
snc_nec16_zerobuf(struct snc_softc *sc, u_int32_t offset, size_t size)
{
	bus_space_tag_t memt = sc->sc_memt;
	bus_space_handle_t memh = sc->sc_memh;
	u_int16_t noffset, onoffset;
	size_t osize = size;

	noffset = snc_nec16_select_bank(sc, offset, 0);
	onoffset = noffset;

	/* XXX: should check if offset + size < 0x2000. */

	if (size > 3)  {
		if (noffset & 3)  {
			size_t asize = 4 - (noffset & 3);

			bus_space_set_region_1(memt, memh, noffset, 0, asize);
			noffset += asize;
			size -= asize;
		}
		bus_space_set_region_4(memt, memh, noffset, 0, size >> 2);
		noffset += size & ~3;
		size -= size & ~3;
	}
	if (size)
		bus_space_set_region_1(memt, memh, noffset, 0, size);

	bus_space_barrier(memt, memh, onoffset, osize,
			  BUS_SPACE_BARRIER_WRITE);
}


/* 
 * Routines to read bytes sequentially from EEPROM through NEC PC-9801-83,
 * 84, 103, 104, PC-9801N-25 and PC-9801N-J02, J02R for NetBSD/pc98.
 * Ported by Kouichi Matsuda.
 * 
 * This algorism is generic to read data sequentially from 4-Wire
 * Microwire Serial EEPROM.
 */

#define	SNEC_EEP_DELAY	1000

void
snc_nec16_read_eeprom(bus_space_tag_t iot, bus_space_handle_t ioh,
    u_int8_t *data)
{
	u_int8_t n, val, bit;

	/* Read bytes from EEPROM; two bytes per an iteration. */
	for (n = 0; n < SNEC_EEPROM_SIZE / 2; n++) {
		/* select SNECR_EEP */
		bus_space_write_1(iot, ioh, SNEC_ADDR, SNECR_EEP);

		bus_space_write_1(iot, ioh, SNEC_CTRLB, 0x00);
		DELAY(SNEC_EEP_DELAY);

		/* Start EEPROM access. */
		bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_SK);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_DI);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_SK | SNECR_EEP_DI);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_DI);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_SK | SNECR_EEP_DI);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS);
		DELAY(SNEC_EEP_DELAY);

		bus_space_write_1(iot, ioh, SNEC_CTRLB,
		    SNECR_EEP_CS | SNECR_EEP_SK);
		DELAY(SNEC_EEP_DELAY);

		/* Pass the iteration count to the chip. */
		for (bit = 0x20; bit != 0x00; bit >>= 1) {
			bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS |
			    ((n & bit) ? SNECR_EEP_DI : 0x00));
			DELAY(SNEC_EEP_DELAY);

			bus_space_write_1(iot, ioh, SNEC_CTRLB,
			    SNECR_EEP_CS | SNECR_EEP_SK |
			    ((n & bit) ? SNECR_EEP_DI : 0x00));
			DELAY(SNEC_EEP_DELAY);
		}

		bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS);
		(void) bus_space_read_1(iot, ioh, SNEC_CTRLB);	/* ACK */
		DELAY(SNEC_EEP_DELAY);

		/* Read a byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(iot, ioh, SNEC_CTRLB,
			    SNECR_EEP_CS | SNECR_EEP_SK);
			DELAY(SNEC_EEP_DELAY);

			bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS);

			if (bus_space_read_1(iot, ioh, SNEC_CTRLB) & SNECR_EEP_DO)
				val |= bit;
		}
		*data++ = val;

		/* Read one more byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			bus_space_write_1(iot, ioh, SNEC_CTRLB,
			    SNECR_EEP_CS | SNECR_EEP_SK);
			DELAY(SNEC_EEP_DELAY);

			bus_space_write_1(iot, ioh, SNEC_CTRLB, SNECR_EEP_CS);

			if (bus_space_read_1(iot, ioh, SNEC_CTRLB) & SNECR_EEP_DO)
				val |= bit;
		}
		*data++ = val;

		bus_space_write_1(iot, ioh, SNEC_CTRLB, 0x00);
		DELAY(SNEC_EEP_DELAY);
	}

#ifdef	SNCDEBUG
	/* Report what we got. */
	data -= SNEC_EEPROM_SIZE;
	log(LOG_INFO, "%s: EEPROM:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    "snc_nec16_read_eeprom",
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

#ifdef	SNCDEBUG
void
snc_nec16_dump_reg(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	u_int8_t n;
	u_int16_t val;

	printf("SONIC registers (word):");
	for (n = 0; n < SNC_NREGS; n++) {
		/* select required SONIC register */
		bus_space_write_1(iot, ioh, SNEC_ADDR, n);
		DELAY(10);
		val = bus_space_read_2(iot, ioh, SNEC_CTRL);
		if ((n % 0x10) == 0)
			printf("\n%04x ", val);
		else
			printf("%04x ", val);
	}
	printf("\n");

	printf("NEC/SONIC registers (byte):\n");
	for (n = SNECR_MEMBS; n <= SNECR_IDENT; n += 2) {
		/* select required SONIC register */
		bus_space_write_1(iot, ioh, SNEC_ADDR, n);
		DELAY(10);
		val = (u_int16_t) bus_space_read_1(iot, ioh, SNEC_CTRLB);
		printf("%04x ", val);
	}
	printf("\n");
}

#endif	/* SNCDEBUG */
