/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Alteon Networks Tigon PCI gigabit ethernet driver for FreeBSD.
 * Manuals, sample driver and firmware source kits are available
 * from http://www.alteon.com/support/openkits.
 * 
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Alteon Networks Tigon chip contains an embedded R4000 CPU,
 * gigabit MAC, dual DMA channels and a PCI interface unit. NICs
 * using the Tigon may have anywhere from 512K to 2MB of SRAM. The
 * Tigon supports hardware IP, TCP and UCP checksumming, multicast
 * filtering and jumbo (9014 byte) frames. The hardware is largely
 * controlled by firmware, which must be loaded into the NIC during
 * initialization.
 *
 * The Tigon 2 contains 2 R4000 CPUs and requires a newer firmware
 * revision, which supports new features such as extended commands,
 * extended jumbo receive ring desciptors and a mini receive ring.
 *
 * Alteon Networks is to be commended for releasing such a vast amount
 * of development material for the Tigon NIC without requiring an NDA
 * (although they really should have done it a long time ago). With
 * any luck, the other vendors will finally wise up and follow Alteon's
 * stellar example.
 *
 * The firmware for the Tigon 1 and 2 NICs is compiled directly into
 * this driver by #including it as a C header file. This bloats the
 * driver somewhat, but it's the easiest method considering that the
 * driver code and firmware code need to be kept in sync. The source
 * for the firmware is not provided with the FreeBSD distribution since
 * compiling it requires a GNU toolchain targeted for mips-sgi-irix5.3.
 *
 * The following people deserve special thanks:
 * - Terry Murphy of 3Com, for providing a 3c985 Tigon 1 board
 *   for testing
 * - Raymond Lee of Netgear, for providing a pair of Netgear
 *   GA620 Tigon 2 boards for testing
 * - Ulf Zimmermann, for bringing the GA260 to my attention and
 *   convincing me to write this driver.
 * - Andrew Gallatin for providing FreeBSD/Alpha support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <pci/if_tireg.h>
#include <pci/ti_fw.h>
#include <pci/ti_fw2.h>

/*
 * Temporarily disable the checksum offload support for now.
 * Tests with ftp.freesoftware.com show that after about 12 hours,
 * the firmware will begin calculating completely bogus TX checksums
 * and refuse to stop until the interface is reset. Unfortunately,
 * there isn't enough time to fully debug this before the 4.1
 * release, so this will need to stay off for now.
 */
#ifdef notdef
#define TI_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_IP_FRAGS)
#else
#define TI_CSUM_FEATURES	0
#endif

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */

static struct ti_type ti_devs[] = {
	{ ALT_VENDORID,	ALT_DEVICEID_ACENIC,
		"Alteon AceNIC 1000baseSX Gigabit Ethernet" },
	{ ALT_VENDORID,	ALT_DEVICEID_ACENIC_COPPER,
		"Alteon AceNIC 1000baseT Gigabit Ethernet" },
	{ TC_VENDORID,	TC_DEVICEID_3C985,
		"3Com 3c985-SX Gigabit Ethernet" },
	{ NG_VENDORID, NG_DEVICEID_GA620,
		"Netgear GA620 1000baseSX Gigabit Ethernet" },
	{ NG_VENDORID, NG_DEVICEID_GA620T,
		"Netgear GA620 1000baseT Gigabit Ethernet" },
	{ SGI_VENDORID, SGI_DEVICEID_TIGON,
		"Silicon Graphics Gigabit Ethernet" },
	{ DEC_VENDORID, DEC_DEVICEID_FARALLON_PN9000SX,
		"Farallon PN9000SX Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int ti_probe		__P((device_t));
static int ti_attach		__P((device_t));
static int ti_detach		__P((device_t));
static void ti_txeof		__P((struct ti_softc *));
static void ti_rxeof		__P((struct ti_softc *));

static void ti_stats_update	__P((struct ti_softc *));
static int ti_encap		__P((struct ti_softc *, struct mbuf *,
					u_int32_t *));

static void ti_intr		__P((void *));
static void ti_start		__P((struct ifnet *));
static int ti_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void ti_init		__P((void *));
static void ti_init2		__P((struct ti_softc *));
static void ti_stop		__P((struct ti_softc *));
static void ti_watchdog		__P((struct ifnet *));
static void ti_shutdown		__P((device_t));
static int ti_ifmedia_upd	__P((struct ifnet *));
static void ti_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static u_int32_t ti_eeprom_putbyte	__P((struct ti_softc *, int));
static u_int8_t	ti_eeprom_getbyte	__P((struct ti_softc *,
						int, u_int8_t *));
static int ti_read_eeprom	__P((struct ti_softc *, caddr_t, int, int));

static void ti_add_mcast	__P((struct ti_softc *, struct ether_addr *));
static void ti_del_mcast	__P((struct ti_softc *, struct ether_addr *));
static void ti_setmulti		__P((struct ti_softc *));

static void ti_mem		__P((struct ti_softc *, u_int32_t,
					u_int32_t, caddr_t));
static void ti_loadfw		__P((struct ti_softc *));
static void ti_cmd		__P((struct ti_softc *, struct ti_cmd_desc *));
static void ti_cmd_ext		__P((struct ti_softc *, struct ti_cmd_desc *,
					caddr_t, int));
static void ti_handle_events	__P((struct ti_softc *));
static int ti_alloc_jumbo_mem	__P((struct ti_softc *));
static void *ti_jalloc		__P((struct ti_softc *));
static void ti_jfree		__P((caddr_t, u_int));
static void ti_jref		__P((caddr_t, u_int));
static int ti_newbuf_std	__P((struct ti_softc *, int, struct mbuf *));
static int ti_newbuf_mini	__P((struct ti_softc *, int, struct mbuf *));
static int ti_newbuf_jumbo	__P((struct ti_softc *, int, struct mbuf *));
static int ti_init_rx_ring_std	__P((struct ti_softc *));
static void ti_free_rx_ring_std	__P((struct ti_softc *));
static int ti_init_rx_ring_jumbo	__P((struct ti_softc *));
static void ti_free_rx_ring_jumbo	__P((struct ti_softc *));
static int ti_init_rx_ring_mini	__P((struct ti_softc *));
static void ti_free_rx_ring_mini	__P((struct ti_softc *));
static void ti_free_tx_ring	__P((struct ti_softc *));
static int ti_init_tx_ring	__P((struct ti_softc *));

static int ti_64bitslot_war	__P((struct ti_softc *));
static int ti_chipinit		__P((struct ti_softc *));
static int ti_gibinit		__P((struct ti_softc *));

static device_method_t ti_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_probe),
	DEVMETHOD(device_attach,	ti_attach),
	DEVMETHOD(device_detach,	ti_detach),
	DEVMETHOD(device_shutdown,	ti_shutdown),
	{ 0, 0 }
};

static driver_t ti_driver = {
	"ti",
	ti_methods,
	sizeof(struct ti_softc)
};

static devclass_t ti_devclass;

DRIVER_MODULE(if_ti, pci, ti_driver, ti_devclass, 0, 0);

/*
 * Send an instruction or address to the EEPROM, check for ACK.
 */
static u_int32_t ti_eeprom_putbyte(sc, byte)
	struct ti_softc		*sc;
	int			byte;
{
	register int		i, ack = 0;

	/*
	 * Make sure we're in TX mode.
	 */
	TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x80; i; i >>= 1) {
		if (byte & i) {
			TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_DOUT);
		} else {
			TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_DOUT);
		}
		DELAY(1);
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
		TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
	}

	/*
	 * Turn off TX mode.
	 */
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);

	/*
	 * Check for ack.
	 */
	TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
	ack = CSR_READ_4(sc, TI_MISC_LOCAL_CTL) & TI_MLC_EE_DIN;
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);

	return(ack);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.'
 * We have to send two address bytes since the EEPROM can hold
 * more than 256 bytes of data.
 */
static u_int8_t ti_eeprom_getbyte(sc, addr, dest)
	struct ti_softc		*sc;
	int			addr;
	u_int8_t		*dest;
{
	register int		i;
	u_int8_t		byte = 0;

	EEPROM_START;

	/*
	 * Send write control code to EEPROM.
	 */
	if (ti_eeprom_putbyte(sc, EEPROM_CTL_WRITE)) {
		printf("ti%d: failed to send write command, status: %x\n",
		    sc->ti_unit, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return(1);
	}

	/*
	 * Send first byte of address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, (addr >> 8) & 0xFF)) {
		printf("ti%d: failed to send address, status: %x\n",
		    sc->ti_unit, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return(1);
	}
	/*
	 * Send second byte address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, addr & 0xFF)) {
		printf("ti%d: failed to send address, status: %x\n",
		    sc->ti_unit, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return(1);
	}

	EEPROM_STOP;
	EEPROM_START;
	/*
	 * Send read control code to EEPROM.
	 */
	if (ti_eeprom_putbyte(sc, EEPROM_CTL_READ)) {
		printf("ti%d: failed to send read command, status: %x\n",
		    sc->ti_unit, CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return(1);
	}

	/*
	 * Start reading bits from EEPROM.
	 */
	TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_TXEN);
	for (i = 0x80; i; i >>= 1) {
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
		if (CSR_READ_4(sc, TI_MISC_LOCAL_CTL) & TI_MLC_EE_DIN)
			byte |= i;
		TI_CLRBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_EE_CLK);
		DELAY(1);
	}

	EEPROM_STOP;

	/*
	 * No ACK generated for read, so just return byte.
	 */

	*dest = byte;

	return(0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int ti_read_eeprom(sc, dest, off, cnt)
	struct ti_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
{
	int			err = 0, i;
	u_int8_t		byte = 0;

	for (i = 0; i < cnt; i++) {
		err = ti_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return(err ? 1 : 0);
}

/*
 * NIC memory access function. Can be used to either clear a section
 * of NIC local memory or (if buf is non-NULL) copy data into it.
 */
static void ti_mem(sc, addr, len, buf)
	struct ti_softc		*sc;
	u_int32_t		addr, len;
	caddr_t			buf;
{
	int			segptr, segsize, cnt;
	caddr_t			ti_winbase, ptr;

	segptr = addr;
	cnt = len;
	ti_winbase = (caddr_t)(sc->ti_vhandle + TI_WINDOW);
	ptr = buf;

	while(cnt) {
		if (cnt < TI_WINLEN)
			segsize = cnt;
		else
			segsize = TI_WINLEN - (segptr % TI_WINLEN);
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));
		if (buf == NULL)
			bzero((char *)ti_winbase + (segptr &
			    (TI_WINLEN - 1)), segsize);
		else {
			bcopy((char *)ptr, (char *)ti_winbase +
			    (segptr & (TI_WINLEN - 1)), segsize);
			ptr += segsize;
		}
		segptr += segsize;
		cnt -= segsize;
	}

	return;
}

/*
 * Load firmware image into the NIC. Check that the firmware revision
 * is acceptable and see if we want the firmware for the Tigon 1 or
 * Tigon 2.
 */
static void ti_loadfw(sc)
	struct ti_softc		*sc;
{
	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		if (tigonFwReleaseMajor != TI_FIRMWARE_MAJOR ||
		    tigonFwReleaseMinor != TI_FIRMWARE_MINOR ||
		    tigonFwReleaseFix != TI_FIRMWARE_FIX) {
			printf("ti%d: firmware revision mismatch; want "
			    "%d.%d.%d, got %d.%d.%d\n", sc->ti_unit,
			    TI_FIRMWARE_MAJOR, TI_FIRMWARE_MINOR,
			    TI_FIRMWARE_FIX, tigonFwReleaseMajor,
			    tigonFwReleaseMinor, tigonFwReleaseFix);
			return;
		}
		ti_mem(sc, tigonFwTextAddr, tigonFwTextLen,
		    (caddr_t)tigonFwText);
		ti_mem(sc, tigonFwDataAddr, tigonFwDataLen,
		    (caddr_t)tigonFwData);
		ti_mem(sc, tigonFwRodataAddr, tigonFwRodataLen,
		    (caddr_t)tigonFwRodata);
		ti_mem(sc, tigonFwBssAddr, tigonFwBssLen, NULL);
		ti_mem(sc, tigonFwSbssAddr, tigonFwSbssLen, NULL);
		CSR_WRITE_4(sc, TI_CPU_PROGRAM_COUNTER, tigonFwStartAddr);
		break;
	case TI_HWREV_TIGON_II:
		if (tigon2FwReleaseMajor != TI_FIRMWARE_MAJOR ||
		    tigon2FwReleaseMinor != TI_FIRMWARE_MINOR ||
		    tigon2FwReleaseFix != TI_FIRMWARE_FIX) {
			printf("ti%d: firmware revision mismatch; want "
			    "%d.%d.%d, got %d.%d.%d\n", sc->ti_unit,
			    TI_FIRMWARE_MAJOR, TI_FIRMWARE_MINOR,
			    TI_FIRMWARE_FIX, tigon2FwReleaseMajor,
			    tigon2FwReleaseMinor, tigon2FwReleaseFix);
			return;
		}
		ti_mem(sc, tigon2FwTextAddr, tigon2FwTextLen,
		    (caddr_t)tigon2FwText);
		ti_mem(sc, tigon2FwDataAddr, tigon2FwDataLen,
		    (caddr_t)tigon2FwData);
		ti_mem(sc, tigon2FwRodataAddr, tigon2FwRodataLen,
		    (caddr_t)tigon2FwRodata);
		ti_mem(sc, tigon2FwBssAddr, tigon2FwBssLen, NULL);
		ti_mem(sc, tigon2FwSbssAddr, tigon2FwSbssLen, NULL);
		CSR_WRITE_4(sc, TI_CPU_PROGRAM_COUNTER, tigon2FwStartAddr);
		break;
	default:
		printf("ti%d: can't load firmware: unknown hardware rev\n",
		    sc->ti_unit);
		break;
	}

	return;
}

/*
 * Send the NIC a command via the command ring.
 */
static void ti_cmd(sc, cmd)
	struct ti_softc		*sc;
	struct ti_cmd_desc	*cmd;
{
	u_int32_t		index;

	if (sc->ti_rdata->ti_cmd_ring == NULL)
		return;

	index = sc->ti_cmd_saved_prodidx;
	CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4), *(u_int32_t *)(cmd));
	TI_INC(index, TI_CMD_RING_CNT);
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, index);
	sc->ti_cmd_saved_prodidx = index;

	return;
}

/*
 * Send the NIC an extended command. The 'len' parameter specifies the
 * number of command slots to include after the initial command.
 */
static void ti_cmd_ext(sc, cmd, arg, len)
	struct ti_softc		*sc;
	struct ti_cmd_desc	*cmd;
	caddr_t			arg;
	int			len;
{
	u_int32_t		index;
	register int		i;

	if (sc->ti_rdata->ti_cmd_ring == NULL)
		return;

	index = sc->ti_cmd_saved_prodidx;
	CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4), *(u_int32_t *)(cmd));
	TI_INC(index, TI_CMD_RING_CNT);
	for (i = 0; i < len; i++) {
		CSR_WRITE_4(sc, TI_GCR_CMDRING + (index * 4),
		    *(u_int32_t *)(&arg[i * 4]));
		TI_INC(index, TI_CMD_RING_CNT);
	}
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, index);
	sc->ti_cmd_saved_prodidx = index;

	return;
}

/*
 * Handle events that have triggered interrupts.
 */
static void ti_handle_events(sc)
	struct ti_softc		*sc;
{
	struct ti_event_desc	*e;

	if (sc->ti_rdata->ti_event_ring == NULL)
		return;

	while (sc->ti_ev_saved_considx != sc->ti_ev_prodidx.ti_idx) {
		e = &sc->ti_rdata->ti_event_ring[sc->ti_ev_saved_considx];
		switch(e->ti_event) {
		case TI_EV_LINKSTAT_CHANGED:
			sc->ti_linkstat = e->ti_code;
			if (e->ti_code == TI_EV_CODE_LINK_UP)
				printf("ti%d: 10/100 link up\n", sc->ti_unit);
			else if (e->ti_code == TI_EV_CODE_GIG_LINK_UP)
				printf("ti%d: gigabit link up\n", sc->ti_unit);
			else if (e->ti_code == TI_EV_CODE_LINK_DOWN)
				printf("ti%d: link down\n", sc->ti_unit);
			break;
		case TI_EV_ERROR:
			if (e->ti_code == TI_EV_CODE_ERR_INVAL_CMD)
				printf("ti%d: invalid command\n", sc->ti_unit);
			else if (e->ti_code == TI_EV_CODE_ERR_UNIMP_CMD)
				printf("ti%d: unknown command\n", sc->ti_unit);
			else if (e->ti_code == TI_EV_CODE_ERR_BADCFG)
				printf("ti%d: bad config data\n", sc->ti_unit);
			break;
		case TI_EV_FIRMWARE_UP:
			ti_init2(sc);
			break;
		case TI_EV_STATS_UPDATED:
			ti_stats_update(sc);
			break;
		case TI_EV_RESET_JUMBO_RING:
		case TI_EV_MCAST_UPDATED:
			/* Who cares. */
			break;
		default:
			printf("ti%d: unknown event: %d\n",
			    sc->ti_unit, e->ti_event);
			break;
		}
		/* Advance the consumer index. */
		TI_INC(sc->ti_ev_saved_considx, TI_EVENT_RING_CNT);
		CSR_WRITE_4(sc, TI_GCR_EVENTCONS_IDX, sc->ti_ev_saved_considx);
	}

	return;
}

/*
 * Memory management for the jumbo receive ring is a pain in the
 * butt. We need to allocate at least 9018 bytes of space per frame,
 * _and_ it has to be contiguous (unless you use the extended
 * jumbo descriptor format). Using malloc() all the time won't
 * work: malloc() allocates memory in powers of two, which means we
 * would end up wasting a considerable amount of space by allocating
 * 9K chunks. We don't have a jumbo mbuf cluster pool. Thus, we have
 * to do our own memory management.
 *
 * The driver needs to allocate a contiguous chunk of memory at boot
 * time. We then chop this up ourselves into 9K pieces and use them
 * as external mbuf storage.
 *
 * One issue here is how much memory to allocate. The jumbo ring has
 * 256 slots in it, but at 9K per slot than can consume over 2MB of
 * RAM. This is a bit much, especially considering we also need
 * RAM for the standard ring and mini ring (on the Tigon 2). To
 * save space, we only actually allocate enough memory for 64 slots
 * by default, which works out to between 500 and 600K. This can
 * be tuned by changing a #define in if_tireg.h.
 */

static int ti_alloc_jumbo_mem(sc)
	struct ti_softc		*sc;
{
	caddr_t			ptr;
	register int		i;
	struct ti_jpool_entry   *entry;

	/* Grab a big chunk o' storage. */
	sc->ti_cdata.ti_jumbo_buf = contigmalloc(TI_JMEM, M_DEVBUF,
		M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->ti_cdata.ti_jumbo_buf == NULL) {
		printf("ti%d: no memory for jumbo buffers!\n", sc->ti_unit);
		return(ENOBUFS);
	}

	SLIST_INIT(&sc->ti_jfree_listhead);
	SLIST_INIT(&sc->ti_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array. Note that we play an evil trick here by using
	 * the first few bytes in the buffer to hold the the address
	 * of the softc structure for this interface. This is because
	 * ti_jfree() needs it, but it is called by the mbuf management
	 * code which will not pass it to us explicitly.
	 */
	ptr = sc->ti_cdata.ti_jumbo_buf;
	for (i = 0; i < TI_JSLOTS; i++) {
		u_int64_t		**aptr;
		aptr = (u_int64_t **)ptr;
		aptr[0] = (u_int64_t *)sc;
		ptr += sizeof(u_int64_t);
		sc->ti_cdata.ti_jslots[i].ti_buf = ptr;
		sc->ti_cdata.ti_jslots[i].ti_inuse = 0;
		ptr += (TI_JLEN - sizeof(u_int64_t));
		entry = malloc(sizeof(struct ti_jpool_entry), 
			       M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			contigfree(sc->ti_cdata.ti_jumbo_buf, TI_JMEM,
			           M_DEVBUF);
			sc->ti_cdata.ti_jumbo_buf = NULL;
			printf("ti%d: no memory for jumbo "
			    "buffer queue!\n", sc->ti_unit);
			return(ENOBUFS);
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->ti_jfree_listhead, entry, jpool_entries);
	}

	return(0);
}

/*
 * Allocate a jumbo buffer.
 */
static void *ti_jalloc(sc)
	struct ti_softc		*sc;
{
	struct ti_jpool_entry   *entry;
	
	entry = SLIST_FIRST(&sc->ti_jfree_listhead);
	
	if (entry == NULL) {
		printf("ti%d: no free jumbo buffers\n", sc->ti_unit);
		return(NULL);
	}

	SLIST_REMOVE_HEAD(&sc->ti_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->ti_jinuse_listhead, entry, jpool_entries);
	sc->ti_cdata.ti_jslots[entry->slot].ti_inuse = 1;
	return(sc->ti_cdata.ti_jslots[entry->slot].ti_buf);
}

/*
 * Adjust usage count on a jumbo buffer. In general this doesn't
 * get used much because our jumbo buffers don't get passed around
 * too much, but it's implemented for correctness.
 */
static void ti_jref(buf, size)
	caddr_t			buf;
	u_int			size;
{
	struct ti_softc		*sc;
	u_int64_t		**aptr;
	register int		i;

	/* Extract the softc struct pointer. */
	aptr = (u_int64_t **)(buf - sizeof(u_int64_t));
	sc = (struct ti_softc *)(aptr[0]);

	if (sc == NULL)
		panic("ti_jref: can't find softc pointer!");

	if (size != TI_JUMBO_FRAMELEN)
		panic("ti_jref: adjusting refcount of buf of wrong size!");

	/* calculate the slot this buffer belongs to */

	i = ((vm_offset_t)aptr 
	     - (vm_offset_t)sc->ti_cdata.ti_jumbo_buf) / TI_JLEN;

	if ((i < 0) || (i >= TI_JSLOTS))
		panic("ti_jref: asked to reference buffer "
		    "that we don't manage!");
	else if (sc->ti_cdata.ti_jslots[i].ti_inuse == 0)
		panic("ti_jref: buffer already free!");
	else
		sc->ti_cdata.ti_jslots[i].ti_inuse++;

	return;
}

/*
 * Release a jumbo buffer.
 */
static void ti_jfree(buf, size)
	caddr_t			buf;
	u_int			size;
{
	struct ti_softc		*sc;
	u_int64_t		**aptr;
	int		        i;
	struct ti_jpool_entry   *entry;

	/* Extract the softc struct pointer. */
	aptr = (u_int64_t **)(buf - sizeof(u_int64_t));
	sc = (struct ti_softc *)(aptr[0]);

	if (sc == NULL)
		panic("ti_jfree: can't find softc pointer!");

	if (size != TI_JUMBO_FRAMELEN)
		panic("ti_jfree: freeing buffer of wrong size!");

	/* calculate the slot this buffer belongs to */

	i = ((vm_offset_t)aptr 
	     - (vm_offset_t)sc->ti_cdata.ti_jumbo_buf) / TI_JLEN;

	if ((i < 0) || (i >= TI_JSLOTS))
		panic("ti_jfree: asked to free buffer that we don't manage!");
	else if (sc->ti_cdata.ti_jslots[i].ti_inuse == 0)
		panic("ti_jfree: buffer already free!");
	else {
		sc->ti_cdata.ti_jslots[i].ti_inuse--;
		if(sc->ti_cdata.ti_jslots[i].ti_inuse == 0) {
			entry = SLIST_FIRST(&sc->ti_jinuse_listhead);
			if (entry == NULL)
				panic("ti_jfree: buffer not in use!");
			entry->slot = i;
			SLIST_REMOVE_HEAD(&sc->ti_jinuse_listhead, 
					  jpool_entries);
			SLIST_INSERT_HEAD(&sc->ti_jfree_listhead, 
					  entry, jpool_entries);
		}
	}

	return;
}


/*
 * Intialize a standard receive ring descriptor.
 */
static int ti_newbuf_std(sc, i, m)
	struct ti_softc		*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("ti%d: mbuf allocation failed "
			    "-- packet dropped!\n", sc->ti_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("ti%d: cluster allocation failed "
			    "-- packet dropped!\n", sc->ti_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);
	sc->ti_cdata.ti_rx_std_chain[i] = m_new;
	r = &sc->ti_rdata->ti_rx_std_ring[i];
	TI_HOSTADDR(r->ti_addr) = vtophys(mtod(m_new, caddr_t));
	r->ti_type = TI_BDTYPE_RECV_BD;
	r->ti_flags = 0;
	if (sc->arpcom.ac_if.if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return(0);
}

/*
 * Intialize a mini receive ring descriptor. This only applies to
 * the Tigon 2.
 */
static int ti_newbuf_mini(sc, i, m)
	struct ti_softc		*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("ti%d: mbuf allocation failed "
			    "-- packet dropped!\n", sc->ti_unit);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MHLEN;
	} else {
		m_new = m;
		m_new->m_data = m_new->m_pktdat;
		m_new->m_len = m_new->m_pkthdr.len = MHLEN;
	}

	m_adj(m_new, ETHER_ALIGN);
	r = &sc->ti_rdata->ti_rx_mini_ring[i];
	sc->ti_cdata.ti_rx_mini_chain[i] = m_new;
	TI_HOSTADDR(r->ti_addr) = vtophys(mtod(m_new, caddr_t));
	r->ti_type = TI_BDTYPE_RECV_BD;
	r->ti_flags = TI_BDFLAG_MINI_RING;
	if (sc->arpcom.ac_if.if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return(0);
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int ti_newbuf_jumbo(sc, i, m)
	struct ti_softc		*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		caddr_t			*buf = NULL;

		/* Allocate the mbuf. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("ti%d: mbuf allocation failed "
			    "-- packet dropped!\n", sc->ti_unit);
			return(ENOBUFS);
		}

		/* Allocate the jumbo buffer */
		buf = ti_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			printf("ti%d: jumbo allocation failed "
			    "-- packet dropped!\n", sc->ti_unit);
			return(ENOBUFS);
		}

		/* Attach the buffer to the mbuf. */
		m_new->m_data = m_new->m_ext.ext_buf = (void *)buf;
		m_new->m_flags |= M_EXT;
		m_new->m_len = m_new->m_pkthdr.len =
		    m_new->m_ext.ext_size = TI_JUMBO_FRAMELEN;
		m_new->m_ext.ext_free = ti_jfree;
		m_new->m_ext.ext_ref = ti_jref;
	} else {
		m_new = m;
		m_new->m_data = m_new->m_ext.ext_buf;
		m_new->m_ext.ext_size = TI_JUMBO_FRAMELEN;
	}

	m_adj(m_new, ETHER_ALIGN);
	/* Set up the descriptor. */
	r = &sc->ti_rdata->ti_rx_jumbo_ring[i];
	sc->ti_cdata.ti_rx_jumbo_chain[i] = m_new;
	TI_HOSTADDR(r->ti_addr) = vtophys(mtod(m_new, caddr_t));
	r->ti_type = TI_BDTYPE_RECV_JUMBO_BD;
	r->ti_flags = TI_BDFLAG_JUMBO_RING;
	if (sc->arpcom.ac_if.if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return(0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int ti_init_rx_ring_std(sc)
	struct ti_softc		*sc;
{
	register int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_SSLOTS; i++) {
		if (ti_newbuf_std(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	TI_UPDATE_STDPROD(sc, i - 1);
	sc->ti_std = i - 1;

	return(0);
}

static void ti_free_rx_ring_std(sc)
	struct ti_softc		*sc;
{
	register int		i;

	for (i = 0; i < TI_STD_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_std_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_std_chain[i]);
			sc->ti_cdata.ti_rx_std_chain[i] = NULL;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_std_ring[i],
		    sizeof(struct ti_rx_desc));
	}

	return;
}

static int ti_init_rx_ring_jumbo(sc)
	struct ti_softc		*sc;
{
	register int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_JUMBO_RX_RING_CNT; i++) {
		if (ti_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	TI_UPDATE_JUMBOPROD(sc, i - 1);
	sc->ti_jumbo = i - 1;

	return(0);
}

static void ti_free_rx_ring_jumbo(sc)
	struct ti_softc		*sc;
{
	register int		i;

	for (i = 0; i < TI_JUMBO_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_jumbo_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_jumbo_chain[i]);
			sc->ti_cdata.ti_rx_jumbo_chain[i] = NULL;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_jumbo_ring[i],
		    sizeof(struct ti_rx_desc));
	}

	return;
}

static int ti_init_rx_ring_mini(sc)
	struct ti_softc		*sc;
{
	register int		i;

	for (i = 0; i < TI_MSLOTS; i++) {
		if (ti_newbuf_mini(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	TI_UPDATE_MINIPROD(sc, i - 1);
	sc->ti_mini = i - 1;

	return(0);
}

static void ti_free_rx_ring_mini(sc)
	struct ti_softc		*sc;
{
	register int		i;

	for (i = 0; i < TI_MINI_RX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_rx_mini_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_rx_mini_chain[i]);
			sc->ti_cdata.ti_rx_mini_chain[i] = NULL;
		}
		bzero((char *)&sc->ti_rdata->ti_rx_mini_ring[i],
		    sizeof(struct ti_rx_desc));
	}

	return;
}

static void ti_free_tx_ring(sc)
	struct ti_softc		*sc;
{
	register int		i;

	if (sc->ti_rdata->ti_tx_ring == NULL)
		return;

	for (i = 0; i < TI_TX_RING_CNT; i++) {
		if (sc->ti_cdata.ti_tx_chain[i] != NULL) {
			m_freem(sc->ti_cdata.ti_tx_chain[i]);
			sc->ti_cdata.ti_tx_chain[i] = NULL;
		}
		bzero((char *)&sc->ti_rdata->ti_tx_ring[i],
		    sizeof(struct ti_tx_desc));
	}

	return;
}

static int ti_init_tx_ring(sc)
	struct ti_softc		*sc;
{
	sc->ti_txcnt = 0;
	sc->ti_tx_saved_considx = 0;
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, 0);
	return(0);
}

/*
 * The Tigon 2 firmware has a new way to add/delete multicast addresses,
 * but we have to support the old way too so that Tigon 1 cards will
 * work.
 */
void ti_add_mcast(sc, addr)
	struct ti_softc		*sc;
	struct ether_addr	*addr;
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->octet[0];

	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		CSR_WRITE_4(sc, TI_GCR_MAR0, htons(m[0]));
		CSR_WRITE_4(sc, TI_GCR_MAR1, (htons(m[1]) << 16) | htons(m[2]));
		TI_DO_CMD(TI_CMD_ADD_MCAST_ADDR, 0, 0);
		break;
	case TI_HWREV_TIGON_II:
		ext[0] = htons(m[0]);
		ext[1] = (htons(m[1]) << 16) | htons(m[2]);
		TI_DO_CMD_EXT(TI_CMD_EXT_ADD_MCAST, 0, 0, (caddr_t)&ext, 2);
		break;
	default:
		printf("ti%d: unknown hwrev\n", sc->ti_unit);
		break;
	}

	return;
}

void ti_del_mcast(sc, addr)
	struct ti_softc		*sc;
	struct ether_addr	*addr;
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->octet[0];

	switch(sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		CSR_WRITE_4(sc, TI_GCR_MAR0, htons(m[0]));
		CSR_WRITE_4(sc, TI_GCR_MAR1, (htons(m[1]) << 16) | htons(m[2]));
		TI_DO_CMD(TI_CMD_DEL_MCAST_ADDR, 0, 0);
		break;
	case TI_HWREV_TIGON_II:
		ext[0] = htons(m[0]);
		ext[1] = (htons(m[1]) << 16) | htons(m[2]);
		TI_DO_CMD_EXT(TI_CMD_EXT_DEL_MCAST, 0, 0, (caddr_t)&ext, 2);
		break;
	default:
		printf("ti%d: unknown hwrev\n", sc->ti_unit);
		break;
	}

	return;
}

/*
 * Configure the Tigon's multicast address filter.
 *
 * The actual multicast table management is a bit of a pain, thanks to
 * slight brain damage on the part of both Alteon and us. With our
 * multicast code, we are only alerted when the multicast address table
 * changes and at that point we only have the current list of addresses:
 * we only know the current state, not the previous state, so we don't
 * actually know what addresses were removed or added. The firmware has
 * state, but we can't get our grubby mits on it, and there is no 'delete
 * all multicast addresses' command. Hence, we have to maintain our own
 * state so we know what addresses have been programmed into the NIC at
 * any given time.
 */
static void ti_setmulti(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	struct ti_cmd_desc	cmd;
	struct ti_mc_entry	*mc;
	u_int32_t		intrs;

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI) {
		TI_DO_CMD(TI_CMD_SET_ALLMULTI, TI_CMD_CODE_ALLMULTI_ENB, 0);
		return;
	} else {
		TI_DO_CMD(TI_CMD_SET_ALLMULTI, TI_CMD_CODE_ALLMULTI_DIS, 0);
	}

	/* Disable interrupts. */
	intrs = CSR_READ_4(sc, TI_MB_HOSTINTR);
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	/* First, zot all the existing filters. */
	while (sc->ti_mc_listhead.slh_first != NULL) {
		mc = sc->ti_mc_listhead.slh_first;
		ti_del_mcast(sc, &mc->mc_addr);
		SLIST_REMOVE_HEAD(&sc->ti_mc_listhead, mc_entries);
		free(mc, M_DEVBUF);
	}

	/* Now program new ones. */
	for (ifma = ifp->if_multiaddrs.lh_first;
	    ifma != NULL; ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct ti_mc_entry), M_DEVBUF, M_NOWAIT);
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    (char *)&mc->mc_addr, ETHER_ADDR_LEN);
		SLIST_INSERT_HEAD(&sc->ti_mc_listhead, mc, mc_entries);
		ti_add_mcast(sc, &mc->mc_addr);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, intrs);

	return;
}

/*
 * Check to see if the BIOS has configured us for a 64 bit slot when
 * we aren't actually in one. If we detect this condition, we can work
 * around it on the Tigon 2 by setting a bit in the PCI state register,
 * but for the Tigon 1 we must give up and abort the interface attach.
 */
static int ti_64bitslot_war(sc)
	struct ti_softc		*sc;
{
	if (!(CSR_READ_4(sc, TI_PCI_STATE) & TI_PCISTATE_32BIT_BUS)) {
		CSR_WRITE_4(sc, 0x600, 0);
		CSR_WRITE_4(sc, 0x604, 0);
		CSR_WRITE_4(sc, 0x600, 0x5555AAAA);
		if (CSR_READ_4(sc, 0x604) == 0x5555AAAA) {
			if (sc->ti_hwrev == TI_HWREV_TIGON)
				return(EINVAL);
			else {
				TI_SETBIT(sc, TI_PCI_STATE,
				    TI_PCISTATE_32BIT_BUS);
				return(0);
			}
		}
	}

	return(0);
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
static int ti_chipinit(sc)
	struct ti_softc		*sc;
{
	u_int32_t		cacheline;
	u_int32_t		pci_writemax = 0;

	/* Initialize link to down state. */
	sc->ti_linkstat = TI_EV_CODE_LINK_DOWN;

	sc->arpcom.ac_if.if_hwassist = TI_CSUM_FEATURES;

	/* Set endianness before we access any non-PCI registers. */
#if BYTE_ORDER == BIG_ENDIAN
	CSR_WRITE_4(sc, TI_MISC_HOST_CTL,
	    TI_MHC_BIGENDIAN_INIT | (TI_MHC_BIGENDIAN_INIT << 24));
#else
	CSR_WRITE_4(sc, TI_MISC_HOST_CTL,
	    TI_MHC_LITTLEENDIAN_INIT | (TI_MHC_LITTLEENDIAN_INIT << 24));
#endif

	/* Check the ROM failed bit to see if self-tests passed. */
	if (CSR_READ_4(sc, TI_CPU_STATE) & TI_CPUSTATE_ROMFAIL) {
		printf("ti%d: board self-diagnostics failed!\n", sc->ti_unit);
		return(ENODEV);
	}

	/* Halt the CPU. */
	TI_SETBIT(sc, TI_CPU_STATE, TI_CPUSTATE_HALT);

	/* Figure out the hardware revision. */
	switch(CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_CHIP_REV_MASK) {
	case TI_REV_TIGON_I:
		sc->ti_hwrev = TI_HWREV_TIGON;
		break;
	case TI_REV_TIGON_II:
		sc->ti_hwrev = TI_HWREV_TIGON_II;
		break;
	default:
		printf("ti%d: unsupported chip revision\n", sc->ti_unit);
		return(ENODEV);
	}

	/* Do special setup for Tigon 2. */
	if (sc->ti_hwrev == TI_HWREV_TIGON_II) {
		TI_SETBIT(sc, TI_CPU_CTL_B, TI_CPUSTATE_HALT);
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_SRAM_BANK_512K);
		TI_SETBIT(sc, TI_MISC_CONF, TI_MCR_SRAM_SYNCHRONOUS);
	}

	/* Set up the PCI state register. */
	CSR_WRITE_4(sc, TI_PCI_STATE, TI_PCI_READ_CMD|TI_PCI_WRITE_CMD);
	if (sc->ti_hwrev == TI_HWREV_TIGON_II) {
		TI_SETBIT(sc, TI_PCI_STATE, TI_PCISTATE_USE_MEM_RD_MULT);
	}

	/* Clear the read/write max DMA parameters. */
	TI_CLRBIT(sc, TI_PCI_STATE, (TI_PCISTATE_WRITE_MAXDMA|
	    TI_PCISTATE_READ_MAXDMA));

	/* Get cache line size. */
	cacheline = CSR_READ_4(sc, TI_PCI_BIST) & 0xFF;

	/*
	 * If the system has set enabled the PCI memory write
	 * and invalidate command in the command register, set
	 * the write max parameter accordingly. This is necessary
	 * to use MWI with the Tigon 2.
	 */
	if (CSR_READ_4(sc, TI_PCI_CMDSTAT) & PCIM_CMD_MWIEN) {
		switch(cacheline) {
		case 1:
		case 4:
		case 8:
		case 16:
		case 32:
		case 64:
			break;
		default:
		/* Disable PCI memory write and invalidate. */
			if (bootverbose)
				printf("ti%d: cache line size %d not "
				    "supported; disabling PCI MWI\n",
				    sc->ti_unit, cacheline);
			CSR_WRITE_4(sc, TI_PCI_CMDSTAT, CSR_READ_4(sc,
			    TI_PCI_CMDSTAT) & ~PCIM_CMD_MWIEN);
			break;
		}
	}

#ifdef __brokenalpha__
	/*
	 * From the Alteon sample driver:
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a 
	 * restriction on some ALPHA platforms with early revision 
	 * 21174 PCI chipsets, such as the AlphaPC 164lx 
	 */
	TI_SETBIT(sc, TI_PCI_STATE, pci_writemax|TI_PCI_READMAX_1024);
#else
	TI_SETBIT(sc, TI_PCI_STATE, pci_writemax);
#endif

	/* This sets the min dma param all the way up (0xff). */
	TI_SETBIT(sc, TI_PCI_STATE, TI_PCISTATE_MINDMA);

	/* Configure DMA variables. */
#if BYTE_ORDER == BIG_ENDIAN
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_BD |
	    TI_OPMODE_BYTESWAP_DATA | TI_OPMODE_WORDSWAP_BD |
	    TI_OPMODE_WARN_ENB | TI_OPMODE_FATAL_ENB |
	    TI_OPMODE_DONT_FRAG_JUMBO);
#else
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_DATA|
	    TI_OPMODE_WORDSWAP_BD|TI_OPMODE_DONT_FRAG_JUMBO|
	    TI_OPMODE_WARN_ENB|TI_OPMODE_FATAL_ENB);
#endif

	/*
	 * Only allow 1 DMA channel to be active at a time.
	 * I don't think this is a good idea, but without it
	 * the firmware racks up lots of nicDmaReadRingFull
	 * errors.  This is not compatible with hardware checksums.
	 */
	if (sc->arpcom.ac_if.if_hwassist == 0)
		TI_SETBIT(sc, TI_GCR_OPMODE, TI_OPMODE_1_DMA_ACTIVE);

	/* Recommended settings from Tigon manual. */
	CSR_WRITE_4(sc, TI_GCR_DMA_WRITECFG, TI_DMA_STATE_THRESH_8W);
	CSR_WRITE_4(sc, TI_GCR_DMA_READCFG, TI_DMA_STATE_THRESH_8W);

	if (ti_64bitslot_war(sc)) {
		printf("ti%d: bios thinks we're in a 64 bit slot, "
		    "but we aren't", sc->ti_unit);
		return(EINVAL);
	}

	return(0);
}

/*
 * Initialize the general information block and firmware, and
 * start the CPU(s) running.
 */
static int ti_gibinit(sc)
	struct ti_softc		*sc;
{
	struct ti_rcb		*rcb;
	int			i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Disable interrupts for now. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	/* Tell the chip where to find the general information block. */
	CSR_WRITE_4(sc, TI_GCR_GENINFO_HI, 0);
	CSR_WRITE_4(sc, TI_GCR_GENINFO_LO, vtophys(&sc->ti_rdata->ti_info));

	/* Load the firmware into SRAM. */
	ti_loadfw(sc);

	/* Set up the contents of the general info and ring control blocks. */

	/* Set up the event ring and producer pointer. */
	rcb = &sc->ti_rdata->ti_info.ti_ev_rcb;

	TI_HOSTADDR(rcb->ti_hostaddr) = vtophys(&sc->ti_rdata->ti_event_ring);
	rcb->ti_flags = 0;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_ev_prodidx_ptr) =
	    vtophys(&sc->ti_ev_prodidx);
	sc->ti_ev_prodidx.ti_idx = 0;
	CSR_WRITE_4(sc, TI_GCR_EVENTCONS_IDX, 0);
	sc->ti_ev_saved_considx = 0;

	/* Set up the command ring and producer mailbox. */
	rcb = &sc->ti_rdata->ti_info.ti_cmd_rcb;

	sc->ti_rdata->ti_cmd_ring =
	    (struct ti_cmd_desc *)(sc->ti_vhandle + TI_GCR_CMDRING);
	TI_HOSTADDR(rcb->ti_hostaddr) = TI_GCR_NIC_ADDR(TI_GCR_CMDRING);
	rcb->ti_flags = 0;
	rcb->ti_max_len = 0;
	for (i = 0; i < TI_CMD_RING_CNT; i++) {
		CSR_WRITE_4(sc, TI_GCR_CMDRING + (i * 4), 0);
	}
	CSR_WRITE_4(sc, TI_GCR_CMDCONS_IDX, 0);
	CSR_WRITE_4(sc, TI_MB_CMDPROD_IDX, 0);
	sc->ti_cmd_saved_prodidx = 0;

	/*
	 * Assign the address of the stats refresh buffer.
	 * We re-use the current stats buffer for this to
	 * conserve memory.
	 */
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_refresh_stats_ptr) =
	    vtophys(&sc->ti_rdata->ti_info.ti_stats);

	/* Set up the standard receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_std_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = vtophys(&sc->ti_rdata->ti_rx_std_ring);
	rcb->ti_max_len = TI_FRAMELEN;
	rcb->ti_flags = 0;
	if (sc->arpcom.ac_if.if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/* Set up the jumbo receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_jumbo_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) =
	    vtophys(&sc->ti_rdata->ti_rx_jumbo_ring);
	rcb->ti_max_len = TI_JUMBO_FRAMELEN;
	rcb->ti_flags = 0;
	if (sc->arpcom.ac_if.if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/*
	 * Set up the mini ring. Only activated on the
	 * Tigon 2 but the slot in the config block is
	 * still there on the Tigon 1.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_mini_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) =
	    vtophys(&sc->ti_rdata->ti_rx_mini_ring);
	rcb->ti_max_len = MHLEN - ETHER_ALIGN;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		rcb->ti_flags = TI_RCB_FLAG_RING_DISABLED;
	else
		rcb->ti_flags = 0;
	if (sc->arpcom.ac_if.if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/*
	 * Set up the receive return ring.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_return_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) =
	    vtophys(&sc->ti_rdata->ti_rx_return_ring);
	rcb->ti_flags = 0;
	rcb->ti_max_len = TI_RETURN_RING_CNT;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_return_prodidx_ptr) =
	    vtophys(&sc->ti_return_prodidx);

	/*
	 * Set up the tx ring. Note: for the Tigon 2, we have the option
	 * of putting the transmit ring in the host's address space and
	 * letting the chip DMA it instead of leaving the ring in the NIC's
	 * memory and accessing it through the shared memory region. We
	 * do this for the Tigon 2, but it doesn't work on the Tigon 1,
	 * so we have to revert to the shared memory scheme if we detect
	 * a Tigon 1 chip.
	 */
	CSR_WRITE_4(sc, TI_WINBASE, TI_TX_RING_BASE);
	if (sc->ti_hwrev == TI_HWREV_TIGON) {
		sc->ti_rdata->ti_tx_ring_nic =
		    (struct ti_tx_desc *)(sc->ti_vhandle + TI_WINDOW);
	}
	bzero((char *)sc->ti_rdata->ti_tx_ring,
	    TI_TX_RING_CNT * sizeof(struct ti_tx_desc));
	rcb = &sc->ti_rdata->ti_info.ti_tx_rcb;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		rcb->ti_flags = 0;
	else
		rcb->ti_flags = TI_RCB_FLAG_HOST_RING;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;
	if (sc->arpcom.ac_if.if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_max_len = TI_TX_RING_CNT;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		TI_HOSTADDR(rcb->ti_hostaddr) = TI_TX_RING_BASE;
	else
		TI_HOSTADDR(rcb->ti_hostaddr) =
		    vtophys(&sc->ti_rdata->ti_tx_ring);
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_tx_considx_ptr) =
	    vtophys(&sc->ti_tx_considx);

	/* Set up tuneables */
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		CSR_WRITE_4(sc, TI_GCR_RX_COAL_TICKS,
		    (sc->ti_rx_coal_ticks / 10));
	else
		CSR_WRITE_4(sc, TI_GCR_RX_COAL_TICKS, sc->ti_rx_coal_ticks);
	CSR_WRITE_4(sc, TI_GCR_TX_COAL_TICKS, sc->ti_tx_coal_ticks);
	CSR_WRITE_4(sc, TI_GCR_STAT_TICKS, sc->ti_stat_ticks);
	CSR_WRITE_4(sc, TI_GCR_RX_MAX_COAL_BD, sc->ti_rx_max_coal_bds);
	CSR_WRITE_4(sc, TI_GCR_TX_MAX_COAL_BD, sc->ti_tx_max_coal_bds);
	CSR_WRITE_4(sc, TI_GCR_TX_BUFFER_RATIO, sc->ti_tx_buf_ratio);

	/* Turn interrupts on. */
	CSR_WRITE_4(sc, TI_GCR_MASK_INTRS, 0);
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	/* Start CPU. */
	TI_CLRBIT(sc, TI_CPU_STATE, (TI_CPUSTATE_HALT|TI_CPUSTATE_STEP));

	return(0);
}

/*
 * Probe for a Tigon chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match.
 */
static int ti_probe(dev)
	device_t		dev;
{
	struct ti_type		*t;

	t = ti_devs;

	while(t->ti_name != NULL) {
		if ((pci_get_vendor(dev) == t->ti_vid) &&
		    (pci_get_device(dev) == t->ti_did)) {
			device_set_desc(dev, t->ti_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

static int ti_attach(dev)
	device_t		dev;
{
	int			s;
	u_int32_t		command;
	struct ifnet		*ifp;
	struct ti_softc		*sc;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct ti_softc));

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

	if (!(command & PCIM_CMD_MEMEN)) {
		printf("ti%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}

	rid = TI_PCI_LOMEM;
	sc->ti_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->ti_res == NULL) {
		printf ("ti%d: couldn't map memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->ti_btag = rman_get_bustag(sc->ti_res);
	sc->ti_bhandle = rman_get_bushandle(sc->ti_res);
	sc->ti_vhandle = (vm_offset_t)rman_get_virtual(sc->ti_res);

	/*
	 * XXX FIXME: rman_get_virtual() on the alpha is currently
	 * broken and returns a physical address instead of a kernel
	 * virtual address. Consequently, we need to do a little
	 * extra mangling of the vhandle on the alpha. This should
	 * eventually be fixed! The whole idea here is to get rid
	 * of platform dependencies.
	 */
#ifdef __alpha__
	if (pci_cvt_to_bwx(sc->ti_vhandle))
		sc->ti_vhandle = pci_cvt_to_bwx(sc->ti_vhandle);
	else
		sc->ti_vhandle = pci_cvt_to_dense(sc->ti_vhandle);
	sc->ti_vhandle = ALPHA_PHYS_TO_K0SEG(sc->ti_vhandle);
#endif

	/* Allocate interrupt */
	rid = 0;
	
	sc->ti_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->ti_irq == NULL) {
		printf("ti%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->ti_irq, INTR_TYPE_NET,
	   ti_intr, sc, &sc->ti_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		printf("ti%d: couldn't set up irq\n", unit);
		goto fail;
	}

	sc->ti_unit = unit;

	if (ti_chipinit(sc)) {
		printf("ti%d: chip initialization failed\n", sc->ti_unit);
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		error = ENXIO;
		goto fail;
	}

	/* Zero out the NIC's on-board SRAM. */
	ti_mem(sc, 0x2000, 0x100000 - 0x2000,  NULL);

	/* Init again -- zeroing memory may have clobbered some registers. */
	if (ti_chipinit(sc)) {
		printf("ti%d: chip initialization failed\n", sc->ti_unit);
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Get station address from the EEPROM. Note: the manual states
	 * that the MAC address is at offset 0x8c, however the data is
	 * stored as two longwords (since that's how it's loaded into
	 * the NIC). This means the MAC address is actually preceeded
	 * by two zero bytes. We need to skip over those.
	 */
	if (ti_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
				TI_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN)) {
		printf("ti%d: failed to read station address\n", unit);
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		error = ENXIO;
		goto fail;
	}

	/*
	 * A Tigon chip was detected. Inform the world.
	 */
	printf("ti%d: Ethernet address: %6D\n", unit,
				sc->arpcom.ac_enaddr, ":");

	/* Allocate the general information block and ring buffers. */
	sc->ti_rdata = contigmalloc(sizeof(struct ti_ring_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->ti_rdata == NULL) {
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		error = ENXIO;
		printf("ti%d: no memory for list buffers!\n", sc->ti_unit);
		goto fail;
	}

	bzero(sc->ti_rdata, sizeof(struct ti_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (ti_alloc_jumbo_mem(sc)) {
		printf("ti%d: jumbo buffer allocation failed\n", sc->ti_unit);
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    TI_PCI_LOMEM, sc->ti_res);
		contigfree(sc->ti_rdata, sizeof(struct ti_ring_data),
		    M_DEVBUF);
		error = ENXIO;
		goto fail;
	}

	/*
	 * We really need a better way to tell a 1000baseTX card
	 * from a 1000baseSX one, since in theory there could be
	 * OEMed 1000baseTX cards from lame vendors who aren't
	 * clever enough to change the PCI ID. For the moment
	 * though, the AceNIC is the only copper card available.
	 */
	if (pci_get_vendor(dev) == ALT_VENDORID &&
	    pci_get_device(dev) == ALT_DEVICEID_ACENIC_COPPER)
		sc->ti_copper = 1;
	/* Ok, it's not the only copper card available. */
	if (pci_get_vendor(dev) == NG_VENDORID &&
	    pci_get_device(dev) == NG_DEVICEID_GA620T)
		sc->ti_copper = 1;

	/* Set default tuneable values. */
	sc->ti_stat_ticks = 2 * TI_TICKS_PER_SEC;
	sc->ti_rx_coal_ticks = TI_TICKS_PER_SEC / 5000;
	sc->ti_tx_coal_ticks = TI_TICKS_PER_SEC / 500;
	sc->ti_rx_max_coal_bds = 64;
	sc->ti_tx_max_coal_bds = 128;
	sc->ti_tx_buf_ratio = 21;

	/* Set up ifnet structure */
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = sc->ti_unit;
	ifp->if_name = "ti";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ti_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = ti_start;
	ifp->if_watchdog = ti_watchdog;
	ifp->if_init = ti_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = TI_TX_RING_CNT - 1;

	/* Set up ifmedia support. */
	ifmedia_init(&sc->ifmedia, IFM_IMASK, ti_ifmedia_upd, ti_ifmedia_sts);
	if (sc->ti_copper) {
		/*
		 * Copper cards allow manual 10/100 mode selection,
		 * but not manual 1000baseTX mode selection. Why?
		 * Becuase currently there's no way to specify the
		 * master/slave setting through the firmware interface,
		 * so Alteon decided to just bag it and handle it
		 * via autonegotiation.
		 */
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_TX|IFM_FDX, 0, NULL);
	} else {
		/* Fiber cards don't support 10/100 modes. */
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
	}
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

fail:
	splx(s);

	return(error);
}

static int ti_detach(dev)
	device_t		dev;
{
	struct ti_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	ti_stop(sc);

	bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
	bus_release_resource(dev, SYS_RES_MEMORY, TI_PCI_LOMEM, sc->ti_res);

	contigfree(sc->ti_cdata.ti_jumbo_buf, TI_JMEM, M_DEVBUF);
	contigfree(sc->ti_rdata, sizeof(struct ti_ring_data), M_DEVBUF);
	ifmedia_removeall(&sc->ifmedia);

	splx(s);

	return(0);
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle three possibilities here:
 * 1) the frame is from the mini receive ring (can only happen)
 *    on Tigon 2 boards)
 * 2) the frame is from the jumbo recieve ring
 * 3) the frame is from the standard receive ring
 */

static void ti_rxeof(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	ifp = &sc->arpcom.ac_if;

	while(sc->ti_rx_saved_considx != sc->ti_return_prodidx.ti_idx) {
		struct ti_rx_desc	*cur_rx;
		u_int32_t		rxidx;
		struct ether_header	*eh;
		struct mbuf		*m = NULL;
		u_int16_t		vlan_tag = 0;
		int			have_tag = 0;

		cur_rx =
		    &sc->ti_rdata->ti_rx_return_ring[sc->ti_rx_saved_considx];
		rxidx = cur_rx->ti_idx;
		TI_INC(sc->ti_rx_saved_considx, TI_RETURN_RING_CNT);

		if (cur_rx->ti_flags & TI_BDFLAG_VLAN_TAG) {
			have_tag = 1;
			vlan_tag = cur_rx->ti_vlan_tag & 0xfff;
		}

		if (cur_rx->ti_flags & TI_BDFLAG_JUMBO_RING) {
			TI_INC(sc->ti_jumbo, TI_JUMBO_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_jumbo_chain[rxidx];
			sc->ti_cdata.ti_rx_jumbo_chain[rxidx] = NULL;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_jumbo(sc, sc->ti_jumbo, m);
				continue;
			}
			if (ti_newbuf_jumbo(sc, sc->ti_jumbo, NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				ti_newbuf_jumbo(sc, sc->ti_jumbo, m);
				continue;
			}
		} else if (cur_rx->ti_flags & TI_BDFLAG_MINI_RING) {
			TI_INC(sc->ti_mini, TI_MINI_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_mini_chain[rxidx];
			sc->ti_cdata.ti_rx_mini_chain[rxidx] = NULL;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_mini(sc, sc->ti_mini, m);
				continue;
			}
			if (ti_newbuf_mini(sc, sc->ti_mini, NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				ti_newbuf_mini(sc, sc->ti_mini, m);
				continue;
			}
		} else {
			TI_INC(sc->ti_std, TI_STD_RX_RING_CNT);
			m = sc->ti_cdata.ti_rx_std_chain[rxidx];
			sc->ti_cdata.ti_rx_std_chain[rxidx] = NULL;
			if (cur_rx->ti_flags & TI_BDFLAG_ERROR) {
				ifp->if_ierrors++;
				ti_newbuf_std(sc, sc->ti_std, m);
				continue;
			}
			if (ti_newbuf_std(sc, sc->ti_std, NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				ti_newbuf_std(sc, sc->ti_std, m);
				continue;
			}
		}

		m->m_pkthdr.len = m->m_len = cur_rx->ti_len;
		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));

		if (ifp->if_hwassist) {
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED |
			    CSUM_DATA_VALID;
			if ((cur_rx->ti_ip_cksum ^ 0xffff) == 0)
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			m->m_pkthdr.csum_data = cur_rx->ti_tcp_udp_cksum;
		}

		/*
		 * If we received a packet with a vlan tag, pass it
		 * to vlan_input() instead of ether_input().
		 */
		if (have_tag) {
			VLAN_INPUT_TAG(eh, m, vlan_tag);
			have_tag = vlan_tag = 0;
			continue;
		}
		ether_input(ifp, eh, m);
	}

	/* Only necessary on the Tigon 1. */
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		CSR_WRITE_4(sc, TI_GCR_RXRETURNCONS_IDX,
		    sc->ti_rx_saved_considx);

	TI_UPDATE_STDPROD(sc, sc->ti_std);
	TI_UPDATE_MINIPROD(sc, sc->ti_mini);
	TI_UPDATE_JUMBOPROD(sc, sc->ti_jumbo);

	return;
}

static void ti_txeof(sc)
	struct ti_softc		*sc;
{
	struct ti_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->ti_tx_saved_considx != sc->ti_tx_considx.ti_idx) {
		u_int32_t		idx = 0;

		idx = sc->ti_tx_saved_considx;
		if (sc->ti_hwrev == TI_HWREV_TIGON) {
			if (idx > 383)
				CSR_WRITE_4(sc, TI_WINBASE,
				    TI_TX_RING_BASE + 6144);
			else if (idx > 255)
				CSR_WRITE_4(sc, TI_WINBASE,
				    TI_TX_RING_BASE + 4096);
			else if (idx > 127)
				CSR_WRITE_4(sc, TI_WINBASE,
				    TI_TX_RING_BASE + 2048);
			else
				CSR_WRITE_4(sc, TI_WINBASE,
				    TI_TX_RING_BASE);
			cur_tx = &sc->ti_rdata->ti_tx_ring_nic[idx % 128];
		} else
			cur_tx = &sc->ti_rdata->ti_tx_ring[idx];
		if (cur_tx->ti_flags & TI_BDFLAG_END)
			ifp->if_opackets++;
		if (sc->ti_cdata.ti_tx_chain[idx] != NULL) {
			m_freem(sc->ti_cdata.ti_tx_chain[idx]);
			sc->ti_cdata.ti_tx_chain[idx] = NULL;
		}
		sc->ti_txcnt--;
		TI_INC(sc->ti_tx_saved_considx, TI_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void ti_intr(xsc)
	void			*xsc;
{
	struct ti_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

#ifdef notdef
	/* Avoid this for now -- checking this register is expensive. */
	/* Make sure this is really our interrupt. */
	if (!(CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_INTSTATE))
		return;
#endif

	/* Ack interrupt and stop others from occuring. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	if (ifp->if_flags & IFF_RUNNING) {
		/* Check RX return ring producer/consumer */
		ti_rxeof(sc);

		/* Check TX ring producer/consumer */
		ti_txeof(sc);
	}

	ti_handle_events(sc);

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		ti_start(ifp);

	return;
}

static void ti_stats_update(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_collisions +=
	   (sc->ti_rdata->ti_info.ti_stats.dot3StatsSingleCollisionFrames +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsMultipleCollisionFrames +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsExcessiveCollisions +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsLateCollisions) -
	   ifp->if_collisions;

	return;
}

/*
 * Encapsulate an mbuf chain in the tx ring  by coupling the mbuf data
 * pointers to descriptors.
 */
static int ti_encap(sc, m_head, txidx)
	struct ti_softc		*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct ti_tx_desc	*f = NULL;
	struct mbuf		*m;
	u_int32_t		frag, cur, cnt = 0;
	u_int16_t		csum_flags = 0;
	struct ifvlan		*ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL &&
	    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
		ifv = m_head->m_pkthdr.rcvif->if_softc;

	m = m_head;
	cur = frag = *txidx;

	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= TI_BDFLAG_IP_CKSUM;
		if (m_head->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP))
			csum_flags |= TI_BDFLAG_TCP_UDP_CKSUM;
		if (m_head->m_flags & M_LASTFRAG)
			csum_flags |= TI_BDFLAG_IP_FRAG_END;
		else if (m_head->m_flags & M_FRAG)
			csum_flags |= TI_BDFLAG_IP_FRAG;
	}
	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (sc->ti_hwrev == TI_HWREV_TIGON) {
				if (frag > 383)
					CSR_WRITE_4(sc, TI_WINBASE,
					    TI_TX_RING_BASE + 6144);
				else if (frag > 255)
					CSR_WRITE_4(sc, TI_WINBASE,
					    TI_TX_RING_BASE + 4096);
				else if (frag > 127)
					CSR_WRITE_4(sc, TI_WINBASE,
					    TI_TX_RING_BASE + 2048);
				else
					CSR_WRITE_4(sc, TI_WINBASE,
					    TI_TX_RING_BASE);
				f = &sc->ti_rdata->ti_tx_ring_nic[frag % 128];
			} else
				f = &sc->ti_rdata->ti_tx_ring[frag];
			if (sc->ti_cdata.ti_tx_chain[frag] != NULL)
				break;
			TI_HOSTADDR(f->ti_addr) = vtophys(mtod(m, vm_offset_t));
			f->ti_len = m->m_len;
			f->ti_flags = csum_flags;

			if (ifv != NULL) {
				f->ti_flags |= TI_BDFLAG_VLAN_TAG;
				f->ti_vlan_tag = ifv->ifv_tag & 0xfff;
			} else {
				f->ti_vlan_tag = 0;
			}

			/*
			 * Sanity check: avoid coming within 16 descriptors
			 * of the end of the ring.
			 */
			if ((TI_TX_RING_CNT - (sc->ti_txcnt + cnt)) < 16)
				return(ENOBUFS);
			cur = frag;
			TI_INC(frag, TI_TX_RING_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	if (frag == sc->ti_tx_saved_considx)
		return(ENOBUFS);

	if (sc->ti_hwrev == TI_HWREV_TIGON)
		sc->ti_rdata->ti_tx_ring_nic[cur % 128].ti_flags |=
		    TI_BDFLAG_END;
	else
		sc->ti_rdata->ti_tx_ring[cur].ti_flags |= TI_BDFLAG_END;
	sc->ti_cdata.ti_tx_chain[cur] = m_head;
	sc->ti_txcnt += cnt;

	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void ti_start(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		prodidx = 0;

	sc = ifp->if_softc;

	prodidx = CSR_READ_4(sc, TI_MB_SENDPROD_IDX);

	while(sc->ti_cdata.ti_tx_chain[prodidx] == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * XXX
		 * safety overkill.  If this is a fragmented packet chain
		 * with delayed TCP/UDP checksums, then only encapsulate
		 * it if we have enough descriptors to handle the entire
		 * chain at once.
		 * (paranoia -- may not actually be needed)
		 */
		if (m_head->m_flags & M_FIRSTFRAG &&
		    m_head->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
			if ((TI_TX_RING_CNT - sc->ti_txcnt) <
			    m_head->m_pkthdr.csum_data + 16) {
				IF_PREPEND(&ifp->if_snd, m_head);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		}

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (ti_encap(sc, m_head, &prodidx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
	}

	/* Transmit */
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, prodidx);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void ti_init(xsc)
	void			*xsc;
{
	struct ti_softc		*sc = xsc;
        int			s;

	s = splimp();

	/* Cancel pending I/O and flush buffers. */
	ti_stop(sc);

	/* Init the gen info block, ring control blocks and firmware. */
	if (ti_gibinit(sc)) {
		printf("ti%d: initialization failure\n", sc->ti_unit);
		splx(s);
		return;
	}

	splx(s);

	return;
}

static void ti_init2(sc)
	struct ti_softc		*sc;
{
	struct ti_cmd_desc	cmd;
	struct ifnet		*ifp;
	u_int16_t		*m;
	struct ifmedia		*ifm;
	int			tmp;

	ifp = &sc->arpcom.ac_if;

	/* Specify MTU and interface index. */
	CSR_WRITE_4(sc, TI_GCR_IFINDEX, ifp->if_unit);
	CSR_WRITE_4(sc, TI_GCR_IFMTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN);
	TI_DO_CMD(TI_CMD_UPDATE_GENCOM, 0, 0);

	/* Load our MAC address. */
	m = (u_int16_t *)&sc->arpcom.ac_enaddr[0];
	CSR_WRITE_4(sc, TI_GCR_PAR0, htons(m[0]));
	CSR_WRITE_4(sc, TI_GCR_PAR1, (htons(m[1]) << 16) | htons(m[2]));
	TI_DO_CMD(TI_CMD_SET_MAC_ADDR, 0, 0);

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC) {
		TI_DO_CMD(TI_CMD_SET_PROMISC_MODE, TI_CMD_CODE_PROMISC_ENB, 0);
	} else {
		TI_DO_CMD(TI_CMD_SET_PROMISC_MODE, TI_CMD_CODE_PROMISC_DIS, 0);
	}

	/* Program multicast filter. */
	ti_setmulti(sc);

	/*
	 * If this is a Tigon 1, we should tell the
	 * firmware to use software packet filtering.
	 */
	if (sc->ti_hwrev == TI_HWREV_TIGON) {
		TI_DO_CMD(TI_CMD_FDR_FILTERING, TI_CMD_CODE_FILT_ENB, 0);
	}

	/* Init RX ring. */
	ti_init_rx_ring_std(sc);

	/* Init jumbo RX ring. */
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		ti_init_rx_ring_jumbo(sc);

	/*
	 * If this is a Tigon 2, we can also configure the
	 * mini ring.
	 */
	if (sc->ti_hwrev == TI_HWREV_TIGON_II)
		ti_init_rx_ring_mini(sc);

	CSR_WRITE_4(sc, TI_GCR_RXRETURNCONS_IDX, 0);
	sc->ti_rx_saved_considx = 0;

	/* Init TX ring. */
	ti_init_tx_ring(sc);

	/* Tell firmware we're alive. */
	TI_DO_CMD(TI_CMD_HOST_STATE, TI_CMD_CODE_STACK_UP, 0);

	/* Enable host interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Make sure to set media properly. We have to do this
	 * here since we have to issue commands in order to set
	 * the link negotiation and we can't issue commands until
	 * the firmware is running.
	 */
	ifm = &sc->ifmedia;
	tmp = ifm->ifm_media;
	ifm->ifm_media = ifm->ifm_cur->ifm_media;
	ti_ifmedia_upd(ifp);
	ifm->ifm_media = tmp;

	return;
}

/*
 * Set media options.
 */
static int ti_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;
	struct ifmedia		*ifm;
	struct ti_cmd_desc	cmd;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    TI_GLNK_FULL_DUPLEX|TI_GLNK_RX_FLOWCTL_Y|
		    TI_GLNK_AUTONEGENB|TI_GLNK_ENB);
		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_100MB|TI_LNK_10MB|
		    TI_LNK_FULL_DUPLEX|TI_LNK_HALF_DUPLEX|
		    TI_LNK_AUTONEGENB|TI_LNK_ENB);
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_BOTH, 0);
		break;
	case IFM_1000_SX:
	case IFM_1000_TX:
		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    TI_GLNK_RX_FLOWCTL_Y|TI_GLNK_ENB);
		CSR_WRITE_4(sc, TI_GCR_LINK, 0);
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			TI_SETBIT(sc, TI_GCR_GLINK, TI_GLNK_FULL_DUPLEX);
		}
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_GIGABIT, 0);
		break;
	case IFM_100_FX:
	case IFM_10_FL:
	case IFM_100_TX:
	case IFM_10_T:
		CSR_WRITE_4(sc, TI_GCR_GLINK, 0);
		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_ENB|TI_LNK_PREF);
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_FX ||
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_100MB);
		} else {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_10MB);
		}
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_FULL_DUPLEX);
		} else {
			TI_SETBIT(sc, TI_GCR_LINK, TI_LNK_HALF_DUPLEX);
		}
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_10_100, 0);
		break;
	}

	return(0);
}

/*
 * Report current media status.
 */
static void ti_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct ti_softc		*sc;
	u_int32_t		media = 0;

	sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (sc->ti_linkstat == TI_EV_CODE_LINK_DOWN)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->ti_linkstat == TI_EV_CODE_GIG_LINK_UP) {
		media = CSR_READ_4(sc, TI_GCR_GLINK_STAT);
		if (sc->ti_copper)
			ifmr->ifm_active |= IFM_1000_TX;
		else
			ifmr->ifm_active |= IFM_1000_SX;
		if (media & TI_GLNK_FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	} else if (sc->ti_linkstat == TI_EV_CODE_LINK_UP) {
		media = CSR_READ_4(sc, TI_GCR_LINK_STAT);
		if (sc->ti_copper) {
			if (media & TI_LNK_100MB)
				ifmr->ifm_active |= IFM_100_TX;
			if (media & TI_LNK_10MB)
				ifmr->ifm_active |= IFM_10_T;
		} else {
			if (media & TI_LNK_100MB)
				ifmr->ifm_active |= IFM_100_FX;
			if (media & TI_LNK_10MB)
				ifmr->ifm_active |= IFM_10_FL;
		}
		if (media & TI_LNK_FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		if (media & TI_LNK_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
	}
	
	return;
}

static int ti_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct ti_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;
	struct ti_cmd_desc	cmd;

	s = splimp();

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > TI_JUMBO_MTU)
			error = EINVAL;
		else {
			ifp->if_mtu = ifr->ifr_mtu;
			ti_init(sc);
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.
			 */
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ti_if_flags & IFF_PROMISC)) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_ENB, 0);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ti_if_flags & IFF_PROMISC) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_DIS, 0);
			} else
				ti_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				ti_stop(sc);
			}
		}
		sc->ti_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_RUNNING) {
			ti_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void ti_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;

	sc = ifp->if_softc;

	printf("ti%d: watchdog timeout -- resetting\n", sc->ti_unit);
	ti_stop(sc);
	ti_init(sc);

	ifp->if_oerrors++;

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void ti_stop(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	ifp = &sc->arpcom.ac_if;

	/* Disable host interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);
	/*
	 * Tell firmware we're shutting down.
	 */
	TI_DO_CMD(TI_CMD_HOST_STATE, TI_CMD_CODE_STACK_DOWN, 0);

	/* Halt and reinitialize. */
	ti_chipinit(sc);
	ti_mem(sc, 0x2000, 0x100000 - 0x2000, NULL);
	ti_chipinit(sc);

	/* Free the RX lists. */
	ti_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	ti_free_rx_ring_jumbo(sc);

	/* Free mini RX list. */
	ti_free_rx_ring_mini(sc);

	/* Free TX buffers. */
	ti_free_tx_ring(sc);

	sc->ti_ev_prodidx.ti_idx = 0;
	sc->ti_return_prodidx.ti_idx = 0;
	sc->ti_tx_considx.ti_idx = 0;
	sc->ti_tx_saved_considx = TI_TXCONS_UNSET;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void ti_shutdown(dev)
	device_t		dev;
{
	struct ti_softc		*sc;

	sc = device_get_softc(dev);

	ti_chipinit(sc);

	return;
}
