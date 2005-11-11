/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ti.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/conf.h>

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

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

/* #define TI_PRIVATE_JUMBOS */

#if !defined(TI_PRIVATE_JUMBOS)
#include <sys/sockio.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/sf_buf.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <sys/vmmeter.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <sys/proc.h>
#endif /* !TI_PRIVATE_JUMBOS */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/tiio.h>
#include <pci/if_tireg.h>
#include <pci/ti_fw.h>
#include <pci/ti_fw2.h>

#define TI_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_IP_FRAGS)
/*
 * We can only turn on header splitting if we're using extended receive
 * BDs.
 */
#if defined(TI_JUMBO_HDRSPLIT) && defined(TI_PRIVATE_JUMBOS)
#error "options TI_JUMBO_HDRSPLIT and TI_PRIVATE_JUMBOS are mutually exclusive"
#endif /* TI_JUMBO_HDRSPLIT && TI_JUMBO_HDRSPLIT */

struct ti_softc *tis[8];

typedef enum {
	TI_SWAP_HTON,
	TI_SWAP_NTOH
} ti_swap_type;


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


static	d_open_t	ti_open;
static	d_close_t	ti_close;
static	d_ioctl_t	ti_ioctl2;

static struct cdevsw ti_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ti_open,
	.d_close =	ti_close,
	.d_ioctl =	ti_ioctl2,
	.d_name =	"ti",
};

static int ti_probe(device_t);
static int ti_attach(device_t);
static int ti_detach(device_t);
static void ti_txeof(struct ti_softc *);
static void ti_rxeof(struct ti_softc *);

static void ti_stats_update(struct ti_softc *);
static int ti_encap(struct ti_softc *, struct mbuf *, u_int32_t *);

static void ti_intr(void *);
static void ti_start(struct ifnet *);
static int ti_ioctl(struct ifnet *, u_long, caddr_t);
static void ti_init(void *);
static void ti_init2(struct ti_softc *);
static void ti_stop(struct ti_softc *);
static void ti_watchdog(struct ifnet *);
static void ti_shutdown(device_t);
static int ti_ifmedia_upd(struct ifnet *);
static void ti_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static u_int32_t ti_eeprom_putbyte(struct ti_softc *, int);
static u_int8_t	ti_eeprom_getbyte(struct ti_softc *, int, u_int8_t *);
static int ti_read_eeprom(struct ti_softc *, caddr_t, int, int);

static void ti_add_mcast(struct ti_softc *, struct ether_addr *);
static void ti_del_mcast(struct ti_softc *, struct ether_addr *);
static void ti_setmulti(struct ti_softc *);

static void ti_mem(struct ti_softc *, u_int32_t, u_int32_t, caddr_t);
static int ti_copy_mem(struct ti_softc *, u_int32_t, u_int32_t, caddr_t, int, int);
static int ti_copy_scratch(struct ti_softc *, u_int32_t, u_int32_t, caddr_t,
		int, int, int);
static int ti_bcopy_swap(const void *, void *, size_t, ti_swap_type);
static void ti_loadfw(struct ti_softc *);
static void ti_cmd(struct ti_softc *, struct ti_cmd_desc *);
static void ti_cmd_ext(struct ti_softc *, struct ti_cmd_desc *, caddr_t, int);
static void ti_handle_events(struct ti_softc *);
#ifdef TI_PRIVATE_JUMBOS
static int ti_alloc_jumbo_mem(struct ti_softc *);
static void *ti_jalloc(struct ti_softc *);
static void ti_jfree(void *, void *);
#endif /* TI_PRIVATE_JUMBOS */
static int ti_newbuf_std(struct ti_softc *, int, struct mbuf *);
static int ti_newbuf_mini(struct ti_softc *, int, struct mbuf *);
static int ti_newbuf_jumbo(struct ti_softc *, int, struct mbuf *);
static int ti_init_rx_ring_std(struct ti_softc *);
static void ti_free_rx_ring_std(struct ti_softc *);
static int ti_init_rx_ring_jumbo(struct ti_softc *);
static void ti_free_rx_ring_jumbo(struct ti_softc *);
static int ti_init_rx_ring_mini(struct ti_softc *);
static void ti_free_rx_ring_mini(struct ti_softc *);
static void ti_free_tx_ring(struct ti_softc *);
static int ti_init_tx_ring(struct ti_softc *);

static int ti_64bitslot_war(struct ti_softc *);
static int ti_chipinit(struct ti_softc *);
static int ti_gibinit(struct ti_softc *);

#ifdef TI_JUMBO_HDRSPLIT
static __inline void ti_hdr_split	(struct mbuf *top, int hdr_len,
					     int pkt_len, int idx);
#endif /* TI_JUMBO_HDRSPLIT */

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

DRIVER_MODULE(ti, pci, ti_driver, ti_devclass, 0, 0);
MODULE_DEPEND(ti, pci, 1, 1, 1);
MODULE_DEPEND(ti, ether, 1, 1, 1);

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

	return (ack);
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
		if_printf(sc->ti_ifp,
		    "failed to send write command, status: %x\n",
		    CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}

	/*
	 * Send first byte of address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, (addr >> 8) & 0xFF)) {
		if_printf(sc->ti_ifp, "failed to send address, status: %x\n",
		    CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}
	/*
	 * Send second byte address of byte we want to read.
	 */
	if (ti_eeprom_putbyte(sc, addr & 0xFF)) {
		if_printf(sc->ti_ifp, "failed to send address, status: %x\n",
		    CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
	}

	EEPROM_STOP;
	EEPROM_START;
	/*
	 * Send read control code to EEPROM.
	 */
	if (ti_eeprom_putbyte(sc, EEPROM_CTL_READ)) {
		if_printf(sc->ti_ifp,
		    "failed to send read command, status: %x\n",
		    CSR_READ_4(sc, TI_MISC_LOCAL_CTL));
		return (1);
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

	return (0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int
ti_read_eeprom(sc, dest, off, cnt)
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

	return (err ? 1 : 0);
}

/*
 * NIC memory access function. Can be used to either clear a section
 * of NIC local memory or (if buf is non-NULL) copy data into it.
 */
static void
ti_mem(sc, addr, len, buf)
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

	while (cnt) {
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
}

static int
ti_copy_mem(sc, tigon_addr, len, buf, useraddr, readdata)
	struct ti_softc		*sc;
	u_int32_t		tigon_addr, len;
	caddr_t			buf;
	int			useraddr, readdata;
{
	int		segptr, segsize, cnt;
	caddr_t		ptr;
	u_int32_t	origwin;
	u_int8_t	tmparray[TI_WINLEN], tmparray2[TI_WINLEN];
	int		resid, segresid;
	int		first_pass;

	/*
	 * At the moment, we don't handle non-aligned cases, we just bail.
	 * If this proves to be a problem, it will be fixed.
	 */
	if ((readdata == 0)
	 && (tigon_addr & 0x3)) {
		if_printf(sc->ti_ifp, "ti_copy_mem: tigon address %#x isn't "
		    "word-aligned\n", tigon_addr);
		if_printf(sc->ti_ifp, "ti_copy_mem: unaligned writes aren't "
		    "yet supported\n");
		return (EINVAL);
	}

	segptr = tigon_addr & ~0x3;
	segresid = tigon_addr - segptr;

	/*
	 * This is the non-aligned amount left over that we'll need to
	 * copy.
	 */
	resid = len & 0x3;

	/* Add in the left over amount at the front of the buffer */
	resid += segresid;

	cnt = len & ~0x3;
	/*
	 * If resid + segresid is >= 4, add multiples of 4 to the count and
	 * decrease the residual by that much.
	 */
	cnt += resid & ~0x3;
	resid -= resid & ~0x3;

	ptr = buf;

	first_pass = 1;

	/*
	 * Make sure we aren't interrupted while we're changing the window
	 * pointer.
	 */
	TI_LOCK(sc);

	/*
	 * Save the old window base value.
	 */
	origwin = CSR_READ_4(sc, TI_WINBASE);

	while (cnt) {
		bus_size_t ti_offset;

		if (cnt < TI_WINLEN)
			segsize = cnt;
		else
			segsize = TI_WINLEN - (segptr % TI_WINLEN);
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));

		ti_offset = TI_WINDOW + (segptr & (TI_WINLEN -1));

		if (readdata) {

			bus_space_read_region_4(sc->ti_btag,
						sc->ti_bhandle, ti_offset,
						(u_int32_t *)tmparray,
						segsize >> 2);
			if (useraddr) {
				/*
				 * Yeah, this is a little on the kludgy
				 * side, but at least this code is only
				 * used for debugging.
				 */
				ti_bcopy_swap(tmparray, tmparray2, segsize,
					      TI_SWAP_NTOH);

				if (first_pass) {
					copyout(&tmparray2[segresid], ptr,
						segsize - segresid);
					first_pass = 0;
				} else
					copyout(tmparray2, ptr, segsize);
			} else {
				if (first_pass) {

					ti_bcopy_swap(tmparray, tmparray2,
						      segsize, TI_SWAP_NTOH);
					bcopy(&tmparray2[segresid], ptr,
					      segsize - segresid);
					first_pass = 0;
				} else
					ti_bcopy_swap(tmparray, ptr, segsize,
						      TI_SWAP_NTOH);
			}

		} else {
			if (useraddr) {
				copyin(ptr, tmparray2, segsize);
				ti_bcopy_swap(tmparray2, tmparray, segsize,
					      TI_SWAP_HTON);
			} else
				ti_bcopy_swap(ptr, tmparray, segsize,
					      TI_SWAP_HTON);

			bus_space_write_region_4(sc->ti_btag,
						 sc->ti_bhandle, ti_offset,
						 (u_int32_t *)tmparray,
						 segsize >> 2);
		}
		segptr += segsize;
		ptr += segsize;
		cnt -= segsize;
	}

	/*
	 * Handle leftover, non-word-aligned bytes.
	 */
	if (resid != 0) {
		u_int32_t	tmpval, tmpval2;
		bus_size_t	ti_offset;

		/*
		 * Set the segment pointer.
		 */
		CSR_WRITE_4(sc, TI_WINBASE, (segptr & ~(TI_WINLEN - 1)));

		ti_offset = TI_WINDOW + (segptr & (TI_WINLEN - 1));

		/*
		 * First, grab whatever is in our source/destination.
		 * We'll obviously need this for reads, but also for
		 * writes, since we'll be doing read/modify/write.
		 */
		bus_space_read_region_4(sc->ti_btag, sc->ti_bhandle,
					ti_offset, &tmpval, 1);

		/*
		 * Next, translate this from little-endian to big-endian
		 * (at least on i386 boxes).
		 */
		tmpval2 = ntohl(tmpval);

		if (readdata) {
			/*
			 * If we're reading, just copy the leftover number
			 * of bytes from the host byte order buffer to
			 * the user's buffer.
			 */
			if (useraddr)
				copyout(&tmpval2, ptr, resid);
			else
				bcopy(&tmpval2, ptr, resid);
		} else {
			/*
			 * If we're writing, first copy the bytes to be
			 * written into the network byte order buffer,
			 * leaving the rest of the buffer with whatever was
			 * originally in there.  Then, swap the bytes
			 * around into host order and write them out.
			 *
			 * XXX KDM the read side of this has been verified
			 * to work, but the write side of it has not been
			 * verified.  So user beware.
			 */
			if (useraddr)
				copyin(ptr, &tmpval2, resid);
			else
				bcopy(ptr, &tmpval2, resid);

			tmpval = htonl(tmpval2);

			bus_space_write_region_4(sc->ti_btag, sc->ti_bhandle,
						 ti_offset, &tmpval, 1);
		}
	}

	CSR_WRITE_4(sc, TI_WINBASE, origwin);

	TI_UNLOCK(sc);

	return (0);
}

static int
ti_copy_scratch(sc, tigon_addr, len, buf, useraddr, readdata, cpu)
	struct ti_softc		*sc;
	u_int32_t		tigon_addr, len;
	caddr_t			buf;
	int			useraddr, readdata;
	int			cpu;
{
	u_int32_t	segptr;
	int		cnt;
	u_int32_t	tmpval, tmpval2;
	caddr_t		ptr;

	/*
	 * At the moment, we don't handle non-aligned cases, we just bail.
	 * If this proves to be a problem, it will be fixed.
	 */
	if (tigon_addr & 0x3) {
		if_printf(sc->ti_ifp, "ti_copy_scratch: tigon address %#x "
		    "isn't word-aligned\n", tigon_addr);
		return (EINVAL);
	}

	if (len & 0x3) {
		if_printf(sc->ti_ifp, "ti_copy_scratch: transfer length %d "
		    "isn't word-aligned\n", len);
		return (EINVAL);
	}

	segptr = tigon_addr;
	cnt = len;
	ptr = buf;

	TI_LOCK(sc);

	while (cnt) {
		CSR_WRITE_4(sc, CPU_REG(TI_SRAM_ADDR, cpu), segptr);

		if (readdata) {
			tmpval2 = CSR_READ_4(sc, CPU_REG(TI_SRAM_DATA, cpu));

			tmpval = ntohl(tmpval2);

			/*
			 * Note:  I've used this debugging interface
			 * extensively with Alteon's 12.3.15 firmware,
			 * compiled with GCC 2.7.2.1 and binutils 2.9.1.
			 *
			 * When you compile the firmware without
			 * optimization, which is necessary sometimes in
			 * order to properly step through it, you sometimes
			 * read out a bogus value of 0xc0017c instead of
			 * whatever was supposed to be in that scratchpad
			 * location.  That value is on the stack somewhere,
			 * but I've never been able to figure out what was
			 * causing the problem.
			 *
			 * The address seems to pop up in random places,
			 * often not in the same place on two subsequent
			 * reads.
			 *
			 * In any case, the underlying data doesn't seem
			 * to be affected, just the value read out.
			 *
			 * KDM, 3/7/2000
			 */

			if (tmpval2 == 0xc0017c)
				if_printf(sc->ti_ifp, "found 0xc0017c at %#x "
				       "(tmpval2)\n", segptr);

			if (tmpval == 0xc0017c)
				if_printf(sc->ti_ifp, "found 0xc0017c at %#x "
				       "(tmpval)\n", segptr);

			if (useraddr)
				copyout(&tmpval, ptr, 4);
			else
				bcopy(&tmpval, ptr, 4);
		} else {
			if (useraddr)
				copyin(ptr, &tmpval2, 4);
			else
				bcopy(ptr, &tmpval2, 4);

			tmpval = htonl(tmpval2);

			CSR_WRITE_4(sc, CPU_REG(TI_SRAM_DATA, cpu), tmpval);
		}

		cnt -= 4;
		segptr += 4;
		ptr += 4;
	}

	TI_UNLOCK(sc);

	return (0);
}

static int
ti_bcopy_swap(src, dst, len, swap_type)
	const void	*src;
	void		*dst;
	size_t		len;
	ti_swap_type	swap_type;
{
	const u_int8_t *tmpsrc;
	u_int8_t *tmpdst;
	size_t tmplen;

	if (len & 0x3) {
		printf("ti_bcopy_swap: length %zd isn't 32-bit aligned\n",
		       len);
		return (-1);
	}

	tmpsrc = src;
	tmpdst = dst;
	tmplen = len;

	while (tmplen) {
		if (swap_type == TI_SWAP_NTOH)
			*(u_int32_t *)tmpdst =
				ntohl(*(const u_int32_t *)tmpsrc);
		else
			*(u_int32_t *)tmpdst =
				htonl(*(const u_int32_t *)tmpsrc);

		tmpsrc += 4;
		tmpdst += 4;
		tmplen -= 4;
	}

	return (0);
}

/*
 * Load firmware image into the NIC. Check that the firmware revision
 * is acceptable and see if we want the firmware for the Tigon 1 or
 * Tigon 2.
 */
static void
ti_loadfw(sc)
	struct ti_softc		*sc;
{
	switch (sc->ti_hwrev) {
	case TI_HWREV_TIGON:
		if (tigonFwReleaseMajor != TI_FIRMWARE_MAJOR ||
		    tigonFwReleaseMinor != TI_FIRMWARE_MINOR ||
		    tigonFwReleaseFix != TI_FIRMWARE_FIX) {
			if_printf(sc->ti_ifp, "firmware revision mismatch; "
			    "want %d.%d.%d, got %d.%d.%d\n",
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
			if_printf(sc->ti_ifp, "firmware revision mismatch; "
			    "want %d.%d.%d, got %d.%d.%d\n",
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
		if_printf(sc->ti_ifp,
		    "can't load firmware: unknown hardware rev\n");
		break;
	}
}

/*
 * Send the NIC a command via the command ring.
 */
static void
ti_cmd(sc, cmd)
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
}

/*
 * Send the NIC an extended command. The 'len' parameter specifies the
 * number of command slots to include after the initial command.
 */
static void
ti_cmd_ext(sc, cmd, arg, len)
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
}

/*
 * Handle events that have triggered interrupts.
 */
static void
ti_handle_events(sc)
	struct ti_softc		*sc;
{
	struct ti_event_desc	*e;

	if (sc->ti_rdata->ti_event_ring == NULL)
		return;

	while (sc->ti_ev_saved_considx != sc->ti_ev_prodidx.ti_idx) {
		e = &sc->ti_rdata->ti_event_ring[sc->ti_ev_saved_considx];
		switch (e->ti_event) {
		case TI_EV_LINKSTAT_CHANGED:
			sc->ti_linkstat = e->ti_code;
			if (e->ti_code == TI_EV_CODE_LINK_UP)
				if_printf(sc->ti_ifp, "10/100 link up\n");
			else if (e->ti_code == TI_EV_CODE_GIG_LINK_UP)
				if_printf(sc->ti_ifp, "gigabit link up\n");
			else if (e->ti_code == TI_EV_CODE_LINK_DOWN)
				if_printf(sc->ti_ifp, "link down\n");
			break;
		case TI_EV_ERROR:
			if (e->ti_code == TI_EV_CODE_ERR_INVAL_CMD)
				if_printf(sc->ti_ifp, "invalid command\n");
			else if (e->ti_code == TI_EV_CODE_ERR_UNIMP_CMD)
				if_printf(sc->ti_ifp, "unknown command\n");
			else if (e->ti_code == TI_EV_CODE_ERR_BADCFG)
				if_printf(sc->ti_ifp, "bad config data\n");
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
			if_printf(sc->ti_ifp, "unknown event: %d\n",
			    e->ti_event);
			break;
		}
		/* Advance the consumer index. */
		TI_INC(sc->ti_ev_saved_considx, TI_EVENT_RING_CNT);
		CSR_WRITE_4(sc, TI_GCR_EVENTCONS_IDX, sc->ti_ev_saved_considx);
	}
}

#ifdef TI_PRIVATE_JUMBOS

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

static int
ti_alloc_jumbo_mem(sc)
	struct ti_softc		*sc;
{
	caddr_t			ptr;
	register int		i;
	struct ti_jpool_entry   *entry;

	/* Grab a big chunk o' storage. */
	sc->ti_cdata.ti_jumbo_buf = contigmalloc(TI_JMEM, M_DEVBUF,
		M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->ti_cdata.ti_jumbo_buf == NULL) {
		if_printf(sc->ti_ifp, "no memory for jumbo buffers!\n");
		return (ENOBUFS);
	}

	SLIST_INIT(&sc->ti_jfree_listhead);
	SLIST_INIT(&sc->ti_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc->ti_cdata.ti_jumbo_buf;
	for (i = 0; i < TI_JSLOTS; i++) {
		sc->ti_cdata.ti_jslots[i] = ptr;
		ptr += TI_JLEN;
		entry = malloc(sizeof(struct ti_jpool_entry),
			       M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			contigfree(sc->ti_cdata.ti_jumbo_buf, TI_JMEM,
			           M_DEVBUF);
			sc->ti_cdata.ti_jumbo_buf = NULL;
			if_printf(sc->ti_ifp, "no memory for jumbo "
			    "buffer queue!\n");
			return (ENOBUFS);
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->ti_jfree_listhead, entry, jpool_entries);
	}

	return (0);
}

/*
 * Allocate a jumbo buffer.
 */
static void *ti_jalloc(sc)
	struct ti_softc		*sc;
{
	struct ti_jpool_entry	*entry;

	entry = SLIST_FIRST(&sc->ti_jfree_listhead);

	if (entry == NULL) {
		if_printf(sc->ti_ifp, "no free jumbo buffers\n");
		return (NULL);
	}

	SLIST_REMOVE_HEAD(&sc->ti_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->ti_jinuse_listhead, entry, jpool_entries);
	return (sc->ti_cdata.ti_jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
static void
ti_jfree(buf, args)
	void			*buf;
	void			*args;
{
	struct ti_softc		*sc;
	int			i;
	struct ti_jpool_entry	*entry;

	/* Extract the softc struct pointer. */
	sc = (struct ti_softc *)args;

	if (sc == NULL)
		panic("ti_jfree: didn't get softc pointer!");

	/* calculate the slot this buffer belongs to */
	i = ((vm_offset_t)buf
	     - (vm_offset_t)sc->ti_cdata.ti_jumbo_buf) / TI_JLEN;

	if ((i < 0) || (i >= TI_JSLOTS))
		panic("ti_jfree: asked to free buffer that we don't manage!");

	entry = SLIST_FIRST(&sc->ti_jinuse_listhead);
	if (entry == NULL)
		panic("ti_jfree: buffer not in use!");
	entry->slot = i;
	SLIST_REMOVE_HEAD(&sc->ti_jinuse_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->ti_jfree_listhead, entry, jpool_entries);
}

#endif /* TI_PRIVATE_JUMBOS */

/*
 * Intialize a standard receive ring descriptor.
 */
static int
ti_newbuf_std(sc, i, m)
	struct ti_softc		*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
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
	if (sc->ti_ifp->if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return (0);
}

/*
 * Intialize a mini receive ring descriptor. This only applies to
 * the Tigon 2.
 */
static int
ti_newbuf_mini(sc, i, m)
	struct ti_softc		*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct ti_rx_desc	*r;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			return (ENOBUFS);
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
	if (sc->ti_ifp->if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return (0);
}

#ifdef TI_PRIVATE_JUMBOS

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
ti_newbuf_jumbo(sc, i, m)
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
			return (ENOBUFS);
		}

		/* Allocate the jumbo buffer */
		buf = ti_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			if_printf(sc->ti_ifp, "jumbo allocation failed "
			    "-- packet dropped!\n");
			return (ENOBUFS);
		}

		/* Attach the buffer to the mbuf. */
		m_new->m_data = (void *) buf;
		m_new->m_len = m_new->m_pkthdr.len = TI_JUMBO_FRAMELEN;
		MEXTADD(m_new, buf, TI_JUMBO_FRAMELEN, ti_jfree,
		    (struct ti_softc *)sc, 0, EXT_NET_DRV);
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
	if (sc->ti_ifp->if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM | TI_BDFLAG_IP_CKSUM;
	r->ti_len = m_new->m_len;
	r->ti_idx = i;

	return (0);
}

#else
#include <vm/vm_page.h>

#if (PAGE_SIZE == 4096)
#define NPAYLOAD 2
#else
#define NPAYLOAD 1
#endif

#define TCP_HDR_LEN (52 + sizeof(struct ether_header))
#define UDP_HDR_LEN (28 + sizeof(struct ether_header))
#define NFS_HDR_LEN (UDP_HDR_LEN)
static int HDR_LEN =  TCP_HDR_LEN;


/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
ti_newbuf_jumbo(sc, idx, m_old)
	struct ti_softc		*sc;
	int			idx;
	struct mbuf		*m_old;
{
	struct mbuf		*cur, *m_new = NULL;
	struct mbuf		*m[3] = {NULL, NULL, NULL};
	struct ti_rx_desc_ext	*r;
	vm_page_t		frame;
	static int		color;
				/* 1 extra buf to make nobufs easy*/
	struct sf_buf		*sf[3] = {NULL, NULL, NULL};
	int			i;

	if (m_old != NULL) {
		m_new = m_old;
		cur = m_old->m_next;
		for (i = 0; i <= NPAYLOAD; i++){
			m[i] = cur;
			cur = cur->m_next;
		}
	} else {
		/* Allocate the mbufs. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			if_printf(sc->ti_ifp, "mbuf allocation failed "
			    "-- packet dropped!\n");
			goto nobufs;
		}
		MGET(m[NPAYLOAD], M_DONTWAIT, MT_DATA);
		if (m[NPAYLOAD] == NULL) {
			if_printf(sc->ti_ifp, "cluster mbuf allocation failed "
			    "-- packet dropped!\n");
			goto nobufs;
		}
		MCLGET(m[NPAYLOAD], M_DONTWAIT);
		if ((m[NPAYLOAD]->m_flags & M_EXT) == 0) {
			if_printf(sc->ti_ifp, "mbuf allocation failed "
			    "-- packet dropped!\n");
			goto nobufs;
		}
		m[NPAYLOAD]->m_len = MCLBYTES;

		for (i = 0; i < NPAYLOAD; i++){
			MGET(m[i], M_DONTWAIT, MT_DATA);
			if (m[i] == NULL) {
				if_printf(sc->ti_ifp, "mbuf allocation failed "
				    "-- packet dropped!\n");
				goto nobufs;
			}
			frame = vm_page_alloc(NULL, color++,
			    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ |
			    VM_ALLOC_WIRED);
			if (frame == NULL) {
				if_printf(sc->ti_ifp, "buffer allocation "
				    "failed -- packet dropped!\n");
				printf("      index %d page %d\n", idx, i);
				goto nobufs;
			}
			sf[i] = sf_buf_alloc(frame, SFB_NOWAIT);
			if (sf[i] == NULL) {
				vm_page_lock_queues();
				vm_page_unwire(frame, 0);
				vm_page_free(frame);
				vm_page_unlock_queues();
				if_printf(sc->ti_ifp, "buffer allocation "
				    "failed -- packet dropped!\n");
				printf("      index %d page %d\n", idx, i);
				goto nobufs;
			}
		}
		for (i = 0; i < NPAYLOAD; i++){
		/* Attach the buffer to the mbuf. */
			m[i]->m_data = (void *)sf_buf_kva(sf[i]);
			m[i]->m_len = PAGE_SIZE;
			MEXTADD(m[i], sf_buf_kva(sf[i]), PAGE_SIZE,
			    sf_buf_mext, sf[i], 0, EXT_DISPOSABLE);
			m[i]->m_next = m[i+1];
		}
		/* link the buffers to the header */
		m_new->m_next = m[0];
		m_new->m_data += ETHER_ALIGN;
		if (sc->ti_hdrsplit)
			m_new->m_len = MHLEN - ETHER_ALIGN;
		else
			m_new->m_len = HDR_LEN;
		m_new->m_pkthdr.len = NPAYLOAD * PAGE_SIZE + m_new->m_len;
	}

	/* Set up the descriptor. */
	r = &sc->ti_rdata->ti_rx_jumbo_ring[idx];
	sc->ti_cdata.ti_rx_jumbo_chain[idx] = m_new;
	TI_HOSTADDR(r->ti_addr0) = vtophys(mtod(m_new, caddr_t));
	r->ti_len0 = m_new->m_len;

	TI_HOSTADDR(r->ti_addr1) = vtophys(mtod(m[0], caddr_t));
	r->ti_len1 = PAGE_SIZE;

	TI_HOSTADDR(r->ti_addr2) = vtophys(mtod(m[1], caddr_t));
	r->ti_len2 = m[1]->m_ext.ext_size; /* could be PAGE_SIZE or MCLBYTES */

	if (PAGE_SIZE == 4096) {
		TI_HOSTADDR(r->ti_addr3) = vtophys(mtod(m[2], caddr_t));
		r->ti_len3 = MCLBYTES;
	} else {
		r->ti_len3 = 0;
	}
	r->ti_type = TI_BDTYPE_RECV_JUMBO_BD;

	r->ti_flags = TI_BDFLAG_JUMBO_RING|TI_RCB_FLAG_USE_EXT_RX_BD;

	if (sc->ti_ifp->if_hwassist)
		r->ti_flags |= TI_BDFLAG_TCP_UDP_CKSUM|TI_BDFLAG_IP_CKSUM;

	r->ti_idx = idx;

	return (0);

nobufs:

	/*
	 * Warning! :
	 * This can only be called before the mbufs are strung together.
	 * If the mbufs are strung together, m_freem() will free the chain,
	 * so that the later mbufs will be freed multiple times.
	 */
	if (m_new)
		m_freem(m_new);

	for (i = 0; i < 3; i++) {
		if (m[i])
			m_freem(m[i]);
		if (sf[i])
			sf_buf_mext((void *)sf_buf_kva(sf[i]), sf[i]);
	}
	return (ENOBUFS);
}
#endif



/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int
ti_init_rx_ring_std(sc)
	struct ti_softc		*sc;
{
	register int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_SSLOTS; i++) {
		if (ti_newbuf_std(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	};

	TI_UPDATE_STDPROD(sc, i - 1);
	sc->ti_std = i - 1;

	return (0);
}

static void
ti_free_rx_ring_std(sc)
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
}

static int
ti_init_rx_ring_jumbo(sc)
	struct ti_softc		*sc;
{
	register int		i;
	struct ti_cmd_desc	cmd;

	for (i = 0; i < TI_JUMBO_RX_RING_CNT; i++) {
		if (ti_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	};

	TI_UPDATE_JUMBOPROD(sc, i - 1);
	sc->ti_jumbo = i - 1;

	return (0);
}

static void
ti_free_rx_ring_jumbo(sc)
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
}

static int
ti_init_rx_ring_mini(sc)
	struct ti_softc		*sc;
{
	register int		i;

	for (i = 0; i < TI_MSLOTS; i++) {
		if (ti_newbuf_mini(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	};

	TI_UPDATE_MINIPROD(sc, i - 1);
	sc->ti_mini = i - 1;

	return (0);
}

static void
ti_free_rx_ring_mini(sc)
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
}

static void
ti_free_tx_ring(sc)
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
}

static int
ti_init_tx_ring(sc)
	struct ti_softc		*sc;
{
	sc->ti_txcnt = 0;
	sc->ti_tx_saved_considx = 0;
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, 0);
	return (0);
}

/*
 * The Tigon 2 firmware has a new way to add/delete multicast addresses,
 * but we have to support the old way too so that Tigon 1 cards will
 * work.
 */
static void
ti_add_mcast(sc, addr)
	struct ti_softc		*sc;
	struct ether_addr	*addr;
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->octet[0];

	switch (sc->ti_hwrev) {
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
		if_printf(sc->ti_ifp, "unknown hwrev\n");
		break;
	}
}

static void
ti_del_mcast(sc, addr)
	struct ti_softc		*sc;
	struct ether_addr	*addr;
{
	struct ti_cmd_desc	cmd;
	u_int16_t		*m;
	u_int32_t		ext[2] = {0, 0};

	m = (u_int16_t *)&addr->octet[0];

	switch (sc->ti_hwrev) {
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
		if_printf(sc->ti_ifp, "unknown hwrev\n");
		break;
	}
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
static void
ti_setmulti(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	struct ti_cmd_desc	cmd;
	struct ti_mc_entry	*mc;
	u_int32_t		intrs;

	ifp = sc->ti_ifp;

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
	while (SLIST_FIRST(&sc->ti_mc_listhead) != NULL) {
		mc = SLIST_FIRST(&sc->ti_mc_listhead);
		ti_del_mcast(sc, &mc->mc_addr);
		SLIST_REMOVE_HEAD(&sc->ti_mc_listhead, mc_entries);
		free(mc, M_DEVBUF);
	}

	/* Now program new ones. */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mc = malloc(sizeof(struct ti_mc_entry), M_DEVBUF, M_NOWAIT);
		if (mc == NULL) {
			if_printf(ifp, "no memory for mcast filter entry\n");
			continue;
		}
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    (char *)&mc->mc_addr, ETHER_ADDR_LEN);
		SLIST_INSERT_HEAD(&sc->ti_mc_listhead, mc, mc_entries);
		ti_add_mcast(sc, &mc->mc_addr);
	}
	IF_ADDR_UNLOCK(ifp);

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, intrs);
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
				return (EINVAL);
			else {
				TI_SETBIT(sc, TI_PCI_STATE,
				    TI_PCISTATE_32BIT_BUS);
				return (0);
			}
		}
	}

	return (0);
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
static int
ti_chipinit(sc)
	struct ti_softc		*sc;
{
	u_int32_t		cacheline;
	u_int32_t		pci_writemax = 0;
	u_int32_t		hdrsplit;

	/* Initialize link to down state. */
	sc->ti_linkstat = TI_EV_CODE_LINK_DOWN;

	if (sc->ti_ifp->if_capenable & IFCAP_HWCSUM)
		sc->ti_ifp->if_hwassist = TI_CSUM_FEATURES;
	else
		sc->ti_ifp->if_hwassist = 0;

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
		if_printf(sc->ti_ifp, "board self-diagnostics failed!\n");
		return (ENODEV);
	}

	/* Halt the CPU. */
	TI_SETBIT(sc, TI_CPU_STATE, TI_CPUSTATE_HALT);

	/* Figure out the hardware revision. */
	switch (CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_CHIP_REV_MASK) {
	case TI_REV_TIGON_I:
		sc->ti_hwrev = TI_HWREV_TIGON;
		break;
	case TI_REV_TIGON_II:
		sc->ti_hwrev = TI_HWREV_TIGON_II;
		break;
	default:
		if_printf(sc->ti_ifp, "unsupported chip revision\n");
		return (ENODEV);
	}

	/* Do special setup for Tigon 2. */
	if (sc->ti_hwrev == TI_HWREV_TIGON_II) {
		TI_SETBIT(sc, TI_CPU_CTL_B, TI_CPUSTATE_HALT);
		TI_SETBIT(sc, TI_MISC_LOCAL_CTL, TI_MLC_SRAM_BANK_512K);
		TI_SETBIT(sc, TI_MISC_CONF, TI_MCR_SRAM_SYNCHRONOUS);
	}

	/*
	 * We don't have firmware source for the Tigon 1, so Tigon 1 boards
	 * can't do header splitting.
	 */
#ifdef TI_JUMBO_HDRSPLIT
	if (sc->ti_hwrev != TI_HWREV_TIGON)
		sc->ti_hdrsplit = 1;
	else
		if_printf(sc->ti_ifp,
		    "can't do header splitting on a Tigon I board\n");
#endif /* TI_JUMBO_HDRSPLIT */

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
		switch (cacheline) {
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
				if_printf(sc->ti_ifp, "cache line size %d not "
				    "supported; disabling PCI MWI\n",
				    cacheline);
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

	if (sc->ti_hdrsplit)
		hdrsplit = TI_OPMODE_JUMBO_HDRSPLIT;
	else
		hdrsplit = 0;

	/* Configure DMA variables. */
#if BYTE_ORDER == BIG_ENDIAN
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_BD |
	    TI_OPMODE_BYTESWAP_DATA | TI_OPMODE_WORDSWAP_BD |
	    TI_OPMODE_WARN_ENB | TI_OPMODE_FATAL_ENB |
	    TI_OPMODE_DONT_FRAG_JUMBO | hdrsplit);
#else /* BYTE_ORDER */
	CSR_WRITE_4(sc, TI_GCR_OPMODE, TI_OPMODE_BYTESWAP_DATA|
	    TI_OPMODE_WORDSWAP_BD|TI_OPMODE_DONT_FRAG_JUMBO|
	    TI_OPMODE_WARN_ENB|TI_OPMODE_FATAL_ENB | hdrsplit);
#endif /* BYTE_ORDER */

	/*
	 * Only allow 1 DMA channel to be active at a time.
	 * I don't think this is a good idea, but without it
	 * the firmware racks up lots of nicDmaReadRingFull
	 * errors.  This is not compatible with hardware checksums.
	 */
	if (sc->ti_ifp->if_hwassist == 0)
		TI_SETBIT(sc, TI_GCR_OPMODE, TI_OPMODE_1_DMA_ACTIVE);

	/* Recommended settings from Tigon manual. */
	CSR_WRITE_4(sc, TI_GCR_DMA_WRITECFG, TI_DMA_STATE_THRESH_8W);
	CSR_WRITE_4(sc, TI_GCR_DMA_READCFG, TI_DMA_STATE_THRESH_8W);

	if (ti_64bitslot_war(sc)) {
		if_printf(sc->ti_ifp, "bios thinks we're in a 64 bit slot, "
		    "but we aren't");
		return (EINVAL);
	}

	return (0);
}

#define	TI_RD_OFF(x)	offsetof(struct ti_ring_data, x)

/*
 * Initialize the general information block and firmware, and
 * start the CPU(s) running.
 */
static int
ti_gibinit(sc)
	struct ti_softc		*sc;
{
	struct ti_rcb		*rcb;
	int			i;
	struct ifnet		*ifp;
	uint32_t		rdphys;

	ifp = sc->ti_ifp;
	rdphys = sc->ti_rdata_phys;

	/* Disable interrupts for now. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	/*
	 * Tell the chip where to find the general information block.
	 * While this struct could go into >4GB memory, we allocate it in a
	 * single slab with the other descriptors, and those don't seem to
	 * support being located in a 64-bit region.
	 */
	CSR_WRITE_4(sc, TI_GCR_GENINFO_HI, 0);
	CSR_WRITE_4(sc, TI_GCR_GENINFO_LO, rdphys + TI_RD_OFF(ti_info));

	/* Load the firmware into SRAM. */
	ti_loadfw(sc);

	/* Set up the contents of the general info and ring control blocks. */

	/* Set up the event ring and producer pointer. */
	rcb = &sc->ti_rdata->ti_info.ti_ev_rcb;

	TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_event_ring);
	rcb->ti_flags = 0;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_ev_prodidx_ptr) =
	    rdphys + TI_RD_OFF(ti_ev_prodidx_r);
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
	    rdphys + TI_RD_OFF(ti_info.ti_stats);

	/* Set up the standard receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_std_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_rx_std_ring);
	rcb->ti_max_len = TI_FRAMELEN;
	rcb->ti_flags = 0;
	if (sc->ti_ifp->if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/* Set up the jumbo receive ring. */
	rcb = &sc->ti_rdata->ti_info.ti_jumbo_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_rx_jumbo_ring);

#ifdef TI_PRIVATE_JUMBOS
	rcb->ti_max_len = TI_JUMBO_FRAMELEN;
	rcb->ti_flags = 0;
#else
	rcb->ti_max_len = PAGE_SIZE;
	rcb->ti_flags = TI_RCB_FLAG_USE_EXT_RX_BD;
#endif
	if (sc->ti_ifp->if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/*
	 * Set up the mini ring. Only activated on the
	 * Tigon 2 but the slot in the config block is
	 * still there on the Tigon 1.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_mini_rx_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_rx_mini_ring);
	rcb->ti_max_len = MHLEN - ETHER_ALIGN;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		rcb->ti_flags = TI_RCB_FLAG_RING_DISABLED;
	else
		rcb->ti_flags = 0;
	if (sc->ti_ifp->if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_flags |= TI_RCB_FLAG_VLAN_ASSIST;

	/*
	 * Set up the receive return ring.
	 */
	rcb = &sc->ti_rdata->ti_info.ti_return_rcb;
	TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_rx_return_ring);
	rcb->ti_flags = 0;
	rcb->ti_max_len = TI_RETURN_RING_CNT;
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_return_prodidx_ptr) =
	    rdphys + TI_RD_OFF(ti_return_prodidx_r);

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
	if (sc->ti_ifp->if_hwassist)
		rcb->ti_flags |= TI_RCB_FLAG_TCP_UDP_CKSUM |
		     TI_RCB_FLAG_IP_CKSUM | TI_RCB_FLAG_NO_PHDR_CKSUM;
	rcb->ti_max_len = TI_TX_RING_CNT;
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		TI_HOSTADDR(rcb->ti_hostaddr) = TI_TX_RING_BASE;
	else
		TI_HOSTADDR(rcb->ti_hostaddr) = rdphys + TI_RD_OFF(ti_tx_ring);
	TI_HOSTADDR(sc->ti_rdata->ti_info.ti_tx_considx_ptr) =
	    rdphys + TI_RD_OFF(ti_tx_considx_r);

	/* Set up tuneables */
#if 0
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		CSR_WRITE_4(sc, TI_GCR_RX_COAL_TICKS,
		    (sc->ti_rx_coal_ticks / 10));
	else
#endif
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

	return (0);
}

static void
ti_rdata_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct ti_softc *sc;

	sc = arg;
	if (error || nseg != 1)
		return;

	/*
	 * All of the Tigon data structures need to live at <4GB.  This
	 * cast is fine since busdma was told about this constraint.
	 */
	sc->ti_rdata_phys = (uint32_t)segs[0].ds_addr;
	return;
}
	
/*
 * Probe for a Tigon chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match.
 */
static int
ti_probe(dev)
	device_t		dev;
{
	struct ti_type		*t;

	t = ti_devs;

	while (t->ti_name != NULL) {
		if ((pci_get_vendor(dev) == t->ti_vid) &&
		    (pci_get_device(dev) == t->ti_did)) {
			device_set_desc(dev, t->ti_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

static int
ti_attach(dev)
	device_t		dev;
{
	struct ifnet		*ifp;
	struct ti_softc		*sc;
	int			error = 0, rid;
	u_char			eaddr[6];

	sc = device_get_softc(dev);
	sc->ti_unit = device_get_unit(dev);

	mtx_init(&sc->ti_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	ifmedia_init(&sc->ifmedia, IFM_IMASK, ti_ifmedia_upd, ti_ifmedia_sts);
	ifp = sc->ti_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
	sc->ti_ifp->if_capabilities = IFCAP_HWCSUM |
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	sc->ti_ifp->if_capenable = sc->ti_ifp->if_capabilities;

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = TI_PCI_LOMEM;
	sc->ti_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE|PCI_RF_DENSE);

	if (sc->ti_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->ti_btag = rman_get_bustag(sc->ti_res);
	sc->ti_bhandle = rman_get_bushandle(sc->ti_res);
	sc->ti_vhandle = (vm_offset_t)rman_get_virtual(sc->ti_res);

	/* Allocate interrupt */
	rid = 0;

	sc->ti_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->ti_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	if (ti_chipinit(sc)) {
		device_printf(dev, "chip initialization failed\n");
		error = ENXIO;
		goto fail;
	}

	/* Zero out the NIC's on-board SRAM. */
	ti_mem(sc, 0x2000, 0x100000 - 0x2000,  NULL);

	/* Init again -- zeroing memory may have clobbered some registers. */
	if (ti_chipinit(sc)) {
		device_printf(dev, "chip initialization failed\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Get station address from the EEPROM. Note: the manual states
	 * that the MAC address is at offset 0x8c, however the data is
	 * stored as two longwords (since that's how it's loaded into
	 * the NIC). This means the MAC address is actually preceded
	 * by two zero bytes. We need to skip over those.
	 */
	if (ti_read_eeprom(sc, eaddr,
				TI_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN)) {
		device_printf(dev, "failed to read station address\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate the general information block and ring buffers. */
	if (bus_dma_tag_create(NULL,			/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				0,			/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->ti_parent_dmat) != 0) {
		device_printf(dev, "Failed to allocate parent dmat\n");
		error = ENOMEM;
		goto fail;
	}

	if (bus_dma_tag_create(sc->ti_parent_dmat,	/* parent */
				PAGE_SIZE, 0,		/* algnmnt, boundary */
				BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				sizeof(struct ti_ring_data),	/* maxsize */
				1,			/* nsegments */
				sizeof(struct ti_ring_data),	/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->ti_rdata_dmat) != 0) {
		device_printf(dev, "Failed to allocate rdata dmat\n");
		error = ENOMEM;
		goto fail;
	}

	if (bus_dmamem_alloc(sc->ti_rdata_dmat, (void**)&sc->ti_rdata,
			     BUS_DMA_NOWAIT, &sc->ti_rdata_dmamap) != 0) {
		device_printf(dev, "Failed to allocate rdata memory\n");
		error = ENOMEM;
		goto fail;
	}

	if (bus_dmamap_load(sc->ti_rdata_dmat, sc->ti_rdata_dmamap,
			    sc->ti_rdata, sizeof(struct ti_ring_data),
			    ti_rdata_cb, sc, BUS_DMA_NOWAIT) != 0) {
		device_printf(dev, "Failed to load rdata segments\n");
		error = ENOMEM;
		goto fail;
	}

	bzero(sc->ti_rdata, sizeof(struct ti_ring_data));

	/* Try to allocate memory for jumbo buffers. */
#ifdef TI_PRIVATE_JUMBOS
	if (ti_alloc_jumbo_mem(sc)) {
		device_printf(dev, "jumbo buffer allocation failed\n");
		error = ENXIO;
		goto fail;
	}
#endif

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
#if 0
	sc->ti_rx_coal_ticks = TI_TICKS_PER_SEC / 5000;
#endif
	sc->ti_rx_coal_ticks = 170;
	sc->ti_tx_coal_ticks = TI_TICKS_PER_SEC / 500;
	sc->ti_rx_max_coal_bds = 64;
#if 0
	sc->ti_tx_max_coal_bds = 128;
#endif
	sc->ti_tx_max_coal_bds = 32;
	sc->ti_tx_buf_ratio = 21;

	/* Set up ifnet structure */
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	tis[sc->ti_unit] = sc;
	ifp->if_ioctl = ti_ioctl;
	ifp->if_start = ti_start;
	ifp->if_watchdog = ti_watchdog;
	ifp->if_init = ti_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = TI_TX_RING_CNT - 1;

	/* Set up ifmedia support. */
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
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_T|IFM_FDX, 0, NULL);
	} else {
		/* Fiber cards don't support 10/100 modes. */
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
	}
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);

	/*
	 * We're assuming here that card initialization is a sequential
	 * thing.  If it isn't, multiple cards probing at the same time
	 * could stomp on the list of softcs here.
	 */

	/* Register the device */
	sc->dev = make_dev(&ti_cdevsw, sc->ti_unit, UID_ROOT, GID_OPERATOR,
			   0600, "ti%d", sc->ti_unit);
	sc->dev->si_drv1 = sc;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->ti_irq, INTR_TYPE_NET,
	   ti_intr, sc, &sc->ti_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (sc && error)
		ti_detach(dev);

	return (error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
ti_detach(dev)
	device_t		dev;
{
	struct ti_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	if (sc->dev)
		destroy_dev(sc->dev);
	KASSERT(mtx_initialized(&sc->ti_mtx), ("ti mutex not initialized"));
	TI_LOCK(sc);
	ifp = sc->ti_ifp;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		ti_stop(sc);
		ether_ifdetach(ifp);
		bus_generic_detach(dev);
	}
	ifmedia_removeall(&sc->ifmedia);

	if (sc->ti_rdata)
		bus_dmamem_free(sc->ti_rdata_dmat, sc->ti_rdata,
				sc->ti_rdata_dmamap);
	if (sc->ti_rdata_dmat)
		bus_dma_tag_destroy(sc->ti_rdata_dmat);
	if (sc->ti_parent_dmat)
		bus_dma_tag_destroy(sc->ti_parent_dmat);
	if (sc->ti_intrhand)
		bus_teardown_intr(dev, sc->ti_irq, sc->ti_intrhand);
	if (sc->ti_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ti_irq);
	if (sc->ti_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, TI_PCI_LOMEM,
		    sc->ti_res);
	}
	if (ifp)
		if_free(ifp);

#ifdef TI_PRIVATE_JUMBOS
	if (sc->ti_cdata.ti_jumbo_buf)
		contigfree(sc->ti_cdata.ti_jumbo_buf, TI_JMEM, M_DEVBUF);
#endif
	if (sc->ti_rdata)
		contigfree(sc->ti_rdata, sizeof(struct ti_ring_data), M_DEVBUF);

	TI_UNLOCK(sc);
	mtx_destroy(&sc->ti_mtx);

	return (0);
}

#ifdef TI_JUMBO_HDRSPLIT
/*
 * If hdr_len is 0, that means that header splitting wasn't done on
 * this packet for some reason.  The two most likely reasons are that
 * the protocol isn't a supported protocol for splitting, or this
 * packet had a fragment offset that wasn't 0.
 *
 * The header length, if it is non-zero, will always be the length of
 * the headers on the packet, but that length could be longer than the
 * first mbuf.  So we take the minimum of the two as the actual
 * length.
 */
static __inline void
ti_hdr_split(struct mbuf *top, int hdr_len, int pkt_len, int idx)
{
	int i = 0;
	int lengths[4] = {0, 0, 0, 0};
	struct mbuf *m, *mp;

	if (hdr_len != 0)
		top->m_len = min(hdr_len, top->m_len);
	pkt_len -= top->m_len;
	lengths[i++] = top->m_len;

	mp = top;
	for (m = top->m_next; m && pkt_len; m = m->m_next) {
		m->m_len = m->m_ext.ext_size = min(m->m_len, pkt_len);
		pkt_len -= m->m_len;
		lengths[i++] = m->m_len;
		mp = m;
	}

#if 0
	if (hdr_len != 0)
		printf("got split packet: ");
	else
		printf("got non-split packet: ");

	printf("%d,%d,%d,%d = %d\n", lengths[0],
	    lengths[1], lengths[2], lengths[3],
	    lengths[0] + lengths[1] + lengths[2] +
	    lengths[3]);
#endif

	if (pkt_len)
		panic("header splitting didn't");

	if (m) {
		m_freem(m);
		mp->m_next = NULL;

	}
	if (mp->m_next != NULL)
		panic("ti_hdr_split: last mbuf in chain should be null");
}
#endif /* TI_JUMBO_HDRSPLIT */

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

static void
ti_rxeof(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	TI_LOCK_ASSERT(sc);

	ifp = sc->ti_ifp;

	while (sc->ti_rx_saved_considx != sc->ti_return_prodidx.ti_idx) {
		struct ti_rx_desc	*cur_rx;
		u_int32_t		rxidx;
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
#ifdef TI_PRIVATE_JUMBOS
			m->m_len = cur_rx->ti_len;
#else /* TI_PRIVATE_JUMBOS */
#ifdef TI_JUMBO_HDRSPLIT
			if (sc->ti_hdrsplit)
				ti_hdr_split(m, TI_HOSTADDR(cur_rx->ti_addr),
					     cur_rx->ti_len, rxidx);
			else
#endif /* TI_JUMBO_HDRSPLIT */
			m_adj(m, cur_rx->ti_len - m->m_pkthdr.len);
#endif /* TI_PRIVATE_JUMBOS */
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
			m->m_len = cur_rx->ti_len;
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
			m->m_len = cur_rx->ti_len;
		}

		m->m_pkthdr.len = cur_rx->ti_len;
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		if (ifp->if_hwassist) {
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED |
			    CSUM_DATA_VALID;
			if ((cur_rx->ti_ip_cksum ^ 0xffff) == 0)
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			m->m_pkthdr.csum_data = cur_rx->ti_tcp_udp_cksum;
		}

		/*
		 * If we received a packet with a vlan tag,
		 * tag it before passing the packet upward.
		 */
		if (have_tag)
			VLAN_INPUT_TAG(ifp, m, vlan_tag, continue);
		TI_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		TI_LOCK(sc);
	}

	/* Only necessary on the Tigon 1. */
	if (sc->ti_hwrev == TI_HWREV_TIGON)
		CSR_WRITE_4(sc, TI_GCR_RXRETURNCONS_IDX,
		    sc->ti_rx_saved_considx);

	TI_UPDATE_STDPROD(sc, sc->ti_std);
	TI_UPDATE_MINIPROD(sc, sc->ti_mini);
	TI_UPDATE_JUMBOPROD(sc, sc->ti_jumbo);
}

static void
ti_txeof(sc)
	struct ti_softc		*sc;
{
	struct ti_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp;

	ifp = sc->ti_ifp;

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
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

static void
ti_intr(xsc)
	void			*xsc;
{
	struct ti_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	TI_LOCK(sc);
	ifp = sc->ti_ifp;

/*#ifdef notdef*/
	/* Avoid this for now -- checking this register is expensive. */
	/* Make sure this is really our interrupt. */
	if (!(CSR_READ_4(sc, TI_MISC_HOST_CTL) & TI_MHC_INTSTATE)) {
		TI_UNLOCK(sc);
		return;
	}
/*#endif*/

	/* Ack interrupt and stop others from occuring. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 1);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* Check RX return ring producer/consumer */
		ti_rxeof(sc);

		/* Check TX ring producer/consumer */
		ti_txeof(sc);
	}

	ti_handle_events(sc);

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, TI_MB_HOSTINTR, 0);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    ifp->if_snd.ifq_head != NULL)
		ti_start(ifp);

	TI_UNLOCK(sc);
}

static void
ti_stats_update(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = sc->ti_ifp;

	ifp->if_collisions +=
	   (sc->ti_rdata->ti_info.ti_stats.dot3StatsSingleCollisionFrames +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsMultipleCollisionFrames +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsExcessiveCollisions +
	   sc->ti_rdata->ti_info.ti_stats.dot3StatsLateCollisions) -
	   ifp->if_collisions;
}

/*
 * Encapsulate an mbuf chain in the tx ring  by coupling the mbuf data
 * pointers to descriptors.
 */
static int
ti_encap(sc, m_head, txidx)
	struct ti_softc		*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct ti_tx_desc	*f = NULL;
	struct mbuf		*m;
	u_int32_t		frag, cur, cnt = 0;
	u_int16_t		csum_flags = 0;
	struct m_tag		*mtag;

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

	mtag = VLAN_OUTPUT_TAG(sc->ti_ifp, m);

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

			if (mtag != NULL) {
				f->ti_flags |= TI_BDFLAG_VLAN_TAG;
				f->ti_vlan_tag = VLAN_TAG_VALUE(mtag) & 0xfff;
			} else {
				f->ti_vlan_tag = 0;
			}

			/*
			 * Sanity check: avoid coming within 16 descriptors
			 * of the end of the ring.
			 */
			if ((TI_TX_RING_CNT - (sc->ti_txcnt + cnt)) < 16)
				return (ENOBUFS);
			cur = frag;
			TI_INC(frag, TI_TX_RING_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return (ENOBUFS);

	if (frag == sc->ti_tx_saved_considx)
		return (ENOBUFS);

	if (sc->ti_hwrev == TI_HWREV_TIGON)
		sc->ti_rdata->ti_tx_ring_nic[cur % 128].ti_flags |=
	            TI_BDFLAG_END;
	else
		sc->ti_rdata->ti_tx_ring[cur].ti_flags |= TI_BDFLAG_END;
	sc->ti_cdata.ti_tx_chain[cur] = m_head;
	sc->ti_txcnt += cnt;

	*txidx = frag;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
ti_start(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		prodidx = 0;

	sc = ifp->if_softc;
	TI_LOCK(sc);

	prodidx = CSR_READ_4(sc, TI_MB_SENDPROD_IDX);

	while (sc->ti_cdata.ti_tx_chain[prodidx] == NULL) {
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
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
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
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

	/* Transmit */
	CSR_WRITE_4(sc, TI_MB_SENDPROD_IDX, prodidx);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	TI_UNLOCK(sc);
}

static void
ti_init(xsc)
	void			*xsc;
{
	struct ti_softc		*sc = xsc;

	/* Cancel pending I/O and flush buffers. */
	ti_stop(sc);

	TI_LOCK(sc);
	/* Init the gen info block, ring control blocks and firmware. */
	if (ti_gibinit(sc)) {
		if_printf(sc->ti_ifp, "initialization failure\n");
		TI_UNLOCK(sc);
		return;
	}

	TI_UNLOCK(sc);
}

static void ti_init2(sc)
	struct ti_softc		*sc;
{
	struct ti_cmd_desc	cmd;
	struct ifnet		*ifp;
	u_int16_t		*m;
	struct ifmedia		*ifm;
	int			tmp;

	ifp = sc->ti_ifp;

	/* Specify MTU and interface index. */
	CSR_WRITE_4(sc, TI_GCR_IFINDEX, sc->ti_unit);
	CSR_WRITE_4(sc, TI_GCR_IFMTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);
	TI_DO_CMD(TI_CMD_UPDATE_GENCOM, 0, 0);

	/* Load our MAC address. */
	m = (u_int16_t *)IF_LLADDR(sc->ti_ifp);
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

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

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
}

/*
 * Set media options.
 */
static int
ti_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;
	struct ifmedia		*ifm;
	struct ti_cmd_desc	cmd;
	u_int32_t		flowctl;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	flowctl = 0;

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		/*
		 * Transmit flow control doesn't work on the Tigon 1.
		 */
		flowctl = TI_GLNK_RX_FLOWCTL_Y;

		/*
		 * Transmit flow control can also cause problems on the
		 * Tigon 2, apparantly with both the copper and fiber
		 * boards.  The symptom is that the interface will just
		 * hang.  This was reproduced with Alteon 180 switches.
		 */
#if 0
		if (sc->ti_hwrev != TI_HWREV_TIGON)
			flowctl |= TI_GLNK_TX_FLOWCTL_Y;
#endif

		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    TI_GLNK_FULL_DUPLEX| flowctl |
		    TI_GLNK_AUTONEGENB|TI_GLNK_ENB);

		flowctl = TI_LNK_RX_FLOWCTL_Y;
#if 0
		if (sc->ti_hwrev != TI_HWREV_TIGON)
			flowctl |= TI_LNK_TX_FLOWCTL_Y;
#endif

		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_100MB|TI_LNK_10MB|
		    TI_LNK_FULL_DUPLEX|TI_LNK_HALF_DUPLEX| flowctl |
		    TI_LNK_AUTONEGENB|TI_LNK_ENB);
		TI_DO_CMD(TI_CMD_LINK_NEGOTIATION,
		    TI_CMD_CODE_NEGOTIATE_BOTH, 0);
		break;
	case IFM_1000_SX:
	case IFM_1000_T:
		flowctl = TI_GLNK_RX_FLOWCTL_Y;
#if 0
		if (sc->ti_hwrev != TI_HWREV_TIGON)
			flowctl |= TI_GLNK_TX_FLOWCTL_Y;
#endif

		CSR_WRITE_4(sc, TI_GCR_GLINK, TI_GLNK_PREF|TI_GLNK_1000MB|
		    flowctl |TI_GLNK_ENB);
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
		flowctl = TI_LNK_RX_FLOWCTL_Y;
#if 0
		if (sc->ti_hwrev != TI_HWREV_TIGON)
			flowctl |= TI_LNK_TX_FLOWCTL_Y;
#endif

		CSR_WRITE_4(sc, TI_GCR_GLINK, 0);
		CSR_WRITE_4(sc, TI_GCR_LINK, TI_LNK_ENB|TI_LNK_PREF|flowctl);
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

	return (0);
}

/*
 * Report current media status.
 */
static void
ti_ifmedia_sts(ifp, ifmr)
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
			ifmr->ifm_active |= IFM_1000_T;
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
}

static int
ti_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct ti_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			mask, error = 0;
	struct ti_cmd_desc	cmd;

	TI_LOCK(sc);

	switch (command) {
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
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ti_if_flags & IFF_PROMISC)) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_ENB, 0);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ti_if_flags & IFF_PROMISC) {
				TI_DO_CMD(TI_CMD_SET_PROMISC_MODE,
				    TI_CMD_CODE_PROMISC_DIS, 0);
			} else
				ti_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				ti_stop(sc);
			}
		}
		sc->ti_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ti_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_HWCSUM) {
			if (IFCAP_HWCSUM & ifp->if_capenable)
				ifp->if_capenable &= ~IFCAP_HWCSUM;
			else
				ifp->if_capenable |= IFCAP_HWCSUM;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ti_init(sc);
		}
		error = 0;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	TI_UNLOCK(sc);

	return (error);
}

static int
ti_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct ti_softc *sc;

	sc = dev->si_drv1;
	if (sc == NULL)
		return (ENODEV);

	TI_LOCK(sc);
	sc->ti_flags |= TI_FLAG_DEBUGING;
	TI_UNLOCK(sc);

	return (0);
}

static int
ti_close(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct ti_softc *sc;

	sc = dev->si_drv1;
	if (sc == NULL)
		return (ENODEV);

	TI_LOCK(sc);
	sc->ti_flags &= ~TI_FLAG_DEBUGING;
	TI_UNLOCK(sc);

	return (0);
}

/*
 * This ioctl routine goes along with the Tigon character device.
 */
static int
ti_ioctl2(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
	int error;
	struct ti_softc *sc;

	sc = dev->si_drv1;
	if (sc == NULL)
		return (ENODEV);

	error = 0;

	switch (cmd) {
	case TIIOCGETSTATS:
	{
		struct ti_stats *outstats;

		outstats = (struct ti_stats *)addr;

		bcopy(&sc->ti_rdata->ti_info.ti_stats, outstats,
		      sizeof(struct ti_stats));
		break;
	}
	case TIIOCGETPARAMS:
	{
		struct ti_params	*params;

		params = (struct ti_params *)addr;

		params->ti_stat_ticks = sc->ti_stat_ticks;
		params->ti_rx_coal_ticks = sc->ti_rx_coal_ticks;
		params->ti_tx_coal_ticks = sc->ti_tx_coal_ticks;
		params->ti_rx_max_coal_bds = sc->ti_rx_max_coal_bds;
		params->ti_tx_max_coal_bds = sc->ti_tx_max_coal_bds;
		params->ti_tx_buf_ratio = sc->ti_tx_buf_ratio;
		params->param_mask = TI_PARAM_ALL;

		error = 0;

		break;
	}
	case TIIOCSETPARAMS:
	{
		struct ti_params *params;

		params = (struct ti_params *)addr;

		if (params->param_mask & TI_PARAM_STAT_TICKS) {
			sc->ti_stat_ticks = params->ti_stat_ticks;
			CSR_WRITE_4(sc, TI_GCR_STAT_TICKS, sc->ti_stat_ticks);
		}

		if (params->param_mask & TI_PARAM_RX_COAL_TICKS) {
			sc->ti_rx_coal_ticks = params->ti_rx_coal_ticks;
			CSR_WRITE_4(sc, TI_GCR_RX_COAL_TICKS,
				    sc->ti_rx_coal_ticks);
		}

		if (params->param_mask & TI_PARAM_TX_COAL_TICKS) {
			sc->ti_tx_coal_ticks = params->ti_tx_coal_ticks;
			CSR_WRITE_4(sc, TI_GCR_TX_COAL_TICKS,
				    sc->ti_tx_coal_ticks);
		}

		if (params->param_mask & TI_PARAM_RX_COAL_BDS) {
			sc->ti_rx_max_coal_bds = params->ti_rx_max_coal_bds;
			CSR_WRITE_4(sc, TI_GCR_RX_MAX_COAL_BD,
				    sc->ti_rx_max_coal_bds);
		}

		if (params->param_mask & TI_PARAM_TX_COAL_BDS) {
			sc->ti_tx_max_coal_bds = params->ti_tx_max_coal_bds;
			CSR_WRITE_4(sc, TI_GCR_TX_MAX_COAL_BD,
				    sc->ti_tx_max_coal_bds);
		}

		if (params->param_mask & TI_PARAM_TX_BUF_RATIO) {
			sc->ti_tx_buf_ratio = params->ti_tx_buf_ratio;
			CSR_WRITE_4(sc, TI_GCR_TX_BUFFER_RATIO,
				    sc->ti_tx_buf_ratio);
		}

		error = 0;

		break;
	}
	case TIIOCSETTRACE: {
		ti_trace_type	trace_type;

		trace_type = *(ti_trace_type *)addr;

		/*
		 * Set tracing to whatever the user asked for.  Setting
		 * this register to 0 should have the effect of disabling
		 * tracing.
		 */
		CSR_WRITE_4(sc, TI_GCR_NIC_TRACING, trace_type);

		error = 0;

		break;
	}
	case TIIOCGETTRACE: {
		struct ti_trace_buf	*trace_buf;
		u_int32_t		trace_start, cur_trace_ptr, trace_len;

		trace_buf = (struct ti_trace_buf *)addr;

		trace_start = CSR_READ_4(sc, TI_GCR_NICTRACE_START);
		cur_trace_ptr = CSR_READ_4(sc, TI_GCR_NICTRACE_PTR);
		trace_len = CSR_READ_4(sc, TI_GCR_NICTRACE_LEN);

#if 0
		if_printf(sc->ti_ifp, "trace_start = %#x, cur_trace_ptr = %#x, "
		       "trace_len = %d\n", trace_start,
		       cur_trace_ptr, trace_len);
		if_printf(sc->ti_ifp, "trace_buf->buf_len = %d\n",
		       trace_buf->buf_len);
#endif

		error = ti_copy_mem(sc, trace_start, min(trace_len,
				    trace_buf->buf_len),
				    (caddr_t)trace_buf->buf, 1, 1);

		if (error == 0) {
			trace_buf->fill_len = min(trace_len,
						  trace_buf->buf_len);
			if (cur_trace_ptr < trace_start)
				trace_buf->cur_trace_ptr =
					trace_start - cur_trace_ptr;
			else
				trace_buf->cur_trace_ptr =
					cur_trace_ptr - trace_start;
		} else
			trace_buf->fill_len = 0;

		break;
	}

	/*
	 * For debugging, five ioctls are needed:
	 * ALT_ATTACH
	 * ALT_READ_TG_REG
	 * ALT_WRITE_TG_REG
	 * ALT_READ_TG_MEM
	 * ALT_WRITE_TG_MEM
	 */
	case ALT_ATTACH:
		/*
		 * From what I can tell, Alteon's Solaris Tigon driver
		 * only has one character device, so you have to attach
		 * to the Tigon board you're interested in.  This seems
		 * like a not-so-good way to do things, since unless you
		 * subsequently specify the unit number of the device
		 * you're interested in in every ioctl, you'll only be
		 * able to debug one board at a time.
		 */
		error = 0;
		break;
	case ALT_READ_TG_MEM:
	case ALT_WRITE_TG_MEM:
	{
		struct tg_mem *mem_param;
		u_int32_t sram_end, scratch_end;

		mem_param = (struct tg_mem *)addr;

		if (sc->ti_hwrev == TI_HWREV_TIGON) {
			sram_end = TI_END_SRAM_I;
			scratch_end = TI_END_SCRATCH_I;
		} else {
			sram_end = TI_END_SRAM_II;
			scratch_end = TI_END_SCRATCH_II;
		}

		/*
		 * For now, we'll only handle accessing regular SRAM,
		 * nothing else.
		 */
		if ((mem_param->tgAddr >= TI_BEG_SRAM)
		 && ((mem_param->tgAddr + mem_param->len) <= sram_end)) {
			/*
			 * In this instance, we always copy to/from user
			 * space, so the user space argument is set to 1.
			 */
			error = ti_copy_mem(sc, mem_param->tgAddr,
					    mem_param->len,
					    mem_param->userAddr, 1,
					    (cmd == ALT_READ_TG_MEM) ? 1 : 0);
		} else if ((mem_param->tgAddr >= TI_BEG_SCRATCH)
			&& (mem_param->tgAddr <= scratch_end)) {
			error = ti_copy_scratch(sc, mem_param->tgAddr,
						mem_param->len,
						mem_param->userAddr, 1,
						(cmd == ALT_READ_TG_MEM) ?
						1 : 0, TI_PROCESSOR_A);
		} else if ((mem_param->tgAddr >= TI_BEG_SCRATCH_B_DEBUG)
			&& (mem_param->tgAddr <= TI_BEG_SCRATCH_B_DEBUG)) {
			if (sc->ti_hwrev == TI_HWREV_TIGON) {
				if_printf(sc->ti_ifp,
				    "invalid memory range for Tigon I\n");
				error = EINVAL;
				break;
			}
			error = ti_copy_scratch(sc, mem_param->tgAddr -
						TI_SCRATCH_DEBUG_OFF,
						mem_param->len,
						mem_param->userAddr, 1,
						(cmd == ALT_READ_TG_MEM) ?
						1 : 0, TI_PROCESSOR_B);
		} else {
			if_printf(sc->ti_ifp, "memory address %#x len %d is "
			        "out of supported range\n",
			        mem_param->tgAddr, mem_param->len);
			error = EINVAL;
		}

		break;
	}
	case ALT_READ_TG_REG:
	case ALT_WRITE_TG_REG:
	{
		struct tg_reg	*regs;
		u_int32_t	tmpval;

		regs = (struct tg_reg *)addr;

		/*
		 * Make sure the address in question isn't out of range.
		 */
		if (regs->addr > TI_REG_MAX) {
			error = EINVAL;
			break;
		}
		if (cmd == ALT_READ_TG_REG) {
			bus_space_read_region_4(sc->ti_btag, sc->ti_bhandle,
						regs->addr, &tmpval, 1);
			regs->data = ntohl(tmpval);
#if 0
			if ((regs->addr == TI_CPU_STATE)
			 || (regs->addr == TI_CPU_CTL_B)) {
				if_printf(sc->ti_ifp, "register %#x = %#x\n",
				       regs->addr, tmpval);
			}
#endif
		} else {
			tmpval = htonl(regs->data);
			bus_space_write_region_4(sc->ti_btag, sc->ti_bhandle,
						 regs->addr, &tmpval, 1);
		}

		break;
	}
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static void
ti_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct ti_softc		*sc;

	sc = ifp->if_softc;
	TI_LOCK(sc);

	/*
	 * When we're debugging, the chip is often stopped for long periods
	 * of time, and that would normally cause the watchdog timer to fire.
	 * Since that impedes debugging, we don't want to do that.
	 */
	if (sc->ti_flags & TI_FLAG_DEBUGING) {
		TI_UNLOCK(sc);
		return;
	}

	if_printf(ifp, "watchdog timeout -- resetting\n");
	ti_stop(sc);
	ti_init(sc);

	ifp->if_oerrors++;
	TI_UNLOCK(sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
ti_stop(sc)
	struct ti_softc		*sc;
{
	struct ifnet		*ifp;
	struct ti_cmd_desc	cmd;

	TI_LOCK(sc);

	ifp = sc->ti_ifp;

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

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	TI_UNLOCK(sc);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
ti_shutdown(dev)
	device_t		dev;
{
	struct ti_softc		*sc;

	sc = device_get_softc(dev);
	TI_LOCK(sc);
	ti_chipinit(sc);
	TI_UNLOCK(sc);
}
