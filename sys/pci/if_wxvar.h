/*                  
 * Copyright (c) 1999, Traakan Software
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
 * $FreeBSD$
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

#if defined(__NetBSD__)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <net/if.h>
#if defined(SIOCSIFMEDIA)
#include <net/if_media.h>
#endif
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/netisr.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <net/if_ether.h>
#if defined(INET)
#include <netinet/if_inarp.h>
#endif
#include <machine/bus.h>
#include <machine/intr.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/if_wxreg.h>

struct wxmdvar {
	struct device 		dev;		/* generic device structures */
	void *			ih;		/* interrupt handler cookie */
	struct ethercom		ethercom;	/* ethernet common part */
	pci_chipset_tag_t	pci_pc;
	pcitag_t		pci_tag;
	u_int8_t		enaddr[6];	/* our mac address */
	u_int32_t		cmdw;
	bus_space_tag_t		st;		/* bus space tag */
	bus_space_handle_t	sh;		/* bus space handle */
	struct ifmedia 		ifm;
	struct wx_softc *	next;
};
#define	wx_dev		w.dev
#define	wx_enaddr	w.enaddr
#define	wx_cmdw		w.cmdw
#define	wx_media	w.ifm
#define	wx_next		w.next

#define	wx_if		w.ethercom.ec_if
#define	wx_name		w.dev.dv_xname

#define	IOCTL_CMD_TYPE			u_long
#define	WXMALLOC(len)			malloc(len, M_DEVBUF, M_NOWAIT)
#define	WXFREE(ptr)			free(ptr, M_DEVBUF)
#define	SOFTC_IFP(ifp)			ifp->if_softc
#define	WX_BPFTAP_ARG(ifp)		(ifp)->if_bpf
#define	TIMEOUT(sc, func, arg, time)	timeout(func, arg, time)
#define	VTIMEOUT(sc, func, arg, time)	timeout(func, arg, time)
#define	UNTIMEOUT(f, arg, sc)		untimeout(f, arg)
#define	INLINE				inline

#define	vm_offset_t		vaddr_t
#ifndef	IFM_1000_SX
#define	IFM_1000_SX		IFM_1000_FX
#endif
#define	READ_CSR	_read_csr
#define	WRITE_CSR	_write_csr

#elif	defined(__FreeBSD__)
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

#include "opt_bdg.h"
#ifdef BRIDGE
#include <net/if_types.h>
#include <net/bridge.h>
#endif

struct wxmdvar {
	struct device *		dev;	/* backpointer to device */
	struct arpcom 		arpcom;	/* per-interface network data */
	struct resource *	mem;	/* resource descriptor for registers */
	struct resource *	irq;	/* resource descriptor for interrupt */
	void *			ih;	/* interrupt handler cookie */
	u_int16_t		cmdw;
	struct callout_handle	sch;	/* handle for timeouts */
	char 			name[8];
	bus_space_tag_t		st;	/* bus space tag */
	bus_space_handle_t	sh;	/* bus space handle */
	struct ifmedia 		ifm;
	struct wx_softc *	next;
};
#define	wx_dev		w.dev
#define	wx_enaddr	w.arpcom.ac_enaddr
#define	wx_cmdw		w.cmdw
#define	wx_media	w.ifm
#define	wx_next		w.next

#define	wx_if		w.arpcom.ac_if
#define	wx_name		w.name

#define	IOCTL_CMD_TYPE			u_long
#define	WXMALLOC(len)			malloc(len, M_DEVBUF, M_NOWAIT)
#define	WXFREE(ptr)			free(ptr, M_DEVBUF)
#define	SOFTC_IFP(ifp)			ifp->if_softc
#define	WX_BPFTAP_ARG(ifp)		ifp
#define	VTIMEOUT(sc, func, arg, time)	(void) timeout(func, arg, time)
#define	TIMEOUT(sc, func, arg, time)	(sc)->w.sch = timeout(func, arg, time)
#define	UNTIMEOUT(f, arg, sc)		untimeout(f, arg, (sc)->w.sch)
#define	INLINE				__inline

#define	READ_CSR(sc, reg)						\
	bus_space_read_4((sc)->w.st, (sc)->w.sh, (reg))
#define	WRITE_CSR(sc, reg, val)						\
	bus_space_write_4((sc)->w.st, (sc)->w.sh, (reg), (val))

#elif defined(__OpenBSD__)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/if_wxreg.h>

struct wxmdvar {
	struct device 		dev;		/* generic device structures */
	void *			ih;		/* interrupt handler cookie */
	struct arpcom		arpcom;		/* ethernet common part */
	pci_chipset_tag_t	pci_pc;
	pcitag_t		pci_tag;
	u_int32_t		cmdw;
	bus_space_tag_t		st;		/* bus space tag */
	bus_space_handle_t	sh;		/* bus space handle */
	struct ifmedia 		ifm;
	struct wx_softc *	next;
};
#define	wx_dev		w.dev
#define	wx_enaddr	w.arpcom.ac_enaddr
#define	wx_cmdw		w.cmdw
#define	wx_media	w.ifm
#define	wx_next		w.next

#define	wx_if		w.arpcom.ac_if
#define	wx_name		w.dev.dv_xname

#define	IOCTL_CMD_TYPE			u_long
#define	WXMALLOC(len)			malloc(len, M_DEVBUF, M_NOWAIT)
#define	WXFREE(ptr)			free(ptr, M_DEVBUF)
#define	SOFTC_IFP(ifp)			ifp->if_softc
#define	WX_BPFTAP_ARG(ifp)		(ifp)->if_bpf
#define	TIMEOUT(sc, func, arg, time)	timeout(func, arg, time)
#define	VTIMEOUT(sc, func, arg, time)	timeout(func, arg, time)
#define	UNTIMEOUT(f, arg, sc)		untimeout(f, arg)
#define	INLINE				inline

#define	vm_offset_t		vaddr_t
#define	READ_CSR	_read_csr
#define	WRITE_CSR	_write_csr

#endif


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
	u_int32_t		:	25,
		wx_no_flow 	:	1,
		wx_ilos		:	1,
		wx_no_ilos	:	1,
		wx_debug	:	1,
		ane_failed	:	1,
		linkup		:	1,
		all_mcasts	:	1;
	u_int32_t wx_idnrev;		/* chip revision && PCI ID */
	u_int16_t wx_cfg1;
	u_int16_t wx_txint_delay;
	u_int32_t wx_ienable;		/* current ienable to use */
	u_int32_t wx_dcr;		/* dcr used */
	u_int32_t wx_icr;		/* last icr */

	/*
	 * Statistics, soft && hard
	 */
	u_int32_t	wx_intr;
	u_int32_t	wx_linkintr;
	u_int32_t	wx_rxintr;
	u_int32_t	wx_xmitgc;
	u_int32_t	wx_xmitpullup;
	u_int32_t	wx_xmitcluster;
	u_int32_t	wx_xmitputback;
	u_int32_t	wx_xmitwanted;
	u_int32_t	wx_xmitblocked;
	u_int32_t	wx_xmitblocked1;
	u_int32_t	wx_xmitrunt;
	u_int32_t	wx_rxnobuf;

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
	struct	mtx wx_mtx;
} wx_softc_t;

#define WX_LOCK(_sc)            mtx_enter(&(_sc)->wx_mtx, MTX_DEF)
#define WX_UNLOCK(_sc)          mtx_exit(&(_sc)->wx_mtx, MTX_DEF)

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
#define	WX_MAX_RDESC	64	/* number of receive descriptors */
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
