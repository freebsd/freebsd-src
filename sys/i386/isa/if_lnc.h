/*
 * Copyright (c) 1994, Paul Richards. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 *
 */

#include "ic/Am7990.h"

/*
 * Initialize multicast address hashing registers to accept
 * all multicasts (only used when in promiscuous mode)
 */
#if NBPFILTER > 0
#define MULTI_INIT_ADDR 0xff
#else
#define MULTI_INIT_ADDR 0
#endif

#define NORMAL 0

#define NRDRE 3
#define NTDRE 3
#define RECVBUFSIZE 1518	/* Packet size rounded to dword boundary */
#define TRANSBUFSIZE 1518
#define MBUF_CACHE_LIMIT 0

#define MEM_SLEW 8


/* BICC port addresses */
#define BICC_IOSIZE    16
#define BICC_RDP     0x0c        /* Register Data Port */
#define BICC_RAP     0x0e        /* Register Address Port */

/* NE2100 port addresses */
#define NE2100_IOSIZE  24
#define PCNET_RDP    0x10        /* Register Data Port */
#define PCNET_RAP    0x12        /* Register Address Port */
#define PCNET_RESET  0x14
#define PCNET_IDP    0x16
#define PCNET_VSW    0x18

/* DEPCA port addresses */
#define DEPCA_IOSIZE   16
#define DEPCA_CTRL   0x00        /* NIC Control and status register */
#define DEPCA_RDP    0x04        /* Register Data Port */
#define DEPCA_RAP    0x06        /* Register Address Port */
#define DEPCA_ADP    0x0c

/* DEPCA specific defines */
#define DEPCA_ADDR_ROM_SIZE 32

/* Chip types */
#define LANCE           1        /* Am7990   */
#define C_LANCE         2        /* Am79C90  */
#define PCnet_ISA       3        /* Am79C960 */
#define PCnet_ISAplus  4        /* Am79C961 */
#define PCnet_32        5        /* Am79C965 */
#define PCnet_PCI       6        /* Am79C970 */

/* CSR88-89: Chip ID masks */
#define AMD_MASK  0x003
#define PART_MASK 0xffff
#define Am79C960  0x0003
#define Am79C961  0x2260
#define Am79C965  0x2430
#define Am79C970  0x0242

/* Board types */
#define UNKNOWN         0
#define BICC            1
#define NE2100          2
#define DEPCA           3

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

struct nic_info {
	int ident;         /* Type of card */
	int ic;            /* Type of ic, Am7990, Am79C960 etc. */
	int mem_mode;
	int iobase;
	int mode;          /* Mode setting at initialization */
};

struct host_ring_entry {
	struct mds *md;
	union {
		struct mbuf *mbuf;
		char *data;
	}buff;
};

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

#define NDESC(len2) (1 << len2)

#define INC_MD_PTR(ptr, no_entries) \
	if (++ptr >= NDESC(no_entries)) \
		ptr = 0;

#define DEC_MD_PTR(ptr, no_entries) \
	if (--ptr < 0) \
		ptr = NDESC(no_entries) - 1;

#define RECV_NEXT (sc->recv_ring->base + sc->recv_next)
#define TRANS_NEXT (sc->trans_ring->base + sc->trans_next)
