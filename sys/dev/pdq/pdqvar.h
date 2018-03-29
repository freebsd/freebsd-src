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
#else

#include <sys/param.h>
#include <sys/systm.h>
#ifndef M_MCAST
#include <sys/mbuf.h>
#endif /* M_CAST */
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#define	PDQ_OS_PREFIX			"%s: "
#define	PDQ_OS_PREFIX_ARGS		pdq->pdq_os_name
#define	PDQ_OS_PAGESIZE			PAGE_SIZE
#define	PDQ_OS_USEC_DELAY(n)		DELAY(n)
#define	PDQ_OS_MEMZERO(p, n)		bzero((caddr_t)(p), (n))
#if !defined(PDQ_BUS_DMA)
#define	PDQ_OS_VA_TO_BUSPA(pdq, p)		vtophys(p)
#endif
#define	PDQ_OS_MEMALLOC(n)		malloc(n, M_DEVBUF, M_NOWAIT)
#define	PDQ_OS_MEMFREE(p, n)		free((void *) p, M_DEVBUF)
#define	PDQ_OS_MEMALLOC_CONTIG(n)	contigmalloc(n, M_DEVBUF, M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0)
#define	PDQ_OS_MEMFREE_CONTIG(p, n)	contigfree((void *) p, n, M_DEVBUF)

#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <machine/cpufunc.h>
#define	ifnet_ret_t void
typedef int ioctl_cmd_t;
typedef enum { PDQ_BUS_EISA, PDQ_BUS_PCI } pdq_bus_t;
typedef	u_int16_t pdq_bus_ioport_t;
typedef volatile pdq_uint32_t *pdq_bus_memaddr_t;
typedef pdq_bus_memaddr_t pdq_bus_memoffset_t;
#define	pdq_os_update_status(a, b)	((void) 0)


#if !defined(PDQ_OS_SPL_RAISE)
#define	PDQ_OS_SPL_RAISE()	splimp()
#endif

#if !defined(PDQ_OS_SPL_LOWER)
#define	PDQ_OS_SPL_LOWER(s)	splx(s)
#endif

#if !defined(PDQ_FDDICOM)
#define	PDQ_FDDICOM(sc)		(&(sc)->sc_ac)
#endif

#if !defined(PDQ_IFNET)
#define PDQ_IFNET(sc)		(PDQ_FDDICOM((sc))->ac_ifp)
#endif

#define	PDQ_BPF_MTAP(sc, m)	bpf_mtap(PDQ_IFNET(sc), m)
#define	PDQ_BPFATTACH(sc, t, s)	bpfattach(PDQ_IFNET(sc), t, s)

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
    struct kern_devconf *sc_kdc;	/* freebsd cruft */
    struct arpcom sc_ac;
#if defined(IFM_FDDI)
    struct ifmedia sc_ifmedia;
#endif
    pdq_t *sc_pdq;
#if defined(__i386__)
    pdq_bus_ioport_t sc_iobase;
#endif
#if defined(PDQ_IOMAPPED)
#define	sc_membase	sc_iobase
#else
    pdq_bus_memaddr_t sc_membase;
#endif
    pdq_bus_t sc_iotag;
    pdq_bus_t sc_csrtag;
    caddr_t sc_bpf;
#if defined(PDQ_BUS_DMA)
     bus_dma_tag_t sc_dmatag;
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


#endif


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
    MGETHDR(x_m0, M_NOWAIT, MT_DATA); \
    if (x_m0 != NULL) { \
	if (!(MCLGET(x_m0, M_NOWAIT))) { \
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
