/*	$NetBSD: pdqvar.h,v 1.27 2000/05/03 19:17:54 thorpej Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 * Id: pdqvar.h,v 1.21 1997/03/21 21:16:04 thomas Exp
 * $FreeBSD$
 *
 */

/*
 * DEC PDQ FDDI Controller; PDQ O/S dependent definitions
 *
 * Written by Matt Thomas
 *
 */

#if defined(PDQ_HWSUPPORT)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
        
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/fddi.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */

#endif	/* PDQ_HWSUPPORT */

typedef struct _pdq_t pdq_t;
typedef struct _pdq_csrs_t pdq_csrs_t;
typedef struct _pdq_pci_csrs_t pdq_pci_csrs_t;
typedef struct _pdq_lanaddr_t pdq_lanaddr_t;
typedef unsigned int pdq_uint32_t;
typedef unsigned short pdq_uint16_t;
typedef unsigned char pdq_uint8_t;
typedef enum _pdq_boolean_t pdq_boolean_t;
typedef enum _pdq_type_t pdq_type_t;
typedef enum _pdq_state_t pdq_state_t;
typedef struct mbuf PDQ_OS_DATABUF_T;

typedef bus_space_tag_t pdq_bus_t;
typedef bus_space_handle_t pdq_bus_memaddr_t;
typedef pdq_bus_memaddr_t pdq_bus_memoffset_t;

extern devclass_t pdq_devclass;

enum _pdq_type_t {
    PDQ_DEFPA,		/* PCI-bus */
    PDQ_DEFEA,		/* EISA-bus */
    PDQ_DEFTA,		/* TurboChannel */
    PDQ_DEFAA,		/* FutureBus+ */
    PDQ_DEFQA		/* Q-bus */
};

#define	sc_ifmedia	ifmedia
#if 0 /* ALTQ */
#define	IFQ_DEQUEUE	IF_DEQUEUE
#define	IFQ_IS_EMPTY(q)	((q)->ifq_len == 0)
#endif

typedef struct _pdq_os_ctx_t {
	struct ifnet		*ifp;
	struct ifmedia		ifmedia;
	device_t		dev;
	int			debug;

	pdq_t *			sc_pdq;
	int			sc_flags;
#define	PDQIF_DOWNCALL		0x0001	/* active calling from if to pdq */

	struct resource *	io;
	int			io_rid;
	int			io_type;
	bus_space_handle_t	io_bsh;
	bus_space_tag_t		io_bst;

	struct resource *	mem;
	int			mem_rid;
	int			mem_type;
	bus_space_handle_t	mem_bsh;
	bus_space_tag_t		mem_bst;

	struct resource *	irq;
	int			irq_rid;
	void *			irq_ih;

	struct mtx		mtx;
	struct callout		watchdog;
	int			timer;
} pdq_softc_t;

#define PDQ_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define PDQ_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define	PDQ_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

#define	PDQ_OS_HDR_OFFSET	PDQ_RX_FC_OFFSET

#define	PDQ_OS_PAGESIZE		PAGE_SIZE
#define	PDQ_OS_TX_TRANSMIT	5

#define	PDQ_OS_IORD_32(bt, bh, off)		bus_space_read_4(bt, bh, off)
#define	PDQ_OS_IOWR_32(bt, bh, off, data)	bus_space_write_4(bt, bh, off, data)
#define	PDQ_OS_IORD_8(bt, bh, off)		bus_space_read_1(bt, bh, off)
#define	PDQ_OS_IOWR_8(bt, bh, off, data)	bus_space_write_1(bt, bh, off, data)

#define	PDQ_CSR_OFFSET(base, offset)		(0 + (offset)*sizeof(pdq_uint32_t))
#define	PDQ_CSR_WRITE(csr, name, data)		PDQ_OS_IOWR_32((csr)->csr_bus, (csr)->csr_base, (csr)->name, data)
#define	PDQ_CSR_READ(csr, name)			PDQ_OS_IORD_32((csr)->csr_bus, (csr)->csr_base, (csr)->name)

#define	PDQ_OS_DATABUF_FREE(pdq, b)		(m_freem(b))

#if defined(PDQ_OSSUPPORT)
#define	PDQ_OS_TX_TIMEOUT		5	/* seconds */

#define	PDQ_OS_IFP_TO_SOFTC(ifp)	((pdq_softc_t *) (ifp)->if_softc)
#define	PDQ_BPF_MTAP(sc, m)		BPF_MTAP((sc)->ifp, m)
#define PDQ_IFNET(sc)			((sc)->ifp)

#endif	/* PDQ_OSSUPPORT */

#if defined(PDQ_HWSUPPORT)

#define	PDQ_OS_PREFIX			"%s: "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name

#define	PDQ_OS_PTR_FMT	"%p"
#define	PDQ_OS_CSR_FMT	"0x%x"

#define	PDQ_OS_USEC_DELAY(n)		DELAY(n)
#define	PDQ_OS_VA_TO_BUSPA(pdq, p)	vtophys(p)

#define	PDQ_OS_MEMALLOC(n)		malloc(n, M_DEVBUF, M_NOWAIT)
#define	PDQ_OS_MEMFREE(p, n)		free((void *) p, M_DEVBUF)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#define	PDQ_OS_MEMALLOC_CONTIG(n)	contigmalloc(n, M_DEVBUF, M_NOWAIT, 0x800000, ~0, PAGE_SIZE, 0)
#define PDQ_OS_MEMFREE_CONTIG(p, n)	contigfree(p, n, M_DEVBUF)

#define	PDQ_OS_DATABUF_SIZE			(MCLBYTES)
#define	PDQ_OS_DATABUF_NEXT(b)			((b)->m_next)
#define	PDQ_OS_DATABUF_NEXT_SET(b, b1)		((b)->m_next = (b1))
#define	PDQ_OS_DATABUF_NEXTPKT(b)		((b)->m_nextpkt)
#define	PDQ_OS_DATABUF_NEXTPKT_SET(b, b1)	((b)->m_nextpkt = (b1))
#define	PDQ_OS_DATABUF_LEN(b)			((b)->m_len)
#define	PDQ_OS_DATABUF_LEN_SET(b, n)		((b)->m_len = (n))
/* #define	PDQ_OS_DATABUF_LEN_ADJ(b, n)		((b)->m_len += (n)) */
#define	PDQ_OS_DATABUF_PTR(b)			(mtod((b), pdq_uint8_t *))
#define	PDQ_OS_DATABUF_ADJ(b, n)		((b)->m_data += (n), (b)->m_len -= (n))

#define	PDQ_OS_DATABUF_ALLOC(pdq, b) do { \
    PDQ_OS_DATABUF_T *x_m0; \
    MGETHDR(x_m0, M_NOWAIT, MT_DATA); \
    if (x_m0 != NULL) { \
	MCLGET(x_m0, M_NOWAIT);	\
	if ((x_m0->m_flags & M_EXT) == 0) { \
	    m_free(x_m0); \
	    (b) = NULL; \
	} else { \
	    (b) = x_m0; \
	    x_m0->m_len = PDQ_OS_DATABUF_SIZE; \
	} \
    } else { \
	(b) = NULL; \
    } \
} while (0)

#define	PDQ_OS_DATABUF_RESET(b)	((b)->m_data = (b)->m_ext.ext_buf, (b)->m_len = MCLBYTES)

#define	PDQ_OS_DATABUF_ENQUEUE(q, b)	do { \
    PDQ_OS_DATABUF_NEXTPKT_SET(b, NULL); \
    if ((q)->q_tail == NULL) \
	(q)->q_head = (b); \
    else \
	PDQ_OS_DATABUF_NEXTPKT_SET(((PDQ_OS_DATABUF_T *)(q)->q_tail), b); \
    (q)->q_tail = (b); \
} while (0)

#define	PDQ_OS_DATABUF_DEQUEUE(q, b)	do { \
    if (((b) = (PDQ_OS_DATABUF_T *) (q)->q_head) != NULL) { \
	if (((q)->q_head = PDQ_OS_DATABUF_NEXTPKT(b)) == NULL) \
	    (q)->q_tail = NULL; \
	PDQ_OS_DATABUF_NEXTPKT_SET(b, NULL); \
    } \
} while (0)

#define PDQ_OS_DATABUF_BUSPA(pdq, b)	PDQ_OS_VA_TO_BUSPA(pdq, PDQ_OS_DATABUF_PTR(b))

#define	PDQ_OS_CONSUMER_PRESYNC(pdq)		do { } while(0)
#define	PDQ_OS_CONSUMER_POSTSYNC(pdq)		do { } while(0)
#define	PDQ_OS_DESC_PRESYNC(pdq, d, s)		do { } while(0)
#define	PDQ_OS_DESC_POSTSYNC(pdq, d, s)		do { } while(0)
#define	PDQ_OS_CMDRQST_PRESYNC(pdq, s)		do { } while(0)
#define	PDQ_OS_CMDRQST_POSTSYNC(pdq, s)		do { } while(0)
#define	PDQ_OS_CMDRSP_PRESYNC(pdq, s)		do { } while(0)
#define	PDQ_OS_CMDRSP_POSTSYNC(pdq, s)		do { } while(0)
#define PDQ_OS_RXPDU_PRESYNC(pdq, b, o, l)	do { } while(0)
#define PDQ_OS_RXPDU_POSTSYNC(pdq, b, o, l)	do { } while(0)
#define PDQ_OS_UNSOL_EVENT_PRESYNC(pdq, e)	do { } while(0)
#define PDQ_OS_UNSOL_EVENT_POSTSYNC(pdq, e)	do { } while(0)

#endif	/* PDQ_HWSUPPORT */

/*
 * OS dependent functions provided by pdq_ifsubr.c to pdq.c
 */
void	pdq_os_addr_fill (pdq_t *pdq, pdq_lanaddr_t *addrs, size_t numaddrs);
void	pdq_os_receive_pdu (pdq_t *, PDQ_OS_DATABUF_T *, size_t, int);
void	pdq_os_restart_transmitter (pdq_t *pdq);
void	pdq_os_transmit_done (pdq_t *, PDQ_OS_DATABUF_T *);
void	pdq_os_update_status (pdq_t *, const void *);

/*
 * Driver interfaces functions provided by pdq.c to pdq_ifsubr.c
 */
pdq_boolean_t	pdq_queue_transmit_data (pdq_t *pdq, PDQ_OS_DATABUF_T *pdu);
void		pdq_run	 (pdq_t *pdq);
pdq_state_t	pdq_stop (pdq_t *pdq);

/*
 * OS dependent functions provided by
 * pdq_ifsubr.c or pdq.c to the bus front ends
 */
int		pdq_ifattach (pdq_softc_t *, const pdq_uint8_t *,
			      pdq_type_t type);
void		pdq_ifdetach (pdq_softc_t *);
void		pdq_free (device_t);
int		pdq_interrupt (pdq_t *pdq);
void		pdq_hwreset (pdq_t *pdq);
pdq_t *		pdq_initialize (pdq_bus_t bus, pdq_bus_memaddr_t csr_va,
				const char *name, int unit,
				void *ctx, pdq_type_t type);
/*
 * Misc prototypes.
 */
void		pdq_flush_transmitter(pdq_t *pdq);
