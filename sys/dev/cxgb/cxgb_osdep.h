/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


$FreeBSD$

***************************************************************************/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/endian.h>
#include <sys/bus.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/mii/mii.h>

#include <common/cxgb_version.h>
#include <cxgb_config.h>

#ifndef _CXGB_OSDEP_H_
#define _CXGB_OSDEP_H_

typedef struct adapter adapter_t;
struct sge_rspq;

enum {
	TP_TMR_RES = 200,	/* TP timer resolution in usec */
	MAX_NPORTS = 4,		/* max # of ports */
	TP_SRAM_OFFSET = 4096,	/* TP SRAM content offset in eeprom */
	TP_SRAM_LEN = 2112,	/* TP SRAM content offset in eeprom */
};

struct t3_mbuf_hdr {
	struct mbuf *mh_head;
	struct mbuf *mh_tail;
};

#ifndef PANIC_IF
#define PANIC_IF(exp) do {                  \
	if (exp)                            \
		panic("BUG: %s", #exp);      \
} while (0)
#endif

#define m_get_priority(m) ((uintptr_t)(m)->m_pkthdr.rcvif)
#define m_set_priority(m, pri) ((m)->m_pkthdr.rcvif = (struct ifnet *)((uintptr_t)pri))
#define m_set_sgl(m, sgl) ((m)->m_pkthdr.header = (sgl))
#define m_get_sgl(m) ((bus_dma_segment_t *)(m)->m_pkthdr.header)
#define m_set_sgllen(m, len) ((m)->m_pkthdr.ether_vtag = len)
#define m_get_sgllen(m) ((m)->m_pkthdr.ether_vtag)

/*
 * XXX FIXME
 */
#define m_set_toep(m, a) ((m)->m_pkthdr.header = (a))
#define m_get_toep(m) ((m)->m_pkthdr.header)
#define m_set_handler(m, handler) ((m)->m_pkthdr.header = (handler))

#define m_set_socket(m, a) ((m)->m_pkthdr.header = (a))
#define m_get_socket(m) ((m)->m_pkthdr.header)

#define	KTR_CXGB	KTR_SPARE2

#define MT_DONTFREE  128

#if __FreeBSD_version > 700030
#define INTR_FILTERS
#define FIRMWARE_LATEST
#endif

#if ((__FreeBSD_version > 602103) && (__FreeBSD_version < 700000))
#define FIRMWARE_LATEST
#endif

#if __FreeBSD_version > 700000
#define MSI_SUPPORTED
#define TSO_SUPPORTED
#define VLAN_SUPPORTED
#define TASKQUEUE_CURRENT
#else
#define if_name(ifp) (ifp)->if_xname
#define M_SANITY(m, n)
#endif

#if __FreeBSD_version >= 701000
#include "opt_inet.h"
#ifdef INET
#define LRO_SUPPORTED
#endif
#define TOE_SUPPORTED
#endif

#define __read_mostly __attribute__((__section__(".data.read_mostly")))

/*
 * Workaround for weird Chelsio issue
 */
#if __FreeBSD_version > 700029
#define PRIV_SUPPORTED
#endif

#define CXGB_TX_CLEANUP_THRESHOLD        32


#ifdef DEBUG_PRINT
#define DPRINTF printf
#else 
#define DPRINTF(...)
#endif

#define TX_MAX_SIZE                (1 << 16)    /* 64KB                          */
#define TX_MAX_SEGS                      36     /* maximum supported by card     */

#define TX_MAX_DESC                       4     /* max descriptors per packet    */


#define TX_START_MIN_DESC  (TX_MAX_DESC << 2)
#define TX_START_MAX_DESC (TX_MAX_DESC << 3)    /* maximum number of descriptors
						 * call to start used per 	 */

#define TX_CLEAN_MAX_DESC (TX_MAX_DESC << 4)    /* maximum tx descriptors
						 * to clean per iteration        */
#define TX_WR_SIZE_MAX    11*1024              /* the maximum total size of packets aggregated into a single
						* TX WR
						*/
#define TX_WR_COUNT_MAX         7              /* the maximum total number of packets that can be
						* aggregated into a single TX WR
						*/


#if defined(__i386__) || defined(__amd64__)
#define mb()    __asm volatile("mfence":::"memory")
#define rmb()   __asm volatile("lfence":::"memory")
#define wmb()   __asm volatile("sfence" ::: "memory")
#define smp_mb() mb()

#define L1_CACHE_BYTES 128
static __inline
void prefetch(void *x) 
{ 
        __asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
} 

extern void kdb_backtrace(void);

#define WARN_ON(condition) do { \
       if (__predict_false((condition)!=0)) {  \
                log(LOG_WARNING, "BUG: warning at %s:%d/%s()\n", __FILE__, __LINE__, __FUNCTION__); \
                kdb_backtrace(); \
        } \
} while (0)


#else /* !i386 && !amd64 */
#define mb()
#define rmb()
#define wmb()
#define smp_mb()
#define prefetch(x)
#define L1_CACHE_BYTES 32
#endif

struct buf_ring {
	caddr_t          *br_ring;
	volatile uint32_t br_cons;
	volatile uint32_t br_prod;
	int               br_size;
	struct mtx        br_lock;
};

struct buf_ring *buf_ring_alloc(int count, int flags);
void buf_ring_free(struct buf_ring *);

static __inline int
buf_ring_count(struct buf_ring *mr)
{
	int size = mr->br_size;
	uint32_t mask = size - 1;
	
	return ((size + mr->br_prod - mr->br_cons) & mask);
}

static __inline int
buf_ring_empty(struct buf_ring *mr)
{
	return (mr->br_cons == mr->br_prod);
}

static __inline int
buf_ring_full(struct buf_ring *mr)
{
	uint32_t mask;

	mask = mr->br_size - 1;
	return (mr->br_cons == ((mr->br_prod + 1) & mask));
}

/*
 * The producer and consumer are independently locked
 * this relies on the consumer providing his own serialization
 *
 */
static __inline void *
buf_ring_dequeue(struct buf_ring *mr)
{
	uint32_t prod, cons, mask;
	caddr_t *ring, m;
	
	ring = (caddr_t *)mr->br_ring;
	mask = mr->br_size - 1;
	cons = mr->br_cons;
	mb();
	prod = mr->br_prod;
	m = NULL;
	if (cons != prod) {
		m = ring[cons];
		ring[cons] = NULL;
		mr->br_cons = (cons + 1) & mask;
		mb();
	}
	return (m);
}

#ifdef DEBUG_BUFRING
static __inline void
__buf_ring_scan(struct buf_ring *mr, void *m, char *file, int line)
{
	int i;

	for (i = 0; i < mr->br_size; i++)
		if (m == mr->br_ring[i])
			panic("%s:%d m=%p present prod=%d cons=%d idx=%d", file,
			    line, m, mr->br_prod, mr->br_cons, i);
}

static __inline void
buf_ring_scan(struct buf_ring *mr, void *m, char *file, int line)
{
	mtx_lock(&mr->br_lock);
	__buf_ring_scan(mr, m, file, line);
	mtx_unlock(&mr->br_lock);
}

#else
static __inline void
__buf_ring_scan(struct buf_ring *mr, void *m, char *file, int line)
{
}

static __inline void
buf_ring_scan(struct buf_ring *mr, void *m, char *file, int line)
{
}
#endif

static __inline int
__buf_ring_enqueue(struct buf_ring *mr, void *m, char *file, int line)
{
	
	uint32_t prod, cons, mask;
	int err;
	
	mask = mr->br_size - 1;
	prod = mr->br_prod;
	mb();
	cons = mr->br_cons;
	__buf_ring_scan(mr, m, file, line);
	if (((prod + 1) & mask) != cons) {
		KASSERT(mr->br_ring[prod] == NULL, ("overwriting entry"));
		mr->br_ring[prod] = m;
		mb();
		mr->br_prod = (prod + 1) & mask;
		err = 0;
	} else
		err = ENOBUFS;

	return (err);
}

static __inline int
buf_ring_enqueue_(struct buf_ring *mr, void *m, char *file, int line)
{
	int err;
	
	mtx_lock(&mr->br_lock);
	err = __buf_ring_enqueue(mr, m, file, line);
	mtx_unlock(&mr->br_lock);

	return (err);
}

#define buf_ring_enqueue(mr, m) buf_ring_enqueue_((mr), (m), __FILE__, __LINE__)


static __inline void *
buf_ring_peek(struct buf_ring *mr)
{
	int prod, cons, mask;
	caddr_t *ring, m;
	
	ring = (caddr_t *)mr->br_ring;
	mask = mr->br_size - 1;
	cons = mr->br_cons;
	prod = mr->br_prod;
	m = NULL;
	if (cons != prod)
		m = ring[cons];

	return (m);
}

#define DBG_RX          (1 << 0)
static const int debug_flags = DBG_RX;

#ifdef DEBUG_PRINT
#define DBG(flag, msg) do {	\
	if ((flag & debug_flags))	\
		printf msg; \
} while (0)
#else
#define DBG(...)
#endif

#include <sys/syslog.h>

#define promisc_rx_mode(rm)  ((rm)->port->ifp->if_flags & IFF_PROMISC) 
#define allmulti_rx_mode(rm) ((rm)->port->ifp->if_flags & IFF_ALLMULTI) 

#define CH_ERR(adap, fmt, ...) log(LOG_ERR, fmt, ##__VA_ARGS__)
#define CH_WARN(adap, fmt, ...)	log(LOG_WARNING, fmt, ##__VA_ARGS__)
#define CH_ALERT(adap, fmt, ...) log(LOG_ALERT, fmt, ##__VA_ARGS__)

#define t3_os_sleep(x) DELAY((x) * 1000)

#define test_and_clear_bit(bit, p) atomic_cmpset_int((p), ((*(p)) | (1<<bit)), ((*(p)) & ~(1<<bit)))

#define max_t(type, a, b) (type)max((a), (b))
#define net_device ifnet
#define cpu_to_be32            htobe32

/* Standard PHY definitions */
#define BMCR_LOOPBACK		BMCR_LOOP
#define BMCR_ISOLATE		BMCR_ISO
#define BMCR_ANENABLE		BMCR_AUTOEN
#define BMCR_SPEED1000		BMCR_SPEED1
#define BMCR_SPEED100		BMCR_SPEED0
#define BMCR_ANRESTART		BMCR_STARTNEG
#define BMCR_FULLDPLX		BMCR_FDX
#define BMSR_LSTATUS		BMSR_LINK
#define BMSR_ANEGCOMPLETE	BMSR_ACOMP

#define MII_LPA			MII_ANLPAR
#define MII_ADVERTISE		MII_ANAR
#define MII_CTRL1000		MII_100T2CR

#define ADVERTISE_PAUSE_CAP	ANAR_FC
#define ADVERTISE_PAUSE_ASYM	ANAR_X_PAUSE_ASYM
#define ADVERTISE_PAUSE		ANAR_X_PAUSE_SYM
#define ADVERTISE_1000HALF	ANAR_X_HD
#define ADVERTISE_1000FULL	ANAR_X_FD
#define ADVERTISE_10FULL	ANAR_10_FD
#define ADVERTISE_10HALF	ANAR_10
#define ADVERTISE_100FULL	ANAR_TX_FD
#define ADVERTISE_100HALF	ANAR_TX


#define ADVERTISE_1000XHALF	ANAR_X_HD
#define ADVERTISE_1000XFULL	ANAR_X_FD
#define ADVERTISE_1000XPSE_ASYM	ANAR_X_PAUSE_ASYM
#define ADVERTISE_1000XPAUSE	ANAR_X_PAUSE_SYM

#define ADVERTISE_CSMA		ANAR_CSMA
#define ADVERTISE_NPAGE		ANAR_NP


/* Standard PCI Extended Capaibilities definitions */
#define PCI_CAP_ID_VPD	0x03
#define PCI_VPD_ADDR	2
#define PCI_VPD_ADDR_F	0x8000
#define PCI_VPD_DATA	4

#define PCI_CAP_ID_EXP	0x10
#define PCI_EXP_DEVCTL	8
#define PCI_EXP_DEVCTL_PAYLOAD 0x00e0
#define PCI_EXP_LNKCTL	16
#define PCI_EXP_LNKSTA	18

/*
 * Linux compatibility macros
 */

/* Some simple translations */
#define __devinit
#define udelay(x) DELAY(x)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define le32_to_cpu(x) le32toh(x)
#define le16_to_cpu(x) le16toh(x)
#define cpu_to_le32(x) htole32(x)
#define swab32(x) bswap32(x)
#define simple_strtoul strtoul


#ifndef LINUX_TYPES_DEFINED
typedef uint8_t 	u8;
typedef uint16_t 	u16;
typedef uint32_t 	u32;
typedef uint64_t 	u64;
 
typedef uint8_t		__u8;
typedef uint16_t	__u16;
typedef uint32_t	__u32;
typedef uint8_t		__be8;
typedef uint16_t	__be16;
typedef uint32_t	__be32;
typedef uint64_t	__be64;
#endif


#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN_BITFIELD
#elif BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN_BITFIELD
#else
#error "Must set BYTE_ORDER"
#endif

/* Indicates what features are supported by the interface. */
#define SUPPORTED_10baseT_Half          (1 << 0)
#define SUPPORTED_10baseT_Full          (1 << 1)
#define SUPPORTED_100baseT_Half         (1 << 2)
#define SUPPORTED_100baseT_Full         (1 << 3)
#define SUPPORTED_1000baseT_Half        (1 << 4)
#define SUPPORTED_1000baseT_Full        (1 << 5)
#define SUPPORTED_Autoneg               (1 << 6)
#define SUPPORTED_TP                    (1 << 7)
#define SUPPORTED_AUI                   (1 << 8)
#define SUPPORTED_MII                   (1 << 9) 
#define SUPPORTED_FIBRE                 (1 << 10)
#define SUPPORTED_BNC                   (1 << 11)
#define SUPPORTED_10000baseT_Full       (1 << 12)
#define SUPPORTED_Pause                 (1 << 13)
#define SUPPORTED_Asym_Pause            (1 << 14)

/* Indicates what features are advertised by the interface. */
#define ADVERTISED_10baseT_Half         (1 << 0)
#define ADVERTISED_10baseT_Full         (1 << 1)
#define ADVERTISED_100baseT_Half        (1 << 2)
#define ADVERTISED_100baseT_Full        (1 << 3)
#define ADVERTISED_1000baseT_Half       (1 << 4)
#define ADVERTISED_1000baseT_Full       (1 << 5)
#define ADVERTISED_Autoneg              (1 << 6)
#define ADVERTISED_TP                   (1 << 7)
#define ADVERTISED_AUI                  (1 << 8)
#define ADVERTISED_MII                  (1 << 9)
#define ADVERTISED_FIBRE                (1 << 10) 
#define ADVERTISED_BNC                  (1 << 11)
#define ADVERTISED_10000baseT_Full      (1 << 12)
#define ADVERTISED_Pause                (1 << 13)
#define ADVERTISED_Asym_Pause           (1 << 14)

/* Enable or disable autonegotiation.  If this is set to enable,
 * the forced link modes above are completely ignored.
 */
#define AUTONEG_DISABLE         0x00
#define AUTONEG_ENABLE          0x01

#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000
#define SPEED_10000		10000
#define DUPLEX_HALF		0
#define DUPLEX_FULL		1

#endif
