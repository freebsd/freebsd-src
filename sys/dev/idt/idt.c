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
 *  The following sysctl variables are used:
 *
 * hw.idt.log_bufstat  (0)   Log free buffers (every few minutes)
 * hw.idt.log_vcs      (0)   Log VC opens, closes, and other events
 * hw.idt.bufs_large  (100)  Max/target number of free 2k buffers
 * hw.idt.bufs_small  (200)  Max/target number of free mbufs
 * hw.idt.cur_large   (R/O)  Current number of free 2k buffers
 * hw.idt.cur_small   (R/O)  Current number of free mbufs
 * hw.idt.qptr_hold    (1)   Optimize TX queue buffer for lowest overhead
 *
 * Note that the read-only buffer counts will not work with multiple cards.
 *
 ******************************************************************************
 *
 *  Assumptions:
 *
 *  1.  All mbuf clusters are 2048 bytes, and aligned.
 *  2.  All mbufs are 256 bytes, and aligned (see idt_intr_tsq).
 *
 *  Bugs:
 *
 *  1.  Function idt_detach() is unusuable because idt_release_mem() is
 *      incomplete.  The mbufs held in the free buffer queues can be
 *      recovered from the "mcheck" hash table.
 *  2.  The memory allocation could be cleaned up quite a bit.
 *
 ******************************************************************************
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/sockio.h>

#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/mman.h>
#include <machine/clock.h>
#include <machine/cpu.h>	/* bootverbose */

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>

/* Gross kludge to make lint compile again.  This sucks, but oh well */
#ifdef COMPILING_LINT
#undef MCLBYTES
#undef MCLSHIFT
#define MCLBYTES 2048
#define MCLSHIFT 11
#endif

#if MCLBYTES != 2048
#error "This nicstar driver depends on 2048 byte mbuf clusters."
#endif

#include <netatm/port.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_cm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_stack.h>
#include <netatm/atm_pcb.h>
#include <netatm/atm_var.h>
#include <netatm/atm_vc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <dev/idt/idtreg.h>
#include <dev/idt/idtvar.h>

#define MAXCARDS 10		/* set to impossibly high */

/******************************************************************************
 *
 *  You may change IDT_LBUFS and IDT_SBUFS if you wish.
 */

#define	NICSTAR_LRG_SIZE	2048	/* must be power of two */
#define	IDT_LBUFS		100	/* default number of 2k buffers */
#define	IDT_SBUFS		200	/* default number of 96-byte buffers */

#define	IDT_TST_START		0x1c000	/* transmit schedule table start */
#define	IDT_SCD_START		0x1d000	/* segmentation channel descriptors start */
#define	IDT_SCD_SIZE		509	/* max number of SCD entries */

#define NICSTAR_FIXPAGES        10

static int idt_sysctl_logbufs = 0;	/* periodic buffer status messages */
       int idt_sysctl_logvcs = 0;	/* log VC open & close events      */
static int idt_sysctl_buflarge = IDT_LBUFS;	/* desired large buffer queue */
static int idt_sysctl_bufsmall = IDT_SBUFS;	/* desired small buffer queue */
static int idt_sysctl_curlarge = 0;	/* current large buffer queue */
static int idt_sysctl_cursmall = 0;	/* current small buffer queue */
static int idt_sysctl_qptrhold = 1;	/* hold TX queue pointer back */
       int idt_sysctl_vbriscbr = 0;	/* use CBR slots for VBR VC's */

SYSCTL_NODE(_hw, OID_AUTO, idt, CTLFLAG_RW, 0, "IDT Nicstar");

SYSCTL_INT(_hw_idt, OID_AUTO, log_bufstat, CTLFLAG_RW,
    &idt_sysctl_logbufs, 0, "Log buffer status");
SYSCTL_INT(_hw_idt, OID_AUTO, log_vcs, CTLFLAG_RW,
    &idt_sysctl_logvcs, 0, "Log VC open/close");

SYSCTL_INT(_hw_idt, OID_AUTO, bufs_large, CTLFLAG_RW,
    &idt_sysctl_buflarge, IDT_LBUFS, "Large buffer queue");
SYSCTL_INT(_hw_idt, OID_AUTO, bufs_small, CTLFLAG_RW,
    &idt_sysctl_bufsmall, IDT_SBUFS, "Small buffer queue");
SYSCTL_INT(_hw_idt, OID_AUTO, cur_large, CTLFLAG_RD,
    &idt_sysctl_curlarge, 0, "Current large queue");
SYSCTL_INT(_hw_idt, OID_AUTO, cur_small, CTLFLAG_RD,
    &idt_sysctl_cursmall, 0, "Current small queue");
SYSCTL_INT(_hw_idt, OID_AUTO, qptr_hold, CTLFLAG_RW,
    &idt_sysctl_qptrhold, 1, "Optimize TX queue ptr");
SYSCTL_INT(_hw_idt, OID_AUTO, vbr_is_cbr, CTLFLAG_RW,
    &idt_sysctl_vbriscbr, 0, "Use CBR for VBR VC's");

/******************************************************************************
 *
 * common VCI values
 *
 * 0/0  Idle cells
 * 0/1  Meta signalling
 * x/1  Meta signalling
 * 0/2  Broadcast signalling
 * x/2  Broadcast signalling
 * x/3  Segment OAM F4 flow
 * x/4  End-end OAM F4 flow
 * 0/5  p-p signalling
 * x/5  p-p signalling
 * x/6  rate management
 * 0/14 SPANS
 * 0/15 SPANS
 * 0/16 ILMI
 * 0/18 PNNI
 */

/*******************************************************************************
 *
 *  fixbuf memory map:
 *
 *  0000 - 1fff:  TSQ  Transmit status queue 1024 entries *  8 bytes each
 *  2000 - 3fff:  RSQ  Receive status queue,  512 entries * 16 bytes each
 *  4000 - 5fff:  VBR  segmentation channel queue (highest priority)
 *  6000 - 7fff:  ABR  segmentation channel queue (middle priority)
 *  8000 - 9fff:  UBR  segmentation channel queue (lowest priority)
 *
 *  IDT device memory map:
 *
 *  1fc00:  RX large buffer queue (4k)
 *  1f800:  RX small buffer queue (4k)
 *  1e800:  RX cells FIFO (16k)
 *  1e7f4:  SCD0 - VBR (12)
 *  1e7e8:  SCD1 - ABR (12)
 *  1e7dc:  SCD2 - UBR (12)
 *  1e7db:  CBR SCD end (last word)
 *  1d000:  CBR SCD start (509 entries)
 *  1cfff:  TST end (4095 available slots)
 *  1c000:  TST start (first CBR slot)
 *
 */

static u_long idt_found = 0;

 /* -------- buffer management -------- */
static int nicstar_sram_wr(nicstar_reg_t * const, u_long,
			   int, u_long, u_long, u_long, u_long);
static int nicstar_sram_rd(nicstar_reg_t * const, u_long, u_long *);
static int nicstar_add_buf(nicstar_reg_t * const, struct mbuf *,
			   struct mbuf *, u_long);
static int nicstar_util_rd(nicstar_reg_t * const, u_long, u_long *);
static int nicstar_util_wr(nicstar_reg_t * const, int, u_long, u_long);
      void nicstar_ld_rcv_buf(nicstar_reg_t * const);

 /* -------- interface routines -------- */
int nicstar_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		   struct rtentry *);
void nicstar_start(struct ifnet *);

 /* -------- VCC open/close routines -------- */
static void nicstar_itrx(nicstar_reg_t *);

 /* -------- receiving routines -------- */
static void nicstar_rawc(nicstar_reg_t *);
static void nicstar_recv(nicstar_reg_t *);
static void nicstar_phys(nicstar_reg_t *);

/*******************************************************************************
 *
 *  New functions
 */

static int idt_buffer_init(IDT *);
static struct mbuf *idt_mbufcl_get(void);

static int idt_connect_init(IDT *, int);
static void idt_connect_newvbr(IDT *);

static void idt_intr_tsq(IDT *);

static vm_offset_t idt_malloc_contig(int);

static int idt_mbuf_align(struct mbuf *, struct mbuf *);
static int idt_mbuf_append4(struct mbuf *, char *);
static struct mbuf *idt_mbuf_copy(IDT *, struct mbuf *);
static int idt_mbuf_prepend(struct mbuf *, char *, int);
static int idt_mbuf_used(struct mbuf *);

static int idt_mcheck_add(IDT *, struct mbuf *);
static int idt_mcheck_rem(IDT *, struct mbuf *);
static int idt_mcheck_init(IDT *);

static int idt_queue_flush(CONNECTION *);
static struct mbuf *idt_queue_get(TX_QUEUE *);
static int idt_queue_init(IDT *);
static int idt_queue_put(CONNECTION *, struct mbuf *);

static int idt_receive_aal5(IDT *, struct mbuf *, struct mbuf *);
static void idt_transmit_drop(IDT *, struct mbuf *);
static void idt_transmit_top(IDT *, TX_QUEUE *);

static int idt_slots_add(IDT *, TX_QUEUE *, int);
static int idt_slots_init(IDT *);
static int idt_slots_rem(IDT *, TX_QUEUE *);

static int idt_phys_detect(IDT *);
static void idt_status_bufs(IDT *);
static int idt_status_wait(IDT *);

/******************************************************************************
 *
 *  VBR queue divisor table
 */

static unsigned char vbr_div_m[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2,
	1, 2, 2, 2, 3, 1, 2, 1, 3, 2, 3, 3, 4, 3, 3, 2, 4, 1, 3, 3,
	1, 5, 5, 4, 4, 5, 4, 4, 6, 5, 1, 5, 4, 6, 2, 6, 7, 7, 4, 1,
	3, 5, 7, 7, 5, 5, 7, 7, 7, 2, 7, 7, 7, 7, 2, 3, 6, 1, 6, 3,
	2, 3, 5, 1, 7, 4, 5, 2, 3, 4, 7, 1, 7, 4, 3, 2, 7, 7, 5, 7,
	1, 7, 5, 7, 5, 2, 7, 3, 4, 6, 7, 1, 1, 7, 4, 7, 5, 7, 2, 5,
	3, 4, 5, 7, 1, 1, 1, 7, 5, 4, 7, 3, 7, 2, 7, 5, 3, 7, 4, 5,
	7, 7, 1, 1, 1, 7, 7, 5, 4, 7, 3, 5, 7, 7, 2, 7, 5, 5, 3, 7,
	4, 5, 6, 7, 7, 1, 1, 1, 1, 7, 7, 7, 5, 5, 4, 7, 3, 3, 5, 5,
	7, 2, 2, 2, 7, 5, 5, 3, 3, 7, 4, 4, 5, 6, 7, 7, 7, 7, 1, 1,
	1, 1, 1, 7, 7, 7, 7, 6, 5, 5, 4, 4, 7, 7, 3, 3, 5, 5, 5, 7,
	7, 2, 2, 2, 2, 7, 7, 5, 5, 5, 3, 3, 3, 7, 7, 4, 4, 5, 5, 5,
	6, 7, 7, 7, 7, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 7, 7, 7, 7,
	7, 6, 6, 5, 5, 4, 4, 4, 7, 7, 7, 3, 3, 3, 3, 3, 5, 5, 5, 7,
	7, 7, 7, 2, 2, 2, 2, 2, 2, 7, 7, 7, 7, 5, 5, 5, 5, 5, 3, 3,
	3, 3, 3, 7, 7, 7, 7, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5,
	5, 4, 4, 4, 4, 4, 4, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 3, 3, 3,
	5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5,
	5, 5, 5, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 7, 7, 7, 7, 7, 7,
	7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
	6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static unsigned char vbr_div_n[] = {
	127, 127, 127, 127, 127, 127, 127, 127, 125, 111, 100, 91, 83, 77, 71,
	67, 125, 59, 111, 105, 50, 95, 91, 87, 125, 40, 77, 37, 107, 69,
	100, 97, 125, 91, 88, 57, 111, 27, 79, 77, 25, 122, 119, 93, 91,
	111, 87, 85, 125, 102, 20, 98, 77, 113, 37, 109, 125, 123, 69, 17,
	50, 82, 113, 111, 78, 77, 106, 104, 103, 29, 100, 99, 97, 96, 27,
	40, 79, 13, 77, 38, 25, 37, 61, 12, 83, 47, 58, 23, 34, 45,
	78, 11, 76, 43, 32, 21, 73, 72, 51, 71, 10, 69, 49, 68, 48,
	19, 66, 28, 37, 55, 64, 9, 9, 62, 35, 61, 43, 60, 17, 42,
	25, 33, 41, 57, 8, 8, 8, 55, 39, 31, 54, 23, 53, 15, 52,
	37, 22, 51, 29, 36, 50, 50, 7, 7, 7, 48, 48, 34, 27, 47,
	20, 33, 46, 46, 13, 45, 32, 32, 19, 44, 25, 31, 37, 43, 43,
	6, 6, 6, 6, 41, 41, 41, 29, 29, 23, 40, 17, 17, 28, 28,
	39, 11, 11, 11, 38, 27, 27, 16, 16, 37, 21, 21, 26, 31, 36,
	36, 36, 36, 5, 5, 5, 5, 5, 34, 34, 34, 34, 29, 24, 24,
	19, 19, 33, 33, 14, 14, 23, 23, 23, 32, 32, 9, 9, 9, 9,
	31, 31, 22, 22, 22, 13, 13, 13, 30, 30, 17, 17, 21, 21, 21,
	25, 29, 29, 29, 29, 29, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	27, 27, 27, 27, 27, 27, 23, 23, 19, 19, 15, 15, 15, 26, 26,
	26, 11, 11, 11, 11, 11, 18, 18, 18, 25, 25, 25, 25, 7, 7,
	7, 7, 7, 7, 24, 24, 24, 24, 17, 17, 17, 17, 17, 10, 10,
	10, 10, 10, 23, 23, 23, 23, 13, 13, 13, 13, 16, 16, 16, 16,
	19, 19, 22, 22, 22, 22, 22, 22, 22, 22, 22, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 20, 20, 20,
	20, 20, 20, 20, 20, 20, 20, 17, 17, 17, 17, 14, 14, 14, 14,
	14, 11, 11, 11, 11, 11, 11, 19, 19, 19, 19, 19, 8, 8, 8,
	8, 8, 8, 8, 8, 13, 13, 13, 13, 13, 13, 13, 18, 18, 18,
	18, 18, 18, 18, 18, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 17, 17, 17, 17, 17, 17, 17, 17, 17, 12, 12, 12, 12, 12,
	12, 12, 12, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 16,
	16, 16, 16, 16, 16, 16, 9, 9, 9, 9, 9, 9, 9, 9, 9,
	11, 11, 11, 11, 11, 11, 11, 11, 11, 13, 13, 13, 13, 13, 13,
	15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 13, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
	13, 13, 11, 11, 11, 11, 11, 11, 11, 11, 11, 9, 9, 9, 9,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 12, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
	11, 11, 11, 11, 11, 11, 11, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 9,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
	9, 9, 9, 9, 9, 9, 9, 9, 9, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/******************************************************************************
 *
 *  Stop the device (shutdown)
 *
 *  in:  IDT device
 *
 * Date first: 11/14/2000  last: 11/14/2000
 */

void
idt_device_stop(IDT * idt)
{
	u_long val;
	int s;

	s = splimp();

	*(idt->reg_cfg) = 0x80000000;	/* put chip into reset */
	val = *(idt->reg_gp);	/* wait... */
	val |= *(idt->reg_gp);	/* wait... */
	val |= *(idt->reg_gp);	/* wait... */
	*(idt->reg_cfg) = 0;	/* out of reset */

	splx(s);

	return;
}

/******************************************************************************
 *
 *  Initialize the hardware
 */

void
phys_init(nicstar_reg_t * const idt)
{
	int i;
	u_long t;

#ifdef NICSTAR_TESTSRAM
	u_long z, s2, bad;
#endif
	u_long x, s1;
	volatile u_long *regCFG = (volatile u_long *)(idt->virt_baseaddr + REGCFG);
	volatile u_long *regGP = (volatile u_long *)(idt->virt_baseaddr + REGGP);
	volatile u_long stat_val;

	/* clean status bits */
	stat_val = *(volatile u_long *)idt->stat_reg;
	*(volatile u_long *)idt->stat_reg = stat_val | 0xcc30;	/* clear ints */

	idt->flg_le25 = 0;	/* is this FORE LE25 with 77105 PHY? */
	idt->flg_igcrc = 0;	/* ignore receive CRC errors? */
	idt->hardware = "?";

	/* start signalling SAR reset */
	*regCFG = 0x80000000;

	/* SAR reset--clear occurs at lease 2 PCI cycles after setting */
	t = *regGP;		/* wait */
	t = *regCFG;
	*regCFG = 0;		/* clear reset */

	*regGP = 0x00000000;	/* clear PHYS reset */
	*regGP = 0x00000008;	/* start PHYS reset */
	t = *regGP;		/* wait */
	t = *regCFG;
	*regGP = 0x00000001;	/* set while changing SUNI settings */
	t = *regGP;		/* wait */
	t = *regCFG;

	idt->flg_le25 = idt_phys_detect(idt);

	if (idt->flg_le25) {
		idt->cellrate_rmax = 59259;
		idt->cellrate_tmax = 59259;
		idt->cellrate_rcur = 0;
		idt->cellrate_tcur = 0;
		idt->txslots_max = 348;	/* use n*348 for higher resolution */
		idt->txslots_cur = 0;
		nicstar_util_wr(idt, 0, 0x00, 0x00);	/* synch (needed for
							 * 77105?) */
		nicstar_util_wr(idt, 1, 0x00, 0x09);	/* enable interrupts */
		nicstar_util_wr(idt, 1, 0x02, 0x10);	/* 77105 RFLUSH */
		nicstar_util_rd(idt, 0x01, &t);	/* read/clear interrupt flag */
	} else {
		idt->cellrate_rmax = 353207;	/* 2075 slots of 1 DS0 each... */
		idt->cellrate_tmax = 353207;
		idt->cellrate_rcur = 0;
		idt->cellrate_tcur = 0;
		idt->txslots_max = 2075;
		idt->txslots_cur = 0;

		/* initialize the 155Mb SUNI */
		nicstar_util_wr(idt, 0, 0x00, 0x00);	/* sync utopia with SAR */
		nicstar_util_wr(idt, 1, 0x00, 0x00);	/* clear SW reset */
		*regGP = 0x00000000;	/* clear when done with SUNI changes */
	}

#ifdef NICSTAR_TESTSRAM
	/*
	 * this will work with 32K and 128K word RAM  because the pattern
	 * repeats every 4 words
	 */
	for (i = 0; i < 0x20000; i += 4)
		(void)nicstar_sram_wr(idt, i, 4, 0xa5a5a5a5, 0x5a5a5a5a,
		    0xa5a5a5a5, 0x5a5a5a5a);
	for (i = 0; i < 0x20000; i += 2) {
		s1 = nicstar_sram_rd(idt, i, &x);
		s2 = nicstar_sram_rd(idt, i + 1, &z);
		if (s1 || s2 || x != 0xa5a5a5a5 || z != 0x5a5a5a5a) {
			printf("sram fail1 %d 0x%08x 0x%08x\n", i, x, z);
			break;
		}
	}
	for (i = 0; i < 0x20000; i += 4)
		(void)nicstar_sram_wr(idt, i, 4, 0x5a5a5a5a, 0xa5a5a5a5,
		    0x5a5a5a5a, 0xa5a5a5a5);
	for (i = 0; i < 0x20000; i += 2) {
		s1 = nicstar_sram_rd(idt, i, &z);
		s2 = nicstar_sram_rd(idt, i + 1, &x);
		if (s1 || s2 || x != 0xa5a5a5a5 || z != 0x5a5a5a5a) {
			printf("sram fail2 %d 0x%08x 0x%08x\n", i, x, z);
			break;
		}
	}
#endif

	/* flush SRAM */
	for (i = 0; i < 0x20000; i += 4)
		(void)nicstar_sram_wr(idt, i, 4, 0, 0, 0, 0);

	/*
 	 * the memory map for the 32K word card has the
	 * addresses 0x8000, 0x10000, 0x18000 mapped back
	 * to address 0, and 0x8001, ..., 0x18001 is mapped
 	 * to address 1. address 0x4000 is mapped to 0x1c000
 	 */

	/* write in the 0 word, see if we read it at 0x10000 */
	(void)nicstar_sram_wr(idt, 0x0, 1, 0xa5a5a5a5, 0, 0, 0);
	s1 = nicstar_sram_rd(idt, 0x10000, &x);
	(void)nicstar_sram_wr(idt, 0x0, 1, 0, 0, 0, 0);
	if (!s1 && x == 0xa5a5a5a5) {
		device_printf(idt->dev, "32K words of RAM\n");
		idt->sram = 0x4000;
	} else {
		device_printf(idt->dev, "128K words of RAM\n");
		idt->sram = 0x10000;
	}
#ifdef NICSTAR_FORCE32K
	idt->sram = 0x4000;
	device_printf(idt->dev, "forced to 32K words of RAM\n");
#endif

	return;
}

/*  Cellrate notes:
 *       The cellrate for OC3 is 353207.55, rounded down above.  This makes
 *       2075 slots of one DS0 (64003) each.
 *
 *       The ATM25 rate is calculated from 25.6mb divided by 424 bits for
 *       cell plus 8 bits for "opcode" == 432 bits.  59259 * 432 = 25599888.
 *       This provides a 47-byte AAL1 bitrate of 22,281,384 bits/sec, or
 *       348 slots of one DS0 (64027) each.  If 8khz synch events are to
 *       be sent, then only 347 slots are available.
 *
 ******************************************************************************
 *
 *  Physical layer detect
 *
 *  in:  IDT device
 * out:  zero = LE155, NZ = LE25
 *
 * Date first: 10/30/2000  last: 06/08/2001
 */

int
idt_phys_detect(IDT * idt)
{
	u_long t;
	int retval;

	retval = 0;

	nicstar_util_wr(idt, 0, 0x00, 0x00);	/* synch (needed for 77105?) */
	nicstar_util_rd(idt, 0x00, &t);	/* get Master Control Register */

	switch (t) {
	/* 25.6 Mbps ATM PHY with TC & PMD */
	/* http://www.idt.com/products/pages/ATM_Products-77105.html */
	case 0x09:
		device_printf(idt->dev, "ATM card is Fore LE25, PHY=77105\n");
		idt->hardware = "ATM25/77105";
		retval = 1;
		break;

	/* S/UNI-155-LITE */
	/* http://www.pmc-sierra.com/products/details/pm5346/index.html */
	case 0x30:
		device_printf(idt->dev, "ATM card is Fore LE155 or IDT, PHY=PM5346\n");
		idt->hardware = "ATM155/PM5346";
		break;

	/* S/UNI-155-ULTRA */
	/* http://www.pmc-sierra.com/products/details/pm5350/index.html */
	case 0x31:
	case 0x70:
	case 0x78:
		device_printf(idt->dev, "ATM card is Fore LE155, PHY=PM5350\n");
		idt->hardware = "ATM155/PM5350";
		break;

	default:
		device_printf(idt->dev,
			"cannot figure out card type, assuming LE155 (reg=%d).\n",
	    		(int)t);
		idt->hardware = "unknown (LE155?)";
		break;
	}
	return (retval);
}

/*  Register 0 values:
 *       77105  = 0x09
 *       PM5346 = 0x30
 *       PM5250 = 0x31  (actually observed)
 *       PM5350 = 0x70 or 0x78 (according to docs)
 *
 ******************************************************************************
 *
 *  Initialize the data structures
 */

void
nicstar_init(nicstar_reg_t * const idt)
{
	int i;
	vm_offset_t buf;
	u_long *p;

	idt_connect_init(idt, 0);	/* initialize for 0 VPI bits (12 VCI
					 * bits) */

	/* allocate space for TSQ, RSQ, SCD for VBR,ABR, UBR */
	idt->fixbuf = (vm_offset_t)contigmalloc(NICSTAR_FIXPAGES * PAGE_SIZE,
	    M_DEVBUF, M_NOWAIT | M_ZERO, 0x100000, 0xffffffff, 0x2000, 0);
	if (idt->fixbuf == 0)
		return;		/* no space card disabled */

	if (idt_buffer_init(idt))	/* allocate large buffers */
		goto freemem;	/* free memory and return */

	if (idt_mcheck_init(idt))
		goto freemem;

	idt_found++;		/* number of cards found on machine */

	if (bootverbose) {
		printf("nicstar: buffer size %d\n", 0);
	}
	idt_queue_init(idt);	/* initialize all TX_QUEUE structures */
	idt_slots_init(idt);	/* initialize CBR table slots */

	/* initialize variable rate mbuf queues */

	/* TSQ initialization */
	for (p = (u_long *)idt->fixbuf; p < (u_long *)(idt->fixbuf + 0x2000);) {
		*p++ = 0x00000000;
		*p++ = 0x80000000;	/* set empty bit */
	}

	buf = vtophys(idt->fixbuf);
	/* Transmit Status Queue Base */
	*(volatile u_long *)(idt->virt_baseaddr + REGTSQB) = buf;
	/* Transmit Status Queue Head */
	*(volatile u_long *)(idt->virt_baseaddr + REGTSQH) = 0;	/* 8k aligned */
	idt->tsq_base = (u_long *)idt->fixbuf;
	idt->tsq_head = (u_long *)idt->fixbuf;
	idt->tsq_size = 1024;

	/* Recieve Status Queue Base */
	*(volatile u_long *)(idt->virt_baseaddr + REGRSQB) = buf + 0x2000;
	/* Transmit Status Queue Head */
	*(volatile u_long *)(idt->virt_baseaddr + REGRSQH) = 0;	/* 8k aligned */
	idt->rsqh = 0;


	/* Now load receive buffers into SRAM */
	nicstar_ld_rcv_buf(idt);

	/* load variable SCQ */
	(void)nicstar_sram_wr(idt, 0x1e7dc, 4, (u_long)(buf + 0x8000), 0,
	    0xffffffff, 0);	/* SD2 */
	(void)nicstar_sram_wr(idt, 0x1e7e0, 4, 0, 0, 0, 0);
	(void)nicstar_sram_wr(idt, 0x1e7e4, 4, 0, 0, 0, 0);

	(void)nicstar_sram_wr(idt, 0x1e7e8, 4, (u_long)(buf + 0x6000), 0,
	    0xffffffff, 0);	/* SD1 */
	(void)nicstar_sram_wr(idt, 0x1e7ec, 4, 0, 0, 0, 0);
	(void)nicstar_sram_wr(idt, 0x1e7f0, 4, 0, 0, 0, 0);

	(void)nicstar_sram_wr(idt, 0x1e7f4, 4, (u_long)(buf + 0x4000), 0,
	    0xffffffff, 0);	/* SD0 */
	(void)nicstar_sram_wr(idt, 0x1e7f8, 4, 0, 0, 0, 0);
	(void)nicstar_sram_wr(idt, 0x1e7fc, 4, 0, 0, 0, 0);

	/* initialize RCT */
	for (i = 0; i < idt->sram; i += 4) {	/* XXX ifdef table size */
		nicstar_sram_wr(idt, i, 4, 0x0, 0x0, 0x0, 0xffffffff);
	}

	/* VPI/VCI mask is 0 */
	*(volatile u_long *)(idt->virt_baseaddr + REGVMSK) = 0;

	/* Set the Transmit Schedule Table base address */
	*(volatile u_long *)(idt->virt_baseaddr + REGTSTB) = IDT_TST_START;


/* Configuration Register settings:
 * Bit(s)	Meaning					value
 * 31		Software reset				0
 * 30		RESERVED				0
 * 29		Recieve Enabled				1
 * 28-27	Small Buffer Size (host memory)		01 (96 bytes)
 * 26-25	Large Buffer Size (host memory)		00 (2048 bytes)
 * 24		Interrupt on empty free buffer queue	1
 *
 * 23-22	Recieve Status Queue Size (host memory)	10 (8192 bytes)
 * 21		Accpect Invalid cells into Raw Queue	1
 * 20		Ignore General Flow control		1
 *
 * 19-18	VPI/VCI Select				00
 * 17-16	Recieve Connect Table Size		00 (32K SRAM)
 *							10 (128K SRAM)
 *
 * 15		Accpect non-open VPI/VCI to Raw Queue	1
 * 14-12	time to delay after Rx and interrupt	001 (0us)
 *
 * 11		Interrupt when a Raw Cell is added	1
 * 10		Interrupt when Recieve Queue near full	1
 *  9		Recieve RM (PTI = 110 or 111)		1
 *  8		RESERVED				0
 *
 *  7		Interrupt on Timer rollover		1
 *  6		RESERVED				0
 *  5		Transmit Enabled			1
 *  4		Interrupt on Transmit Status Indicator	1
 *
 *  3		Interrupt on transmit underruns		1
 *  2		UTOPIA cell/byte mode			0 (cell)
 *  1		Interrupt on nearly full TSQ		1
 *  0		Enable Physical Interrupt		1
 */

	/* original values:  0x31b09ebb and 0x31b29eb  */
	/*
	 * 11/01/2000: changed from 0x31b09eb to 0x29b09eb for 96-byte
	 * sm-buf
	 */

	if (idt->sram == 0x4000)/* 32K */
		*(volatile u_long *)(idt->virt_baseaddr + REGCFG) = 0x29b09ebb;
	else			/* 128K */
		*(volatile u_long *)(idt->virt_baseaddr + REGCFG) = 0x29b29ebb;

	return;

freemem:
	/* free memory and return */
	idt_release_mem(idt);
	device_printf(idt->dev, "cannot allocate memory\n");
	return;			/* no space card disabled */
}

/******************************************************************************
 *
 *  Release all allocated memory
 *
 *  in:  IDT device
 *
 * Date first: 11/14/2000  last: 11/14/2000
 */

void
idt_release_mem(IDT * idt)
{
	if (idt->fixbuf != 0)
		kmem_free(kernel_map, idt->fixbuf,
			  (NICSTAR_FIXPAGES * PAGE_SIZE));

	if (idt->cbr_base != 0)
		kmem_free(kernel_map, (vm_offset_t)idt->cbr_base, idt->cbr_size);

	printf("%s() is NOT SAFE!\n", __func__);

	/* we also have idt->connection and idt->mcheck to do as well... */
}

/******************************************************************************
 *
 *  Write one to four words to SRAM
 *
 *    writes one to four words into sram starting at "sram_location"
 *
 *    returns -1 if sram location is out of range.
 *    returns count, if count is not in the range from 1-4.
 *    returns 0 if parameters were acceptable
 */

static int
nicstar_sram_wr(nicstar_reg_t * const idt, u_long address, int count,
    u_long data0, u_long data1, u_long data2, u_long data3)
{
	if (address >= 0x20000)	/* bad address */
		return (-1);

	if (idt_status_wait(idt))	/* 12/06/2000 */
		return (-1);

	switch (--count) {
	case 3:
		*(idt->reg_data + 3) = data3;	/* drop down to do others */
	case 2:
		*(idt->reg_data + 2) = data2;	/* drop down to do others */
	case 1:
		*(idt->reg_data + 1) = data1;	/* drop down to do others */
	case 0:
		*idt->reg_data = data0;	/* load last data item */
		break;		/* done loading values */
	default:
		return (count);	/* nothing to do */
	}
						/* write the word(s) */
	*idt->reg_cmd = 0x40000000 | (address << 2) | count;

	return (0);
}

/*  05/31/2001:  Removed wait between data register(s) and write command.
 *       The docs do not state it is helpful, and the example only has one
 *       wait, before the data register load.  The wait time is very high -
 *       aproximately 6 microseconds per wait.
 *
 ******************************************************************************
 *
 *  Read one word from SRAM
 *
 *    reads one word of sram at "sram_location" and places the value
 *    in "answer_pointer"
 *
 *    returns -1 if sram location is out of range.
 *    returns 0 if parameters were acceptable
 */
static int
nicstar_sram_rd(nicstar_reg_t * const idt, u_long address, u_long *data0)
{
	if (address >= 0x20000)	/* bad address */
		return (-1);

	if (idt_status_wait(idt))
		return (-1);

	*idt->reg_cmd = 0x50000000 | (address << 2);	/* read a word */

	if (idt_status_wait(idt))
		return (-1);

	*data0 = *idt->reg_data;/* save word */

	return (0);
}

/*******************************************************************************
 *
 *  Open or Close connection in IDT Receive Connection Table
 *
 *  in:  IDT device, VPI, VCI, opflag (0 = close, 1 = open)
 * out:  zero = success
 *
 *  Date first: 12/14/2000  last: 12/14/2000
 */

int
idt_connect_opencls(IDT * idt, CONNECTION * connection, int opflag)
{
	int address;
	int word1;

	if (connection->vpi >= idt->conn_maxvpi ||
	    connection->vci >= idt->conn_maxvci)
		return (1);

	address = connection->vpi * idt->conn_maxvci + connection->vci;
	address <<= 2;		/* each entry is 4 words */

	if (opflag) {
		switch (connection->aal) {
		case ATM_AAL0:
			word1 = 0x00038000;
			break;	/* raw cell queue */
		case ATM_AAL1:
			word1 = 0x00008000;
			break;	/* Nicstar "AAL0" */
		case ATM_AAL3_4:
			word1 = 0x00018000;
			break;
		case ATM_AAL5:
			word1 = 0x00028000;
			break;
		default:
			return (1);
		}
		nicstar_sram_wr(idt, address, 4, word1, 0, 0, 0xffffffff);
		opflag = 0x00080000;	/* bit-19 set or clear */
	}
	if (idt_status_wait(idt))
		return (1);

	*idt->reg_cmd = 0x20000000 | opflag | address << 2;
	return (0);
}

/*******************************************************************************
 *
 *    nicstar_add_buf    ( card, mbuf1, mbuf2, which_queue)
 *
 *    This adds two buffers to the specified queue. This uses the
 *    mbuf address as handle and the buffer physical address must be
 *    the DMA address.
 *
 *    returns -1 if queue is full, the address is not word aligned, or
 *    an invalid queue is specified.
 *    returns 0 if parameters were acceptable.
 */

int
nicstar_add_buf(nicstar_reg_t * const idt, struct mbuf * buf0,
    struct mbuf * buf1, u_long islrg)
{
	u_long stat_val;
	u_long val0, val1, val2, val3;

	if (islrg > 1)			/* bad buffer size */
		return (-1);

	stat_val = *idt->reg_stat;

	if (islrg) {
		if (stat_val & 0x80)	/* large queue is full */
			return (-1);
	} else if (stat_val & 0x100)	/* small queue is full */
		return (-1);

	if (!buf0 || !buf1 || ((u_long)(buf0->m_data) & 0x7)
	    || ((u_long)(buf1->m_data) & 0x7)) {
		return (-1);		/* buffers must word aligned */
	}
	if (idt->raw_headm == NULL)	/* raw cell buffer pointer not
					 * initialized */
		if (islrg) {
			idt->raw_headm = buf0;
			idt->raw_headp = vtophys(buf0->m_data);
		}
	if (idt_status_wait(idt))	/* 12/06/2000 */
		return (-1);

	val0 = (u_long)buf0;		/* mbuf address is handle */
	val1 = vtophys(buf0->m_data);	/* DMA addr of buff1 */
	val2 = (u_long)buf1;		/* mbuf address is handle */
	val3 = vtophys(buf1->m_data);	/* DMA addr of buff2 */

	*(idt->reg_data + 0) = val0;
	*(idt->reg_data + 1) = val1;
	*(idt->reg_data + 2) = val2;
	*(idt->reg_data + 3) = val3;

	*idt->reg_cmd = 0x60000000 | islrg;

	idt_mcheck_add(idt, buf0);
	idt_mcheck_add(idt, buf1);

	return (0);
}

/******************************************************************************
 *
 *    nicstar_util_rd    ( card, util_location, answer_pointer )
 *
 *    reads one byte from the utility bus at "util_location" and places the
 *    value in "answer_pointer"
 *
 *    returns -1 if util location is out of range.
 *    returns 0 if parameters were acceptable
 */
static int
nicstar_util_rd(nicstar_reg_t * const idt, u_long address, u_long *data)
{

	if (address >= 0x81)			/* bad address */
		return (-1);

	if (idt_status_wait(idt))
		return (-1);

	*idt->reg_cmd = 0x80000200 | address;	/* read a word */

	if (idt_status_wait(idt))
		return (-1);

	*data = *idt->reg_data & 0xff;		/* save word */

	return (0);
}

/******************************************************************************
 *
 *    nicstar_util_wr    ( card, util location, data )
 *
 *    writes one byte to the utility bus at "util_location"
 *
 *    returns -1 if util location is out of range.
 *    returns 0 if parameters were acceptable
 */
static int
nicstar_util_wr(nicstar_reg_t * const idt, int cs, u_long address, u_long data)
{

	if (address >= 0x81)	/* bad address */
		return (-1);
	if (cs > 1)
		return (-1);

	if (idt_status_wait(idt))
		return (-1);

	*idt->reg_data = data & 0xff;	/* load last data item */

	if (cs == 0)
		*idt->reg_cmd = 0x90000100 | address;	/* write the byte, CS1 */
	else
		*idt->reg_cmd = 0x90000200 | address;	/* write the byte, CS2 */

	return (0);
}

/******************************************************************************
 *
 *    nicstar_eeprom_rd    ( card , byte_location )
 *
 *    reads one byte from the utility bus at "byte_location" and return the
 *    value as an integer. this routint is only used to read the MAC address
 *    from the EEPROM at boot time.
 */
int
nicstar_eeprom_rd(nicstar_reg_t * const idt, u_long address)
{
	volatile u_long *regGP = (volatile u_long *)(idt->virt_baseaddr + REGGP);
	volatile u_long gp = *regGP & 0xfffffff0;
	int i, value = 0;

	DELAY(5);		/* make sure idle */
	*regGP = gp | 0x06;	/* CS and Clock high */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	/* toggle in  READ CMD (00000011) */
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp;		/* CS and Clock low */
	DELAY(5);
	*regGP = gp | 0x04;	/* Clock high (data 0) */
	DELAY(5);
	*regGP = gp | 0x01;	/* CS and Clock low data 1 */
	DELAY(5);
	*regGP = gp | 0x05;	/* Clock high (data 1) */
	DELAY(5);
	*regGP = gp | 0x01;	/* CS and Clock low data 1 */
	DELAY(5);
	*regGP = gp | 0x05;	/* Clock high (data 1) */
	DELAY(5);
	/* toggle in the address */
	for (i = 7; i >= 0; i--) {
		*regGP = (gp | ((address >> i) & 1));	/* Clock low */
		DELAY(5);
		*regGP = (gp | 0x04 | ((address >> i) & 1));	/* Clock high */
		DELAY(5);
	}
	/* read EEPROM data */
	for (i = 7; i >= 0; i--) {
		*regGP = gp;	/* Clock low */
		DELAY(5);
		value |= ((*regGP & 0x10000) >> (16 - i));
		*regGP = gp | 0x04;	/* Clock high */
		DELAY(5);
	}
	*regGP = gp;		/* CS and Clock low */
	return (value);
}

/*******************************************************************************
 *
 *  Load the card receive buffers
 *
 *  in:  IDT device
 *
 *  Date first: 11/01/2000  last: 05/25/2000
 */

void
nicstar_ld_rcv_buf(IDT * idt)
{
	struct mbuf *m1, *m2;
	u_long stat_reg;
	int card_small;
	int card_large;
	int s;

	s = splimp();

	stat_reg = *(volatile u_long *)idt->stat_reg;

	card_small = (stat_reg & 0xff000000) >> 23;	/* reg is number of
							 * pairs */
	card_large = (stat_reg & 0x00ff0000) >> 15;

	if (idt_sysctl_bufsmall > 510)
		idt_sysctl_bufsmall = 510;
	if (idt_sysctl_buflarge > 510)
		idt_sysctl_buflarge = 510;
	if (idt_sysctl_bufsmall < 10)
		idt_sysctl_bufsmall = 10;
	if (idt_sysctl_buflarge < 10)
		idt_sysctl_buflarge = 10;

	while (card_small < idt_sysctl_bufsmall) {	/* 05/25/2001 from fixed */
		MGETHDR(m1, M_DONTWAIT, MT_DATA);
		if (m1 == NULL)
			break;
		MGETHDR(m2, M_DONTWAIT, MT_DATA);
		if (m2 == NULL) {
			m_free(m1);
			break;
		}
		MH_ALIGN(m1, 96);	/* word align & allow lots of
					 * prepending */
		MH_ALIGN(m2, 96);
		if (nicstar_add_buf(idt, m1, m2, 0)) {
			device_printf(idt->dev,
				"Cannot add small buffers, size=%d.\n",
				card_small);
			m_free(m1);
			m_free(m2);
			break;
		}
		card_small += 2;
	}

	while (card_large < idt_sysctl_buflarge) {	/* 05/25/2001 from fixed */
		m1 = idt_mbufcl_get();
		if (m1 == NULL)
			break;
		m2 = idt_mbufcl_get();
		if (m2 == NULL) {
			m_free(m1);
			break;
		}
		if (nicstar_add_buf(idt, m1, m2, 1)) {
			device_printf(idt->dev,
				"Cannot add large buffers, size=%d.\n",
				card_large);
			m_free(m1);
			m_free(m2);
			break;
		}
		card_large += 2;
	}
	idt_sysctl_curlarge = card_large;
	idt_sysctl_cursmall = card_small;

	splx(s);
}

/*******************************************************************************
 *
 *  Wait for command to finish
 *
 *  in:  IDT device
 * out:  zero = success
 *
 *  Date first: 12/06/2000  last: 12/16/2000
 */

int
idt_status_wait(IDT * idt)
{
	int timeout;

	timeout = 33 * 100;	/* allow 100 microseconds timeout */

	while (*idt->reg_stat & 0x200)
		if (--timeout == 0) {
			device_printf(idt->dev,
				      "timeout waiting for device status.\n");
			idt->stats_cmderrors++;
			return (1);
		}
	return (0);
}

/*******************************************************************************
 *
 *  Log status of system buffers
 *
 *  in:  IDT device
 *
 *  Date first: 10/31/2000  last: 05/25/2001
 */

void
idt_status_bufs(IDT * idt)
{
	u_long stat_reg;
	int card_small;
	int card_large;
	int s;

	s = splimp();

	stat_reg = *(volatile u_long *)idt->stat_reg;

	card_small = (stat_reg & 0xff000000) >> 23;	/* reg is number of
							 * pairs */
	card_large = (stat_reg & 0x00ff0000) >> 15;

	splx(s);

	device_printf(idt->dev, "BUFFER STATUS: small=%d/%d, large=%d/%d.\n",
	    card_small, idt_sysctl_bufsmall,
	    card_large, idt_sysctl_buflarge);
}

/*  Since this is called when the card timer wraps, we should only see
 *  this 16 times (LE155) or 10 (LE25) per hour.
 *
 *******************************************************************************
 *
 *  Add mbuf into "owned" list
 *
 *  in:  IDT device, mbuf
 * out:  zero = success
 *
 * Date first: 11/13/2000  last: 11/13/2000
 */

int
idt_mcheck_add(IDT * idt, struct mbuf * m)
{
	int hpos;
	int s;

	hpos = (((int)m) >> 8) & 1023;
	s = splimp();

	m->m_next = idt->mcheck[hpos];
	idt->mcheck[hpos] = m;

	splx(s);
	return (0);
}

/******************************************************************************
 *
 *  Remove mbuf from "owned" list
 *
 *  in:  IDT device, mbuf
 * out:  zero = success
 *
 * Date first: 11/13/2000  last: 11/13/2000
 */

int
idt_mcheck_rem(IDT * idt, struct mbuf * m)
{
	struct mbuf *nbuf;
	int hpos;
	int s;

	hpos = (((int)m) >> 8) & 1023;
	s = splimp();

	nbuf = idt->mcheck[hpos];

	if (nbuf == m) {
		idt->mcheck[hpos] = m->m_next;
		splx(s);
		m->m_next = NULL;
		return (0);
	}
	while (nbuf != NULL) {
		if (nbuf->m_next != m) {
			nbuf = nbuf->m_next;
			continue;
		}
		nbuf->m_next = m->m_next;
		splx(s);
		m->m_next = NULL;
		return (0);
	}

	splx(s);
	device_printf(idt->dev, "Card should not have this mbuf! %x\n", (int)m);
	return (1);
}

/******************************************************************************
 *
 *  Initialize mbuf "owned" list
 *
 *  in:  IDT device
 * out:  zero = success
 *
 * Date first: 11/13/2000  last: 05/26/2001
 */

int
idt_mcheck_init(IDT * idt)
{
	int size;
	int x;

	size = round_page(sizeof(struct mbuf *) * 1024);
	idt->mcheck = contigmalloc(size, M_DEVBUF, M_NOWAIT,
	    0x100000, 0xffffffff, 0x2000, 0);
	if (idt->mcheck == NULL)
		return (1);

	for (x = 0; x < 1024; x++)
		idt->mcheck[x] = NULL;

	return (0);
}

/******************************************************************************
 *
 *  Allocate contiguous, fixed memory
 *
 *  in:  number of pages
 * out:  pointer, NULL = failure
 *
 * Date first: 11/29/2000  last: 11/29/2000
 */

vm_offset_t
idt_malloc_contig(int pages)
{
	vm_offset_t retval;

	retval = (vm_offset_t)contigmalloc(pages * PAGE_SIZE,
	    M_DEVBUF, M_NOWAIT, 0x100000, 0xffffffff, 0x2000, 0);
#ifdef UNDEF
	printf("idt: vm_offset_t allocated %d pages at %x\n", pages, retval);
#endif

	return (retval);
}

/*******************************************************************************
 *
 *  Initialize all TX_QUEUE structures
 *
 *  in:  IDT device
 * out:  zero = succes
 *
 *  Date first: 11/29/2000  last: 11/29/2000
 */
static int
idt_queue_init(IDT * idt)
{
	TX_QUEUE *txqueue;
	vm_offset_t scqbase;
	int x;

	idt->cbr_size = IDT_MAX_CBRQUEUE * 16 * 64;
	idt->cbr_base = idt_malloc_contig(idt->cbr_size / PAGE_SIZE);
	scqbase = idt->cbr_base;
	if (scqbase == 0)
		return (1);
	idt->cbr_freect = idt->cbr_size / (16 * 64);

	for (x = 0; x < idt->cbr_freect; x++) {
		txqueue = &idt->cbr_txqb[x];
		txqueue->mget = NULL;
		txqueue->mput = NULL;
		txqueue->scd = IDT_SCD_START + x * 12;
		txqueue->scq_base = (u_long *)scqbase;
		txqueue->scq_next = txqueue->scq_base;
		txqueue->scq_last = txqueue->scq_next;
		txqueue->scq_len = 64;	/* all CBR queues use 64 entries */
		txqueue->scq_cur = 0;
		txqueue->rate = 0;
		txqueue->vbr_m = 0;	/* m & n set to zero for CBR */
		txqueue->vbr_n = 0;
		idt->cbr_free[x] = txqueue;
		scqbase += 64 * 16;
		nicstar_sram_wr(idt, txqueue->scd, 4,
		    vtophys(txqueue->scq_base), 0, 0xffffffff, 0);
	}

	txqueue = &idt->queue_vbr;	/* VBR queue */
	txqueue->mget = NULL;
	txqueue->mput = NULL;
	txqueue->scd = 0x1e7f4;
	txqueue->scq_base = (u_long *)(idt->fixbuf + 0x4000);
	txqueue->scq_next = txqueue->scq_base;
	txqueue->scq_last = txqueue->scq_next;
	txqueue->scq_len = 512;	/* all VBR queues use 512 entries */
	txqueue->scq_cur = 0;
	txqueue->rate = 0;
	txqueue->vbr_m = 1;
	txqueue->vbr_n = 1;
	nicstar_sram_wr(idt, txqueue->scd, 4,
	    vtophys(txqueue->scq_base), 0, 0xffffffff, 0);

	txqueue = &idt->queue_abr;	/* ABR queue (not currently used) */
	txqueue->mget = NULL;
	txqueue->mput = NULL;
	txqueue->scd = 0x1e7e8;
	txqueue->scq_base = (u_long *)(idt->fixbuf + 0x6000);
	txqueue->scq_next = txqueue->scq_base;
	txqueue->scq_last = txqueue->scq_next;
	txqueue->scq_len = 512;
	txqueue->scq_cur = 0;
	txqueue->rate = 0;
	txqueue->vbr_m = 1;
	txqueue->vbr_n = 1;
	nicstar_sram_wr(idt, txqueue->scd, 4,
	    vtophys(txqueue->scq_base), 0, 0xffffffff, 0);

	txqueue = &idt->queue_ubr;	/* UBR queue */
	txqueue->mget = NULL;
	txqueue->mput = NULL;
	txqueue->scd = 0x1e7dc;
	txqueue->scq_base = (u_long *)(idt->fixbuf + 0x8000);
	txqueue->scq_next = txqueue->scq_base;
	txqueue->scq_last = txqueue->scq_next;
	txqueue->scq_len = 512;
	txqueue->scq_cur = 0;
	txqueue->rate = 0;
	txqueue->vbr_m = 1;	/* since the ABR queue is lowest priority, */
	txqueue->vbr_n = 1;	/* these factors should never change */
	nicstar_sram_wr(idt, txqueue->scd, 4,
	    vtophys(txqueue->scq_base), 0, 0xffffffff, 0);

	return (0);
}

/*******************************************************************************
 *
 *  Get mbuf chain from TX_QUEUE
 *
 *  in:  CONNECTION
 * out:  mbuf, NULL = empty
 *
 *  Date first: 12/03/2000  last: 12/03/2000
 */
static struct mbuf *
idt_queue_get(TX_QUEUE * txqueue)
{
	struct mbuf *m1, *m2;
	int s;

	if (txqueue == NULL)
		return (NULL);

	s = splimp();

	m1 = txqueue->mget;
	if (m1 != NULL) {
		m2 = m1->m_nextpkt;
		txqueue->mget = m2;
		if (m2 == NULL)	/* is queue empty now? */
			txqueue->mput = NULL;
	}
	splx(s);

	return (m1);
}

/*******************************************************************************
 *
 *  Add mbuf chain to connection TX_QUEUE
 *
 *  in:  CONNECTION, mbuf chain
 * out:  zero = succes
 *
 *  Date first: 12/03/2000  last: 06/01/2001
 */
static int
idt_queue_put(CONNECTION * connection, struct mbuf * m)
{
	TX_QUEUE *txqueue;
	int s;

	if (connection == NULL) {
		m_freem(m);
		return (1);
	}
	txqueue = connection->queue;
	if (txqueue == NULL) {
		m_freem(m);
		return (1);
	}
	m->m_nextpkt = NULL;
	m->m_pkthdr.rcvif = (void *) connection;

	s = splimp();

	if (txqueue->mput != NULL) {
		*txqueue->mput = m;
		txqueue->mput = &m->m_nextpkt;
	} else {		/* queue is empty */
		txqueue->mget = m;
		txqueue->mput = &m->m_nextpkt;
	}
	splx(s);

	return (0);
}

/*******************************************************************************
 *
 *  Flush all connection mbufs from TX_QUEUE
 *
 *  in:  CONNECTION
 * out:  zero = succes
 *
 *  Date first: 12/03/2000  last: 12/03/2000
 */
static int
idt_queue_flush(CONNECTION * connection)
{
	TX_QUEUE *txqueue;
	struct mbuf **m0, *m1;
	int s;

	if (connection == NULL)
		return (1);
	txqueue = connection->queue;
	if (txqueue == NULL)
		return (1);

	s = splimp();

	m0 = &txqueue->mget;
	m1 = *m0;
	while (m1 != NULL) {
		if (m1->m_pkthdr.rcvif == (void *) connection) {
			*m0 = m1->m_nextpkt;
			m_freem(m1);
			m1 = *m0;
			continue;
		}
		m0 = &m1->m_nextpkt;
		m1 = *m0;
	}
	txqueue->mput = m0;
	splx(s);

	return (0);
}

/*******************************************************************************
 *
 *  Calculate number of table positions for CBR connection
 *
 *  in:  IDT device, PCR (cells/second)
 * out:  table positions needed (minimum = 1)
 *
 *  Date first: 11/29/2000  last: 06/12/2001
 */
int
idt_slots_cbr(IDT * idt, int pcr)
{
	unsigned int bitrate;
	unsigned int slots;
	unsigned int rem;

	if (pcr == 171) {
		if (idt_sysctl_logvcs)
			device_printf(idt->dev,
				"idt_slots_cbr:  CBR channel=64000, 1 slot\n");
		return (1);
	}
	if (pcr < 171) {
		if (idt_sysctl_logvcs)
			device_printf(idt->dev,
				"idt_slots_cbr:  CBR pcr %d rounded up to 1 slot\n", pcr);
		return (1);
	}
	bitrate = pcr * 47 * 8;
	slots = bitrate / 64000;
	rem = bitrate % 64000;
	if (rem && idt_sysctl_logvcs)
		device_printf(idt->dev,
			"idt_slots_cbr: CBR cell rate rounded down to %d from %d\n",
			((slots * 64000) / 376), pcr);	/* slots++;  */

	if (idt_sysctl_logvcs)
		device_printf(idt->dev,
			"idt_slots_cbr:  CBR pcr=%d, slots=%d.\n", pcr, slots);
	return (slots);
}

/*  The original algorithm rounded up or down by 32k, the goal being to
 *  map 64000 requests exactly.  Unfortunately, this caused one particular
 *  SVC to be set one slot too low, causing mbuf cluster starvation.
 *  We can still handle the single 64k channel with a special case, and
 *  let all others fall where they may.
 *
 *******************************************************************************
 *
 *  Add TX QUEUE pointer to slots in CBR table
 *
 *  in:  IDT device, TX_QUEUE, number slots
 * out:  zero = success
 *
 *  Date first: 11/29/2000  last: 06/11/2001
 */
static int
idt_slots_add(IDT * idt, TX_QUEUE * queue, int slots)
{
	TX_QUEUE *curval;
	int p_max;		/* extra precision slots maximum */
	int p_spc;		/* extra precision spacing value */
	int p_ptr;		/* extra precision pointer */
	int qptr, qmax;
	int qlast;
	int scdval;

	if (slots < 1)
		return (1);

	qmax = idt->txslots_max;
	p_max = qmax << 8;
	p_spc = p_max / slots;
	p_ptr = p_spc >> 1;	/* use half spacing for start point */
	qptr = p_ptr >> 8;
	qlast = qptr;

	scdval = 0x20000000 | queue->scd;

	if (CBR_VERBOSE) {
		printf("idt_slots_add: p_max = %d\n", p_max);
		printf("idt_slots_add: p_spc = %d\n", p_spc);
		printf("idt_slots_add: p_ptr = %d\n", p_ptr);
		printf("idt_slots_add: qptr  = %d\n", qptr);
	}
	while (slots) {
		if (qptr >= qmax)	/* handle wrap for empty slot choosing */
			qptr -= qmax;
		curval = idt->cbr_slot[qptr];
		if (curval != NULL) {	/* this slot has CBR, so try next */
			qptr++;	/* next slot */
			continue;
		}
		if (CBR_VERBOSE) {
			printf("idt_slots_add: using qptr %d (%d)\n", qptr, qptr - qlast);
			qlast = qptr;
		}
		idt->cbr_slot[qptr] = queue;
		nicstar_sram_wr(idt, qptr + IDT_TST_START, 1, scdval, 0, 0, 0);
		slots--;
		p_ptr += p_spc;
		if (p_ptr >= p_max)	/* main pointer wrap */
			p_ptr -= p_max;
		qptr = p_ptr >> 8;
	}
	return (0);
}

/* 06/11/2001:  Extra precision pointer is used in order to handle cases where
 *       fractional slot spacing causes a large area of slots to be filled.
 *       This can cause further CBR circuits to get slots that have very
 *       poor spacing.
 *
 *******************************************************************************
 *
 *  Remove TX QUEUE pointer from slots in CBR table
 *
 *  in:  IDT device, TX_QUEUE
 * out:  number of CBR slots released
 *
 *  Date first: 12/03/2000  last: 12/03/2000
 */
static int
idt_slots_rem(IDT * idt, TX_QUEUE * queue)
{
	int qptr, qmax;
	int slots;

	qmax = idt->txslots_max;
	slots = 0;

	for (qptr = 0; qptr < qmax; qptr++) {
		if (idt->cbr_slot[qptr] != queue)
			continue;
		idt->cbr_slot[qptr] = NULL;
		nicstar_sram_wr(idt, qptr + IDT_TST_START, 1, 0x40000000, 0, 0, 0);
		slots++;
	}
	return (slots);
}

/*******************************************************************************
 *
 *  Initialize slots in CBR table
 *
 *  in:  IDT device
 * out:  zero = success
 *
 *  Date first: 11/29/2000  last: 11/29/2000
 */
static int
idt_slots_init(IDT * idt)
{
	int start;		/* table start pointer */
	int qptr;

	start = IDT_TST_START;

	/* first, fill up the TX CBR table with 'VBR' entries  */

	for (qptr = 0; qptr < idt->txslots_max; qptr++) {
		idt->cbr_slot[qptr] = NULL;
		nicstar_sram_wr(idt, qptr + start, 1, 0x40000000, 0, 0, 0);
	}

	/* now write the jump back to the table start */

	nicstar_sram_wr(idt, qptr + start, 1, 0x60000000 | start, 0, 0, 0);

	return (0);
}

/*******************************************************************************
 *
 *  Open output queue for connection
 *
 *  in:  IDT device, connection (class, traf_pcr, & traf_scr fields valid)
 * out:  zero = success
 *
 *  Date first: 11/29/2000  last: 06/13/2001
 */

int
idt_connect_txopen(IDT * idt, CONNECTION * connection)
{
	TX_QUEUE *txqueue;
	int cellrate;
	int cbr_slots;
	int s;

	cellrate = connection->traf_scr;	/* 06/13/2001 use SCR instead
						 * of PCR */

	if (connection->class == T_ATM_UBR) {	/* UBR takes whatever is left
						 * over */
		connection->queue = &idt->queue_ubr;
		if (idt_sysctl_logvcs)
			printf("idt_connect_txopen: UBR connection for %d/%d\n",
			    connection->vpi, connection->vci);
		return (0);
	}
	if (connection->class == T_ATM_ABR) {	/* ABR treated as UBR-plus */
		connection->queue = &idt->queue_abr;
		if (idt_sysctl_logvcs)
			printf("idt_connect_txopen: UBR+ connection for %d/%d\n",
			    connection->vpi, connection->vci);
		return (0);
	}
	if (connection->class == T_ATM_CBR) {
		cbr_slots = idt_slots_cbr(idt, cellrate);
		s = splimp();
		if (cbr_slots > (idt->txslots_max - idt->txslots_cur) ||
		    idt->cbr_freect < 1) {
			splx(s);
			return (1);	/* requested rate not available */
		}
		idt->txslots_cur += cbr_slots;
		idt->cellrate_tcur += cellrate;
		idt->cbr_freect--;
		txqueue = idt->cbr_free[idt->cbr_freect];
		txqueue->rate = cellrate;	/* was connection->traf_pcr */

		if (idt_slots_add(idt, txqueue, cbr_slots)) {
			idt->txslots_cur -= cbr_slots;	/* cannot add CBR slots */
			idt->cellrate_tcur -= cellrate;
			idt->cbr_free[idt->cbr_freect] = txqueue;
			idt->cbr_freect++;
			splx(s);
			return (1);
		}
		splx(s);
		if (idt_sysctl_logvcs)
			printf("idt_connect_txopen: CBR connection for %d/%d\n",
			    connection->vpi, connection->vci);
		connection->queue = txqueue;
	}
	if (connection->class == T_ATM_VBR) {
		txqueue = &idt->queue_vbr;
		connection->queue = txqueue;
		txqueue->rate += connection->traf_scr;	/* from traf_pcr
							 * 12/17/2000 */
		if (idt_sysctl_logvcs)
			printf("idt_connect_txopen: VBR connection for %d/%d\n",
			    connection->vpi, connection->vci);
	}
	idt_connect_newvbr(idt);/* recalculate VBR divisor values */

	if (connection->class == T_ATM_CBR ||
	    connection->class == T_ATM_VBR)
		return (0);

	return (1);		/* unknown class */
}

/*******************************************************************************
 *
 *  Close connection output queue
 *
 *  in:  IDT device, connection (class, traf_pcr, & traf_scr fields valid)
 * out:  zero = success
 *
 *  Date first: 12/03/2000  last: 12/03/2000
 */
int
idt_connect_txclose(IDT * idt, CONNECTION * connection)
{
	TX_QUEUE *txqueue;
	int cellrate;
	int slots;
	int s;

	cellrate = connection->traf_pcr;
	txqueue = connection->queue;
	if (idt_sysctl_logvcs)
		printf("idt_connect_txclose: closing connection for %d/%d\n",
		    connection->vpi, connection->vci);

	idt_queue_flush(connection);	/* flush all connection mbufs */

	if (connection->class == T_ATM_UBR ||	/* UBR takes whatever is left
						 * over */
	    connection->class == T_ATM_ABR) {	/* ABR not supported, use UBR */
		connection->queue = NULL;
		return (0);
	}
	if (connection->class == T_ATM_CBR) {
		slots = idt_slots_rem(idt, txqueue);	/* remove this queue
							 * from CBR slots */
		s = splimp();
		idt->txslots_cur -= slots;
		idt->cellrate_tcur -= cellrate;
		if (txqueue != NULL) {	/* 06/12/2001 check for failure on
					 * open */
			idt->cbr_free[idt->cbr_freect] = txqueue;
			idt->cbr_freect++;
		}
		splx(s);
		connection->queue = NULL;
	}
	if (connection->class == T_ATM_VBR) {
		txqueue = &idt->queue_vbr;
		connection->queue = NULL;
		txqueue->rate -= connection->traf_scr;	/* from traf_pcr
							 * 12/17/2000 */
	}
	idt_connect_newvbr(idt);/* recalculate VBR divisor values */

	if (connection->class == T_ATM_CBR ||
	    connection->class == T_ATM_VBR)
		return (0);

	return (1);		/* unknown class */
}

/*******************************************************************************
 *
 *  Calculate new VBR divisor values
 *
 *  in:  IDT device
 *
 * Date first: 12/03/2000  last: 12/03/2000
 */
static void
idt_connect_newvbr(IDT * idt)
{
	TX_QUEUE *txqueue;
	int rate_newvbr;
	int rate_noncbr;
	int divisor;

	txqueue = &idt->queue_vbr;

	rate_newvbr = txqueue->rate;
	rate_noncbr = idt->cellrate_tmax - idt->cellrate_tcur;

	if (rate_newvbr < 1)	/* keep sane and prevent divide by zero */
		rate_newvbr = 1;

	if (rate_newvbr >= rate_noncbr) {
		txqueue->vbr_m = 1;
		txqueue->vbr_n = 1;
		return;
	}
	divisor = rate_newvbr * 1000;	/* size of lookup table */
	divisor += rate_newvbr >> 1;	/* apply rounding to divide */
	divisor /= rate_noncbr;	/* always < 1000, since newvbr < noncbr */

	if (idt_sysctl_logvcs)
		printf("idt_connect_newvbr: divisor=%d\n", divisor);
	txqueue->vbr_m = vbr_div_m[divisor];
	txqueue->vbr_n = vbr_div_n[divisor];
	if (idt_sysctl_logvcs)
		printf("idt_connect_newvbr: m=%d, n=%d\n", txqueue->vbr_m, txqueue->vbr_n);
}

/*  For VBR, we track the sum of all the VBR peak cellrates, and divide
 *  that from the "remaining" bandwidth, which is total minus current CBR.
 *
 *  We will need to adjust the VBR divisor whenever we add a CBR or VBR.
 *
 *  Because of the integer scalign (1000) preload, the cellrate for the
 *  VBR channel should not exceed 2 million (aprox 5 OC3s).  This is
 *  protected by the check for rate_newvbr >= rate_noncbr.
 *
 *******************************************************************************
 *
 *  Initialize large buffers, indexes, and reference counts
 *
 *  in:  IDT device
 * out:  zero = success
 *
 * Date first: 11/01/2000  last: 05/25/2001
 */

int
idt_buffer_init(IDT * idt)
{

	idt->raw_headm = NULL;	/* nicstar_add_buf() will initialize */
	idt->raw_headp = 0;

	return (0);
}

/*******************************************************************************
 *
 *  Get large buffer from kernel pool
 *
 * out:  mbuf, NULL = error
 *
 * Date first: 05/25/2001  last: 05/25/2001
 */

struct mbuf *
idt_mbufcl_get(void)
{
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);

	MCLGET(m, M_DONTWAIT);
	if (m->m_flags & M_EXT)
		return (m);

	m_freem(m);
	return (NULL);
}

/*******************************************************************************
 *
 *  Initialize connection table
 *
 *  in:  IDT, number of VPI bits (0, 1, or 2)
 * out:  zero = success
 *
 * Date first: 10/29/2000  last: 12/10/2000
 */

int
idt_connect_init(IDT * idt, int vpibits)
{
	CONNECTION *connection;
	int pages;
	int vpi;
	int vci;

	switch (vpibits) {
	case 1:
		idt->conn_maxvpi = 2;
		idt->conn_maxvci = 2048;
		break;
	case 2:
		idt->conn_maxvpi = 4;
		idt->conn_maxvci = 1024;
		break;
	default:
		idt->conn_maxvpi = 1;
		idt->conn_maxvci = 4096;
	}

	pages = (sizeof(CONNECTION) * MAX_CONNECTION) + PAGE_SIZE - 1;
	pages /= PAGE_SIZE;
	idt->connection = contigmalloc(pages * PAGE_SIZE, M_DEVBUF, M_NOWAIT,
	    0x100000, 0xffffffff, 0x2000, 0);
	if (idt->connection == NULL)
		return (1);

	for (vpi = 0; vpi < idt->conn_maxvpi; vpi++)
		for (vci = 0; vci < idt->conn_maxvci; vci++) {
			connection = &idt->connection[vpi * idt->conn_maxvci + vci];
			connection->vccinf = NULL;	/* may want to change to
							 * "unclaimed" */
			connection->status = 0;	/* closed */
			connection->vpi = vpi;
			connection->vci = vci;
			connection->queue = NULL;	/* no current TX queue */
			connection->recv = NULL;	/* no current receive
							 * mbuf */
			connection->rlen = 0;
			connection->maxpdu = 0;
			connection->traf_pcr = 0;
			connection->traf_scr = 0;
			connection->aal = 0;
			connection->class = 0;
			connection->flg_mpeg2ts = 0;
			connection->flg_clp = 0;
		}

	return (0);
}

/*******************************************************************************
 *
 *  Look up a connection
 *
 *  in:  IDT, vpi, vci
 * out:  CONNECTION, NULL=invalid vpi/vci
 *
 * Date first: 10/29/2000  last: 10/29/2000
 */

CONNECTION *
idt_connect_find(IDT * idt, int vpi, int vci)
{
	if (vpi >= idt->conn_maxvpi)
		return (NULL);
	if (vci >= idt->conn_maxvci)
		return (NULL);

	return (&idt->connection[vpi * idt->conn_maxvci + vci]);
}

/******************************************************************************
 *
 *                       MBUF SECTION
 *
 ******************************************************************************
 *
 *  Align data in mbuf (to 32-bit boundary)
 *
 *  in:  mbuf
 * out:  zero = success
 *
 * Date first: 11/08/2000  last: 11/15/2000
 */

int
idt_mbuf_align(struct mbuf * m, struct mbuf * prev)
{
	caddr_t buf_base;
	int buf_size;
	int offset;
	int newlen;
	int count;

	if (m == NULL)
		return (1);
	if (((int)m->m_data & 3) == 0)
		return (0);

	if (m->m_flags & M_EXT) {	/* external storage */
		buf_base = m->m_ext.ext_buf;
		buf_size = m->m_ext.ext_size;

		/*
		 * we should really bail out at this point, since we cannot
		 * just shift the data in an external mbuf
		 */

	} else {
		if (m->m_flags & M_PKTHDR) {	/* internal storage, packet
						 * header */
			buf_base = m->m_pktdat;
			buf_size = MHLEN;
		} else {
			buf_base = m->m_dat;	/* internal storage, no packet
						 * header */
			buf_size = MLEN;
		}
	}
	offset = 4 - ((int)buf_base & 3);
	offset &= 3;
	buf_base += offset;	/* new (aligned) buffer base */

	if (m->m_len + offset > buf_size)	/* not enough space to just
						 * move */
		if (prev != NULL)
			if (idt_mbuf_append4(prev, m->m_data) == 0) {	/* give word to prev
									 * mbuf */
				m->m_data += 4;
				m->m_len -= 4;
			}
	if (m->m_len + offset > buf_size)	/* still not enough space */
		if (m->m_next != NULL) {
			newlen = buf_size - offset;	/* maximum new length */
			newlen &= 0xfffffc;	/* fix the length too... */
			count = buf_size - newlen;	/* bytes we have to get
							 * rid of */
			if (idt_mbuf_prepend(m->m_next, m->m_data + newlen, count) == 0)
				m->m_len = newlen;
		}
	if (m->m_len + offset > buf_size)	/* we're stuck... */
		return (1);

	bcopy(m->m_data, buf_base, m->m_len);	/* move data to aligned
						 * position */
	m->m_data = buf_base;
	return (0);
}

/*******************************************************************************
 *
 *  Append 4 bytes to mbuf
 *
 *  in:  mbuf, data pointer
 * out:  zero = success
 *
 * Date first: 11/08/2000  last: 12/13/2000
 */

int
idt_mbuf_append4(struct mbuf * m, char *newdata)
{
	caddr_t buf_base;
	int buf_size;
	int align;
	int space;

	if (m == NULL)
		return (1);

	if (m->m_flags & M_EXT)	/* external storage */
		return (1);	/* 12/13/2000 we must not touch it */

	if (m->m_flags & M_PKTHDR) {	/* internal storage, packet header */
		buf_base = m->m_pktdat;
		buf_size = MHLEN;
	} else {
		buf_base = m->m_dat;	/* internal storage, no packet header */
		buf_size = MLEN;
	}

	align = (4 - ((int)buf_base & 3)) & 3;
	buf_base += align;
	buf_size -= align;
	buf_size &= 0xfffffc;

	space = buf_size - m->m_len;
	if (space < 4)		/* enough space to add 4 bytes? */
		return (1);

	space -= m->m_data - buf_base;	/* get space at end */

	if (space < 4) {
		bcopy(m->m_data, buf_base, m->m_len);
		m->m_data = buf_base;
	}
	bcopy(newdata, m->m_data + m->m_len, 4);
	m->m_len += 4;

	return (0);
}

/*******************************************************************************
 *
 *  Get current base of data storage
 *
 *  in:  mbuf
 * out:  base
 *
 * Date first: 11/16/2000  last: 11/16/2000
 */

caddr_t
idt_mbuf_base(struct mbuf * m)
{
	if (m == NULL)
		return (NULL);

	if (m->m_flags & M_EXT)	/* external storage */
		return (m->m_ext.ext_buf);

	if (m->m_flags & M_PKTHDR)	/* internal storage, packet header */
		return (m->m_pktdat);

	return (m->m_dat);	/* internal storage, no packet header */
}

/*******************************************************************************
 *
 *  Copy mbuf chain to new chain (aligned)
 *
 *  in:  mbuf
 * out:  new mbuf chain, NULL=error
 *
 * Date first: 11/19/2000  last: 05/25/2001
 */

struct mbuf *
idt_mbuf_copy(IDT * idt, struct mbuf * m)
{
	struct mbuf *nbuf, *dbuf, *sbuf;
	u_char *sptr;
	int slen;
	int clen;

	nbuf = idt_mbufcl_get();
	if (nbuf == NULL)
		return (NULL);
	dbuf = nbuf;
	dbuf->m_len = 0;

	for (sbuf = m; sbuf != NULL; sbuf = sbuf->m_next) {
		sptr = sbuf->m_data;
		slen = sbuf->m_len;
		while (slen) {
			clen = slen;
			if (clen > NICSTAR_LRG_SIZE - dbuf->m_len)
				clen = NICSTAR_LRG_SIZE - dbuf->m_len;
			bcopy(sptr, dbuf->m_data + dbuf->m_len, clen);
			sptr += clen;
			slen -= clen;
			dbuf->m_len += clen;
			if (dbuf->m_len >= NICSTAR_LRG_SIZE) {
				dbuf->m_next = idt_mbufcl_get();
				if (dbuf->m_next == NULL) {
					m_freem(nbuf);
					return (NULL);
				}
				dbuf = dbuf->m_next;
				dbuf->m_len = 0;
			}	/* if need dest buf */
		}		/* while(slen) */
	}			/* for... source buf */
	m_freem(m);
	return (nbuf);
}

/*******************************************************************************
 *
 *  Prepend data to mbuf (no alignment done)
 *
 *  in:  mbuf, data pointer, data length
 * out:  zero = success
 *
 * Date first: 11/15/2000  last: 12/13/2000
 */

int
idt_mbuf_prepend(struct mbuf * m, char *newdata, int newlen)
{
	caddr_t buf_base;
	int buf_size;
	int space;

	if (m == NULL)
		return (1);

	if (m->m_flags & M_EXT)	/* external storage */
		return (1);	/* 12/13/2000 we must not touch it */

	if (m->m_flags & M_PKTHDR) {	/* internal storage, packet header */
		buf_base = m->m_pktdat;
		buf_size = MHLEN;
	} else {
		buf_base = m->m_dat;	/* internal storage, no packet header */
		buf_size = MLEN;
	}

	space = m->m_data - buf_base;

	if (space >= newlen) {	/* already space at head of mbuf */
		m->m_data -= newlen;
		m->m_len += newlen;
		bcopy(newdata, m->m_data, newlen);
		return (0);
	}
	space = buf_size - m->m_len;	/* can we get the space by shifting? */
	if (space < newlen)
		return (1);

	bcopy(m->m_data, m->m_data + newlen, m->m_len);
	bcopy(newdata, m->m_data, newlen);
	m->m_len += newlen;

	return (0);
}

/*******************************************************************************
 *
 *  Get amount of data used in mbuf chain
 *
 *  in:  mbuf chain
 * out:  used space
 *
 * Date first: 11/10/2000  last: 11/10/2000
 */

int
idt_mbuf_used(struct mbuf * mfirst)
{
	struct mbuf *m1;
	int mbuf_used;

	mbuf_used = 0;		/* used mbuf space */

	for (m1 = mfirst; m1 != NULL; m1 = m1->m_next)
		mbuf_used += m1->m_len;

	return (mbuf_used);
}

/*******************************************************************************
 *
 *  Notes on transmit buffers:
 *
 *  According to the IDT Nicstar User Manual (version 1.0 2/26/1997), we must
 *  follow these rules for the transmit buffers (page 66):
 *
 *  1.  The buffer length must not be zero.
 *  2.  The buffer length must be a multiple of four bytes.
 *  3.  The sum of the buffer lengths must be a multiple of 48 bytes if
 *      it is a CS-PDU (eg AAL5).
 *  4.  All buffers for a CS-PDU must be contiguous and grouped (no other
 *      PDU buffers or even TSRs).
 *  5.  For AAL5 PDUs, the buffer lengths must include 8 bytes for the
 *      AAL5 length/control and CRC fields.
 *  6.  For AAL5 PDUs, the buffer length of the last buffer must be > 8 bytes.
 *  7.  For AAL5 PDUs, all buffers containing bytes for the last cell must
 *      have the END_CS_PDU bit set to 1.
 *
 *  Also, from the IDT applications note ("FAQ") 77211_AN_97088.pdf file:
 *    Page 5, under "General Technical Questions" (copied EXACTLY):
 *
 *  5).  Can the NicStar begin segmentation from a non-word aligned buffer?
 *       No, the transmit buffer must point to a word aligned buffer.
 *
 *  Since the buffers MUST be word aligned and MUST be word lengths, we have
 *  two potential problems with M_EXT mbufs:
 *
 *  1.  If the M_EXT mbuf has a non word aligned address, we have to copy
 *      the whole thing to a fresh buffer.  Unless - the previous mbuf is
 *      not M_EXT, and it is short by exactly the same amount.  Unlikely.
 *
 *  2.  If the M_EXT mbuf has a non word length, we have to push those bytes
 *      to the next mbuf.  If the next mbuf is also M_EXT, we are stuck.
 *      Unless - the extra bytes from both mbufs are exactly 4 bytes.  Then
 *      we can MGET an empty buf to splice in between.
 *
 *  Also, these rules mean that if any buffer is not word-length, all of the
 *  following buffers will need to be copied/shifted, unless one or more have
 *  lengths off by the right amount to fix the earlier buffer.
 *
 *******************************************************************************
 *
 *  Put mbuf chain on transmit queue
 *
 *  in:  IDT device, mbuf chain, vpi, vci, flags (2 MPEG2 TS == 8 AAL5 cells)
 * out:  (nothing)
 *
 * Date first: 11/08/2000  last: 05/30/2000
 */

void
idt_transmit(IDT * idt, struct mbuf * mfirst, int vpi, int vci, int flags)
{
	CONNECTION *connection;
	struct mbuf *m1, *m0, *malign, *msend;
	int tot_size, tot_scq, x;
	int this_len;
	int padding;

	connection = idt_connect_find(idt, vpi, vci);
	if (connection == NULL) {	/* this VPI/VCI not open */
		idt_transmit_drop(idt, mfirst);
		return;
	}
	if (connection->queue == NULL) {
		idt_transmit_drop(idt, mfirst);
		connection->vccinf->vc_oerrors++;
		return;
	}
	if (flags)
		connection->flg_mpeg2ts = 1;
	else
		connection->flg_mpeg2ts = 0;

	/*
	 * New strategy:  assume that all the buffers are aligned and word
	 * length.  Drop out and handle exceptions below.
	 */

	tot_size = 0;
	tot_scq = 1;
	malign = NULL;

	for (m1 = mfirst; m1 != NULL; m1 = m1->m_next) {
		this_len = m1->m_len;
		tot_size += this_len;
		tot_scq++;
		if (malign != NULL)
			continue;
		if ((int)(m1->m_data) & 3) {	/* bad alignment */
			malign = m1;
			continue;
		}
		if ((this_len & 3) == 0)	/* mbuf length is ok */
			continue;
		if (m1->m_next != NULL) {	/* bad length (in middle) */
			malign = m1;
			continue;
		}
		padding = 4 - (this_len & 3);
		tot_size += padding;
		m1->m_len += padding;
		break;		/* last mbuf, so avoid the loop test */
	}
	if (malign == NULL) {	/* perfect packet, no copy needed */
		mfirst->m_pkthdr.len = tot_size;
		if (connection->flg_mpeg2ts)
			tot_scq += tot_size / 376;	/* more entries needed
							 * for split */
		mfirst->m_pkthdr.csum_data = tot_scq;

		if (idt_queue_put(connection, mfirst))	/* put packet on TX
							 * queue */
			device_printf(idt->dev, "Cannot queue packet for %d/%d.\n", vpi, vci);
		if (connection->queue->mget == mfirst)	/* was the queue empty? */
			idt_transmit_top(idt, connection->queue);	/* IFF empty, prime it
									 * now */
		return;
	}
	/*
	 * Bad alignment or length, so fall through to old code... The first
	 * alignment problem is at 'malign'
	 */
	if (idt_sysctl_logvcs)
		device_printf(idt->dev, "Bad TX buf alignment, len=%d.\n", tot_size);

	if (idt_mbuf_align(mfirst, NULL)) {
		printf("idt_transmit: cannot align first mbuf.\n");
		idt_transmit_drop(idt, mfirst);
		connection->vccinf->vc_oerrors++;
		return;
	}
	/* find first mbuf with bad alignment (if any) */

	m0 = mfirst;
	for (m1 = mfirst->m_next; m1 != NULL; m0 = m1, m1 = m1->m_next) {
		if (m1->m_len & 3)
			break;
		if ((int)(m1->m_data) & 3)
			break;
	}
	if (m1 != NULL) {
		m1 = idt_mbuf_copy(idt, m1);	/* copy the rest into new
						 * mbufs */
		m0->m_next = m1;
		if (m1 == NULL) {
			printf("idt_transmit: could not copy buffers.\n");
			idt_transmit_drop(idt, mfirst);
			connection->vccinf->vc_oerrors++;
			return;	/* FIX THIS - this path has been taken */
		}
	}
	msend = mfirst;

	/* The mbuf chain is aligned, now we need to pad to word length */

	tot_size = idt_mbuf_used(msend);	/* forget the pkthdr length... */
	msend->m_pkthdr.len = tot_size;

	padding = (4 - (tot_size & 3)) & 3;
	if (padding) {
		for (m1 = msend; m1->m_next != NULL; m1 = m1->m_next);
		m1->m_len += padding;
	}
	x = 1;			/* now calculate the SCQ entries needed */
	for (m1 = msend; m1 != NULL; m1 = m1->m_next)
		x++;
	if (connection->flg_mpeg2ts)
		x += tot_size / 376;	/* more entries needed for split */
	msend->m_pkthdr.csum_data = x;

	/* now we have an mbuf chain, from *msend to *m1 ready to go */

	if (idt_queue_put(connection, msend))	/* put packet on TX queue */
		device_printf(idt->dev, "Cannot queue packet for %d/%d.\n", vpi, vci);

	if (connection->queue->mget == msend)	/* was the queue empty? */
		idt_transmit_top(idt, connection->queue);	/* IFF empty, prime it
								 * now */
}

/*  Notes on mbuf usage in the transmit queue:
 *
 *  m_pkthdr.rcvif       Connection pointer (set by idt_queue_put)
 *  m_pkthdr.len         Length of PDU
 *  m_pkthdr.header      TX queue pointer (06/01/2001)
 *  m_pkthdr.csum_flags  Unused, keep zero
 *  m_pkthdr.csum_data   Number of SCQ entries needed or used
 *
 *******************************************************************************
 *
 *  Drop transmit mbuf chain and update counters
 *
 *  in:  IDT device, mbuf chain
 * out:  (nothing)
 *
 * Date first: 11/08/2000  last: 11/08/2000
 */

void
idt_transmit_drop(IDT * idt, struct mbuf * mfirst)
{
	struct mbuf *next;
	int mesglen;

	mesglen = 0;
	while (mfirst != NULL) {
		mesglen += mfirst->m_len;
		next = m_free(mfirst);
		mfirst = next;
	}
	device_printf(idt->dev, "dropping transmit packet, size=%d\n", mesglen);
	idt->stats_oerrors++;	/* 12/15/2000 */
}

/*******************************************************************************
 *
 *  Put mbuf chain on transmit queue
 *
 *  in:  IDT device, TX_QUEUE
 * out:  (nothing)
 *
 * Date first: 12/03/2000  last: 06/01/2001
 */

void
idt_transmit_top(IDT * idt, TX_QUEUE * txqueue)
{
	CONNECTION *connection;
	struct mbuf *top, *m;
	static int padding[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int scq_space;
	int val, val1, val3, val4;
	int count, mlen, tlen, pad;
	char *mptr;
	int pdulen;
	int vci, vpi;
	int s;

	if (txqueue == NULL)	/* 12/12/2000 */
		return;
	if (txqueue->mget == NULL)	/* check for empty queue */
		return;

	s = splimp();

	scq_space = txqueue->scq_len - txqueue->scq_cur;

	/* Now we can add the queue entries for the PDUs */

	count = 0;

	for (;;) {
		top = txqueue->mget;	/* next available mbuf */
		if (top == NULL)
			break;

		if (top->m_pkthdr.csum_data + 4 > scq_space)
			break;	/* not enough space for this PDU */

		top = idt_queue_get(txqueue);
		if (top == NULL)
			break;
		connection = (CONNECTION *) top->m_pkthdr.rcvif;
		vpi = connection->vpi;
		vci = connection->vci;
		top->m_pkthdr.header = (void *)connection->queue;

		top->m_pkthdr.csum_data = 0;	/* track actual number of SCQ
						 * entries used */

		tlen = top->m_pkthdr.len;
		switch (connection->aal) {
		case ATM_AAL0:
			val = 0;
			break;
		case ATM_AAL3_4:
			val = 0x04000000;
			break;
		case ATM_AAL5:
			val = 0x08000000;
			break;
		default:
			device_printf(idt->dev, "bad AAL for %d/%d\n", vpi, vci);
			m_freem(top);
			connection->vccinf->vc_oerrors++;
			continue;
		}
		val |= txqueue->vbr_m << 23;
		val |= txqueue->vbr_n << 16;
		val4 = (vpi << 20) | (vci << 4);
		if (connection->flg_clp)
			val4 |= 1;	/* set CLP flag */

		/*
		 * Now we are ready to start mapping the mbuf(s) to transmit
		 * buffer descriptors.  If the MPEG2TS flag is set, we want
		 * to create AAL5 PDUs of exactly 384 data bytes each.
		 */

		pdulen = top->m_pkthdr.len;	/* default case: don't split
						 * PDU */
		pad = 0;
		if (connection->flg_mpeg2ts) {
			if ((pdulen % 376) == 0) {	/* correct multiple */
				pdulen = 376;	/* cut off every pdu at 374
						 * data bytes */
				pad = 8;
			} else
				device_printf(idt->dev, "Bad MPEG2 PDU buffer (%d bytes).\n", pdulen);
		}
		val3 = pdulen;	/* actual (unpadded) PDU length */

		pdulen += (4 - (pdulen & 3)) & 3;

		if (pad == 0) {	/* normal padding (PDU not split) */
			pad = pdulen;
			if (connection->aal == ATM_AAL5)
				pad += 8;
			pad = 48 - (pad % 48);
			if (pad == 48)
				pad = 0;
			if (connection->aal == ATM_AAL5)
				pad += 8;	/* pad of up to 52 is
						 * possible/neccessary */
		}
		tlen = 0;
		for (m = top; m != NULL; m = m->m_next) {
			while ((mlen = m->m_len)) {
				if (mlen + tlen > pdulen)
					mlen = pdulen - tlen;	/* how much of this
								 * buffer can we use? */
				mptr = m->m_data;
				tlen += mlen;	/* length of this PDU */
				m->m_len -= mlen;	/* bytes remaining in
							 * mbuf */
				m->m_data += mlen;	/* new data pointer */

				val1 = val;
				if (tlen > pdulen + pad - 48)	/* is this buffer in the
								 * last cell? */
					val1 |= 0x40000000;	/* last buffer in PDU */

				if (tlen == pdulen) {	/* end of PDU, so figure
							 * padding needed */
					idt->stats_opdus++;	/* 12/15/2000 */
					idt->stats_obytes += pdulen;	/* 12/15/2000 */
					connection->vccinf->vc_opdus++;
					connection->vccinf->vc_obytes += pdulen;
					tlen = 0;
					if (pad <= 8)
						mlen += pad;	/* just "add" padding to
								 * this buffer */
				}
				*txqueue->scq_next++ = val1 | mlen;
				*txqueue->scq_next++ = vtophys(mptr);
				*txqueue->scq_next++ = val3;
				*txqueue->scq_next++ = val4;
				if (txqueue->scq_next - txqueue->scq_base >= txqueue->scq_len * 4)
					txqueue->scq_next = txqueue->scq_base;
				scq_space--;
				top->m_pkthdr.csum_data++;	/* 12/22/2000 */

				/*
				 * if we need more than 8 bytes of padding,
				 * use the zero-filled buffer defined above.
				 */
				if (tlen == 0 && pad > 8) {	/* end of PDU, do we
								 * need padding? */
					val1 |= 0x40000000;	/* last buffer in PDU */
					*txqueue->scq_next++ = val1 | pad;
					*txqueue->scq_next++ = vtophys(padding);
					*txqueue->scq_next++ = val3;
					*txqueue->scq_next++ = val4;
					if (txqueue->scq_next - txqueue->scq_base >= txqueue->scq_len * 4)
						txqueue->scq_next = txqueue->scq_base;
					scq_space--;
					top->m_pkthdr.csum_data++;	/* 12/22/2000 */
				}
			}
		}

		/*
		 * Now that we have set up the descriptors, add the entry
		 * for Transmit Status Request so we know when the PDU(s)
		 * are done.
		 */

		*txqueue->scq_next++ = 0xa0000000;	/* TSR with interrupt */
		*txqueue->scq_next++ = (u_long)top;
		*txqueue->scq_next++ = 0;
		*txqueue->scq_next++ = 0;

		if (txqueue->scq_next - txqueue->scq_base >= txqueue->scq_len * 4)
			txqueue->scq_next = txqueue->scq_base;
		scq_space--;
		top->m_pkthdr.csum_data++;	/* 12/22/2000 */
		count++;

		txqueue->scq_cur += top->m_pkthdr.csum_data;
	}

	/*
	 * 05/31/2001: Optimization: Since writing to SRAM is very
	 * expensive, we will only do this when the pointer is stale (half
	 * of the queue). If the queue is less than 1/4 full, then write the
	 * pointer anyway.
	 */

	if (idt_sysctl_qptrhold) {
		scq_space = txqueue->scq_next - txqueue->scq_last;	/* number pending */
		scq_space /= 4;
		if (scq_space < 0)
			scq_space += txqueue->scq_len;
		if (scq_space * 2 < txqueue->scq_len &&	/* less than half
							 * pending */
		    txqueue->scq_cur > txqueue->scq_len / 4)	/* and queue is active */
			count = 0;
	}
	if (count) {		/* we need to update the queue pointer */
		nicstar_sram_wr(idt, txqueue->scd, 1, vtophys(txqueue->scq_next), 0, 0, 0);
		txqueue->scq_last = txqueue->scq_next;
	}
	splx(s);

	return;
}

/*  Once a packet has been put in the Segmentation Channel Queue, it will
 *  be sent, and then the mbuf will harvested by idt_intr_tsq().  While it
 *  is in the SCQ, m_pkthdr.header is the pointer to the TX queue.  This is
 *  important because if the connection is closed while there are still
 *  mbufs in the SCQ, idt_intr_tsq() still needs to update the TX queue.
 *
 ******************************************************************************
 *
 *  Handle entries in Transmit Status Queue (end of PDU interrupt or TSQ full)
 *
 *  in:  IDT device
 *
 *  Date first: 12/04/2000  last: 06/10/2001
 */
static void
idt_intr_tsq(IDT * idt)
{
	CONNECTION *connection;
	TX_QUEUE *txqueue;
	u_long *tsq_ptr;
	u_long val;
	struct mbuf *m;
	int count, s;

	s = splimp();

	tsq_ptr = idt->tsq_head;

	count = 0;
	while ((tsq_ptr[1] & 0x80000000) == 0) {
		m = (struct mbuf *) tsq_ptr[0];
		if (m != NULL) {/* first test for timer rollover entry */
			if (((int)m & 0x000000ff))	/* now do sanity check
							 * on the mbuf ptr */
				device_printf(idt->dev,
					"DANGER! bad mbuf (%x), stamp=%x\n",
					(int)m, (int)tsq_ptr[1]);
			else {
				connection = (CONNECTION *) m->m_pkthdr.rcvif;
				txqueue = (TX_QUEUE *) m->m_pkthdr.header;
				txqueue->scq_cur -= m->m_pkthdr.csum_data;
				if (txqueue->scq_cur < 0 || txqueue->scq_cur > txqueue->scq_len)
					device_printf(idt->dev, "DANGER! scq_cur is %d\n", txqueue->scq_len);
				m->m_pkthdr.header = NULL;
				m_freem(m);
				idt_transmit_top(idt, txqueue);	/* move more into queue */
			}
		}
		tsq_ptr[0] = 0;
		tsq_ptr[1] = 0x80000000;	/* reset TSQ entry */
		tsq_ptr += 2;
		if (tsq_ptr >= idt->tsq_base + idt->tsq_size * 2)
			tsq_ptr = idt->tsq_base;
		count++;
	}
	idt->tsq_head = tsq_ptr;

	if (count) {
		val = (int)tsq_ptr - (int)idt->tsq_base;
		val -= 8;	/* always stay one behind */
		val &= 0x001ff8;
		*idt->reg_tsqh = val;
	}
	splx(s);
}

/*  There is a problem with the pointer rollover where the SAR will think the
 *  TSQ buffer is full (forever?) unless we hold the head pointer back.
 *  This is not mentioned in the 77211 docs, but is a resolved issue in
 *  revision D of the 77252 chips (see 77252 errata).
 *
 *  If a connection is closed while there are still mbufs in the TX queue,
 *  the connection TX queue pointer will be NULL.  That is why we have a
 *  special copy of the pointer in m_pkthdr.header.  Also, idt_transmit_top()
 *  will allow the TX queue for that connection to empty properly.
 *
 *  It is possible for a TSQ entry to be 0x00ffffff/0x00ffffff, which is
 *  obviously not an mbuf and not a timer rollover entry.  We now have an
 *  mbuf sanity check for this.
 *
 ******************************************************************************
 *
 *    nicstar_itrx ( card )
 *
 *    service error in transmitting PDU interrupt.
 *
*/
static void
nicstar_itrx(nicstar_reg_t * idt)
{
	/* trace mbuf and release */
}

/******************************************************************************
 *
 *  Raw cell receive interrupt
 *
 *    service raw cell reception interrupt.
 *
 */

static void
nicstar_rawc(nicstar_reg_t * idt)
{
	u_long ptr_tail;
	struct mbuf *qmbuf;
	u_long *qptr;
	u_long next_mbuf;
	u_long next_phys;

	if (idt->raw_headm == NULL ||
	    idt->raw_headp == 0) {
		device_printf(idt->dev,
			"RAW cell received, buffers not ready (%x/%x).\n",
			(int)idt->raw_headm, (int)idt->raw_headp);
		return;
	}
	ptr_tail = *(volatile u_long *)(idt->virt_baseaddr + REGRAWT);
	if ((ptr_tail & 0xfffff800) == idt->raw_headp)
		return;		/* still in the same large buffer */

	if ((ptr_tail & 0x7ff) < 64)	/* wait until something in new buffer */
		return;

	qmbuf = idt->raw_headm;
	qptr = (u_long *)qmbuf->m_data;

	next_mbuf = qptr[31 * 16 + 1];	/* next handle (virtual) */
	next_phys = qptr[31 * 16 + 0];	/* next physical address */

	/* if we want to do anything with the raw data, this is the place  */

	idt_mcheck_rem(idt, qmbuf);
	m_free(qmbuf);

	idt->raw_headm = (struct mbuf *) next_mbuf;
	idt->raw_headp = next_phys;
}

/*****************************************************************************
 *
 *  Handle AAL5 PDU length
 *
 *  in:  IDT device, first mbuf in chain, last mbuf
 * out:  zero = success, nz = failure (mbuf chain freed)
 *
 * Date first: 11/18/2000  last: 12/14/2000
 */

int
idt_receive_aal5(IDT * idt, struct mbuf * mfirst, struct mbuf * mdata)
{
	struct mbuf *m2;
	unsigned char *aal5len;
	int plen;
	int diff;

	aal5len = mdata->m_data + mdata->m_len - 6;	/* aal5 length = 16 bits */
	plen = aal5len[0] * 256 + aal5len[1];
	diff = mfirst->m_pkthdr.len - plen;	/* number of bytes to trim */

	if (diff == 0)
		return (0);

	if (diff < 0) {
		device_printf(idt->dev,
			"AAL5 PDU length (%d) greater than cells (%d), discarding\n",
			plen, mfirst->m_pkthdr.len);
		m_freem(mfirst);
		return (1);
	}
	while (mdata->m_len < diff) {	/* last mbuf not big enough */
		diff -= mdata->m_len;
		m2 = mdata;
		m_free(mdata);
		if (mdata == mfirst) {	/* we just tossed the whole PDU */
			device_printf(idt->dev, "AAL5 PDU length failed, discarding.\n");
			return (1);	/* the packetheadr length was bad! */
		}
		for (mdata = mfirst; mdata->m_next != m2; mdata = mdata->m_next);
		mdata->m_next = NULL;	/* remove old link to free'd mbuf */
	}
	mdata->m_len -= diff;	/* trim last mbuf */
	mfirst->m_pkthdr.len = plen;

	return (0);
}

/* 12/14/2000: Removed "pruning" log message.
 *
 *****************************************************************************
 *
 *    nicstar_recv ( card )
 *
 *    rebuilds PDUs from entries in the Recieve Status Queue.
 *
 */
struct rsq_entry {
	u_long vpivci;
	struct mbuf *mdata;
	u_long crc;
	u_long flags;
};

static void
nicstar_recv(nicstar_reg_t * idt)
{
	CONNECTION *connection;
	volatile u_long *regh = (volatile u_long *)(idt->virt_baseaddr + REGRSQH);
	struct rsq_entry *rsq;
	struct mbuf *mdata, *mptr;
	u_long flags;
	u_long crc;
	int vpi;
	int vci;
	int clen;
	int x, s;

	s = splimp();

	rsq = (struct rsq_entry *) (idt->fixbuf + 0x2000 + (idt->rsqh & 0x1ffc));

	if ((rsq->flags & 0x80000000) == 0) {
		splx(s);
		return;
	}
	while (rsq->flags & 0x80000000) {
		vpi = rsq->vpivci >> 16;	/* first, grab the RSQ data */
		vci = rsq->vpivci & 0xffff;
		mdata = rsq->mdata;
		crc = rsq->crc;
		flags = rsq->flags;
		clen = (flags & 0x1ff) * 48;

		rsq->vpivci = 0;/* now recycle the RSQ entry */
		rsq->mdata = NULL;
		rsq->crc = 0;
		rsq->flags = 0;	/* turn off valid bit */
		rsq++;
		if (rsq == (struct rsq_entry *) (idt->fixbuf + 0x4000))
			rsq = (struct rsq_entry *) (idt->fixbuf + 0x2000);

		idt_mcheck_rem(idt, mdata);

		connection = idt_connect_find(idt, vpi, vci);
		if (connection == NULL) {	/* we don't want this PDU */
			printf("nicstar_recv: No connection %d/%d - discarding packet.\n",
			    vpi, vci);
			m_free(mdata);	/* throw mbuf away */
			continue;
		}
		mdata->m_len = clen;

		mptr = connection->recv;
		if (mptr == NULL) {
			if (mdata->m_flags & M_PKTHDR)
				connection->recv = mdata;
			else {
				idt->stats_ierrors++;	/* 12/15/2000 */
				connection->vccinf->vc_ierrors++;
				m_free(mdata);
				continue;
			}
		} else {
			x = 0;
			while (mptr->m_next != NULL) {	/* find last mbuf in
							 * chain */
				mptr = mptr->m_next;
				x++;
				if (x > 25)
					break;
			}
			if (x > 25) {
				mptr = connection->recv;
				printf("nicstar_recv: invalid mbuf chain - probable corruption!\n");
				m_free(mdata);
				idt->stats_ierrors++;	/* 12/15/2000 */
				connection->vccinf->vc_ierrors++;
				connection->recv = NULL;
				connection->rlen = 0;
				continue;
			}
			mptr->m_next = mdata;
		}
		connection->rlen += clen;

		if (flags & 0x2000) {	/* end of PDU */
			mptr = connection->recv;	/* one or more mbufs
							 * will be here */
			clen = connection->rlen;	/* length based on cell
							 * count */
			connection->recv = NULL;
			connection->rlen = 0;

			mptr->m_pkthdr.len = clen;
			mptr->m_pkthdr.rcvif = NULL;
			mptr->m_nextpkt = NULL;

			if (mptr->m_pkthdr.csum_flags) {
				device_printf(idt->dev,
					"received pkthdr.csum_flags=%x\n",
					mptr->m_pkthdr.csum_flags);
				mptr->m_pkthdr.csum_flags = 0;
			}
			if (flags & 0x200 &&	/* bad CRC */
			    idt->flg_igcrc == 0) {
				printf("nicstar_recv: Bad CRC - discarding PDU: %d/%d\n", vpi, vci);
				idt->stats_ierrors++;	/* 12/15/2000 */
				connection->vccinf->vc_ierrors++;
				m_freem(mptr);
				continue;
			}
			if (connection->aal == ATM_AAL5) {
				if (idt_receive_aal5(idt, mptr, mdata))	/* adjust for AAL5
									 * length */
					continue;
			}
			idt->stats_ipdus++;	/* 12/15/2000 */
			idt->stats_ibytes += mptr->m_pkthdr.len;	/* 12/15/2000 */
			connection->vccinf->vc_ipdus++;
			connection->vccinf->vc_ibytes += mptr->m_pkthdr.len;
			idt_receive(idt, mptr, vpi, vci);
		} else if (connection->rlen > connection->maxpdu) {	/* this packet is insane */
			printf("nicstar_recv: Bad packet, len=%d - discarding.\n",
			    connection->rlen);
			connection->recv = NULL;
			connection->rlen = 0;
			idt->stats_ierrors++;	/* 12/15/2000 */
			connection->vccinf->vc_ierrors++;
			m_freem(mptr);
		}		/* end of PDU */
	}

	idt->rsqh = vtophys((u_long)rsq) & 0x1ffc;
	*regh = (idt->rsqh - sizeof(struct rsq_entry)) & 0x1ff0;

	splx(s);
}

/******************************************************************************
 *
 *  Physical Interrupt handler
 *
 *  service phyical interrupt.
 *
 */

static void
nicstar_phys(nicstar_reg_t * idt)
{
	u_long t;

	if (idt->flg_le25) {
		nicstar_util_rd(idt, 0x01, &t);	/* get interrupt cause */
		if (t & 0x01) {
			nicstar_util_wr(idt, 1, 0x02, 0x10);	/* reset rx fifo */
			device_printf(idt->dev, "PHY cleared.\n");
		}
	} else
		device_printf(idt->dev, "Physical interrupt.\n");
}

/******************************************************************************
 *
 *  Status register values
 */

#define STAT_REG_RSQAF   0x0002	/* receive status queue almost full */
#define STAT_REG_LBMT    0x0004	/* large buffer queue empty */
#define STAT_REG_SBMT    0x0008	/* small buffer queue empty */
#define STAT_REG_RAWC    0x0010	/* raw cell interrupt */
#define STAT_REG_EPDU    0x0020	/* end of PDU interrupt */
#define STAT_REG_PHY     0x0400	/* physical interrupt */
#define STAT_REG_TIME    0x0800	/* timer overflow interrupt */
#define STAT_REG_TSQAF   0x1000	/* transmit status queue almost full */
#define STAT_REG_TXIN    0x4000	/* TX PDU incomplete */
#define STAT_REG_TXOK    0x8000	/* TX status indicator */

/******************************************************************************
 *
 *  Interrupt handler
 *
 *    service card interrupt.
 *
 *    nicstar_intr ( card )
 */

void
nicstar_intr(void *arg)
{
	IDT *idt;
	volatile u_long stat_val, config_val;
	int int_flags;
	volatile int i;
	int s;

	idt = (IDT *) arg;

	i = 0;

	s = splnet();

	config_val = *idt->reg_cfg;
	stat_val = *idt->reg_stat;

	int_flags =
	    STAT_REG_TSQAF |	/* transmit status queue almost full */
	    STAT_REG_RSQAF |	/* receive status queue almost full */
	    STAT_REG_RAWC |	/* raw cell interrupt */
	    STAT_REG_EPDU |	/* end of PDU interrupt */
	    STAT_REG_TIME |	/* timer overflow interrupt */
	    STAT_REG_TXIN |	/* TX PDU incomplete */
	    STAT_REG_TXOK;	/* TX status indicator */

	if (idt->flg_le25)
		int_flags |= STAT_REG_PHY;	/* include flag for physical
						 * interrupt */

	if (stat_val & (STAT_REG_LBMT | STAT_REG_SBMT)) {	/* buffer queue(s) empty */
		if (stat_val & STAT_REG_SBMT)
			device_printf(idt->dev, "small free buffer queue empty.\n");
		if (stat_val & STAT_REG_LBMT)
			device_printf(idt->dev, "large free buffer queue empty.\n");
		nicstar_ld_rcv_buf(idt);

		if (*idt->reg_stat & STAT_REG_LBMT) {	/* still empty, so
							 * disable IRQ */
			config_val &= ~0x01000000;
			*idt->reg_cfg = config_val;
		}
	}
	/* loop until no more interrupts to service */

	while (stat_val & int_flags) {
		i++;
		if (i < 0 || i > 100)
			break;

		*idt->reg_stat = stat_val & int_flags;	/* clear status bits */

		if (stat_val & STAT_REG_EPDU) {	/* receive PDU */
			nicstar_recv(idt);
			nicstar_ld_rcv_buf(idt);	/* replace buffers,
							 * moved here 11/14/2000 */
		}
		if (stat_val & STAT_REG_RAWC) {	/* raw cell */
			nicstar_rawc(idt);
		}
		if (stat_val & STAT_REG_TXOK) {	/* transmit complete */
			idt_intr_tsq(idt);
		}
		if (stat_val & STAT_REG_TXIN) {	/* bad transmit */
			nicstar_itrx(idt);
			device_printf(idt->dev, "Bad transmit.\n");
		}
		if (stat_val & STAT_REG_TIME) {	/* timer wrap */
			idt->timer_wrap++;
			idt_intr_tsq(idt);	/* check the TSQ */
			nicstar_recv(idt);	/* check the receive queue */
			if (idt_sysctl_logbufs)
				idt_status_bufs(idt);	/* show the buffer
							 * status */
		}
		if (stat_val & STAT_REG_PHY) {	/* physical interrupt */
			nicstar_phys(idt);
			*idt->reg_stat = STAT_REG_PHY;	/* clear the int flag */
		}
		if (stat_val & STAT_REG_RSQAF) {	/* RSQ almost full */
			nicstar_recv(idt);
			device_printf(idt->dev, "warning, RSQ almost full.\n");
			if (*idt->reg_stat & STAT_REG_RSQAF) {	/* RSQ full */
				printf("RSQ is full, disabling interrupt.\n");
				config_val &= 0x00000800;
				*idt->reg_cfg = config_val;
			}
		}
		if (stat_val & STAT_REG_TSQAF) {	/* TSQ almost full */
			idt_intr_tsq(idt);
			device_printf(idt->dev, "warning, TSQ almost full.\n");
			if (*idt->reg_stat & STAT_REG_TSQAF) {
				printf("TSQ is full, disabling interrupt.\n");
				config_val &= ~0x00000002;
				*idt->reg_cfg = config_val;
			}
		}
		stat_val = *idt->reg_stat;
	}

	splx(s);
	if (i < 1 || i > 50)
		device_printf(idt->dev, "i=%3d, status=%08x\n", i, (int)stat_val);
}
