/*-
 * Copyright (c) 2000, 2001 Richard Hodges and Matriplex, inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by Matriplex, inc.
 * 4. The name of the author may not be used to endorse or promote products
 *	derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 *
 * This driver is derived from the Nicstar driver by Mark Tinguely, and
 * some of the original driver still exists here.  Those portions are...
 *   Copyright (c) 1996, 1997, 1998, 1999 Mark Tinguely
 *   All rights reserved.
 *
 ******************************************************************************
 *
 *  This driver supports the Fore LE155, LE25, and IDT 77211 cards.
 *
 *  ATM CBR connections are supported, and bandwidth is allocated in
 *  slots of 64k each.  Three VBR queues handle traffic for VBR and
 *  UBR.  Two UBR queues prioritize UBR traffic.  ILMI and signalling
 *  get the higher priority queue, as well as UBR traffic that specifies
 *  a peak cell rate.  All other UBR traffic goes into the lower queue.
 *
 ******************************************************************************
 *
 * $FreeBSD: src/sys/dev/idt/idtvar.h,v 1.3 2005/01/06 01:42:46 imp Exp $
 */

/*******************************************************************************
 *
 *  New data types
 */

typedef struct {
	struct mbuf *mget;	/* head of mbuf queue, pull mbufs from here */
	struct mbuf **mput;	/* tail (ptr to m_nextpkt)  put mbufs here */
	u_long scd;		/* segmentation channel descriptor address */
	u_long *scq_base;	/* segmentation channel queue base address */
	u_long *scq_next;	/* next address */
	u_long *scq_last;	/* last address written */
	int scq_len;		/* size of SCQ buffer (64 or 512) */
	int scq_cur;		/* current number entries in SCQ buffer */
	int rate;		/* cells per second allocated to this queue */
	int vbr_m;		/* VBR m/n = max duty cycle for queue */
	int vbr_n;		/* 1 <= m <= 7 and 1 <= n <= 127 */
} TX_QUEUE;

/*  To avoid expensive SRAM reads, scq_cur tracks the number of SCQ entries
 *  in use.  Only idt_transmit_top may increase this, and only idt_intr_tsq
 *  may decrease it.
 */

/*  mbuf chains on the queue use the fields:
 *  m_next     is the usual pointer to next mbuf
 *  m_nextpkt  is the next packet on the queue
 *  m_pkthdr.rcvif is a pointer to the connection
 *  m_pkthdr.header is a pointer to the TX queue
 */

typedef struct {
	struct vccb *vccinf;
	char status;		/* zero if closed */
	char vpi;
	u_short vci;
	TX_QUEUE *queue;	/* transmit queue for this connection */
	struct mbuf *recv;	/* current receive mbuf, or NULL */
	int rlen;		/* current receive length */
	int maxpdu;		/* largest PDU we will ever see */
	int traf_pcr;		/* peak cell rate */
	int traf_scr;		/* sustained cell rate */
	u_char aal;		/* AAL for this connection */
	u_char class;		/* T_ATM_CBR, T_ATM_VBR, or T_ATM_UBR */
	u_char flg_mpeg2ts:1;	/* send data as 2 TS == 8 AAL5 cells */
	u_char flg_clp:1;	/* CLP flag for outbound cells */
} CONNECTION;

#define MAX_CONNECTION 4096	/* max number of connections */

#define GET_RDTSC(var) {__asm__ volatile("rdtsc":"=A"(var)); }

/*******************************************************************************
 *
 *  Device softc structure
 */

struct idt_softc {
	/* HARP data */
	/* XXX: must be first member of struct. */
	Cmn_unit		iu_cmn;	/* Common unit stuff */

#if 0
	struct arpcom		idt_ac;	/* ifnet for device */
#endif

	/* Device data */
	device_t		dev;
	int			debug;

	struct resource *	mem;
	int			mem_rid;
	int			mem_type;
	bus_space_tag_t		bustag;
	bus_space_handle_t	bushandle;

	struct resource *	irq;
	int			irq_rid;
	void *			irq_ih;

	struct callout_handle	ch;

	struct mtx		mtx;

	vm_offset_t virt_baseaddr;	/* nicstar register virtual address */
	vm_offset_t cmd_reg;	/* command register offset 0x14 */
	vm_offset_t stat_reg;	/* status register offset 0x60 */
	vm_offset_t fixbuf;	/* buffer that holds TSQ, RSQ, variable SCQ */

	u_long timer_wrap;	/* keep track of wrapped timers */
	u_long rsqh;		/* Recieve Status Queue, reg is write-only */

	CONNECTION *connection;	/* connection table */
	int conn_maxvpi;	/* number of VPI values */
	int conn_maxvci;	/* number of VCI values */
	int cellrate_rmax;	/* max RX cells per second */
	int cellrate_tmax;	/* max TX cells per second */
	int cellrate_rcur;	/* current committed RX cellrate */
	int cellrate_tcur;	/* current committed TX cellrate */
	int txslots_max;	/* number of CBR TX slots for interface */
	int txslots_cur;	/* current CBR TX slots in use */
	TX_QUEUE cbr_txqb[IDT_MAX_CBRQUEUE];
	TX_QUEUE *cbr_slot[IDT_MAX_CBRSLOTS];
	TX_QUEUE *cbr_free[IDT_MAX_CBRQUEUE];
	TX_QUEUE queue_vbr;
	TX_QUEUE queue_abr;
	TX_QUEUE queue_ubr;
	vm_offset_t cbr_base;	/* base of memory for CBR TX queues */
	int cbr_size;		/* size of memory for CBR TX queues */
	int cbr_freect;
	u_long raw_headp;	/* head of raw cell queue, physical */
	struct mbuf *raw_headm;	/* head of raw cell queue, virtual */
	u_long *tsq_base;	/* virtual TSQ base address */
	u_long *tsq_head;	/* virtual TSQ head pointer */
	int tsq_size;		/* number of TSQ entries (1024) */
	volatile u_long *reg_cfg;
	volatile u_long *reg_cmd;
	volatile u_long *reg_data;
	volatile u_long *reg_tsqh;
	volatile u_long *reg_gp;
	volatile u_long *reg_stat;
	struct mbuf **mcheck;

	int sram;		/* amount of SRAM */
	int pci_rev;		/* hardware revision ID */
	char *hardware;		/* hardware description string */
	u_char flg_le25:1;	/* flag indicates LE25 instead of LE155 */
	u_char flg_igcrc:1;	/* ignore receive CRC errors */
};

typedef struct idt_softc nicstar_reg_t;
typedef struct idt_softc IDT;

#define	iu_pif		iu_cmn.cu_pif
#define	stats_ipdus	iu_pif.pif_ipdus
#define	stats_opdus	iu_pif.pif_opdus
#define	stats_ibytes	iu_pif.pif_ibytes
#define	stats_obytes	iu_pif.pif_obytes
#define	stats_ierrors	iu_pif.pif_ierrors
#define	stats_oerrors	iu_pif.pif_oerrors
#define	stats_cmderrors	iu_pif.pif_cmderrors

/*
 * Device VCC Entry
 * 
 * Contains the common and IDT-specific information for each VCC
 * which is opened through an IDT device.
 */
struct nidt_vcc {
	struct cmn_vcc iv_cmn;  /* Common VCC stuff */
};

typedef struct nidt_vcc Idt_vcc;

extern int idt_sysctl_logvcs;
extern int idt_sysctl_vbriscbr;

void nicstar_intr(void *);
void phys_init(nicstar_reg_t * const);
void nicstar_init(nicstar_reg_t * const);
int idt_harp_init(nicstar_reg_t * const);
void idt_device_stop(IDT *);
void idt_release_mem(IDT *);

CONNECTION *idt_connect_find(IDT *, int, int);
caddr_t idt_mbuf_base(struct mbuf *);
int idt_slots_cbr(IDT *, int);

int idt_connect_opencls(IDT *, CONNECTION *, int);
int idt_connect_txopen(IDT *, CONNECTION *);
int idt_connect_txclose(IDT *, CONNECTION *);

int nicstar_eeprom_rd(nicstar_reg_t * const, u_long);

void idt_receive(IDT *, struct mbuf *, int, int);
void idt_transmit(IDT *, struct mbuf *, int, int, int);
