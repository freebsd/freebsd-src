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

#ifndef _PDQ_OS_H
#define	_PDQ_OS_H

#define	PDQ_OS_TX_TIMEOUT		5	/* seconds */

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

enum _pdq_type_t {
    PDQ_DEFPA,		/* PCI-bus */
    PDQ_DEFEA,		/* EISA-bus */
    PDQ_DEFTA,		/* TurboChannel */
    PDQ_DEFAA,		/* FutureBus+ */
    PDQ_DEFQA		/* Q-bus */
};

#if defined(PDQTEST)
#include <pdq_os_test.h>
#elif defined(__FreeBSD__) || defined(__bsdi__) || defined(__NetBSD__)

#include <sys/param.h>
#include <sys/systm.h>
#ifndef M_MCAST
#include <sys/mbuf.h>
#endif /* M_CAST */
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#define	PDQ_USE_MBUFS
#if defined(__NetBSD__) || defined(__FreeBSD__)
#define	PDQ_OS_PREFIX			"%s: "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name
#else
#define	PDQ_OS_PREFIX			"%s%d: "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name, pdq->pdq_unit
#endif
#if defined(__FreeBSD__) && BSD >= 199506
#define	PDQ_OS_PAGESIZE			PAGE_SIZE
#else
#define	PDQ_OS_PAGESIZE			NBPG
#endif
#define	PDQ_OS_USEC_DELAY(n)		DELAY(n)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#if defined(__NetBSD__) && !defined(PDQ_NO_BUS_DMA)
#define PDQ_BUS_DMA
#endif
#if !defined(PDQ_BUS_DMA)
#define	PDQ_OS_VA_TO_BUSPA(pdq, p)		vtophys(p)
#endif
#define	PDQ_OS_MEMALLOC(n)		malloc(n, M_DEVBUF, M_NOWAIT)
#define	PDQ_OS_MEMFREE(p, n)		free((void *) p, M_DEVBUF)
#ifdef __FreeBSD__
#define	PDQ_OS_MEMALLOC_CONTIG(n)	contigmalloc(n, M_DEVBUF, M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0)
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	contigfree((void *) p, n, M_DEVBUF)
#else
#if !defined(PDQ_BUS_DMA)
#define	PDQ_OS_MEMALLOC_CONTIG(n)	uvm_km_alloc(kernel_map, round_page(n))
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	uvm_km_free(kernel_map, (vaddr_t) p, n)
#endif
#endif /* __FreeBSD__ */

#if defined(__FreeBSD__)
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <machine/cpufunc.h>
#include <machine/clock.h>
#define	ifnet_ret_t void
typedef int ioctl_cmd_t;
typedef enum { PDQ_BUS_EISA, PDQ_BUS_PCI } pdq_bus_t;
typedef	u_int16_t pdq_bus_ioport_t;
typedef volatile pdq_uint32_t *pdq_bus_memaddr_t;
typedef pdq_bus_memaddr_t pdq_bus_memoffset_t;
#if BSD >= 199506	/* __FreeBSD__ */
#define	PDQ_BPF_MTAP(sc, m)	bpf_mtap(&(sc)->sc_if, m)
#define	PDQ_BPFATTACH(sc, t, s)	bpfattach(&(sc)->sc_if, t, s)
#endif

#define	pdq_os_update_status(a, b)	((void) 0)

#elif defined(__bsdi__)
#if !defined(PDQ_HWSUPPORT) && (_BSDI_VERSION >= 199701)
#include <net/if_media.h>
#endif
#include <machine/inline.h>
#define	ifnet_ret_t int
typedef int ioctl_cmd_t;
typedef enum { PDQ_BUS_EISA, PDQ_BUS_PCI } pdq_bus_t;
typedef	u_int16_t pdq_bus_ioport_t;
typedef volatile pdq_uint32_t *pdq_bus_memaddr_t;
typedef pdq_bus_memaddr_t pdq_bus_memoffset_t;


#elif defined(__NetBSD__)
#if !defined(PDQ_HWSUPPORT)
#include <net/if_media.h>
#endif
#include <machine/bus.h>
#include <machine/intr.h>
#define PDQ_OS_HDR_OFFSET	(PDQ_RX_FC_OFFSET-3)
#define	PDQ_OS_PTR_FMT		"%p"
#define	PDQ_OS_CSR_FMT		"0x%lx"
#define	ifnet_ret_t void
typedef u_long ioctl_cmd_t;
typedef	bus_space_tag_t pdq_bus_t;
typedef	bus_space_handle_t pdq_bus_ioport_t;
typedef	bus_space_handle_t pdq_bus_memaddr_t;
typedef bus_addr_t pdq_bus_memoffset_t;
#define	PDQ_OS_SPL_RAISE()	splnet()
#define	PDQ_OS_IOMEM
#define PDQ_OS_IORD_32(t, base, offset)		bus_space_read_4  (t, base, offset)
#define PDQ_OS_IOWR_32(t, base, offset, data)	bus_space_write_4 (t, base, offset, data)
#define PDQ_OS_IORD_8(t, base, offset)		bus_space_read_1  (t, base, offset)
#define PDQ_OS_IOWR_8(t, base, offset, data)	bus_space_write_1 (t, base, offset, data)
#define	PDQ_CSR_OFFSET(base, offset)		(0 + (offset)*sizeof(pdq_uint32_t))

#ifdef PDQ_BUS_DMA
#define	PDQ_OS_UNSOL_EVENT_PRESYNC(pdq, event) \
	pdq_os_unsolicited_event_sync((pdq)->pdq_os_ctx, \
			(u_int8_t *) (event) - \
				(u_int8_t *) (pdq)->pdq_unsolicited_info.ui_events, \
			sizeof(*event), BUS_DMASYNC_PREREAD)
#define	PDQ_OS_UNSOL_EVENT_POSTSYNC(pdq, event) \
	pdq_os_unsolicited_event_sync((pdq)->pdq_os_ctx, \
			(u_int8_t *) (event) - \
				(u_int8_t *) (pdq)->pdq_unsolicited_info.ui_events, \
			sizeof(*event), BUS_DMASYNC_POSTREAD)
#define	PDQ_OS_DESCBLOCK_SYNC(pdq, what, length, why) \
	pdq_os_descriptor_block_sync((pdq)->pdq_os_ctx, \
			(u_int8_t *) (what) - (u_int8_t *) (pdq)->pdq_dbp, \
			(length), (why))
#define	PDQ_OS_CONSUMER_PRESYNC(pdq) \
	pdq_os_consumer_block_sync((pdq)->pdq_os_ctx, \
			      BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)
#define	PDQ_OS_CONSUMER_POSTSYNC(pdq) \
	pdq_os_consumer_block_sync((pdq)->pdq_os_ctx, \
			      BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)
#define	PDQ_OS_DESC_PRESYNC(pdq, d, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), (d), (s), BUS_DMASYNC_PREWRITE)
#define	PDQ_OS_DESC_POSTSYNC(pdq, d, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), (d), (s), BUS_DMASYNC_POSTWRITE)
#define	PDQ_OS_CMDRQST_PRESYNC(pdq, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), \
			      (pdq)->pdq_command_info.ci_request_bufstart, \
			      (s), BUS_DMASYNC_PREWRITE)
#define	PDQ_OS_CMDRSP_PRESYNC(pdq, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), \
			      (pdq)->pdq_command_info.ci_response_bufstart, \
			      (s), BUS_DMASYNC_PREREAD)
#define	PDQ_OS_CMDRQST_POSTSYNC(pdq, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), \
			      (pdq)->pdq_command_info.ci_request_bufstart, \
			      (s), BUS_DMASYNC_POSTWRITE)
#define	PDQ_OS_CMDRSP_POSTSYNC(pdq, s) \
	PDQ_OS_DESCBLOCK_SYNC((pdq), \
			      (pdq)->pdq_command_info.ci_response_bufstart, \
			      (s), BUS_DMASYNC_POSTREAD)
#define	PDQ_OS_RXPDU_PRESYNC(pdq, b, o, l) \
	pdq_os_databuf_sync((pdq)->pdq_os_ctx, (b), (o), (l), \
			    BUS_DMASYNC_PREREAD)
#define	PDQ_OS_RXPDU_POSTSYNC(pdq, b, o, l) \
	pdq_os_databuf_sync((pdq)->pdq_os_ctx, (b), (o), (l), \
			    BUS_DMASYNC_POSTREAD)
#define	PDQ_OS_DATABUF_ALLOC(pdq, b)	((void)((b) = pdq_os_databuf_alloc((pdq)->pdq_os_ctx)))
#define	PDQ_OS_DATABUF_FREE(pdq, b)	pdq_os_databuf_free((pdq)->pdq_os_ctx, (b))
#define PDQ_OS_DATABUF_BUSPA(pdq, b)	(M_GETCTX((b), bus_dmamap_t)->dm_segs[0].ds_addr + 0)
struct _pdq_os_ctx_t;
extern void pdq_os_descriptor_block_sync(struct _pdq_os_ctx_t *osctx, size_t offset,
					 size_t length, int ops);
extern void pdq_os_consumer_block_sync(struct _pdq_os_ctx_t *osctx, int ops);
extern void pdq_os_unsolicited_event_sync(struct _pdq_os_ctx_t *osctx, size_t offset,
				 	  size_t length, int ops);
extern struct mbuf *pdq_os_databuf_alloc(struct _pdq_os_ctx_t *osctx);
extern void pdq_os_databuf_sync(struct _pdq_os_ctx_t *osctx, struct mbuf *b,
				size_t offset, size_t length, int ops);
extern void pdq_os_databuf_free(struct _pdq_os_ctx_t *osctx, struct mbuf *m);
#define M_HASTXDMAMAP		M_LINK1
#define M_HASRXDMAMAP		M_LINK2
#endif

#define	PDQ_CSR_WRITE(csr, name, data)		PDQ_OS_IOWR_32((csr)->csr_bus, (csr)->csr_base, (csr)->name, data)
#define	PDQ_CSR_READ(csr, name)			PDQ_OS_IORD_32((csr)->csr_bus, (csr)->csr_base, (csr)->name)

#define	PDQ_OS_IFP_TO_SOFTC(ifp)		((pdq_softc_t *) (ifp)->if_softc)
#define	PDQ_ARP_IFINIT(sc, ifa)			arp_ifinit(&(sc)->sc_if, (ifa))
#define	PDQ_FDDICOM(sc)				(&(sc)->sc_ec)
#define	PDQ_LANADDR(sc)				LLADDR((sc)->sc_if.if_sadl)
#define	PDQ_LANADDR_SIZE(sc)			((sc)->sc_if.if_sadl->sdl_alen)
#endif

#if !defined(PDQ_BPF_MTAP)
#define	PDQ_BPF_MTAP(sc, m)	bpf_mtap((sc)->sc_bpf, m)
#endif

#if !defined(PDQ_BPFATTACH)
#define	PDQ_BPFATTACH(sc, t, s)	bpfattach(&(sc)->sc_bpf, &(sc)->sc_if, t, s)
#endif

#if !defined(PDQ_OS_SPL_RAISE)
#define	PDQ_OS_SPL_RAISE()	splimp()
#endif

#if !defined(PDQ_OS_SPL_LOWER)
#define	PDQ_OS_SPL_LOWER(s)	splx(s)
#endif

#if !defined(PDQ_FDDICOM)
#define	PDQ_FDDICOM(sc)		(&(sc)->sc_ac)
#endif

#if !defined(PDQ_ARP_IFINIT)
#define	PDQ_ARP_IFINIT(sc, ifa)	arp_ifinit(&(sc)->sc_ac, (ifa))
#endif

#if !defined(PDQ_OS_PTR_FMT)
#define	PDQ_OS_PTR_FMT	"0x%x"
#endif

#if !defined(PDQ_OS_CSR_FMT)
#define	PDQ_OS_CSR_FMT	"0x%x"
#endif

#if !defined(PDQ_LANADDR)
#define	PDQ_LANADDR(sc)		((sc)->sc_ac.ac_enaddr)
#define	PDQ_LANADDR_SIZE(sc)	(sizeof((sc)->sc_ac.ac_enaddr))
#endif

#if !defined(PDQ_OS_IOMEM)
#define PDQ_OS_IORD_32(t, base, offset)		inl((base) + (offset))
#define PDQ_OS_IOWR_32(t, base, offset, data)	outl((base) + (offset), data)
#define PDQ_OS_IORD_8(t, base, offset)		inb((base) + (offset))
#define PDQ_OS_IOWR_8(t, base, offset, data)	outb((base) + (offset), data)
#define PDQ_OS_MEMRD_32(t, base, offset)	(0 + *((base) + (offset)))
#define PDQ_OS_MEMWR_32(t, base, offset, data)	do *((base) + (offset)) = (data); while (0)
#endif
#ifndef PDQ_CSR_OFFSET
#define	PDQ_CSR_OFFSET(base, offset)		(0 + (base) + (offset))
#endif

#ifndef PDQ_CSR_WRITE
#define	PDQ_CSR_WRITE(csr, name, data)		PDQ_OS_MEMWR_32((csr)->csr_bus, (csr)->name, 0, data)
#define	PDQ_CSR_READ(csr, name)			PDQ_OS_MEMRD_32((csr)->csr_bus, (csr)->name, 0)
#endif

#ifndef PDQ_OS_IFP_TO_SOFTC
#define	PDQ_OS_IFP_TO_SOFTC(ifp)	((pdq_softc_t *) ((caddr_t) ifp - offsetof(pdq_softc_t, sc_ac.ac_if)))
#endif


#if !defined(PDQ_HWSUPPORT)

typedef struct _pdq_os_ctx_t {
#if defined(__bsdi__)
    struct device sc_dev;		/* base device */
    struct isadev sc_id;		/* ISA device */
    struct intrhand sc_ih;		/* interrupt vectoring */
    struct atshutdown sc_ats;		/* shutdown routine */
    struct arpcom sc_ac;
#define	sc_if		sc_ac.ac_if
#elif defined(__NetBSD__)
    struct device sc_dev;		/* base device */
    void *sc_ih;			/* interrupt vectoring */
    void *sc_ats;			/* shutdown hook */
    struct ethercom sc_ec;
    bus_dma_tag_t sc_dmatag;
#define	sc_if		sc_ec.ec_if
#elif defined(__FreeBSD__)
    struct kern_devconf *sc_kdc;	/* freebsd cruft */
    struct arpcom sc_ac;
#define	sc_if		sc_ac.ac_if
#endif
#if defined(IFM_FDDI)
    struct ifmedia sc_ifmedia;
#endif
    pdq_t *sc_pdq;
#if defined(__alpha__) || defined(__i386__)
    pdq_bus_ioport_t sc_iobase;
#endif
#if defined(PDQ_IOMAPPED) && !defined(__NetBSD__)
#define	sc_membase	sc_iobase
#else
    pdq_bus_memaddr_t sc_membase;
#endif
    pdq_bus_t sc_iotag;
    pdq_bus_t sc_csrtag;
#if !defined(__bsdi__) || _BSDI_VERSION >= 199401
#define	sc_bpf		sc_if.if_bpf
#else
    caddr_t sc_bpf;
#endif
#if defined(PDQ_BUS_DMA)
#if !defined(__NetBSD__)
     bus_dma_tag_t sc_dmatag;
#endif
     bus_dmamap_t sc_dbmap;		/* DMA map for descriptor block */
     bus_dmamap_t sc_uimap;		/* DMA map for unsolicited events */
     bus_dmamap_t sc_cbmap;		/* DMA map for consumer block */
#endif
} pdq_softc_t;


extern void pdq_ifreset(pdq_softc_t *sc);
extern void pdq_ifinit(pdq_softc_t *sc);
extern void pdq_ifwatchdog(struct ifnet *ifp);
extern ifnet_ret_t pdq_ifstart(struct ifnet *ifp);
extern int pdq_ifioctl(struct ifnet *ifp, ioctl_cmd_t cmd, caddr_t data);
extern void pdq_ifattach(pdq_softc_t *sc, ifnet_ret_t (*ifwatchdog)(int unit));
#endif /* !PDQ_HWSUPPORT */


#elif defined(DLPI_PDQ)
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/stream.h>

#define	PDQ_USE_STREAMS
#define	PDQ_OS_PREFIX			"%s board %d "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name, pdq->pdq_unit

#define	PDQ_OS_PAGESIZE			PAGESIZE
#define	PDQ_OS_USEC_DELAY(n)		drv_usecwait(n)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#define	PDQ_OS_VA_TO_BUSPA(pdq, p)		vtop((caddr_t)p, NULL)
#define	PDQ_OS_MEMALLOC(n)		kmem_zalloc(n, KM_NOSLEEP)
#define	PDQ_OS_MEMFREE(p, n)		kmem_free((caddr_t) p, n)
#define	PDQ_OS_MEMALLOC_CONTIG(n)	kmem_zalloc_physreq(n, decfddiphysreq_db, KM_NOSLEEP)
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	PDQ_OS_MEMFREE(p, n)

extern physreq_t *decfddiphysreq_db;
extern physreq_t *decfddiphysreq_mblk;

#define	PDQ_OS_DATABUF_ALLOC(pdq, b)		((void) (((b) = allocb_physreq(PDQ_OS_DATABUF_SIZE, BPRI_MED, decfddiphysreq_mblk)) && ((b)->b_wptr = (b)->b_rptr + PDQ_OS_DATABUF_SIZE)))

#define PDQ_OS_IORD_8(port)		inb(port)
#define PDQ_OS_IOWR_8(port, data)	outb(port, data)
#endif


#ifdef PDQ_USE_MBUFS
#define	PDQ_OS_DATABUF_SIZE			(MCLBYTES)
#ifndef PDQ_OS_DATABUF_FREE
#define	PDQ_OS_DATABUF_FREE(pdq, b)		(m_freem(b))
#endif
#define	PDQ_OS_DATABUF_NEXT(b)			((b)->m_next)
#define	PDQ_OS_DATABUF_NEXT_SET(b, b1)		((b)->m_next = (b1))
#define	PDQ_OS_DATABUF_NEXTPKT(b)		((b)->m_nextpkt)
#define	PDQ_OS_DATABUF_NEXTPKT_SET(b, b1)	((b)->m_nextpkt = (b1))
#define	PDQ_OS_DATABUF_LEN(b)			((b)->m_len)
#define	PDQ_OS_DATABUF_LEN_SET(b, n)		((b)->m_len = (n))
/* #define	PDQ_OS_DATABUF_LEN_ADJ(b, n)		((b)->m_len += (n)) */
#define	PDQ_OS_DATABUF_PTR(b)			(mtod((b), pdq_uint8_t *))
#define	PDQ_OS_DATABUF_ADJ(b, n)		((b)->m_data += (n), (b)->m_len -= (n))
typedef struct mbuf PDQ_OS_DATABUF_T;

#ifndef PDQ_OS_DATABUF_ALLOC
#define	PDQ_OS_DATABUF_ALLOC(pdq, b) do { \
    PDQ_OS_DATABUF_T *x_m0; \
    MGETHDR(x_m0, M_DONTWAIT, MT_DATA); \
    if (x_m0 != NULL) { \
	MCLGET(x_m0, M_DONTWAIT);	\
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
#endif
#define	PDQ_OS_DATABUF_RESET(b)	((b)->m_data = (b)->m_ext.ext_buf, (b)->m_len = MCLBYTES)
#endif /* PDQ_USE_MBUFS */

#ifdef PDQ_USE_STREAMS
#define	PDQ_OS_DATABUF_SIZE			(2048)
#define	PDQ_OS_DATABUF_FREE(pdq, b)		(freemsg(b))
#define	PDQ_OS_DATABUF_NEXT(b)			((b)->b_cont)
#define	PDQ_OS_DATABUF_NEXT_SET(b, b1)		((b)->b_cont = (b1))
#define	PDQ_OS_DATABUF_NEXTPKT(b)		((b)->b_next)
#define	PDQ_OS_DATABUF_NEXTPKT_SET(b, b1)	((b)->b_next = (b1))
#define	PDQ_OS_DATABUF_LEN(b)			((b)->b_wptr - (b)->b_rptr)
#define	PDQ_OS_DATABUF_LEN_SET(b, n)		((b)->b_wptr = (b)->b_rptr + (n))
/*#define	PDQ_OS_DATABUF_LEN_ADJ(b, n)		((b)->b_wptr += (n))*/
#define	PDQ_OS_DATABUF_PTR(b)			((pdq_uint8_t *) (b)->b_rptr)
#define	PDQ_OS_DATABUF_ADJ(b, n)		((b)->b_rptr += (n))
typedef mblk_t PDQ_OS_DATABUF_T;

#ifndef	PDQ_OS_DATABUF_ALLOC
#define	PDQ_OS_DATABUF_ALLOC(pdq, b)			((void) (((b) = allocb(PDQ_OS_DATABUF_SIZE, BPRI_MED)) && ((b)->b_wptr = (b)->b_rptr + PDQ_OS_DATABUF_SIZE)))
#endif /* PDQ_OS_DATABUF_ALLOC */
#endif /* PDQ_USE_STREAMS */

#define	PDQ_OS_TX_TRANSMIT		5

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

#if !defined(PDQ_OS_CONSUMER_PRESYNC)
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
#endif

#ifndef PDQ_OS_DATABUF_BUSPA
#define PDQ_OS_DATABUF_BUSPA(pdq, b)	PDQ_OS_VA_TO_BUSPA(pdq, PDQ_OS_DATABUF_PTR(b))
#endif

#ifndef PDQ_OS_HDR_OFFSET
#define	PDQ_OS_HDR_OFFSET	PDQ_RX_FC_OFFSET
#endif

extern void pdq_os_addr_fill(pdq_t *pdq, pdq_lanaddr_t *addrs, size_t numaddrs);
extern void pdq_os_receive_pdu(pdq_t *, PDQ_OS_DATABUF_T *pdu, size_t pdulen, int drop);
extern void pdq_os_restart_transmitter(pdq_t *pdq);
extern void pdq_os_transmit_done(pdq_t *pdq, PDQ_OS_DATABUF_T *pdu);
#if !defined(pdq_os_update_status)
extern void pdq_os_update_status(pdq_t *pdq, const void *rsp);
#endif
#if !defined(PDQ_OS_MEMALLOC_CONTIG)
extern int pdq_os_memalloc_contig(pdq_t *pdq);
#endif
extern pdq_boolean_t pdq_queue_transmit_data(pdq_t *pdq, PDQ_OS_DATABUF_T *pdu);
extern void pdq_flush_transmitter(pdq_t *pdq);

extern void pdq_run(pdq_t *pdq);
extern pdq_state_t pdq_stop(pdq_t *pdq);
extern void pdq_hwreset(pdq_t *pdq);

extern int pdq_interrupt(pdq_t *pdq);
extern pdq_t *pdq_initialize(pdq_bus_t bus, pdq_bus_memaddr_t csr_va,
			     const char *name, int unit,
			     void *ctx, pdq_type_t type);
#endif /* _PDQ_OS_H */
