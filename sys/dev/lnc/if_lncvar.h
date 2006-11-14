/*-
 * Copyright (c) 1994-1998
 *      Paul Richards.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
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

/*
 * Initialize multicast address hashing registers to accept
 * all multicasts (only used when in promiscuous mode)
 */
#define MULTI_INIT_ADDR 0xff

#define NORMAL 0

#define NRDRE 3
#define NTDRE 3
#define RECVBUFSIZE 1518	/* Packet size rounded to dword boundary */
#define TRANSBUFSIZE 1518
#define MBUF_CACHE_LIMIT 0

#define MEM_SLEW 8

/* LNC Flags */
#define LNC_INITIALISED 1
#define LNC_ALLMULTI 2

/* BICC port addresses */
#define BICC_IOSIZE    16
#define BICC_RDP     0x0c        /* Register Data Port */
#define BICC_RAP     0x0e        /* Register Address Port */

/* NE2100 port addresses */
#define NE2100_IOSIZE  24
#define PCNET_RDP    0x10        /* Register Data Port */
#define PCNET_RAP    0x12        /* Register Address Port */
#define PCNET_RESET  0x14
#define PCNET_BDP    0x16
#define PCNET_VSW    0x18

/* DEPCA port addresses */
#define DEPCA_IOSIZE   16
#define DEPCA_CTRL   0x00        /* NIC Control and status register */
#define DEPCA_RDP    0x04        /* Register Data Port */
#define DEPCA_RAP    0x06        /* Register Address Port */
#define DEPCA_ADP    0x0c

/* DEPCA specific defines */
#define DEPCA_ADDR_ROM_SIZE 32

/* C-NET(98)S port addresses */
/* Notice, we can ignore fragmantation by using isa_alloc_resourcev(). */
#define CNET98S_IOSIZE   32     
#define CNET98S_RDP    0x10      /* Register Data Port */
#define CNET98S_RAP    0x12      /* Register Address Port */
#define CNET98S_RESET  0x14
#define CNET98S_IDP    0x16
#define CNET98S_EEPROM 0x1e

/* Chip types */
#define LANCE           1        /* Am7990   */
#define C_LANCE         2        /* Am79C90  */
#define PCnet_ISA       3        /* Am79C960 */
#define PCnet_ISAplus   4        /* Am79C961 */
#define PCnet_ISA_II    5        /* Am79C961A */
#define PCnet_32        6        /* Am79C965 */
#define PCnet_PCI       7        /* Am79C970 */
#define PCnet_PCI_II    8        /* Am79C970A */
#define PCnet_FAST      9        /* Am79C971 */
#define PCnet_FASTplus  10       /* Am79C972 */
#define PCnet_Home	11	 /* Am79C978 */


/* CSR88-89: Chip ID masks */
#define AMD_MASK  0x003
#define PART_MASK 0xffff
#define Am79C960  0x0003
#define Am79C961  0x2260
#define Am79C961A 0x2261
#define Am79C965  0x2430
#define Am79C970  0x0242
#define Am79C970A 0x2621
#define Am79C971  0x2623
#define Am79C972  0x2624
#define Am79C973  0x2625
#define Am79C978  0x2626

/* Board types */
#define UNKNOWN         0
#define BICC            1
#define NE2100          2
#define DEPCA           3
#define CNET98S         4	/* PC-98 */

/* mem_mode values */
#define DMA_FIXED       1
#define DMA_MBUF        2
#define SHMEM           4

#define MEM_MODES \
	"\20\3SHMEM\2DMA_MBUF\1DMA_FIXED"

#define CSR0_FLAGS \
	"\20\20ERR\17BABL\16CERR\15MISS\14MERR\13RINT\12TINT\11IDON\
	    \10INTR\07INEA\06RXON\05TXON\04TDMD\03STOP\02STRT\01INIT"

#define INIT_MODE \
	"\20\20PROM\07INTL\06DRTY\05COLL\04DTCR\03LOOP\02DTX\01DRX"

#define RECV_MD1 \
	"\20\10OWN\7ERR\6FRAM\5OFLO\4CRC\3BUFF\2STP\1ENP"

#define TRANS_MD1 \
	"\20\10OWN\7ERR\6RES\5MORE\4ONE\3DEF\2STP\1ENP"

#define TRANS_MD3 \
	"\20\6BUFF\5UFLO\4RES\3LCOL\2LCAR\1RTRY"

#ifdef LNC_KEEP_STATS
#define LNCSTATS_STRUCT \
	struct lnc_stats { \
		int idon; \
		int rint; \
		int tint; \
		int cerr; \
		int babl; \
		int miss; \
		int merr; \
		int rxoff; \
		int txoff; \
		int terr; \
		int lcol; \
		int lcar; \
		int tbuff; \
		int def; \
		int more; \
		int one; \
		int uflo; \
		int rtry; \
		int rerr; \
		int fram; \
		int oflo; \
		int crc; \
		int rbuff; \
		int drop_packet; \
		int trans_ring_full; \
	} lnc_stats;
#define LNCSTATS(X) ++(sc->lnc_stats.X);
#else
#define LNCSTATS_STRUCT
#define LNCSTATS(X)
#endif

struct nic_info {
	int ident;         /* Type of card */
	int ic;            /* Type of ic, Am7990, Am79C960 etc. */
	int mem_mode;
	int iobase;
	int mode;          /* Mode setting at initialization */
};

typedef struct lnc_softc {
	struct resource *irqres;
	int irqrid;
	struct resource *drqres;
	int drqrid;
	struct resource *portres;
	int portrid;
	bus_space_tag_t lnc_btag;
	bus_space_handle_t lnc_bhandle;
	void *intrhand;
	bus_dma_tag_t	dmat;
	bus_dmamap_t	dmamap;
	struct ifnet *ifp;
	struct nic_info nic;                /* NIC specific info */
	int nrdre;
	struct host_ring_entry *recv_ring;  /* start of alloc'd mem */
	int recv_next;
	int ntdre;
	struct host_ring_entry *trans_ring;
	int trans_next;
	struct init_block *init_block;      /* Initialisation block */
	int pending_transmits;        /* No. of transmit descriptors in
	use */
	int next_to_send;
	struct mbuf *mbufs;
	int mbuf_count;
	int flags;
	int rap;
	int rdp;
	int bdp;
	#ifdef DEBUG
	int lnc_debug;
	#endif
	LNCSTATS_STRUCT
} lnc_softc_t;

struct host_ring_entry {
	struct mds *md;
	union {
		struct mbuf *mbuf;
		char *data;
	}buff;
};

#define NDESC(len2) (1 << len2)

#define INC_MD_PTR(ptr, no_entries) \
	if (++ptr >= NDESC(no_entries)) \
		ptr = 0;

#define DEC_MD_PTR(ptr, no_entries) \
	if (--ptr < 0) \
		ptr = NDESC(no_entries) - 1;

#define RECV_NEXT (sc->recv_ring->base + sc->recv_next)
#define TRANS_NEXT (sc->trans_ring->base + sc->trans_next)

#define lnc_inb(port) \
	bus_space_read_1(sc->lnc_btag, sc->lnc_bhandle, (port))
#define lnc_inw(port) \
	bus_space_read_2(sc->lnc_btag, sc->lnc_bhandle, (port))
#define lnc_outw(port, val) \
	bus_space_write_2(sc->lnc_btag, sc->lnc_bhandle, (port), (val))

/* Functional declarations */
extern int lance_probe(struct lnc_softc *);
extern void lnc_release_resources(device_t);
extern int lnc_attach_common(device_t);
extern int lnc_detach_common(device_t);
extern void lnc_stop(struct lnc_softc *);

extern void write_csr(struct lnc_softc *, u_short, u_short);
extern u_short read_csr(struct lnc_softc *, u_short);

/* Variable declarations */
extern driver_intr_t lncintr; 
extern devclass_t lnc_devclass;
