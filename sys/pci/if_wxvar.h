/* $FreeBSD$ */
/*                  
 * Principal Author: Matthew Jacob
 * Copyright (c) 1999, 2001 by Traakan Software
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
 *
 * Additional Copyright (c) 2001 by Parag Patel
 * under same licence for MII PHY code.
 */

/*
 * Softc definitions for the Intel Gigabit Ethernet driver.
 *
 * Guidance and inspiration from David Greenman's
 * if_fxp driver gratefully acknowledged here.
 */

/*
 * Platform specific defines and inline functions go here.
 * Look further below for more generic structures.
 */

/*
 * Enable for FreeBSD 5.0 SMP code
 */
#define	SMPNG		1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif
#include <net/bpf.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/if_wxreg.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <sys/sysctl.h>

#define	NBPFILTER	1

MODULE_DEPEND(wx, miibus, 1, 1, 1);
#include "miibus_if.h"

#include "opt_bdg.h"
#ifdef BRIDGE
#include <net/if_types.h>
#include <net/bridge.h>
#endif

struct wxmdvar {
	/*
	 * arpcom must be first
	 */
	struct arpcom 		arpcom;	/* per-interface network data */
	struct device *		dev;	/* backpointer to device */
	struct resource *	mem;	/* resource descriptor for registers */
	struct resource *	irq;	/* resource descriptor for interrupt */
	void *			ih;	/* interrupt handler cookie */
	u_int16_t		cmdw;
	struct callout_handle	sch;	/* handle for timeouts */
	char 			name[8];
	bus_space_tag_t		st;	/* bus space tag */
	bus_space_handle_t	sh;	/* bus space handle */
	struct ifmedia 		ifm;
	device_t		miibus;
	struct wx_softc *	next;
#ifdef	SMPNG
	struct mtx		wxmtx;
#else
	int			spl;
#endif
};
#define	wx_dev		w.dev
#define	wx_enaddr	w.arpcom.ac_enaddr
#define	wx_cmdw		w.cmdw
#define	wx_media	w.ifm
#define	wx_next		w.next

#define	wx_if		w.arpcom.ac_if
#define	wx_name		w.name
#define	wx_mtx		w.wxmtx

#define	IOCTL_CMD_TYPE			u_long
#define	WXMALLOC(len)			malloc(len, M_DEVBUF, M_NOWAIT)
#define	WXFREE(ptr)			free(ptr, M_DEVBUF)
#define	SOFTC_IFP(ifp)			ifp->if_softc
#define	WX_BPFTAP_ARG(ifp)		ifp
#define	VTIMEOUT(sc, func, arg, time)	(void) timeout(func, arg, time)
#define	TIMEOUT(sc, func, arg, time)	(sc)->w.sch = timeout(func, arg, time)
#define	UNTIMEOUT(f, arg, sc)		untimeout(f, arg, (sc)->w.sch)
#define	INLINE				__inline
#ifdef	SMPNG
#define WX_LOCK(_sc)			mtx_lock(&(_sc)->wx_mtx)
#define WX_UNLOCK(_sc)			mtx_unlock(&(_sc)->wx_mtx)
#define	WX_ILOCK(_sc)			mtx_lock(&(_sc)->wx_mtx)
#define	WX_IUNLK(_sc)			mtx_unlock(&(_sc)->wx_mtx)
#else
#define	WX_LOCK(_sc)			_sc->w.spl = splimp()
#define	WX_UNLOCK(_sc)			splx(_sc->w.spl)
#define	WX_ILOCK(_sc)
#define	WX_IUNLK(_sc)
#endif
#define	WX_SOFTC_FROM_MII_ARG(x)	device_get_softc(x)
#define	WX_MII_FROM_SOFTC(x)		device_get_softc((x)->w.miibus)


#define	READ_CSR(sc, reg)						\
	bus_space_read_4((sc)->w.st, (sc)->w.sh, (reg))
#define	WRITE_CSR(sc, reg, val)						\
	bus_space_write_4((sc)->w.st, (sc)->w.sh, (reg), (val))

/*
 * Transmit soft descriptor, used to manage packets as they come in.
 */
typedef struct rxpkt {
	struct mbuf *dptr;	/* pointer to receive frame */
	u_int32_t dma_addr;	/* dma address */
} rxpkt_t;

	
/*
 * Transmit soft descriptor, used to manage packets as they are transmitted.
 */
typedef struct txpkt {
	struct txpkt *next;	/* next in a chain */
	struct mbuf *dptr;	/* pointer to mbuf being sent */
	u_int32_t sidx;		/* start index */
	u_int32_t eidx;		/* end index */
} txpkt_t;


typedef struct wx_softc {
	/*
	 * OS dependent storage... must be first...
	 */
	struct wxmdvar w;

	/*
	 * misc goodies
	 */
	u_int32_t		:	22,
		wx_needreinit	:	1,
		wx_mii		:	1,	/* non-zero if we have a PHY */
		wx_no_flow 	:	1,
		wx_ilos		:	1,
		wx_no_ilos	:	1,
		wx_verbose	:	1,
		wx_debug	:	1,
		ane_failed	:	1,
		linkup		:	1,
		all_mcasts	:	1;
	u_int32_t wx_idnrev;		/* chip revision && PCI ID */
	u_int16_t wx_cfg1;
	u_int16_t wx_unused;
	u_int32_t wx_ienable;		/* current ienable to use */
	u_int32_t wx_dcr;		/* dcr used */
	u_int32_t wx_icr;		/* last icr */

	/*
	 * Statistics, soft && hard
	 */
	u_int32_t	wx_intr;
	u_int32_t	wx_linkintr;
	u_int32_t	wx_rxintr;
	u_int32_t	wx_txqe;
	u_int32_t	wx_xmitgc;
	u_int32_t	wx_xmitpullup;
	u_int32_t	wx_xmitcluster;
	u_int32_t	wx_xmitputback;
	u_int32_t	wx_xmitwanted;
	u_int32_t	wx_xmitblocked;
	u_int32_t	wx_xmitrunt;
	u_int32_t	wx_rxnobuf;
	u_int32_t	wx_oddpkt;

	/*
	 * Soft copies of multicast addresses. We're only
	 * using (right now) the rest of the receive address
	 * registers- not the hashed multicast table.
	 */
	u_int8_t	wx_mcaddr[WX_RAL_TAB_SIZE-1][6];
	u_int8_t	wx_nmca;		/* # active multicast addrs */


	/*
 	 * Receive Management
	 * We have software and shared memory rings in a buddy store format.
	 */
	wxrd_t	*rdescriptors;		/* receive descriptor ring */
	rxpkt_t *rbase;			/* base of soft rdesc list */
	u_int16_t rnxt;			/* next descriptor to check */
	u_int16_t _pad;
	struct mbuf *rpending;		/* pending partial packet */

	/*
 	 * Transmit Management
	 * We have software and shared memory rings in a buddy store format.
	 */
	txpkt_t *tbase;			/* base of soft soft management */
	txpkt_t *tbsyf, *tbsyl;		/* linked busy list */
	wxtd_t	*tdescriptors;		/* transmit descriptor ring */
	u_int16_t tnxtfree;		/* next free index (circular) */
	u_int16_t tactive;		/* # active */
} wx_softc_t;

/*
 * We offset the the receive frame header by two bytes so that the actual
 * payload is 32 bit aligned. On platforms that require strict structure
 * alignment, this means that the ethernet frame header may have to be shifted
 * to align it at interrupt time, but because it's such a small amount
 * (fourteen bytes) and processors have gotten pretty fast, that's okay.
 * It may even turn out on some platforms that this doesn't have to happen.
 */
#define	WX_RX_OFFSET_VALUE	2

/*
 * Tunable Parameters.
 *
 * Descriptor lengths must be in multiples of 8.
 */
#define	WX_MAX_TDESC	256	/* number of transmit descriptors */
#define	T_NXT_IDX(x)	((x + 1) & (WX_MAX_TDESC - 1))
#define	T_PREV_IDX(x)	((x - 1) & (WX_MAX_TDESC - 1))
#define	WX_MAX_RDESC	256	/* number of receive descriptors */
#ifdef	PADDED_CELL
#define	RXINCR		2
#else
#define	RXINCR		1
#endif
#define	R_NXT_IDX(x)	((x + RXINCR) & (WX_MAX_RDESC - 1))
#define	R_PREV_IDX(x)	((x - RXINCR) & (WX_MAX_RDESC - 1))

/*
 * Link Up timeout, in milliseconds.
 */

#define	WX_LINK_UP_TIMEOUT	500
