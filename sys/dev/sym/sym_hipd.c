/*
 *  Device driver optimized for the Symbios/LSI 53C896/53C895A/53C1010 
 *  PCI-SCSI controllers.
 *
 *  Copyright (C) 1999-2000  Gerard Roudier <groudier@club-internet.fr>
 *
 *  This driver also supports the following Symbios/LSI PCI-SCSI chips:
 *	53C810A, 53C825A, 53C860, 53C875, 53C876, 53C885, 53C895.
 *
 *  but does not support earlier chips as the following ones:
 *	53C810, 53C815, 53C825.
 *  
 *  This driver for FreeBSD-CAM is derived from the Linux sym53c8xx driver.
 *  Copyright (C) 1998-1999  Gerard Roudier
 *
 *  The sym53c8xx driver is derived from the ncr53c8xx driver that had been 
 *  a port of the FreeBSD ncr driver to Linux-1.2.13.
 *
 *  The original ncr driver has been written for 386bsd and FreeBSD by
 *          Wolfgang Stanglmeier        <wolf@cologne.de>
 *          Stefan Esser                <se@mi.Uni-Koeln.de>
 *  Copyright (C) 1994  Wolfgang Stanglmeier
 *
 *  The initialisation code, and part of the code that addresses 
 *  FreeBSD-CAM services is based on the aic7xxx driver for FreeBSD-CAM 
 *  written by Justin T. Gibbs.
 *
 *  Other major contributions:
 *
 *  NVRAM detection and reading.
 *  Copyright (C) 1997 Richard Waltham <dormouse@farsrobt.demon.co.uk>
 *
 *-----------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD: src/sys/dev/sym/sym_hipd.c,v 1.6 2000/02/13 12:14:07 groudier Exp $ */

#define SYM_DRIVER_NAME	"sym-1.3.2-20000206"

/* #define	SYM_DEBUG_PM_WITH_WSR (current debugging) */

#include <pci.h>
#include <stddef.h>	/* For offsetof */

#include <sys/param.h>
/*
 *  Only use the BUS stuff for PCI under FreeBSD 4 and later versions.
 *  Note that the old BUS stuff also works for FreeBSD 4 and spares 
 *  about 1.5KB for the driver objet file.
 */
#if 	__FreeBSD_version >= 400000
#define	FreeBSD_4_Bus
#endif

#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#ifdef FreeBSD_4_Bus
#include <sys/module.h>
#include <sys/bus.h>
#endif

#include <sys/buf.h>
#include <sys/proc.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#ifdef FreeBSD_4_Bus
#include <machine/resource.h>
#include <sys/rman.h>
#endif
#include <machine/clock.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#if 0
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <vm/vm_extern.h>
#endif

/* Short and quite clear integer types */
typedef int8_t    s8;
typedef int16_t   s16;
typedef	int32_t   s32;
typedef u_int8_t  u8;
typedef u_int16_t u16;
typedef	u_int32_t u32;

/* Driver configuration and definitions */
#if 1
#include "opt_sym.h"
#include <dev/sym/sym_conf.h>
#include <dev/sym/sym_defs.h>
#else
#include "ncr.h"	/* To know if the ncr has been configured */
#include <pci/sym_conf.h>
#include <pci/sym_defs.h>
#endif

/*
 *  On x86 architecture, write buffers management does not 
 *  reorder writes to memory. So, preventing compiler from  
 *  optimizing the code is enough to guarantee some ordering 
 *  when the CPU is writing data accessed by the PCI chip.
 *  On Alpha architecture, explicit barriers are to be used.
 *  By the way, the *BSD semantic associates the barrier 
 *  with some window on the BUS and the corresponding verbs 
 *  are for now unused. What a strangeness. The driver must 
 *  ensure that accesses from the CPU to the start and done 
 *  queues are not reordered by either the compiler or the 
 *  CPU and uses 'volatile' for this purpose.
 */

#ifdef	__alpha__
#define MEMORY_BARRIER()	alpha_mb()
#else /*__i386__*/
#define MEMORY_BARRIER()	do { ; } while(0)
#endif

/*
 *  A la VMS/CAM-3 queue management.
 */

typedef struct sym_quehead {
	struct sym_quehead *flink;	/* Forward  pointer */
	struct sym_quehead *blink;	/* Backward pointer */
} SYM_QUEHEAD;

#define sym_que_init(ptr) do { \
	(ptr)->flink = (ptr); (ptr)->blink = (ptr); \
} while (0)

static __inline struct sym_quehead *sym_que_first(struct sym_quehead *head)
{
	return (head->flink == head) ? 0 : head->flink;
}

static __inline struct sym_quehead *sym_que_last(struct sym_quehead *head)
{
	return (head->blink == head) ? 0 : head->blink;
}

static __inline void __sym_que_add(struct sym_quehead * new,
	struct sym_quehead * blink,
	struct sym_quehead * flink)
{
	flink->blink	= new;
	new->flink	= flink;
	new->blink	= blink;
	blink->flink	= new;
}

static __inline void __sym_que_del(struct sym_quehead * blink,
	struct sym_quehead * flink)
{
	flink->blink = blink;
	blink->flink = flink;
}

static __inline int sym_que_empty(struct sym_quehead *head)
{
	return head->flink == head;
}

static __inline void sym_que_splice(struct sym_quehead *list,
	struct sym_quehead *head)
{
	struct sym_quehead *first = list->flink;

	if (first != list) {
		struct sym_quehead *last = list->blink;
		struct sym_quehead *at   = head->flink;

		first->blink = head;
		head->flink  = first;

		last->flink = at;
		at->blink   = last;
	}
}

#define sym_que_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


#define sym_insque(new, pos)		__sym_que_add(new, pos, (pos)->flink)

#define sym_remque(el)			__sym_que_del((el)->blink, (el)->flink)

#define sym_insque_head(new, head)	__sym_que_add(new, head, (head)->flink)

static __inline struct sym_quehead *sym_remque_head(struct sym_quehead *head)
{
	struct sym_quehead *elem = head->flink;

	if (elem != head)
		__sym_que_del(head, elem->flink);
	else
		elem = 0;
	return elem;
}

#define sym_insque_tail(new, head)	__sym_que_add(new, (head)->blink, head)

static __inline struct sym_quehead *sym_remque_tail(struct sym_quehead *head)
{
	struct sym_quehead *elem = head->blink;

	if (elem != head)
		__sym_que_del(elem->blink, head);
	else
		elem = 0;
	return elem;
}

/*
 *  This one may be usefull.
 */
#define FOR_EACH_QUEUED_ELEMENT(head, qp) \
	for (qp = (head)->flink; qp != (head); qp = qp->flink)
/*
 *  FreeBSD does not offer our kind of queue in the CAM CCB.
 *  So, we have to cast.
 */
#define sym_qptr(p)	((struct sym_quehead *) (p))

/*
 *  Simple bitmap operations.
 */ 
#define sym_set_bit(p, n)	(((u32 *)(p))[(n)>>5] |=  (1<<((n)&0x1f)))
#define sym_clr_bit(p, n)	(((u32 *)(p))[(n)>>5] &= ~(1<<((n)&0x1f)))
#define sym_is_bit(p, n)	(((u32 *)(p))[(n)>>5] &   (1<<((n)&0x1f)))

/*
 *  Number of tasks per device we want to handle.
 */
#if	SYM_CONF_MAX_TAG_ORDER > 8
#error	"more than 256 tags per logical unit not allowed."
#endif
#define	SYM_CONF_MAX_TASK	(1<<SYM_CONF_MAX_TAG_ORDER)

/*
 *  Donnot use more tasks that we can handle.
 */
#ifndef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif
#if	SYM_CONF_MAX_TAG > SYM_CONF_MAX_TASK
#undef	SYM_CONF_MAX_TAG
#define	SYM_CONF_MAX_TAG	SYM_CONF_MAX_TASK
#endif

/*
 *    This one means 'NO TAG for this job'
 */
#define NO_TAG	(256)

/*
 *  Number of SCSI targets.
 */
#if	SYM_CONF_MAX_TARGET > 16
#error	"more than 16 targets not allowed."
#endif

/*
 *  Number of logical units per target.
 */
#if	SYM_CONF_MAX_LUN > 64
#error	"more than 64 logical units per target not allowed."
#endif

/*
 *    Asynchronous pre-scaler (ns). Shall be 40 for 
 *    the SCSI timings to be compliant.
 */
#define	SYM_CONF_MIN_ASYNC (40)

/*
 *  Number of entries in the START and DONE queues.
 *
 *  We limit to 1 PAGE in order to succeed allocation of 
 *  these queues. Each entry is 8 bytes long (2 DWORDS).
 */
#ifdef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_QUEUE (SYM_CONF_MAX_START+2)
#else
#define	SYM_CONF_MAX_QUEUE (7*SYM_CONF_MAX_TASK+2)
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

#if	SYM_CONF_MAX_QUEUE > PAGE_SIZE/8
#undef	SYM_CONF_MAX_QUEUE
#define	SYM_CONF_MAX_QUEUE   PAGE_SIZE/8
#undef	SYM_CONF_MAX_START
#define	SYM_CONF_MAX_START (SYM_CONF_MAX_QUEUE-2)
#endif

/*
 *  For this one, we want a short name :-)
 */
#define MAX_QUEUE	SYM_CONF_MAX_QUEUE

/*
 *  This one should have been already defined.
 */
#ifndef offsetof
#define offsetof(t, m)	((size_t) (&((t *)0)->m))
#endif

/*
 *  Active debugging tags and verbosity.
 */
#define DEBUG_ALLOC	(0x0001)
#define DEBUG_PHASE	(0x0002)
#define DEBUG_POLL	(0x0004)
#define DEBUG_QUEUE	(0x0008)
#define DEBUG_RESULT	(0x0010)
#define DEBUG_SCATTER	(0x0020)
#define DEBUG_SCRIPT	(0x0040)
#define DEBUG_TINY	(0x0080)
#define DEBUG_TIMING	(0x0100)
#define DEBUG_NEGO	(0x0200)
#define DEBUG_TAGS	(0x0400)
#define DEBUG_POINTER	(0x0800)

#if 0
static int sym_debug = 0;
	#define DEBUG_FLAGS sym_debug
#else
/*	#define DEBUG_FLAGS (0x0631) */
	#define DEBUG_FLAGS (0x0000)
#endif
#define sym_verbose	(np->verbose)

/*
 *  Virtual to bus address translation.
 */
#ifdef	__alpha__
#define	vtobus(p)	alpha_XXX_dmamap((vm_offset_t)(p))
#else /*__i386__*/
#define vtobus(p)	vtophys(p)
#endif

/*
 *  Copy from main memory to PCI memory space.
 */
#ifdef	__alpha__
#define memcpy_to_pci(d, s, n)	memcpy_toio((u32)(d), (void *)(s), (n))
#else /*__i386__*/
#define memcpy_to_pci(d, s, n)	bcopy((s), (void *)(d), (n))
#endif

/*
 *  Insert a delay in micro-seconds and milli-seconds.
 */
static void UDELAY(long us) { DELAY(us); }
static void MDELAY(long ms) { while (ms--) UDELAY(1000); }

/*
 *  Memory allocation/allocator.
 *  We assume allocations are naturally aligned and if it is 
 *  not guaranteed, we may use our internal allocator.
 */
#ifdef	SYM_CONF_USE_INTERNAL_ALLOCATOR
/*
 *  Simple power of two buddy-like allocator.
 *
 *  This simple code is not intended to be fast, but to 
 *  provide power of 2 aligned memory allocations.
 *  Since the SCRIPTS processor only supplies 8 bit arithmetic, 
 *  this allocator allows simple and fast address calculations  
 *  from the SCRIPTS code. In addition, cache line alignment 
 *  is guaranteed for power of 2 cache line size.
 *
 *  This allocator has been developped for the Linux sym53c8xx  
 *  driver, since this O/S does not provide naturally aligned 
 *  allocations.
 *  It has the vertue to allow the driver to use private pages 
 *  of memory that will be useful if we ever need to deal with 
 *  IO MMU for PCI.
 */

#define MEMO_SHIFT	4	/* 16 bytes minimum memory chunk */
#define MEMO_PAGE_ORDER	0	/* 1 PAGE maximum (for now (ever?) */
typedef unsigned long addr;	/* Enough bits to bit-hack addresses */

#if 0
#define MEMO_FREE_UNUSED	/* Free unused pages immediately */
#endif

struct m_link {
	struct m_link *next;	/* Simple links are enough */
};

#ifndef M_DMA_32BIT
#define M_DMA_32BIT	0	/* Will this flag ever exist */
#endif

#define get_pages() \
	malloc(PAGE_SIZE<<MEMO_PAGE_ORDER, M_DEVBUF, M_NOWAIT)
#define free_pages(p) \
	free((p), M_DEVBUF)

/*
 *  Lists of available memory chunks.
 *  Starts with 16 bytes chunks until 1 PAGE chunks.
 */
static struct m_link h[PAGE_SHIFT-MEMO_SHIFT+MEMO_PAGE_ORDER+1];

/*
 *  Allocate a memory area aligned on the lowest power of 2 
 *  greater than the requested size.
 */
static void *__sym_malloc(int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	int j;
	addr a ;

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return 0;

	while (size > s) {
		s <<= 1;
		++i;
	}

	j = i;
	while (!h[j].next) {
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			h[j].next = (struct m_link *)get_pages();
			if (h[j].next)
				h[j].next->next = 0;
			break;
		}
		++j;
		s <<= 1;
	}
	a = (addr) h[j].next;
	if (a) {
		h[j].next = h[j].next->next;
		while (j > i) {
			j -= 1;
			s >>= 1;
			h[j].next = (struct m_link *) (a+s);
			h[j].next->next = 0;
		}
	}
#ifdef DEBUG
	printf("__sym_malloc(%d) = %p\n", size, (void *) a);
#endif
	return (void *) a;
}

/*
 *  Free a memory area allocated using sym_malloc().
 *  Coalesce buddies.
 *  Free pages that become unused if MEMO_FREE_UNUSED is 
 *  defined.
 */
static void __sym_mfree(void *ptr, int size)
{
	int i = 0;
	int s = (1 << MEMO_SHIFT);
	struct m_link *q;
	addr a, b;

#ifdef DEBUG
	printf("sym_mfree(%p, %d)\n", ptr, size);
#endif

	if (size > (PAGE_SIZE << MEMO_PAGE_ORDER))
		return;

	while (size > s) {
		s <<= 1;
		++i;
	}

	a = (addr) ptr;

	while (1) {
#ifdef MEMO_FREE_UNUSED
		if (s == (PAGE_SIZE << MEMO_PAGE_ORDER)) {
			free_pages(a);
			break;
		}
#endif
		b = a ^ s;
		q = &h[i];
		while (q->next && q->next != (struct m_link *) b) {
			q = q->next;
		}
		if (!q->next) {
			((struct m_link *) a)->next = h[i].next;
			h[i].next = (struct m_link *) a;
			break;
		}
		q->next = q->next->next;
		a = a & b;
		s <<= 1;
		++i;
	}
}

#else	/* !defined SYSCONF_USE_INTERNAL_ALLOCATOR */

/*
 *  Using directly the system memory allocator.
 */

#define	__sym_mfree(ptr, size)		free((ptr), M_DEVBUF)
#define	__sym_malloc(size)		malloc((size), M_DEVBUF, M_NOWAIT)

#endif	/* SYM_CONF_USE_INTERNAL_ALLOCATOR */

#define MEMO_WARN	1

static void *sym_calloc2(int size, char *name, int uflags)
{
	void *p;

	p = __sym_malloc(size);

	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printf ("new %-10s[%4d] @%p.\n", name, size, p);

	if (p)
		bzero(p, size);
	else if (uflags & MEMO_WARN)
		printf ("sym_calloc: failed to allocate %s[%d]\n", name, size);

	return p;
}

#define sym_calloc(s, n)	sym_calloc2(s, n, MEMO_WARN)

static void sym_mfree(void *ptr, int size, char *name)
{
	if (DEBUG_FLAGS & DEBUG_ALLOC)
		printf ("freeing %-10s[%4d] @%p.\n", name, size, ptr);

	__sym_mfree(ptr, size);
}

/*
 *  Print a buffer in hexadecimal format.
 */
static void sym_printb_hex (u_char *p, int n)
{
	while (n-- > 0)
		printf (" %x", *p++);
}

/*
 *  Same with a label at beginning and .\n at end.
 */
static void sym_printl_hex (char *label, u_char *p, int n)
{
	printf ("%s", label);
	sym_printb_hex (p, n);
	printf (".\n");
}

/*
 *  Return a string for SCSI BUS mode.
 */
static char *sym_scsi_bus_mode(int mode)
{
	switch(mode) {
	case SMODE_HVD:	return "HVD";
	case SMODE_SE:	return "SE";
	case SMODE_LVD: return "LVD";
	}
	return "??";
}

/*
 *  Some poor sync table that refers to Tekram NVRAM layout.
 */
#ifdef SYM_CONF_NVRAM_SUPPORT
static u_char Tekram_sync[16] =
	{25,31,37,43, 50,62,75,125, 12,15,18,21, 6,7,9,10};
#endif

/*
 *  Union of supported NVRAM formats.
 */
struct sym_nvram {
	int type;
#define	SYM_SYMBIOS_NVRAM	(1)
#define	SYM_TEKRAM_NVRAM	(2)
#ifdef	SYM_CONF_NVRAM_SUPPORT
	union {
		Symbios_nvram Symbios;
		Tekram_nvram Tekram;
	} data;
#endif
};

/*
 *  This one is hopefully useless, but actually useful. :-)
 */
#ifndef assert
#define	assert(expression) { \
	if (!(expression)) { \
		(void)panic( \
			"assertion \"%s\" failed: file \"%s\", line %d\n", \
			#expression, \
			__FILE__, __LINE__); \
	} \
}
#endif

/*
 *  Some provision for a possible big endian support.
 *  By the way some Symbios chips also may support some kind 
 *  of big endian byte ordering.
 *  For now, this stuff does not deserve any comments. :)
 */

#define sym_offb(o)	(o)
#define sym_offw(o)	(o)

#define cpu_to_scr(dw)	(dw)
#define scr_to_cpu(dw)	(dw)

/*
 *  Access to the controller chip.
 *
 *  If SYM_CONF_IOMAPPED is defined, the driver will use 
 *  normal IOs instead of the MEMORY MAPPED IO method  
 *  recommended by PCI specifications.
 */

/*
 *  Define some understable verbs so we will not suffer of 
 *  having to deal with the stupid PC tokens for IO.
 */
#define io_read8(p)	 scr_to_cpu(inb((p)))
#define	io_read16(p)	 scr_to_cpu(inw((p)))
#define io_read32(p)	 scr_to_cpu(inl((p)))
#define	io_write8(p, v)	 outb((p), cpu_to_scr(v))
#define io_write16(p, v) outw((p), cpu_to_scr(v))
#define io_write32(p, v) outl((p), cpu_to_scr(v))

#ifdef	__alpha__

#define mmio_read8(a)	     readb(a)
#define mmio_read16(a)	     readw(a)
#define mmio_read32(a)	     readl(a)
#define mmio_write8(a, b)    writeb(a, b)
#define mmio_write16(a, b)   writew(a, b)
#define mmio_write32(a, b)   writel(a, b)

#else /*__i386__*/

#define mmio_read8(a)	     scr_to_cpu((*(volatile unsigned char *) (a)))
#define mmio_read16(a)	     scr_to_cpu((*(volatile unsigned short *) (a)))
#define mmio_read32(a)	     scr_to_cpu((*(volatile unsigned int *) (a)))
#define mmio_write8(a, b)   (*(volatile unsigned char *) (a)) = cpu_to_scr(b)
#define mmio_write16(a, b)  (*(volatile unsigned short *) (a)) = cpu_to_scr(b)
#define mmio_write32(a, b)  (*(volatile unsigned int *) (a)) = cpu_to_scr(b)

#endif

/*
 *  Normal IO
 */
#if defined(SYM_CONF_IOMAPPED)

#define	INB_OFF(o)	io_read8(np->io_port + sym_offb(o))
#define	OUTB_OFF(o, v)	io_write8(np->io_port + sym_offb(o), (v))

#define	INW_OFF(o)	io_read16(np->io_port + sym_offw(o))
#define	OUTW_OFF(o, v)	io_write16(np->io_port + sym_offw(o), (v))

#define	INL_OFF(o)	io_read32(np->io_port + (o))
#define	OUTL_OFF(o, v)	io_write32(np->io_port + (o), (v))

#else	/* Memory mapped IO */

#define	INB_OFF(o)	mmio_read8(np->mmio_va + sym_offb(o))
#define	OUTB_OFF(o, v)	mmio_write8(np->mmio_va + sym_offb(o), (v))

#define	INW_OFF(o)	mmio_read16(np->mmio_va + sym_offw(o))
#define	OUTW_OFF(o, v)	mmio_write16(np->mmio_va + sym_offw(o), (v))

#define	INL_OFF(o)	mmio_read32(np->mmio_va + (o))
#define	OUTL_OFF(o, v)	mmio_write32(np->mmio_va + (o), (v))

#endif

/*
 *  Common to both normal IO and MMIO.
 */
#define INB(r)		INB_OFF(offsetof(struct sym_reg,r))
#define INW(r)		INW_OFF(offsetof(struct sym_reg,r))
#define INL(r)		INL_OFF(offsetof(struct sym_reg,r))

#define OUTB(r, v)	OUTB_OFF(offsetof(struct sym_reg,r), (v))
#define OUTW(r, v)	OUTW_OFF(offsetof(struct sym_reg,r), (v))
#define OUTL(r, v)	OUTL_OFF(offsetof(struct sym_reg,r), (v))

#define OUTONB(r, m)	OUTB(r, INB(r) | (m))
#define OUTOFFB(r, m)	OUTB(r, INB(r) & ~(m))
#define OUTONW(r, m)	OUTW(r, INW(r) | (m))
#define OUTOFFW(r, m)	OUTW(r, INW(r) & ~(m))
#define OUTONL(r, m)	OUTL(r, INL(r) | (m))
#define OUTOFFL(r, m)	OUTL(r, INL(r) & ~(m))

/*
 *  Command control block states.
 */
#define HS_IDLE		(0)
#define HS_BUSY		(1)
#define HS_NEGOTIATE	(2)	/* sync/wide data transfer*/
#define HS_DISCONNECT	(3)	/* Disconnected by target */

#define HS_DONEMASK	(0x80)
#define HS_COMPLETE	(4|HS_DONEMASK)
#define HS_SEL_TIMEOUT	(5|HS_DONEMASK)	/* Selection timeout      */
#define HS_UNEXPECTED	(6|HS_DONEMASK)	/* Unexpected disconnect  */
#define HS_COMP_ERR	(7|HS_DONEMASK)	/* Completed with error	  */

/*
 *  Software Interrupt Codes
 */
#define	SIR_BAD_SCSI_STATUS	(1)
#define	SIR_SEL_ATN_NO_MSG_OUT	(2)
#define	SIR_MSG_RECEIVED	(3)
#define	SIR_MSG_WEIRD		(4)
#define	SIR_NEGO_FAILED		(5)
#define	SIR_NEGO_PROTO		(6)
#define	SIR_SCRIPT_STOPPED	(7)
#define	SIR_REJECT_TO_SEND	(8)
#define	SIR_SWIDE_OVERRUN	(9)
#define	SIR_SODL_UNDERRUN	(10)
#define	SIR_RESEL_NO_MSG_IN	(11)
#define	SIR_RESEL_NO_IDENTIFY	(12)
#define	SIR_RESEL_BAD_LUN	(13)
#define	SIR_TARGET_SELECTED	(14)
#define	SIR_RESEL_BAD_I_T_L	(15)
#define	SIR_RESEL_BAD_I_T_L_Q	(16)
#define	SIR_ABORT_SENT		(17)
#define	SIR_RESEL_ABORTED	(18)
#define	SIR_MSG_OUT_DONE	(19)
#define	SIR_COMPLETE_ERROR	(20)
#ifdef	SYM_DEBUG_PM_WITH_WSR
#define	SIR_PM_WITH_WSR		(21)
#define	SIR_MAX			(21)
#else
#define	SIR_MAX			(20)
#endif

/*
 *  Extended error bit codes.
 *  xerr_status field of struct sym_ccb.
 */
#define	XE_EXTRA_DATA	(1)	/* unexpected data phase	 */
#define	XE_BAD_PHASE	(1<<1)	/* illegal phase (4/5)		 */
#define	XE_PARITY_ERR	(1<<2)	/* unrecovered SCSI parity error */
#define	XE_SODL_UNRUN	(1<<3)	/* ODD transfer in DATA OUT phase */
#define	XE_SWIDE_OVRUN	(1<<4)	/* ODD transfer in DATA IN phase */

/*
 *  Negotiation status.
 *  nego_status field of struct sym_ccb.
 */
#define NS_SYNC		(1)
#define NS_WIDE		(2)
#define NS_PPR		(3)

/*
 *  A CCB hashed table is used to retrieve CCB address 
 *  from DSA value.
 */
#define CCB_HASH_SHIFT		8
#define CCB_HASH_SIZE		(1UL << CCB_HASH_SHIFT)
#define CCB_HASH_MASK		(CCB_HASH_SIZE-1)
#define CCB_HASH_CODE(dsa)	(((dsa) >> 9) & CCB_HASH_MASK)

/*
 *  Device flags.
 */
#define SYM_DISC_ENABLED	(1)
#define SYM_TAGS_ENABLED	(1<<1)
#define SYM_SCAN_BOOT_DISABLED	(1<<2)
#define SYM_SCAN_LUNS_DISABLED	(1<<3)

/*
 *  Host adapter miscellaneous flags.
 */
#define SYM_AVOID_BUS_RESET	(1)
#define SYM_SCAN_TARGETS_HILO	(1<<1)

/*
 *  Device quirks.
 *  Some devices, for example the CHEETAH 2 LVD, disconnects without 
 *  saving the DATA POINTER then reconnect and terminates the IO.
 *  On reselection, the automatic RESTORE DATA POINTER makes the 
 *  CURRENT DATA POINTER not point at the end of the IO.
 *  This behaviour just breaks our calculation of the residual.
 *  For now, we just force an AUTO SAVE on disconnection and will 
 *  fix that in a further driver version.
 */
#define SYM_QUIRK_AUTOSAVE 1

/*
 *  Misc.
 */
#define SYM_SNOOP_TIMEOUT (10000000)
#define SYM_PCI_IO	PCIR_MAPS
#define SYM_PCI_MMIO	(PCIR_MAPS + 4)
#define SYM_PCI_RAM	(PCIR_MAPS + 8)
#define SYM_PCI_RAM64	(PCIR_MAPS + 12)

/*
 *  Back-pointer from the CAM CCB to our data structures.
 */
#define sym_hcb_ptr	spriv_ptr0
/* #define sym_ccb_ptr	spriv_ptr1 */

/*
 *  We mostly have to deal with pointers.
 *  Thus these typedef's.
 */
typedef struct sym_tcb *tcb_p;
typedef struct sym_lcb *lcb_p;
typedef struct sym_ccb *ccb_p;
typedef struct sym_hcb *hcb_p;
typedef struct sym_scr  *script_p;
typedef struct sym_scrh *scripth_p;

/*
 *  Gather negotiable parameters value
 */
struct sym_trans {
	u8 period;
	u8 offset;
	u8 width;
	u8 options;	/* PPR options */
};

struct sym_tinfo {
	struct sym_trans current;
	struct sym_trans goal;
	struct sym_trans user;
};

#define BUS_8_BIT	MSG_EXT_WDTR_BUS_8_BIT
#define BUS_16_BIT	MSG_EXT_WDTR_BUS_16_BIT

/*
 *  Target Control Block
 */
struct sym_tcb {
	/*
	 *  LUN table used by the SCRIPTS processor.
	 *  An array of bus addresses is used on reselection.
	 *  LUN #0 is a special case, since multi-lun devices are rare, 
	 *  and we we want to speed-up the general case and not waste 
	 *  resources.
	 */
	u32	*luntbl;	/* LCBs bus address table	*/
	u32	luntbl_sa;	/* bus address of this table	*/
	u32	lun0_sa;	/* bus address of LCB #0	*/

	/*
	 *  LUN table used by the C code.
	 */
	lcb_p	lun0p;		/* LCB of LUN #0 (usual case)	*/
#if SYM_CONF_MAX_LUN > 1
	lcb_p	*lunmp;		/* Other LCBs [1..MAX_LUN]	*/
#endif

	/*
	 *  Bitmap that tells about LUNs that succeeded at least 
	 *  1 IO and therefore assumed to be a real device.
	 *  Avoid useless allocation of the LCB structure.
	 */
	u32	lun_map[(SYM_CONF_MAX_LUN+31)/32];

	/*
	 *  Bitmap that tells about LUNs that haven't yet an LCB 
	 *  allocated (not discovered or LCB allocation failed).
	 */
	u32	busy0_map[(SYM_CONF_MAX_LUN+31)/32];

	/*
	 *  Actual SYNC/WIDE IO registers value for this target.
	 *  'sval', 'wval' and 'uval' are read from SCRIPTS and 
	 *  so have alignment constraints.
	 */
/*0*/	u_char	uval;		/* -> SCNTL4 register		*/
/*1*/	u_char	sval;		/* -> SXFER  io register	*/
/*2*/	u_char	filler1;
/*3*/	u_char	wval;		/* -> SCNTL3 io register	*/

	/*
	 *  Transfer capabilities (SIP)
	 */
	struct sym_tinfo tinfo;

	/*
	 * Keep track of the CCB used for the negotiation in order
	 * to ensure that only 1 negotiation is queued at a time.
	 */
	ccb_p   nego_cp;	/* CCB used for the nego		*/

	/*
	 *  Set when we want to reset the device.
	 */
	u_char	to_reset;

	/*
	 *  Other user settable limits and options.
	 *  These limits are read from the NVRAM if present.
	 */
	u_char	usrflags;
	u_short	usrtags;
};

/*
 *  Logical Unit Control Block
 */
struct sym_lcb {
	/*
	 *  SCRIPTS address jumped by SCRIPTS on reselection.
	 *  For not probed logical units, this address points to 
	 *  SCRIPTS that deal with bad LU handling (must be at 
	 *  offset zero for that reason).
	 */
/*0*/	u32	resel_sa;

	/*
	 *  Task (bus address of a CCB) read from SCRIPTS that points 
	 *  to the unique ITL nexus allowed to be disconnected.
	 */
	u32	itl_task_sa;

	/*
	 *  Task table read from SCRIPTS that contains pointers to 
	 *  ITLQ nexuses (bus addresses read from SCRIPTS).
	 */
	u32	*itlq_tbl;	/* Kernel virtual address	*/
	u32	itlq_tbl_sa;	/* Bus address used by SCRIPTS	*/

	/*
	 *  Busy CCBs management.
	 */
	u_short	busy_itlq;	/* Number of busy tagged CCBs	*/
	u_short	busy_itl;	/* Number of busy untagged CCBs	*/

	/*
	 *  Circular tag allocation buffer.
	 */
	u_short	ia_tag;		/* Tag allocation index		*/
	u_short	if_tag;		/* Tag release index		*/
	u_char	*cb_tags;	/* Circular tags buffer		*/

	/*
	 *  Set when we want to clear all tasks.
	 */
	u_char to_clear;

	/*
	 *  Capabilities.
	 */
	u_char	user_flags;
	u_char	current_flags;
};

/*
 *  Action from SCRIPTS on a task.
 *  Is part of the CCB, but is also used separately to plug 
 *  error handling action to perform from SCRIPTS.
 */
struct sym_actscr {
	u32	start;		/* Jumped by SCRIPTS after selection	*/
	u32	restart;	/* Jumped by SCRIPTS on relection	*/
};

/*
 *  Phase mismatch context.
 *
 *  It is part of the CCB and is used as parameters for the 
 *  DATA pointer. We need two contexts to handle correctly the 
 *  SAVED DATA POINTER.
 */
struct sym_pmc {
	struct	sym_tblmove sg;	/* Updated interrupted SG block	*/
	u32	ret;		/* SCRIPT return address	*/
};

/*
 *  LUN control block lookup.
 *  We use a direct pointer for LUN #0, and a table of 
 *  pointers which is only allocated for devices that support 
 *  LUN(s) > 0.
 */
#if SYM_CONF_MAX_LUN <= 1
#define sym_lp(np, tp, lun) (!lun) ? (tp)->lun0p : 0
#else
#define sym_lp(np, tp, lun) \
	(!lun) ? (tp)->lun0p : (tp)->lunmp ? (tp)->lunmp[(lun)] : 0
#endif

/*
 *  Status are used by the host and the script processor.
 *
 *  The last four bytes (status[4]) are copied to the 
 *  scratchb register (declared as scr0..scr3) just after the 
 *  select/reselect, and copied back just after disconnecting.
 *  Inside the script the XX_REG are used.
 *
 *  The first four bytes (scr_st[4]) are used inside the 
 *  script by "LOAD/STORE" commands.
 *  Because source and destination must have the same alignment
 *  in a DWORD, the fields HAVE to be at the choosen offsets.
 *  	xerr_st		0	(0x34)	scratcha
 *  	nego_st		2
 */

/*
 *  Last four bytes (script)
 */
#define  QU_REG	scr0
#define  HS_REG	scr1
#define  HS_PRT	nc_scr1
#define  SS_REG	scr2
#define  SS_PRT	nc_scr2
#define  HF_REG	scr3
#define  HF_PRT	nc_scr3

/*
 *  Last four bytes (host)
 */
#define  actualquirks  phys.status[0]
#define  host_status   phys.status[1]
#define  ssss_status   phys.status[2]
#define  host_flags    phys.status[3]

/*
 *  Host flags
 */
#define HF_IN_PM0	1u
#define HF_IN_PM1	(1u<<1)
#define HF_ACT_PM	(1u<<2)
#define HF_DP_SAVED	(1u<<3)
#define HF_SENSE	(1u<<4)
#define HF_EXT_ERR	(1u<<5)
#ifdef SYM_CONF_IARB_SUPPORT
#define HF_HINT_IARB	(1u<<7)
#endif

/*
 *  First four bytes (script)
 */
#define  xerr_st       scr_st[0]
#define  nego_st       scr_st[2]

/*
 *  First four bytes (host)
 */
#define  xerr_status   phys.xerr_st
#define  nego_status   phys.nego_st

/*
 *  Data Structure Block
 *
 *  During execution of a ccb by the script processor, the 
 *  DSA (data structure address) register points to this 
 *  substructure of the ccb.
 */
struct dsb {
	/*
	 *  Start and restart SCRIPTS addresses (must be at 0).
	 */
/*0*/	struct sym_actscr go;

	/*
	 *  SCRIPTS jump address that deal with data pointers.
	 *  'savep' points to the position in the script responsible 
	 *  for the	actual transfer of data.
	 *  It's written on reception of a SAVE_DATA_POINTER message.
	 */
	u32	savep;		/* Jump address to saved data pointer	*/
	u32	lastp;		/* SCRIPTS address at end of data	*/
	u32	goalp;		/* Not used for now			*/

	/*
	 *  Status fields.
	 */
	u8	scr_st[4];	/* script status		*/
	u8	status[4];	/* host status			*/

	/*
	 *  Table data for Script
	 */
	struct sym_tblsel  select;
	struct sym_tblmove smsg;
	struct sym_tblmove smsg_ext;
	struct sym_tblmove cmd;
	struct sym_tblmove sense;
	struct sym_tblmove wresid;
	struct sym_tblmove data [SYM_CONF_MAX_SG];

	/*
	 *  Phase mismatch contexts.
	 *  We need two to handle correctly the SAVED DATA POINTER.
	 */
	struct sym_pmc pm0;
	struct sym_pmc pm1;

	/*
	 *  Extra bytes count transferred in case of data overrun.
	 */
	u32	extra_bytes;
};

/*
 *  Our Command Control Block
 */
struct sym_ccb {
	/*
	 *  This is the data structure which is pointed by the DSA 
	 *  register when it is executed by the script processor.
	 *  It must be the first entry.
	 */
	struct dsb phys;

	/*
	 *  Pointer to CAM ccb and related stuff.
	 */
	union ccb *cam_ccb;	/* CAM scsiio ccb		*/
	int	data_len;	/* Total data length		*/
	int	segments;	/* Number of SG segments	*/

	/*
	 *  Message areas.
	 *  We prepare a message to be sent after selection.
	 *  We may use a second one if the command is rescheduled 
	 *  due to CHECK_CONDITION or COMMAND TERMINATED.
	 *  Contents are IDENTIFY and SIMPLE_TAG.
	 *  While negotiating sync or wide transfer,
	 *  a SDTR or WDTR message is appended.
	 */
	u_char	scsi_smsg [12];
	u_char	scsi_smsg2[12];

	/*
	 *  Auto request sense related fields.
	 */
	u_char	sensecmd[6];	/* Request Sense command	*/
	u_char	sv_scsi_status;	/* Saved SCSI status 		*/
	u_char	sv_xerr_status;	/* Saved extended status	*/
	int	sv_resid;	/* Saved residual		*/
	
	/*
	 *  Other fields.
	 */
	u_long	ccb_ba;		/* BUS address of this CCB	*/
	u_short	tag;		/* Tag for this transfer	*/
				/*  NO_TAG means no tag		*/
	u_char	target;
	u_char	lun;
	ccb_p	link_ccbh;	/* Host adapter CCB hash chain	*/
	SYM_QUEHEAD
		link_ccbq;	/* Link to free/busy CCB queue	*/
	u32	startp;		/* Initial data pointer		*/
	int	ext_sg;		/* Extreme data pointer, used	*/
	int	ext_ofs;	/*  to calculate the residual.	*/
	u_char	to_abort;	/* Want this IO to be aborted	*/
};

#define CCB_PHYS(cp,lbl)	(cp->ccb_ba + offsetof(struct sym_ccb, lbl))

/*
 *  Host Control Block
 */
struct sym_hcb {
	/*
	 *  Idle task and invalid task actions and 
	 *  their bus addresses.
	 */
	struct sym_actscr idletask, notask, bad_itl, bad_itlq;
	vm_offset_t idletask_ba, notask_ba, bad_itl_ba, bad_itlq_ba;

	/*
	 *  Dummy lun table to protect us against target 
	 *  returning bad lun number on reselection.
	 */
	u32	*badluntbl;	/* Table physical address	*/
	u32	badlun_sa;	/* SCRIPT handler BUS address	*/

	/*
	 *  Bit 32-63 of the on-chip RAM bus address in LE format.
	 *  The START_RAM64 script loads the MMRS and MMWS from this 
	 *  field.
	 */
	u32	scr_ram_seg;

	/*
	 *  Chip and controller indentification.
	 */
#ifdef FreeBSD_4_Bus
	device_t device;
#else
	pcici_t	pci_tag;
#endif
	int	unit;
	char	inst_name[8];

	/*
	 *  Initial value of some IO register bits.
	 *  These values are assumed to have been set by BIOS, and may 
	 *  be used to probe adapter implementation differences.
	 */
	u_char	sv_scntl0, sv_scntl3, sv_dmode, sv_dcntl, sv_ctest3, sv_ctest4,
		sv_ctest5, sv_gpcntl, sv_stest2, sv_stest4, sv_scntl4,
		sv_stest1;

	/*
	 *  Actual initial value of IO register bits used by the 
	 *  driver. They are loaded at initialisation according to  
	 *  features that are to be enabled/disabled.
	 */
	u_char	rv_scntl0, rv_scntl3, rv_dmode, rv_dcntl, rv_ctest3, rv_ctest4, 
		rv_ctest5, rv_stest2, rv_ccntl0, rv_ccntl1, rv_scntl4;

	/*
	 *  Target data used by the CPU.
	 */
	struct sym_tcb	target[SYM_CONF_MAX_TARGET];

	/*
	 *  Target control block bus address array used by the SCRIPT 
	 *  on reselection.
	 */
	u32		*targtbl;

	/*
	 *  CAM SIM information for this instance.
	 */
	struct		cam_sim  *sim;
	struct		cam_path *path;

	/*
	 *  Allocated hardware resources.
	 */
#ifdef FreeBSD_4_Bus
	struct resource	*irq_res;
	struct resource	*io_res;
	struct resource	*mmio_res;
	struct resource	*ram_res;
	int		ram_id;
	void *intr;
#endif

	/*
	 *  Bus stuff.
	 *
	 *  My understanding of PCI is that all agents must share the 
	 *  same addressing range and model.
	 *  But some hardware architecture guys provide complex and  
	 *  brain-deaded stuff that makes shit.
	 *  This driver only support PCI compliant implementations and 
	 *  deals with part of the BUS stuff complexity only to fit O/S 
	 *  requirements.
	 */
#ifdef FreeBSD_4_Bus
	bus_space_handle_t	io_bsh;
	bus_space_tag_t		io_tag;
	bus_space_handle_t	mmio_bsh;
	bus_space_tag_t		mmio_tag;
	bus_space_handle_t	ram_bsh;
	bus_space_tag_t		ram_tag;
#endif

	/*
	 *  Virtual and physical bus addresses of the chip.
	 */
	vm_offset_t	mmio_va;	/* MMIO kernel virtual address	*/
	vm_offset_t	mmio_pa;	/* MMIO CPU physical address	*/
	vm_offset_t	mmio_ba;	/* MMIO BUS address		*/
	int		mmio_ws;	/* MMIO Window size		*/

	vm_offset_t	ram_va;		/* RAM kernel virtual address	*/
	vm_offset_t	ram_pa;		/* RAM CPU physical address	*/
	vm_offset_t	ram_ba;		/* RAM BUS address		*/
	int		ram_ws;		/* RAM window size		*/
	u32		io_port;	/* IO port address		*/

	/*
	 *  SCRIPTS virtual and physical bus addresses.
	 *  'script'  is loaded in the on-chip RAM if present.
	 *  'scripth' stays in main memory for all chips except the 
	 *  53C895A, 53C896 and 53C1010 that provide 8K on-chip RAM.
	 */
	struct sym_scr	*script0;	/* Copies of script and scripth	*/
	struct sym_scrh	*scripth0;	/*  relocated for this host.	*/
	vm_offset_t	script_ba;	/* Actual script and scripth	*/
	vm_offset_t	scripth_ba;	/*  bus addresses.		*/
	vm_offset_t	scripth0_ba;

	/*
	 *  General controller parameters and configuration.
	 */
	u_short	device_id;	/* PCI device id		*/
	u_char	revision_id;	/* PCI device revision id	*/
	u_int	features;	/* Chip features map		*/
	u_char	myaddr;		/* SCSI id of the adapter	*/
	u_char	maxburst;	/* log base 2 of dwords burst	*/
	u_char	maxwide;	/* Maximum transfer width	*/
	u_char	minsync;	/* Min sync period factor (ST)	*/
	u_char	maxsync;	/* Max sync period factor (ST)	*/
	u_char	minsync_dt;	/* Min sync period factor (DT)	*/
	u_char	maxsync_dt;	/* Max sync period factor (DT)	*/
	u_char	maxoffs;	/* Max scsi offset		*/
	u_char	multiplier;	/* Clock multiplier (1,2,4)	*/
	u_char	clock_divn;	/* Number of clock divisors	*/
	u_long	clock_khz;	/* SCSI clock frequency in KHz	*/

	/*
	 *  Start queue management.
	 *  It is filled up by the host processor and accessed by the 
	 *  SCRIPTS processor in order to start SCSI commands.
	 */
	volatile		/* Prevent code optimizations	*/
	u32	*squeue;	/* Start queue			*/
	u_short	squeueput;	/* Next free slot of the queue	*/
	u_short	actccbs;	/* Number of allocated CCBs	*/

	/*
	 *  Command completion queue.
	 *  It is the same size as the start queue to avoid overflow.
	 */
	u_short	dqueueget;	/* Next position to scan	*/
	volatile		/* Prevent code optimizations	*/
	u32	*dqueue;	/* Completion (done) queue	*/

	/*
	 *  Miscellaneous buffers accessed by the scripts-processor.
	 *  They shall be DWORD aligned, because they may be read or 
	 *  written with a script command.
	 */
	u_char		msgout[8];	/* Buffer for MESSAGE OUT 	*/
	u_char		msgin [8];	/* Buffer for MESSAGE IN	*/
	u32		lastmsg;	/* Last SCSI message sent	*/
	u_char		scratch;	/* Scratch for SCSI receive	*/

	/*
	 *  Miscellaneous configuration and status parameters.
	 */
	u_char		usrflags;	/* Miscellaneous user flags	*/
	u_char		scsi_mode;	/* Current SCSI BUS mode	*/
	u_char		verbose;	/* Verbosity for this controller*/
	u32		cache;		/* Used for cache test at init.	*/

	/*
	 *  CCB lists and queue.
	 */
	ccb_p ccbh[CCB_HASH_SIZE];	/* CCB hashed by DSA value	*/
	SYM_QUEHEAD	free_ccbq;	/* Queue of available CCBs	*/
	SYM_QUEHEAD	busy_ccbq;	/* Queue of busy CCBs		*/

	/*
	 *  During error handling and/or recovery,
	 *  active CCBs that are to be completed with 
	 *  error or requeued are moved from the busy_ccbq
	 *  to the comp_ccbq prior to completion.
	 */
	SYM_QUEHEAD	comp_ccbq;

	/*
	 *  CAM CCB pending queue.
	 */
	SYM_QUEHEAD	cam_ccbq;

	/*
	 *  IMMEDIATE ARBITRATION (IARB) control.
	 *
	 *  We keep track in 'last_cp' of the last CCB that has been 
	 *  queued to the SCRIPTS processor and clear 'last_cp' when 
	 *  this CCB completes. If last_cp is not zero at the moment 
	 *  we queue a new CCB, we set a flag in 'last_cp' that is 
	 *  used by the SCRIPTS as a hint for setting IARB.
	 *  We donnot set more than 'iarb_max' consecutive hints for 
	 *  IARB in order to leave devices a chance to reselect.
	 *  By the way, any non zero value of 'iarb_max' is unfair. :)
	 */
#ifdef SYM_CONF_IARB_SUPPORT
	u_short		iarb_max;	/* Max. # consecutive IARB hints*/
	u_short		iarb_count;	/* Actual # of these hints	*/
	ccb_p		last_cp;
#endif

	/*
	 *  Command abort handling.
	 *  We need to synchronize tightly with the SCRIPTS 
	 *  processor in order to handle things correctly.
	 */
	u_char		abrt_msg[4];	/* Message to send buffer	*/
	struct sym_tblmove abrt_tbl;	/* Table for the MOV of it 	*/
	struct sym_tblsel  abrt_sel;	/* Sync params for selection	*/
	u_char		istat_sem;	/* Tells the chip to stop (SEM)	*/
};

#define SCRIPT_BA(np,lbl)   (np->script_ba   + offsetof(struct sym_scr, lbl))
#define SCRIPTH_BA(np,lbl)  (np->scripth_ba  + offsetof(struct sym_scrh,lbl))
#define SCRIPTH0_BA(np,lbl) (np->scripth0_ba + offsetof(struct sym_scrh,lbl))

/*
 *  Scripts for SYMBIOS-Processor
 *
 *  Use sym_fill_scripts() to create the variable parts.
 *  Use sym_bind_script()  to make a copy and bind to 
 *  physical bus addresses.
 *  We have to know the offsets of all labels before we reach 
 *  them (for forward jumps). Therefore we declare a struct 
 *  here. If you make changes inside the script,
 *
 *  DONT FORGET TO CHANGE THE LENGTHS HERE!
 */

/*
 *  Script fragments which are loaded into the on-chip RAM 
 *  of 825A, 875, 876, 895, 895A, 896 and 1010 chips.
 *  Must not exceed 4K bytes.
 */
struct sym_scr {
	u32 start		[ 14];
	u32 getjob_begin	[  4];
	u32 getjob_end		[  4];
	u32 select		[  8];
	u32 wf_sel_done		[  2];
	u32 send_ident		[  2];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 select2		[  8];
#else
	u32 select2		[  2];
#endif
	u32 command		[  2];
	u32 dispatch		[ 30];
	u32 sel_no_cmd		[ 10];
	u32 init		[  6];
	u32 clrack		[  4];
	u32 disp_status		[  4];
	u32 datai_done		[ 26];
	u32 datao_done		[ 12];
	u32 dataphase		[  2];
	u32 msg_in		[  2];
	u32 msg_in2		[ 10];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 status		[ 14];
#else
	u32 status		[ 10];
#endif
	u32 complete		[  8];
	u32 complete2		[ 12];
	u32 complete_error	[  4];
	u32 done		[ 14];
	u32 done_end		[  2];
	u32 save_dp		[  8];
	u32 restore_dp		[  4];
	u32 disconnect		[ 20];
#ifdef SYM_CONF_IARB_SUPPORT
	u32 idle		[  4];
#else
	u32 idle		[  2];
#endif
#ifdef SYM_CONF_IARB_SUPPORT
	u32 ungetjob		[  6];
#else
	u32 ungetjob		[  4];
#endif
	u32 reselect		[  4];
	u32 reselected		[ 20];
	u32 resel_scntl4	[ 28];
#if   SYM_CONF_MAX_TASK*4 > 512
	u32 resel_tag		[ 26];
#elif SYM_CONF_MAX_TASK*4 > 256
	u32 resel_tag		[ 20];
#else
	u32 resel_tag		[ 16];
#endif
	u32 resel_dsa		[  2];
	u32 resel_dsa1		[  6];
	u32 resel_no_tag	[  6];
	u32 data_in		[SYM_CONF_MAX_SG * 2];
	u32 data_in2		[  4];
	u32 data_out		[SYM_CONF_MAX_SG * 2];
	u32 data_out2		[  4];
	u32 pm0_data		[ 16];
	u32 pm1_data		[ 16];
};

/*
 *  Script fragments which stay in main memory for all chips 
 *  except for chips that support 8K on-chip RAM.
 */
struct sym_scrh {
	u32 start64		[  2];
	u32 no_data		[  2];
	u32 sel_for_abort	[ 18];
	u32 sel_for_abort_1	[  2];
	u32 select_no_atn	[  8];
	u32 wf_sel_done_no_atn	[  4];

	u32 msg_in_etc		[ 14];
	u32 msg_received	[  4];
	u32 msg_weird_seen	[  4];
	u32 msg_extended	[ 20];
	u32 msg_bad		[  6];
	u32 msg_weird		[  4];
	u32 msg_weird1		[  8];

	u32 wdtr_resp		[  6];
	u32 send_wdtr		[  4];
	u32 sdtr_resp		[  6];
	u32 send_sdtr		[  4];
	u32 ppr_resp		[  6];
	u32 send_ppr		[  4];
	u32 nego_bad_phase	[  4];
	u32 msg_out		[  4];
	u32 msg_out_done	[  4];
	u32 data_ovrun		[ 18];
	u32 data_ovrun1		[ 20];
	u32 abort_resel		[ 16];
	u32 resend_ident	[  4];
	u32 ident_break		[  4];
	u32 ident_break_atn	[  4];
	u32 sdata_in		[  6];
	u32 resel_bad_lun	[  4];
	u32 bad_i_t_l		[  4];
	u32 bad_i_t_l_q		[  4];
	u32 bad_status		[  6];
	u32 pm_handle		[ 20];
	u32 pm_handle1		[  4];
	u32 pm_save		[  4];
	u32 pm0_save		[ 14];
	u32 pm1_save		[ 14];

	/* WSR handling */
#ifdef	SYM_DEBUG_PM_WITH_WSR
	u32 pm_wsr_handle	[ 44];
#else
	u32 pm_wsr_handle	[ 42];
#endif
	u32 wsr_ma_helper	[  4];

	/* Data area */
	u32 zero		[  1];
	u32 scratch		[  1];
	u32 pm0_data_addr	[  1];
	u32 pm1_data_addr	[  1];
	u32 saved_dsa		[  1];
	u32 saved_drs		[  1];
	u32 done_pos		[  1];
	u32 startpos		[  1];
	u32 targtbl		[  1];
	/* End of data area */

	u32 snooptest		[  6];
	u32 snoopend		[  2];
};

/*
 *  Function prototypes.
 */
static void sym_fill_scripts (script_p scr, scripth_p scrh);
static void sym_bind_script (hcb_p np, u32 *src, u32 *dst, int len);
static void sym_save_initial_setting (hcb_p np);
static int  sym_prepare_setting (hcb_p np, struct sym_nvram *nvram);
static int  sym_prepare_nego (hcb_p np, ccb_p cp, int nego, u_char *msgptr);
static void sym_put_start_queue (hcb_p np, ccb_p cp);
static void sym_chip_reset (hcb_p np);
static void sym_soft_reset (hcb_p np);
static void sym_start_reset (hcb_p np);
static int  sym_reset_scsi_bus (hcb_p np, int enab_int);
static int  sym_wakeup_done (hcb_p np);
static void sym_flush_busy_queue (hcb_p np, int cam_status);
static void sym_flush_comp_queue (hcb_p np, int cam_status);
static void sym_init (hcb_p np, int reason);
static int  sym_getsync(hcb_p np, u_char dt, u_char sfac, u_char *divp,
		        u_char *fakp);
static void sym_setsync (hcb_p np, ccb_p cp, u_char ofs, u_char per,
			 u_char div, u_char fak);
static void sym_setwide (hcb_p np, ccb_p cp, u_char wide);
static void sym_setpprot(hcb_p np, ccb_p cp, u_char dt, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak);
static void sym_settrans(hcb_p np, ccb_p cp, u_char dt, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak);
static void sym_log_hard_error (hcb_p np, u_short sist, u_char dstat);
static void sym_intr (void *arg);
static void sym_poll (struct cam_sim *sim);
static void sym_recover_scsi_int (hcb_p np, u_char hsts);
static void sym_int_sto (hcb_p np);
static void sym_int_udc (hcb_p np);
static void sym_int_sbmc (hcb_p np);
static void sym_int_par (hcb_p np, u_short sist);
static void sym_int_ma (hcb_p np);
static int  sym_dequeue_from_squeue(hcb_p np, int i, int target, int lun, 
				    int task);
static void sym_sir_bad_scsi_status (hcb_p np, int num, ccb_p cp);
static int  sym_clear_tasks (hcb_p np, int status, int targ, int lun, int task);
static void sym_sir_task_recovery (hcb_p np, int num);
static int  sym_evaluate_dp (hcb_p np, ccb_p cp, u32 scr, int *ofs);
static void sym_modify_dp (hcb_p np, tcb_p tp, ccb_p cp, int ofs);
static int  sym_compute_residual (hcb_p np, ccb_p cp);
static int  sym_show_msg (u_char * msg);
static void sym_print_msg (ccb_p cp, char *label, u_char *msg);
static void sym_sync_nego (hcb_p np, tcb_p tp, ccb_p cp);
static void sym_ppr_nego (hcb_p np, tcb_p tp, ccb_p cp);
static void sym_wide_nego (hcb_p np, tcb_p tp, ccb_p cp);
static void sym_nego_default (hcb_p np, tcb_p tp, ccb_p cp);
static void sym_nego_rejected (hcb_p np, tcb_p tp, ccb_p cp);
static void sym_int_sir (hcb_p np);
static void sym_free_ccb (hcb_p np, ccb_p cp);
static ccb_p sym_get_ccb (hcb_p np, u_char tn, u_char ln, u_char tag_order);
static ccb_p sym_alloc_ccb (hcb_p np);
static ccb_p sym_ccb_from_dsa (hcb_p np, u_long dsa);
static lcb_p sym_alloc_lcb (hcb_p np, u_char tn, u_char ln);
static void sym_alloc_lcb_tags (hcb_p np, u_char tn, u_char ln);
static int  sym_snooptest (hcb_p np);
static void sym_selectclock(hcb_p np, u_char scntl3);
static void sym_getclock (hcb_p np, int mult);
static int  sym_getpciclock (hcb_p np);
static void sym_complete_ok (hcb_p np, ccb_p cp);
static void sym_complete_error (hcb_p np, ccb_p cp);
static void sym_timeout (void *arg);
static int  sym_abort_scsiio (hcb_p np, union ccb *ccb, int timed_out);
static void sym_reset_dev (hcb_p np, union ccb *ccb);
static void sym_action (struct cam_sim *sim, union ccb *ccb);
static void sym_action1 (struct cam_sim *sim, union ccb *ccb);
static int  sym_setup_cdb (hcb_p np, struct ccb_scsiio *csio, ccb_p cp);
static int  sym_setup_data(hcb_p np, struct ccb_scsiio *csio, ccb_p cp);
static int  sym_scatter_virtual (hcb_p np, ccb_p cp, vm_offset_t vaddr,
				 vm_size_t len);
static int  sym_scatter_physical (hcb_p np, ccb_p cp, vm_offset_t vaddr,
				 vm_size_t len);
static void sym_action2 (struct cam_sim *sim, union ccb *ccb);
static void sym_update_trans (hcb_p np, tcb_p tp, struct sym_trans *tip,
			      struct ccb_trans_settings *cts);
static void sym_update_dflags(hcb_p np, u_char *flags,
			      struct ccb_trans_settings *cts);

#ifdef FreeBSD_4_Bus
static struct sym_pci_chip *sym_find_pci_chip (device_t dev);
static int  sym_pci_probe (device_t dev);
static int  sym_pci_attach (device_t dev);
#else
static struct sym_pci_chip *sym_find_pci_chip (pcici_t tag);
static const char *sym_pci_probe (pcici_t tag, pcidi_t type);
static void sym_pci_attach (pcici_t tag, int unit);
static int sym_pci_attach2 (pcici_t tag, int unit);
#endif

static void sym_pci_free (hcb_p np);
static int  sym_cam_attach (hcb_p np);
static void sym_cam_free (hcb_p np);

static void sym_nvram_setup_host (hcb_p np, struct sym_nvram *nvram);
static void sym_nvram_setup_target (hcb_p np, int targ, struct sym_nvram *nvp);
static int sym_read_nvram (hcb_p np, struct sym_nvram *nvp);

/*
 *  Return the name of the controller.
 */
static __inline char *sym_name(hcb_p np)
{
	return np->inst_name;
}

/*
 *  Scripts for SYMBIOS-Processor
 *
 *  Use sym_bind_script for binding to physical addresses.
 *
 *  NADDR generates a reference to a field of the controller data.
 *  PADDR generates a reference to another part of the script.
 *  RADDR generates a reference to a script processor register.
 *  FADDR generates a reference to a script processor register
 *        with offset.
 *
 */
#define	RELOC_SOFTC	0x40000000
#define	RELOC_LABEL	0x50000000
#define	RELOC_REGISTER	0x60000000
#if 0
#define	RELOC_KVAR	0x70000000
#endif
#define	RELOC_LABELH	0x80000000
#define	RELOC_MASK	0xf0000000

#define	NADDR(label)	(RELOC_SOFTC  | offsetof(struct sym_hcb, label))
#define PADDR(label)    (RELOC_LABEL  | offsetof(struct sym_scr, label))
#define PADDRH(label)   (RELOC_LABELH | offsetof(struct sym_scrh, label))
#define	RADDR(label)	(RELOC_REGISTER | REG(label))
#define	FADDR(label,ofs)(RELOC_REGISTER | ((REG(label))+(ofs)))
#define	KVAR(which)	(RELOC_KVAR | (which))

#define SCR_DATA_ZERO	0xf00ff00f

#ifdef	RELOC_KVAR
#define	SCRIPT_KVAR_JIFFIES	(0)
#define	SCRIPT_KVAR_FIRST	SCRIPT_KVAR_XXXXXXX
#define	SCRIPT_KVAR_LAST	SCRIPT_KVAR_XXXXXXX
/*
 * Kernel variables referenced in the scripts.
 * THESE MUST ALL BE ALIGNED TO A 4-BYTE BOUNDARY.
 */
static void *script_kvars[] =
	{ (void *)&xxxxxxx };
#endif

static struct sym_scr script0 = {
/*--------------------------< START >-----------------------*/ {
	/*
	 *  This NOP will be patched with LED ON
	 *  SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	 */
	SCR_NO_OP,
		0,
	/*
	 *      Clear SIGP.
	 */
	SCR_FROM_REG (ctest2),
		0,
	/*
	 *  Stop here if the C code wants to perform 
	 *  some error recovery procedure manually.
	 *  (Indicate this by setting SEM in ISTAT)
	 */
	SCR_FROM_REG (istat),
		0,
	/*
	 *  Report to the C code the next position in 
	 *  the start queue the SCRIPTS will schedule.
	 *  The C code must not change SCRATCHA.
	 */
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (startpos),
	SCR_INT ^ IFTRUE (MASK (SEM, SEM)),
		SIR_SCRIPT_STOPPED,
	/*
	 *  Start the next job.
	 *
	 *  @DSA	 = start point for this job.
	 *  SCRATCHA = address of this job in the start queue.
	 *
	 *  We will restore startpos with SCRATCHA if we fails the 
	 *  arbitration or if it is the idle job.
	 *
	 *  The below GETJOB_BEGIN to GETJOB_END section of SCRIPTS 
	 *  is a critical path. If it is partially executed, it then 
	 *  may happen that the job address is not yet in the DSA 
	 *  and the the next queue position points to the next JOB.
	 */
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (startpos),
	SCR_LOAD_REL (temp, 4),
		4,
}/*-------------------------< GETJOB_BEGIN >------------------*/,{
	SCR_STORE_ABS (temp, 4),
		PADDRH (startpos),
	SCR_LOAD_REL (dsa, 4),
		0,
}/*-------------------------< GETJOB_END >--------------------*/,{
	SCR_LOAD_REL (temp, 4),
		0,
	SCR_RETURN,
		0,
}/*-------------------------< SELECT >----------------------*/,{
	/*
	 *  DSA	contains the address of a scheduled
	 *  	data structure.
	 *
	 *  SCRATCHA contains the address of the start queue  
	 *  	entry which points to the next job.
	 *
	 *  Set Initiator mode.
	 *
	 *  (Target mode is left as an exercise for the reader)
	 */
	SCR_CLR (SCR_TRG),
		0,
	/*
	 *      And try to select this target.
	 */
	SCR_SEL_TBL_ATN ^ offsetof (struct dsb, select),
		PADDR (ungetjob),
	/*
	 *  Now there are 4 possibilities:
	 *
	 *  (1) The chip looses arbitration.
	 *  This is ok, because it will try again,
	 *  when the bus becomes idle.
	 *  (But beware of the timeout function!)
	 *
	 *  (2) The chip is reselected.
	 *  Then the script processor takes the jump
	 *  to the RESELECT label.
	 *
	 *  (3) The chip wins arbitration.
	 *  Then it will execute SCRIPTS instruction until 
	 *  the next instruction that checks SCSI phase.
	 *  Then will stop and wait for selection to be 
	 *  complete or selection time-out to occur.
	 *
	 *  After having won arbitration, the SCRIPTS  
	 *  processor is able to execute instructions while 
	 *  the SCSI core is performing SCSI selection.
	 */
	/*
	 *      load the savep (saved data pointer) into
	 *      the actual data pointer.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	/*
	 *      Initialize the status registers
	 */
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct sym_ccb, phys.status),
}/*-------------------------< WF_SEL_DONE >----------------------*/,{
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		SIR_SEL_ATN_NO_MSG_OUT,
}/*-------------------------< SEND_IDENT >----------------------*/,{
	/*
	 *  Selection complete.
	 *  Send the IDENTIFY and possibly the TAG message 
	 *  and negotiation message if present.
	 */
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct dsb, smsg),
}/*-------------------------< SELECT2 >----------------------*/,{
#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  Set IMMEDIATE ARBITRATION if we have been given 
	 *  a hint to do so. (Some job to do after this one).
	 */
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (HF_HINT_IARB, HF_HINT_IARB)),
		8,
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	/*
	 *  Anticipate the COMMAND phase.
	 *  This is the PHASE we expect at this point.
	 */
	SCR_JUMP ^ IFFALSE (WHEN (SCR_COMMAND)),
		PADDR (sel_no_cmd),
}/*-------------------------< COMMAND >--------------------*/,{
	/*
	 *  ... and send the command
	 */
	SCR_MOVE_TBL ^ SCR_COMMAND,
		offsetof (struct dsb, cmd),
}/*-----------------------< DISPATCH >----------------------*/,{
	/*
	 *  MSG_IN is the only phase that shall be 
	 *  entered at least once for each (re)selection.
	 *  So we test it first.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_OUT)),
		PADDR (dataphase),
	SCR_JUMP ^ IFTRUE (IF (SCR_DATA_IN)),
		PADDR (dataphase),
	SCR_JUMP ^ IFTRUE (IF (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP ^ IFTRUE (IF (SCR_COMMAND)),
		PADDR (command),
	SCR_JUMP ^ IFTRUE (IF (SCR_MSG_OUT)),
		PADDRH (msg_out),

	/*
	 *  Set the extended error flag.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_EXT_ERR),
		0,
	/*
	 *  Discard one illegal phase byte, if required.
	 */
	SCR_LOAD_REL (scratcha, 1),
		offsetof (struct sym_ccb, xerr_status),
	SCR_REG_REG (scratcha,  SCR_OR,  XE_BAD_PHASE),
		0,
	SCR_STORE_REL (scratcha, 1),
		offsetof (struct sym_ccb, xerr_status),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_OUT)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_OUT,
		NADDR (scratch),
	SCR_JUMPR ^ IFFALSE (IF (SCR_ILG_IN)),
		8,
	SCR_MOVE_ABS (1) ^ SCR_ILG_IN,
		NADDR (scratch),

	SCR_JUMP,
		PADDR (dispatch),
}/*---------------------< SEL_NO_CMD >----------------------*/,{
	/*
	 *  The target does not switch to command 
	 *  phase after IDENTIFY has been sent.
	 *
	 *  If it stays in MSG OUT phase send it 
	 *  the IDENTIFY again.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (resend_ident),
	/*
	 *  If target does not switch to MSG IN phase 
	 *  and we sent a negotiation, assert the 
	 *  failure immediately.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_FROM_REG (HS_REG),
		0,
	SCR_INT ^ IFTRUE (DATA (HS_NEGOTIATE)),
		SIR_NEGO_FAILED,
	/*
	 *  Jump to dispatcher.
	 */
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< INIT >------------------------*/,{
	/*
	 *  Wait for the SCSI RESET signal to be 
	 *  inactive before restarting operations, 
	 *  since the chip may hang on SEL_ATN 
	 *  if SCSI RESET is active.
	 */
	SCR_FROM_REG (sstat0),
		0,
	SCR_JUMPR ^ IFTRUE (MASK (IRST, IRST)),
		-16,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< CLRACK >----------------------*/,{
	/*
	 *  Terminate possible pending message phase.
	 */
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< DISP_STATUS >----------------------*/,{
	/*
	 *  Anticipate STATUS phase.
	 *
	 *  Does spare 3 SCRIPTS instructions when we have 
	 *  completed the INPUT of the data.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_STATUS)),
		PADDR (status),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< DATAI_DONE >-------------------*/,{
	/*
	 *  If the device still wants to send us data,
	 *  we must count the extra bytes.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_DATA_IN)),
		PADDRH (data_ovrun),
	/*
	 *  If the SWIDE is not full, jump to dispatcher.
	 *  We anticipate a STATUS phase.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFFALSE (MASK (WSR, WSR)),
		PADDR (disp_status),
	/*
	 *  The SWIDE is full.
	 *  Clear this condition.
	 */
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	/*
	 *  We are expecting an IGNORE RESIDUE message 
	 *  from the device, otherwise we are in data 
	 *  overrun condition. Check against MSG_IN phase.
	 */
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_SWIDE_OVERRUN,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (disp_status),
	/*
	 *  We are in MSG_IN phase,
	 *  Read the first byte of the message.
	 *  If it is not an IGNORE RESIDUE message,
	 *  signal overrun and jump to message 
	 *  processing.
	 */
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
	SCR_INT ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		SIR_SWIDE_OVERRUN,
	SCR_JUMP ^ IFFALSE (DATA (M_IGN_RESIDUE)),
		PADDR (msg_in2),
	/*
	 *  We got the message we expected.
	 *  Read the 2nd byte, and jump to dispatcher.
	 */
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP,
		PADDR (disp_status),
}/*-------------------------< DATAO_DONE >-------------------*/,{
	/*
	 *  If the device wants us to send more data,
	 *  we must count the extra bytes.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_DATA_OUT)),
		PADDRH (data_ovrun),
	/*
	 *  If the SODL is not full jump to dispatcher.
	 *  We anticipate a STATUS phase.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMP ^ IFFALSE (MASK (WSS, WSS)),
		PADDR (disp_status),
	/*
	 *  The SODL is full, clear this condition.
	 */
	SCR_REG_REG (scntl2, SCR_OR, WSS),
		0,
	/*
	 *  And signal a DATA UNDERRUN condition 
	 *  to the C code.
	 */
	SCR_INT,
		SIR_SODL_UNDERRUN,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< DATAPHASE >------------------*/,{
	SCR_RETURN,
 		0,
}/*-------------------------< MSG_IN >--------------------*/,{
	/*
	 *  Get the first byte of the message.
	 *
	 *  The script processor doesn't negate the
	 *  ACK signal after this transfer.
	 */
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[0]),
}/*-------------------------< MSG_IN2 >--------------------*/,{
	/*
	 *  Check first against 1 byte messages 
	 *  that we handle from SCRIPTS.
	 */
	SCR_JUMP ^ IFTRUE (DATA (M_COMPLETE)),
		PADDR (complete),
	SCR_JUMP ^ IFTRUE (DATA (M_DISCONNECT)),
		PADDR (disconnect),
	SCR_JUMP ^ IFTRUE (DATA (M_SAVE_DP)),
		PADDR (save_dp),
	SCR_JUMP ^ IFTRUE (DATA (M_RESTORE_DP)),
		PADDR (restore_dp),
	/*
	 *  We handle all other messages from the 
	 *  C code, so no need to waste on-chip RAM 
	 *  for those ones.
	 */
	SCR_JUMP,
		PADDRH (msg_in_etc),
}/*-------------------------< STATUS >--------------------*/,{
	/*
	 *  get the status
	 */
	SCR_MOVE_ABS (1) ^ SCR_STATUS,
		NADDR (scratch),
#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  If STATUS is not GOOD, clear IMMEDIATE ARBITRATION, 
	 *  since we may have to tamper the start queue from 
	 *  the C code.
	 */
	SCR_JUMPR ^ IFTRUE (DATA (S_GOOD)),
		8,
	SCR_REG_REG (scntl1, SCR_AND, ~IARB),
		0,
#endif
	/*
	 *  save status to scsi_status.
	 *  mark as complete.
	 */
	SCR_TO_REG (SS_REG),
		0,
	SCR_LOAD_REG (HS_REG, HS_COMPLETE),
		0,
	/*
	 *  Anticipate the MESSAGE PHASE for 
	 *  the TASK COMPLETE message.
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_IN)),
		PADDR (msg_in),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< COMPLETE >-----------------*/,{
	/*
	 *  Complete message.
	 *
	 *  Copy the data pointer to LASTP.
	 */
	SCR_STORE_REL (temp, 4),
		offsetof (struct sym_ccb, phys.lastp),
	/*
	 *  When we terminate the cycle by clearing ACK,
	 *  the target may disconnect immediately.
	 *
	 *  We don't want to be told of an "unexpected disconnect",
	 *  so we disable this feature.
	 */
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	/*
	 *  Terminate cycle ...
	 */
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	 *  ... and wait for the disconnect.
	 */
	SCR_WAIT_DISC,
		0,
}/*-------------------------< COMPLETE2 >-----------------*/,{
	/*
	 *  Save host status.
	 */
	SCR_STORE_REL (scr0, 4),
		offsetof (struct sym_ccb, phys.status),
	/*
	 *  Some bridges may reorder DMA writes to memory.
	 *  We donnot want the CPU to deal with completions  
	 *  without all the posted write having been flushed 
	 *  to memory. This DUMMY READ should flush posted 
	 *  buffers prior to the CPU having to deal with 
	 *  completions.
	 */
	SCR_LOAD_REL (scr0, 4),	/* DUMMY READ */
		offsetof (struct sym_ccb, phys.status),

	/*
	 *  If command resulted in not GOOD status,
	 *  call the C code if needed.
	 */
	SCR_FROM_REG (SS_REG),
		0,
	SCR_CALL ^ IFFALSE (DATA (S_GOOD)),
		PADDRH (bad_status),
	/*
	 *  If we performed an auto-sense, call 
	 *  the C code to synchronyze task aborts 
	 *  with UNIT ATTENTION conditions.
	 */
	SCR_FROM_REG (HF_REG),
		0,
	SCR_JUMPR ^ IFTRUE (MASK (0 ,(HF_SENSE|HF_EXT_ERR))),
		16,
}/*-------------------------< COMPLETE_ERROR >-----------------*/,{
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (startpos),
	SCR_INT,
		SIR_COMPLETE_ERROR,
}/*------------------------< DONE >-----------------*/,{
	/*
	 *  Copy the DSA to the DONE QUEUE and 
	 *  signal completion to the host.
	 *  If we are interrupted between DONE 
	 *  and DONE_END, we must reset, otherwise 
	 *  the completed CCB may be lost.
	 */
	SCR_STORE_ABS (dsa, 4),
		PADDRH (saved_dsa),
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (done_pos),
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (saved_dsa),
	SCR_STORE_REL (scratcha, 4),
		0,
	/*
	 *  The instruction below reads the DONE QUEUE next 
	 *  free position from memory.
	 *  In addition it ensures that all PCI posted writes  
	 *  are flushed and so the DSA value of the done 
	 *  CCB is visible by the CPU before INTFLY is raised.
	 */
	SCR_LOAD_REL (temp, 4),
		4,
	SCR_INT_FLY,
		0,
	SCR_STORE_ABS (temp, 4),
		PADDRH (done_pos),
}/*------------------------< DONE_END >-----------------*/,{
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< SAVE_DP >------------------*/,{
	/*
	 *  Clear ACK immediately.
	 *  No need to delay it.
	 */
	SCR_CLR (SCR_ACK),
		0,
	/*
	 *  Keep track we received a SAVE DP, so 
	 *  we will switch to the other PM context 
	 *  on the next PM since the DP may point 
	 *  to the current PM context.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_DP_SAVED),
		0,
	/*
	 *  SAVE_DP message:
	 *  Copy the data pointer to SAVEP.
	 */
	SCR_STORE_REL (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< RESTORE_DP >---------------*/,{
	/*
	 *  RESTORE_DP message:
	 *  Copy SAVEP to actual data pointer.
	 */
	SCR_LOAD_REL  (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	SCR_JUMP,
		PADDR (clrack),
}/*-------------------------< DISCONNECT >---------------*/,{
	/*
	 *  DISCONNECTing  ...
	 *
	 *  disable the "unexpected disconnect" feature,
	 *  and remove the ACK signal.
	 */
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	/*
	 *  Wait for the disconnect.
	 */
	SCR_WAIT_DISC,
		0,
	/*
	 *  Status is: DISCONNECTED.
	 */
	SCR_LOAD_REG (HS_REG, HS_DISCONNECT),
		0,
	/*
	 *  Save host status.
	 */
	SCR_STORE_REL (scr0, 4),
		offsetof (struct sym_ccb, phys.status),
	/*
	 *  If QUIRK_AUTOSAVE is set,
	 *  do an "save pointer" operation.
	 */
	SCR_FROM_REG (QU_REG),
		0,
	SCR_JUMP ^ IFFALSE (MASK (SYM_QUIRK_AUTOSAVE, SYM_QUIRK_AUTOSAVE)),
		PADDR (start),
	/*
	 *  like SAVE_DP message:
	 *  Remember we saved the data pointer.
	 *  Copy data pointer to SAVEP.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_DP_SAVED),
		0,
	SCR_STORE_REL (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< IDLE >------------------------*/,{
	/*
	 *  Nothing to do?
	 *  Wait for reselect.
	 *  This NOP will be patched with LED OFF
	 *  SCR_REG_REG (gpreg, SCR_OR, 0x01)
	 */
	SCR_NO_OP,
		0,
#ifdef SYM_CONF_IARB_SUPPORT
	SCR_JUMPR,
		8,
#endif
}/*-------------------------< UNGETJOB >-----------------*/,{
#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  Set IMMEDIATE ARBITRATION, for the next time.
	 *  This will give us better chance to win arbitration 
	 *  for the job we just wanted to do.
	 */
	SCR_REG_REG (scntl1, SCR_OR, IARB),
		0,
#endif
	/*
	 *  We are not able to restart the SCRIPTS if we are 
	 *  interrupted and these instruction haven't been 
	 *  all executed. BTW, this is very unlikely to 
	 *  happen, but we check that from the C code.
	 */
	SCR_LOAD_REG (dsa, 0xff),
		0,
	SCR_STORE_ABS (scratcha, 4),
		PADDRH (startpos),
}/*-------------------------< RESELECT >--------------------*/,{
	/*
	 *  Make sure we are in initiator mode.
	 */
	SCR_CLR (SCR_TRG),
		0,
	/*
	 *  Sleep waiting for a reselection.
	 */
	SCR_WAIT_RESEL,
		PADDR(start),
}/*-------------------------< RESELECTED >------------------*/,{
	/*
	 *  This NOP will be patched with LED ON
	 *  SCR_REG_REG (gpreg, SCR_AND, 0xfe)
	 */
	SCR_NO_OP,
		0,
	/*
	 *  load the target id into the sdid
	 */
	SCR_REG_SFBR (ssid, SCR_AND, 0x8F),
		0,
	SCR_TO_REG (sdid),
		0,
	/*
	 *  Load the target control block address
	 */
	SCR_LOAD_ABS (dsa, 4),
		PADDRH (targtbl),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0x3c),
		0,
	SCR_LOAD_REL (dsa, 4),
		0,
	/*
	 *  Load the legacy synchronous transfer registers.
	 */
	SCR_LOAD_REL (scntl3, 1),
		offsetof(struct sym_tcb, wval),
	SCR_LOAD_REL (sxfer, 1),
		offsetof(struct sym_tcb, sval),
}/*-------------------------< RESEL_SCNTL4 >------------------*/,{
	/*
	 *  If C1010, patched with the load of SCNTL4 that
	 *  allows a new synchronous timing scheme.
	 *
	 *	SCR_LOAD_REL (scntl4, 1),
	 * 		offsetof(struct tcb, uval),
	 */
	SCR_NO_OP,
		0,
	/*
	 *  We expect MESSAGE IN phase.
	 *  If not, get help from the C code.
	 */
	SCR_INT ^ IFFALSE (WHEN (SCR_MSG_IN)),
		SIR_RESEL_NO_MSG_IN,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin),
	/*
	 *  If IDENTIFY LUN #0, use a faster path 
	 *  to find the LCB structure.
	 */
	SCR_JUMPR ^ IFTRUE (MASK (0x80, 0xbf)),
		56,
	/*
	 *  If message isn't an IDENTIFY, 
	 *  tell the C code about.
	 */
	SCR_INT ^ IFFALSE (MASK (0x80, 0x80)),
		SIR_RESEL_NO_IDENTIFY,
	/*
	 *  It is an IDENTIFY message,
	 *  Load the LUN control block address.
	 */
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct sym_tcb, luntbl_sa),
	SCR_SFBR_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_SHL, 0),
		0,
	SCR_REG_REG (dsa, SCR_AND, 0xfc),
		0,
	SCR_LOAD_REL (dsa, 4),
		0,
	SCR_JUMPR,
		8,
	/*
	 *  LUN 0 special case (but usual one :))
	 */
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct sym_tcb, lun0_sa),
	/*
	 *  Jump indirectly to the reselect action for this LUN.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof(struct sym_lcb, resel_sa),
	SCR_RETURN,
		0,
	/* In normal situations, we jump to RESEL_TAG or RESEL_NO_TAG */
}/*-------------------------< RESEL_TAG >-------------------*/,{
	/*
	 *  ACK the IDENTIFY or TAG previously received.
	 */
	SCR_CLR (SCR_ACK),
		0,
	/*
	 *  It shall be a tagged command.
	 *  Read SIMPLE+TAG.
	 *  The C code will deal with errors.
	 *  Agressive optimization, is'nt it? :)
	 */
	SCR_MOVE_ABS (2) ^ SCR_MSG_IN,
		NADDR (msgin),
	/*
	 *  Load the pointer to the tagged task 
	 *  table for this LUN.
	 */
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct sym_lcb, itlq_tbl_sa),
	/*
	 *  The SIDL still contains the TAG value.
	 *  Agressive optimization, isn't it? :):)
	 */
	SCR_REG_SFBR (sidl, SCR_SHL, 0),
		0,
#if SYM_CONF_MAX_TASK*4 > 512
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 2),
		0,
	SCR_REG_REG (sfbr, SCR_SHL, 0),
		0,
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#elif SYM_CONF_MAX_TASK*4 > 256
	SCR_JUMPR ^ IFFALSE (CARRYSET),
		8,
	SCR_REG_REG (dsa1, SCR_OR, 1),
		0,
#endif
	/*
	 *  Retrieve the DSA of this task.
	 *  JUMP indirectly to the restart point of the CCB.
	 */
	SCR_SFBR_REG (dsa, SCR_AND, 0xfc),
		0,
	SCR_LOAD_REL (dsa, 4),
		0,
	SCR_LOAD_REL (temp, 4),
		offsetof(struct sym_ccb, phys.go.restart),
	SCR_RETURN,
		0,
	/* In normal situations we branch to RESEL_DSA */
}/*-------------------------< RESEL_DSA >-------------------*/,{
	/*
	 *  ACK the IDENTIFY or TAG previously received.
	 */
	SCR_CLR (SCR_ACK),
		0,
}/*-------------------------< RESEL_DSA1 >------------------*/,{
	/*
	 *      load the savep (saved pointer) into
	 *      the actual data pointer.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	/*
	 *      Initialize the status registers
	 */
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct sym_ccb, phys.status),
	/*
	 *  Jump to dispatcher.
	 */
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< RESEL_NO_TAG >-------------------*/,{
	/*
	 *  Load the DSA with the unique ITL task.
	 */
	SCR_LOAD_REL (dsa, 4),
		offsetof(struct sym_lcb, itl_task_sa),
	/*
	 *  JUMP indirectly to the restart point of the CCB.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof(struct sym_ccb, phys.go.restart),
	SCR_RETURN,
		0,
	/* In normal situations we branch to RESEL_DSA */
}/*-------------------------< DATA_IN >--------------------*/,{
/*
 *  Because the size depends on the
 *  #define SYM_CONF_MAX_SG parameter,
 *  it is filled in at runtime.
 *
 *  ##===========< i=0; i<SYM_CONF_MAX_SG >=========
 *  ||	SCR_CHMOV_TBL ^ SCR_DATA_IN,
 *  ||		offsetof (struct dsb, data[ i]),
 *  ##==========================================
 */
0
}/*-------------------------< DATA_IN2 >-------------------*/,{
	SCR_CALL,
		PADDR (datai_done),
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-------------------------< DATA_OUT >--------------------*/,{
/*
 *  Because the size depends on the
 *  #define SYM_CONF_MAX_SG parameter,
 *  it is filled in at runtime.
 *
 *  ##===========< i=0; i<SYM_CONF_MAX_SG >=========
 *  ||	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
 *  ||		offsetof (struct dsb, data[ i]),
 *  ##==========================================
 */
0
}/*-------------------------< DATA_OUT2 >-------------------*/,{
	SCR_CALL,
		PADDR (datao_done),
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-------------------------< PM0_DATA >--------------------*/,{
	/*
	 *  Keep track we are executing the PM0 DATA 
	 *  mini-script.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM0),
		0,
	/*
	 *  MOVE the data according to the actual 
	 *  DATA direction.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_IN)),
		16,
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.pm0.sg),
	SCR_JUMPR,
		8,
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct sym_ccb, phys.pm0.sg),
	/*
	 *  Clear the flag that told we were in 
	 *  the PM0 DATA mini-script.
	 */
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM0)),
		0,
	/*
	 *  Return to the previous DATA script which 
	 *  is guaranteed by design (if no bug) to be 
	 *  the main DATA script for this transfer.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof (struct sym_ccb, phys.pm0.ret),
	SCR_RETURN,
		0,
}/*-------------------------< PM1_DATA >--------------------*/,{
	/*
	 *  Keep track we are executing the PM1 DATA 
	 *  mini-script.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_IN_PM1),
		0,
	/*
	 *  MOVE the data according to the actual 
	 *  DATA direction.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_IN)),
		16,
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.pm1.sg),
	SCR_JUMPR,
		8,
	SCR_CHMOV_TBL ^ SCR_DATA_OUT,
		offsetof (struct sym_ccb, phys.pm1.sg),
	/*
	 *  Clear the flag that told we were in 
	 *  the PM1 DATA mini-script.
	 */
	SCR_REG_REG (HF_REG, SCR_AND, (~HF_IN_PM1)),
		0,
	/*
	 *  Return to the previous DATA script which 
	 *  is guaranteed by design (if no bug) to be 
	 *  the main DATA script for this transfer.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof (struct sym_ccb, phys.pm1.ret),
	SCR_RETURN,
		0,
}/*---------------------------------------------------------*/
};

static struct sym_scrh scripth0 = {
/*------------------------< START64 >-----------------------*/{
	/*
	 *  SCRIPT entry point for the 895A, 896 and 1010.
	 *  For now, there is no specific stuff for those 
	 *  chips at this point, but this may come.
	 */
	SCR_JUMP,
		PADDR (init),
}/*-------------------------< NO_DATA >-------------------*/,{
	SCR_JUMP,
		PADDRH (data_ovrun),
}/*-----------------------< SEL_FOR_ABORT >------------------*/,{
	/*
	 *  We are jumped here by the C code, if we have 
	 *  some target to reset or some disconnected 
	 *  job to abort. Since error recovery is a serious 
	 *  busyness, we will really reset the SCSI BUS, if 
	 *  case of a SCSI interrupt occuring in this path.
	 */

	/*
	 *  Set initiator mode.
	 */
	SCR_CLR (SCR_TRG),
		0,
	/*
	 *      And try to select this target.
	 */
	SCR_SEL_TBL_ATN ^ offsetof (struct sym_hcb, abrt_sel),
		PADDR (reselect),
	/*
	 *  Wait for the selection to complete or 
	 *  the selection to time out.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		-8,
	/*
	 *  Call the C code.
	 */
	SCR_INT,
		SIR_TARGET_SELECTED,
	/*
	 *  The C code should let us continue here. 
	 *  Send the 'kiss of death' message.
	 *  We expect an immediate disconnect once 
	 *  the target has eaten the message.
	 */
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_TBL ^ SCR_MSG_OUT,
		offsetof (struct sym_hcb, abrt_tbl),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	/*
	 *  Tell the C code that we are done.
	 */
	SCR_INT,
		SIR_ABORT_SENT,
}/*-----------------------< SEL_FOR_ABORT_1 >--------------*/,{
	/*
	 *  Jump at scheduler.
	 */
	SCR_JUMP,
		PADDR (start),

}/*------------------------< SELECT_NO_ATN >-----------------*/,{
	/*
	 *  Set Initiator mode.
	 *  And try to select this target without ATN.
	 */
	SCR_CLR (SCR_TRG),
		0,
	SCR_SEL_TBL ^ offsetof (struct dsb, select),
		PADDR (ungetjob),
	/*
	 *  load the savep (saved pointer) into
	 *  the actual data pointer.
	 */
	SCR_LOAD_REL (temp, 4),
		offsetof (struct sym_ccb, phys.savep),
	/*
	 *  Initialize the status registers
	 */
	SCR_LOAD_REL (scr0, 4),
		offsetof (struct sym_ccb, phys.status),
}/*------------------------< WF_SEL_DONE_NO_ATN >-----------------*/,{
	/*
	 *  Wait immediately for the next phase or 
	 *  the selection to complete or time-out.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		0,
	SCR_JUMP,
		PADDR (select2),
}/*-------------------------< MSG_IN_ETC >--------------------*/,{
	/*
	 *  If it is an EXTENDED (variable size message)
	 *  Handle it.
	 */
	SCR_JUMP ^ IFTRUE (DATA (M_EXTENDED)),
		PADDRH (msg_extended),
	/*
	 *  Let the C code handle any other 
	 *  1 byte message.
	 */
	SCR_INT ^ IFTRUE (MASK (0x00, 0xf0)),
		SIR_MSG_RECEIVED,
	SCR_INT ^ IFTRUE (MASK (0x10, 0xf0)),
		SIR_MSG_RECEIVED,
	/*
	 *  We donnot handle 2 bytes messages from SCRIPTS.
	 *  So, let the C code deal with these ones too.
	 */
	SCR_INT ^ IFFALSE (MASK (0x20, 0xf0)),
		SIR_MSG_WEIRD,
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	SCR_INT,
		SIR_MSG_RECEIVED,

}/*-------------------------< MSG_RECEIVED >--------------------*/,{
	SCR_LOAD_REL (scratcha, 4),	/* DUMMY READ */
		0,
	SCR_INT,
		SIR_MSG_RECEIVED,

}/*-------------------------< MSG_WEIRD_SEEN >------------------*/,{
	SCR_LOAD_REL (scratcha, 4),	/* DUMMY READ */
		0,
	SCR_INT,
		SIR_MSG_WEIRD,

}/*-------------------------< MSG_EXTENDED >--------------------*/,{
	/*
	 *  Clear ACK and get the next byte 
	 *  assumed to be the message length.
	 */
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (msgin[1]),
	/*
	 *  Try to catch some unlikely situations as 0 length 
	 *  or too large the length.
	 */
	SCR_JUMP ^ IFTRUE (DATA (0)),
		PADDRH (msg_weird_seen),
	SCR_TO_REG (scratcha),
		0,
	SCR_REG_REG (sfbr, SCR_ADD, (256-8)),
		0,
	SCR_JUMP ^ IFTRUE (CARRYSET),
		PADDRH (msg_weird_seen),
	/*
	 *  We donnot handle extended messages from SCRIPTS.
	 *  Read the amount of data correponding to the 
	 *  message length and call the C code.
	 */
	SCR_STORE_REL (scratcha, 1),
		offsetof (struct dsb, smsg_ext.size),
	SCR_CLR (SCR_ACK),
		0,
	SCR_MOVE_TBL ^ SCR_MSG_IN,
		offsetof (struct dsb, smsg_ext),
	SCR_JUMP,
		PADDRH (msg_received),

}/*-------------------------< MSG_BAD >------------------*/,{
	/*
	 *  unimplemented message - reject it.
	 */
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (clrack),
}/*-------------------------< MSG_WEIRD >--------------------*/,{
	/*
	 *  weird message received
	 *  ignore all MSG IN phases and reject it.
	 */
	SCR_INT,
		SIR_REJECT_TO_SEND,
	SCR_SET (SCR_ATN),
		0,
}/*-------------------------< MSG_WEIRD1 >--------------------*/,{
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_IN)),
		PADDR (dispatch),
	SCR_MOVE_ABS (1) ^ SCR_MSG_IN,
		NADDR (scratch),
	SCR_JUMP,
		PADDRH (msg_weird1),
}/*-------------------------< WDTR_RESP >----------------*/,{
	/*
	 *  let the target fetch our answer.
	 */
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),
}/*-------------------------< SEND_WDTR >----------------*/,{
	/*
	 *  Send the M_X_WIDE_REQ
	 */
	SCR_MOVE_ABS (4) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),
}/*-------------------------< SDTR_RESP >-------------*/,{
	/*
	 *  let the target fetch our answer.
	 */
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),
}/*-------------------------< SEND_SDTR >-------------*/,{
	/*
	 *  Send the M_X_SYNC_REQ
	 */
	SCR_MOVE_ABS (5) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),
}/*-------------------------< PPR_RESP >-------------*/,{
	/*
	 *  let the target fetch our answer.
	 */
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	SCR_JUMP ^ IFFALSE (WHEN (SCR_MSG_OUT)),
		PADDRH (nego_bad_phase),
}/*-------------------------< SEND_PPR >-------------*/,{
	/*
	 *  Send the M_X_PPR_REQ
	 */
	SCR_MOVE_ABS (8) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_JUMP,
		PADDRH (msg_out_done),
}/*-------------------------< NEGO_BAD_PHASE >------------*/,{
	SCR_INT,
		SIR_NEGO_PROTO,
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< MSG_OUT >-------------------*/,{
	/*
	 *  The target requests a message.
	 *  We donnot send messages that may 
	 *  require the device to go to bus free.
	 */
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	/*
	 *  ... wait for the next phase
	 *  if it's a message out, send it again, ...
	 */
	SCR_JUMP ^ IFTRUE (WHEN (SCR_MSG_OUT)),
		PADDRH (msg_out),
}/*-------------------------< MSG_OUT_DONE >--------------*/,{
	/*
	 *  Let the C code be aware of the 
	 *  sent message and clear the message.
	 */
	SCR_INT,
		SIR_MSG_OUT_DONE,
	/*
	 *  ... and process the next phase
	 */
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< NO_DATA >--------------------*/,{
	/*
	 *  The target may want to transfer too much data.
	 *
	 *  If phase is DATA OUT write 1 byte and count it.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_OUT)),
		16,
	SCR_CHMOV_ABS (1) ^ SCR_DATA_OUT,
		NADDR (scratch),
	SCR_JUMP,
		PADDRH (data_ovrun1),
	/*
	 *  If WSR is set, clear this condition, and 
	 *  count this byte.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_JUMPR ^ IFFALSE (MASK (WSR, WSR)),
		16,
	SCR_REG_REG (scntl2, SCR_OR, WSR),
		0,
	SCR_JUMP,
		PADDRH (data_ovrun1),
	/*
	 *  Finally check against DATA IN phase.
	 *  Jump to dispatcher if not so.
	 *  Read 1 byte otherwise and count it.
	 */
	SCR_JUMP ^ IFFALSE (IF (SCR_DATA_IN)),
		PADDR (dispatch),
	SCR_CHMOV_ABS (1) ^ SCR_DATA_IN,
		NADDR (scratch),
}/*-------------------------< NO_DATA1 >--------------------*/,{
	/*
	 *  Set the extended error flag.
	 */
	SCR_REG_REG (HF_REG, SCR_OR, HF_EXT_ERR),
		0,
	SCR_LOAD_REL (scratcha, 1),
		offsetof (struct sym_ccb, xerr_status),
	SCR_REG_REG (scratcha,  SCR_OR,  XE_EXTRA_DATA),
		0,
	SCR_STORE_REL (scratcha, 1),
		offsetof (struct sym_ccb, xerr_status),
	/*
	 *  Count this byte.
	 *  This will allow to return a negative 
	 *  residual to user.
	 */
	SCR_LOAD_REL (scratcha, 4),
		offsetof (struct sym_ccb, phys.extra_bytes),
	SCR_REG_REG (scratcha,  SCR_ADD,  0x01),
		0,
	SCR_REG_REG (scratcha1, SCR_ADDC, 0),
		0,
	SCR_REG_REG (scratcha2, SCR_ADDC, 0),
		0,
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct sym_ccb, phys.extra_bytes),
	/*
	 *  .. and repeat as required.
	 */
	SCR_JUMP,
		PADDRH (data_ovrun),

}/*-------------------------< ABORT_RESEL >----------------*/,{
	SCR_SET (SCR_ATN),
		0,
	SCR_CLR (SCR_ACK),
		0,
	/*
	 *  send the abort/abortag/reset message
	 *  we expect an immediate disconnect
	 */
	SCR_REG_REG (scntl2, SCR_AND, 0x7f),
		0,
	SCR_MOVE_ABS (1) ^ SCR_MSG_OUT,
		NADDR (msgout),
	SCR_CLR (SCR_ACK|SCR_ATN),
		0,
	SCR_WAIT_DISC,
		0,
	SCR_INT,
		SIR_RESEL_ABORTED,
	SCR_JUMP,
		PADDR (start),
}/*-------------------------< RESEND_IDENT >-------------------*/,{
	/*
	 *  The target stays in MSG OUT phase after having acked 
	 *  Identify [+ Tag [+ Extended message ]]. Targets shall
	 *  behave this way on parity error.
	 *  We must send it again all the messages.
	 */
	SCR_SET (SCR_ATN), /* Shall be asserted 2 deskew delays before the  */
		0,         /* 1rst ACK = 90 ns. Hope the chip isn't too fast */
	SCR_JUMP,
		PADDR (send_ident),
}/*-------------------------< IDENT_BREAK >-------------------*/,{
	SCR_CLR (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (select2),
}/*-------------------------< IDENT_BREAK_ATN >----------------*/,{
	SCR_SET (SCR_ATN),
		0,
	SCR_JUMP,
		PADDR (select2),
}/*-------------------------< SDATA_IN >-------------------*/,{
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct dsb, sense),
	SCR_CALL,
		PADDR (datai_done),
	SCR_JUMP,
		PADDRH (data_ovrun),

}/*-------------------------< RESEL_BAD_LUN >---------------*/,{
	/*
	 *  Message is an IDENTIFY, but lun is unknown.
	 *  Signal problem to C code for logging the event.
	 *  Send a M_ABORT to clear all pending tasks.
	 */
	SCR_INT,
		SIR_RESEL_BAD_LUN,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_I_T_L >------------------*/,{
	/*
	 *  We donnot have a task for that I_T_L.
	 *  Signal problem to C code for logging the event.
	 *  Send a M_ABORT message.
	 */
	SCR_INT,
		SIR_RESEL_BAD_I_T_L,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_I_T_L_Q >----------------*/,{
	/*
	 *  We donnot have a task that matches the tag.
	 *  Signal problem to C code for logging the event.
	 *  Send a M_ABORTTAG message.
	 */
	SCR_INT,
		SIR_RESEL_BAD_I_T_L_Q,
	SCR_JUMP,
		PADDRH (abort_resel),
}/*-------------------------< BAD_STATUS >-----------------*/,{
	/*
	 *  Anything different from INTERMEDIATE 
	 *  CONDITION MET should be a bad SCSI status, 
	 *  given that GOOD status has already been tested.
	 *  Call the C code.
	 */
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (startpos),
	SCR_INT ^ IFFALSE (DATA (S_COND_MET)),
		SIR_BAD_SCSI_STATUS,
	SCR_RETURN,
		0,

}/*-------------------------< PM_HANDLE >------------------*/,{
	/*
	 *  Phase mismatch handling.
	 *
	 *  Since we have to deal with 2 SCSI data pointers  
	 *  (current and saved), we need at least 2 contexts.
	 *  Each context (pm0 and pm1) has a saved area, a 
	 *  SAVE mini-script and a DATA phase mini-script.
	 */
	/*
	 *  Get the PM handling flags.
	 */
	SCR_FROM_REG (HF_REG),
		0,
	/*
	 *  If no flags (1rst PM for example), avoid 
	 *  all the below heavy flags testing.
	 *  This makes the normal case a bit faster.
	 */
	SCR_JUMP ^ IFTRUE (MASK (0, (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED))),
		PADDRH (pm_handle1),
	/*
	 *  If we received a SAVE DP, switch to the 
	 *  other PM context since the savep may point 
	 *  to the current PM context.
	 */
	SCR_JUMPR ^ IFFALSE (MASK (HF_DP_SAVED, HF_DP_SAVED)),
		8,
	SCR_REG_REG (sfbr, SCR_XOR, HF_ACT_PM),
		0,
	/*
	 *  If we have been interrupt in a PM DATA mini-script,
	 *  we take the return address from the corresponding 
	 *  saved area.
	 *  This ensure the return address always points to the 
	 *  main DATA script for this transfer.
	 */
	SCR_JUMP ^ IFTRUE (MASK (0, (HF_IN_PM0 | HF_IN_PM1))),
		PADDRH (pm_handle1),
	SCR_JUMPR ^ IFFALSE (MASK (HF_IN_PM0, HF_IN_PM0)),
		16,
	SCR_LOAD_REL (ia, 4),
		offsetof(struct sym_ccb, phys.pm0.ret),
	SCR_JUMP,
		PADDRH (pm_save),
	SCR_LOAD_REL (ia, 4),
		offsetof(struct sym_ccb, phys.pm1.ret),
	SCR_JUMP,
		PADDRH (pm_save),
}/*-------------------------< PM_HANDLE1 >-----------------*/,{
	/*
	 *  Normal case.
	 *  Update the return address so that it 
	 *  will point after the interrupted MOVE.
	 */
	SCR_REG_REG (ia, SCR_ADD, 8),
		0,
	SCR_REG_REG (ia1, SCR_ADDC, 0),
		0,
}/*-------------------------< PM_SAVE >--------------------*/,{
	/*
	 *  Clear all the flags that told us if we were 
	 *  interrupted in a PM DATA mini-script and/or 
	 *  we received a SAVE DP.
	 */
	SCR_SFBR_REG (HF_REG, SCR_AND, (~(HF_IN_PM0|HF_IN_PM1|HF_DP_SAVED))),
		0,
	/*
	 *  Choose the current PM context.
	 */
	SCR_JUMP ^ IFTRUE (MASK (HF_ACT_PM, HF_ACT_PM)),
		PADDRH (pm1_save),
}/*-------------------------< PM0_SAVE >-------------------*/,{
	SCR_STORE_REL (ia, 4),
		offsetof(struct sym_ccb, phys.pm0.ret),
	/*
	 *  If WSR bit is set, either UA and RBC may 
	 *  have to be changed whether the device wants 
	 *  to ignore this residue or not.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_CALL ^ IFTRUE (MASK (WSR, WSR)),
		PADDRH (pm_wsr_handle),
	/*
	 *  Save the remaining byte count, the updated 
	 *  address and the return address.
	 */
	SCR_STORE_REL (rbc, 4),
		offsetof(struct sym_ccb, phys.pm0.sg.size),
	SCR_STORE_REL (ua, 4),
		offsetof(struct sym_ccb, phys.pm0.sg.addr),
	/*
	 *  Set the current pointer at the PM0 DATA mini-script.
	 */
	SCR_LOAD_ABS (temp, 4),
		PADDRH (pm0_data_addr),
	SCR_JUMP,
		PADDR (dispatch),
}/*-------------------------< PM1_SAVE >-------------------*/,{
	SCR_STORE_REL (ia, 4),
		offsetof(struct sym_ccb, phys.pm1.ret),
	/*
	 *  If WSR bit is set, either UA and RBC may 
	 *  have to be changed whether the device wants 
	 *  to ignore this residue or not.
	 */
	SCR_FROM_REG (scntl2),
		0,
	SCR_CALL ^ IFTRUE (MASK (WSR, WSR)),
		PADDRH (pm_wsr_handle),
	/*
	 *  Save the remaining byte count, the updated 
	 *  address and the return address.
	 */
	SCR_STORE_REL (rbc, 4),
		offsetof(struct sym_ccb, phys.pm1.sg.size),
	SCR_STORE_REL (ua, 4),
		offsetof(struct sym_ccb, phys.pm1.sg.addr),
	/*
	 *  Set the current pointer at the PM1 DATA mini-script.
	 */
	SCR_LOAD_ABS (temp, 4),
		PADDRH (pm1_data_addr),
	SCR_JUMP,
		PADDR (dispatch),

}/*--------------------------< PM_WSR_HANDLE >-----------------------*/,{
	/*
	 *  Phase mismatch handling from SCRIPT with WSR set.
	 *  Such a condition can occur if the chip wants to 
	 *  execute a CHMOV(size > 1) when the WSR bit is 
	 *  set and the target changes PHASE.
	 */
#ifdef	SYM_DEBUG_PM_WITH_WSR
	/*
	 *  Some debugging may still be needed.:)
	 */ 
	SCR_INT,
		SIR_PM_WITH_WSR,
#endif
	/*
	 *  We must move the residual byte to memory.
	 *
	 *  UA contains bit 0..31 of the address to 
	 *  move the residual byte.
	 *  Move it to the table indirect.
	 */
	SCR_STORE_REL (ua, 4),
		offsetof (struct sym_ccb, phys.wresid.addr),
	/*
	 *  Increment UA (move address to next position).
	 */
	SCR_REG_REG (ua, SCR_ADD, 1),
		0,
	SCR_REG_REG (ua1, SCR_ADDC, 0),
		0,
	SCR_REG_REG (ua2, SCR_ADDC, 0),
		0,
	SCR_REG_REG (ua3, SCR_ADDC, 0),
		0,
	/*
	 *  Compute SCRATCHA as:
	 *  - size to transfer = 1 byte.
	 *  - bit 24..31 = high address bit [32...39].
	 */
	SCR_LOAD_ABS (scratcha, 4),
		PADDRH (zero),
	SCR_REG_REG (scratcha, SCR_OR, 1),
		0,
	SCR_FROM_REG (rbc3),
		0,
	SCR_TO_REG (scratcha3),
		0,
	/*
	 *  Move this value to the table indirect.
	 */
	SCR_STORE_REL (scratcha, 4),
		offsetof (struct sym_ccb, phys.wresid.size),
	/*
	 *  Wait for a valid phase.
	 *  While testing with bogus QUANTUM drives, the C1010 
	 *  sometimes raised a spurious phase mismatch with 
	 *  WSR and the CHMOV(1) triggered another PM.
	 *  Waiting explicitely for the PHASE seemed to avoid 
	 *  the nested phase mismatch. Btw, this didn't happen 
	 *  using my IBM drives.
	 */
	SCR_JUMPR ^ IFFALSE (WHEN (SCR_DATA_IN)),
		0,
	/*
	 *  Perform the move of the residual byte.
	 */
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.wresid),
	/*
	 *  We can now handle the phase mismatch with UA fixed.
	 *  RBC[0..23]=0 is a special case that does not require 
	 *  a PM context. The C code also checks against this.
	 */
	SCR_FROM_REG (rbc),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	SCR_FROM_REG (rbc1),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	SCR_FROM_REG (rbc2),
		0,
	SCR_RETURN ^ IFFALSE (DATA (0)),
		0,
	/*
	 *  RBC[0..23]=0.
	 *  Not only we donnot need a PM context, but this would 
	 *  lead to a bogus CHMOV(0). This condition means that 
	 *  the residual was the last byte to move from this CHMOV.
	 *  So, we just have to move the current data script pointer 
	 *  (i.e. TEMP) to the SCRIPTS address following the 
	 *  interrupted CHMOV and jump to dispatcher.
	 */
	SCR_STORE_ABS (ia, 4),
		PADDRH (scratch),
	SCR_LOAD_ABS (temp, 4),
		PADDRH (scratch),
	SCR_JUMP,
		PADDR (dispatch),
}/*--------------------------< WSR_MA_HELPER >-----------------------*/,{
	/*
	 *  Helper for the C code when WSR bit is set.
	 *  Perform the move of the residual byte.
	 */
	SCR_CHMOV_TBL ^ SCR_DATA_IN,
		offsetof (struct sym_ccb, phys.wresid),
	SCR_JUMP,
		PADDR (dispatch),

}/*-------------------------< ZERO >------------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SCRATCH >---------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< PM0_DATA_ADDR >---------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< PM1_DATA_ADDR >---------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SAVED_DSA >-------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< SAVED_DRS >-------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< DONE_POS >--------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< STARTPOS >--------------------*/,{
	SCR_DATA_ZERO,
}/*-------------------------< TARGTBL >---------------------*/,{
	SCR_DATA_ZERO,

}/*-------------------------< SNOOPTEST >-------------------*/,{
	/*
	 *  Read the variable.
	 */
	SCR_LOAD_REL (scratcha, 4),
		offsetof(struct sym_hcb, cache),
	SCR_STORE_REL (temp, 4),
		offsetof(struct sym_hcb, cache),
	SCR_LOAD_REL (temp, 4),
		offsetof(struct sym_hcb, cache),
}/*-------------------------< SNOOPEND >-------------------*/,{
	/*
	 *  And stop.
	 */
	SCR_INT,
		99,
}/*--------------------------------------------------------*/
};

/*
 *  Fill in #define dependent parts of the scripts
 */
static void sym_fill_scripts (script_p scr, scripth_p scrh)
{
	int	i;
	u32	*p;

	p = scr->data_in;
	for (i=0; i<SYM_CONF_MAX_SG; i++) {
		*p++ =SCR_CHMOV_TBL ^ SCR_DATA_IN;
		*p++ =offsetof (struct dsb, data[i]);
	};
	assert ((u_long)p == (u_long)&scr->data_in + sizeof (scr->data_in));

	p = scr->data_out;
	for (i=0; i<SYM_CONF_MAX_SG; i++) {
		*p++ =SCR_CHMOV_TBL ^ SCR_DATA_OUT;
		*p++ =offsetof (struct dsb, data[i]);
	};
	assert ((u_long)p == (u_long)&scr->data_out + sizeof (scr->data_out));
}

/*
 *  Copy and bind a script.
 */
static void sym_bind_script (hcb_p np, u32 *src, u32 *dst, int len)
{
	u32 opcode, new, old, tmp1, tmp2;
	u32 *start, *end;
	int relocs;
	int opchanged = 0;

	start = src;
	end = src + len/4;

	while (src < end) {

		opcode = *src++;
		*dst++ = cpu_to_scr(opcode);

		/*
		 *  If we forget to change the length
		 *  in scripts, a field will be
		 *  padded with 0. This is an illegal
		 *  command.
		 */
		if (opcode == 0) {
			printf ("%s: ERROR0 IN SCRIPT at %d.\n",
				sym_name(np), (int) (src-start-1));
			MDELAY (10000);
			continue;
		};

		/*
		 *  We use the bogus value 0xf00ff00f ;-)
		 *  to reserve data area in SCRIPTS.
		 */
		if (opcode == SCR_DATA_ZERO) {
			dst[-1] = 0;
			continue;
		}

		if (DEBUG_FLAGS & DEBUG_SCRIPT)
			printf ("%p:  <%x>\n", (src-1), (unsigned)opcode);

		/*
		 *  We don't have to decode ALL commands
		 */
		switch (opcode >> 28) {
		case 0xf:
			/*
			 *  LOAD / STORE DSA relative, don't relocate.
			 */
			relocs = 0;
			break;
		case 0xe:
			/*
			 *  LOAD / STORE absolute.
			 */
			relocs = 1;
			break;
		case 0xc:
			/*
			 *  COPY has TWO arguments.
			 */
			relocs = 2;
			tmp1 = src[0];
			tmp2 = src[1];
#ifdef	RELOC_KVAR
			if ((tmp1 & RELOC_MASK) == RELOC_KVAR)
				tmp1 = 0;
			if ((tmp2 & RELOC_MASK) == RELOC_KVAR)
				tmp2 = 0;
#endif
			if ((tmp1 ^ tmp2) & 3) {
				printf ("%s: ERROR1 IN SCRIPT at %d.\n",
					sym_name(np), (int) (src-start-1));
				MDELAY (1000);
			}
			/*
			 *  If PREFETCH feature not enabled, remove 
			 *  the NO FLUSH bit if present.
			 */
			if ((opcode & SCR_NO_FLUSH) &&
			    !(np->features & FE_PFEN)) {
				dst[-1] = cpu_to_scr(opcode & ~SCR_NO_FLUSH);
				++opchanged;
			}
			break;
		case 0x0:
			/*
			 *  MOVE/CHMOV (absolute address)
			 */
			if (!(np->features & FE_WIDE))
				dst[-1] = cpu_to_scr(opcode | OPC_MOVE);
			relocs = 1;
			break;
		case 0x1:
			/*
			 *  MOVE/CHMOV (table indirect)
			 */
			if (!(np->features & FE_WIDE))
				dst[-1] = cpu_to_scr(opcode | OPC_MOVE);
			relocs = 0;
			break;
		case 0x8:
			/*
			 *  JUMP / CALL
			 *  dont't relocate if relative :-)
			 */
			if (opcode & 0x00800000)
				relocs = 0;
			else if ((opcode & 0xf8400000) == 0x80400000)/*JUMP64*/
				relocs = 2;
			else
				relocs = 1;
			break;
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
			relocs = 1;
			break;
		default:
			relocs = 0;
			break;
		};

		if (!relocs) {
			*dst++ = cpu_to_scr(*src++);
			continue;
		}
		while (relocs--) {
			old = *src++;

			switch (old & RELOC_MASK) {
			case RELOC_REGISTER:
				new = (old & ~RELOC_MASK) + np->mmio_ba;
				break;
			case RELOC_LABEL:
				new = (old & ~RELOC_MASK) + np->script_ba;
				break;
			case RELOC_LABELH:
				new = (old & ~RELOC_MASK) + np->scripth_ba;
				break;
			case RELOC_SOFTC:
				new = (old & ~RELOC_MASK) + vtobus(np);
				break;
#ifdef	RELOC_KVAR
			case RELOC_KVAR:
				if (((old & ~RELOC_MASK) < SCRIPT_KVAR_FIRST) ||
				    ((old & ~RELOC_MASK) > SCRIPT_KVAR_LAST))
					panic("KVAR out of range");
				new = vtobus(script_kvars[old & ~RELOC_MASK]);
#endif
				break;
			case 0:
				/* Don't relocate a 0 address. */
				if (old == 0) {
					new = old;
					break;
				}
				/* fall through */
			default:
				new = 0;	/* For 'cc' not to complain */
				panic("sym_bind_script: "
				      "weird relocation %x\n", old);
				break;
			}

			*dst++ = cpu_to_scr(new);
		}
	};
}

/*
 *  Print something which allows to retrieve the controler type, 
 *  unit, target, lun concerned by a kernel message.
 */
static void PRINT_TARGET (hcb_p np, int target)
{
	printf ("%s:%d:", sym_name(np), target);
}

static void PRINT_LUN(hcb_p np, int target, int lun)
{
	printf ("%s:%d:%d:", sym_name(np), target, lun);
}

static void PRINT_ADDR (ccb_p cp)
{
	if (cp && cp->cam_ccb)
		xpt_print_path(cp->cam_ccb->ccb_h.path);
}

/*
 *  Take into account this ccb in the freeze count.
 *  The flag that tells user about avoids doing that 
 *  more than once for a ccb.
 */	
static void sym_freeze_cam_ccb(union ccb *ccb)
{
	if (!(ccb->ccb_h.flags & CAM_DEV_QFRZDIS)) {
		if (!(ccb->ccb_h.status & CAM_DEV_QFRZN)) {
			ccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(ccb->ccb_h.path, 1);
		}
	}
}

/*
 *  Set the status field of a CAM CCB.
 */
static __inline void sym_set_cam_status(union ccb *ccb, cam_status status)
{
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status |= status;
}

/*
 *  Get the status field of a CAM CCB.
 */
static __inline int sym_get_cam_status(union ccb *ccb)
{
	return ccb->ccb_h.status & CAM_STATUS_MASK;
}

/*
 *  Enqueue a CAM CCB.
 */
static void sym_enqueue_cam_ccb(hcb_p np, union ccb *ccb)
{
	assert(!(ccb->ccb_h.status & CAM_SIM_QUEUED));
	ccb->ccb_h.status = CAM_REQ_INPROG;

	ccb->ccb_h.timeout_ch = timeout(sym_timeout, (caddr_t) ccb,
				       ccb->ccb_h.timeout*hz/1000);
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	ccb->ccb_h.sym_hcb_ptr = np;

	sym_insque_tail(sym_qptr(&ccb->ccb_h.sim_links), &np->cam_ccbq);
}

/*
 *  Complete a pending CAM CCB.
 */
static void sym_xpt_done(hcb_p np, union ccb *ccb)
{
	if (ccb->ccb_h.status & CAM_SIM_QUEUED) {
		untimeout(sym_timeout, (caddr_t) ccb, ccb->ccb_h.timeout_ch);
		sym_remque(sym_qptr(&ccb->ccb_h.sim_links));
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		ccb->ccb_h.sym_hcb_ptr = 0;
	}
	if (ccb->ccb_h.flags & CAM_DEV_QFREEZE)
		sym_freeze_cam_ccb(ccb);
	xpt_done(ccb);
}

static void sym_xpt_done2(hcb_p np, union ccb *ccb, int cam_status)
{
	sym_set_cam_status(ccb, cam_status);
	sym_xpt_done(np, ccb);
}

/*
 *  SYMBIOS chip clock divisor table.
 *
 *  Divisors are multiplied by 10,000,000 in order to make 
 *  calculations more simple.
 */
#define _5M 5000000
static u_long div_10M[] =
	{2*_5M, 3*_5M, 4*_5M, 6*_5M, 8*_5M, 12*_5M, 16*_5M};

/*
 *  SYMBIOS chips allow burst lengths of 2, 4, 8, 16, 32, 64,
 *  128 transfers. All chips support at least 16 transfers 
 *  bursts. The 825A, 875 and 895 chips support bursts of up 
 *  to 128 transfers and the 895A and 896 support bursts of up
 *  to 64 transfers. All other chips support up to 16 
 *  transfers bursts.
 *
 *  For PCI 32 bit data transfers each transfer is a DWORD.
 *  It is a QUADWORD (8 bytes) for PCI 64 bit data transfers.
 *  Only the 896 is able to perform 64 bit data transfers.
 *
 *  We use log base 2 (burst length) as internal code, with 
 *  value 0 meaning "burst disabled".
 */

/*
 *  Burst length from burst code.
 */
#define burst_length(bc) (!(bc))? 0 : 1 << (bc)

/*
 *  Burst code from io register bits.
 */
#define burst_code(dmode, ctest4, ctest5) \
	(ctest4) & 0x80? 0 : (((dmode) & 0xc0) >> 6) + ((ctest5) & 0x04) + 1

/*
 *  Set initial io register bits from burst code.
 */
static __inline void sym_init_burst(hcb_p np, u_char bc)
{
	np->rv_ctest4	&= ~0x80;
	np->rv_dmode	&= ~(0x3 << 6);
	np->rv_ctest5	&= ~0x4;

	if (!bc) {
		np->rv_ctest4	|= 0x80;
	}
	else {
		--bc;
		np->rv_dmode	|= ((bc & 0x3) << 6);
		np->rv_ctest5	|= (bc & 0x4);
	}
}


/*
 * Print out the list of targets that have some flag disabled by user.
 */
static void sym_print_targets_flag(hcb_p np, int mask, char *msg)
{
	int cnt;
	int i;

	for (cnt = 0, i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		if (i == np->myaddr)
			continue;
		if (np->target[i].usrflags & mask) {
			if (!cnt++)
				printf("%s: %s disabled for targets",
					sym_name(np), msg);
			printf(" %d", i);
		}
	}
	if (cnt)
		printf(".\n");
}

/*
 *  Save initial settings of some IO registers.
 *  Assumed to have been set by BIOS.
 *  We cannot reset the chip prior to reading the 
 *  IO registers, since informations will be lost.
 *  Since the SCRIPTS processor may be running, this 
 *  is not safe on paper, but it seems to work quite 
 *  well. :)
 */
static void sym_save_initial_setting (hcb_p np)
{
	np->sv_scntl0	= INB(nc_scntl0) & 0x0a;
	np->sv_scntl3	= INB(nc_scntl3) & 0x07;
	np->sv_dmode	= INB(nc_dmode)  & 0xce;
	np->sv_dcntl	= INB(nc_dcntl)  & 0xa8;
	np->sv_ctest3	= INB(nc_ctest3) & 0x01;
	np->sv_ctest4	= INB(nc_ctest4) & 0x80;
	np->sv_gpcntl	= INB(nc_gpcntl);
	np->sv_stest1	= INB(nc_stest1);
	np->sv_stest2	= INB(nc_stest2) & 0x20;
	np->sv_stest4	= INB(nc_stest4);
	if (np->features & FE_C10) {	/* Always large DMA fifo + ultra3 */
		np->sv_scntl4	= INB(nc_scntl4);
		np->sv_ctest5	= INB(nc_ctest5) & 0x04;
	}
	else
		np->sv_ctest5	= INB(nc_ctest5) & 0x24;
}

/*
 *  Prepare io register values used by sym_init() according 
 *  to selected and supported features.
 */
static int sym_prepare_setting(hcb_p np, struct sym_nvram *nvram)
{
	u_char	burst_max;
	u_long	period;
	int i;

	/*
	 *  Wide ?
	 */
	np->maxwide	= (np->features & FE_WIDE)? 1 : 0;

	/*
	 *  Get the frequency of the chip's clock.
	 */
	if	(np->features & FE_QUAD)
		np->multiplier	= 4;
	else if	(np->features & FE_DBLR)
		np->multiplier	= 2;
	else
		np->multiplier	= 1;

	np->clock_khz	= (np->features & FE_CLK80)? 80000 : 40000;
	np->clock_khz	*= np->multiplier;

	if (np->clock_khz != 40000)
		sym_getclock(np, np->multiplier);

	/*
	 * Divisor to be used for async (timer pre-scaler).
	 */
	i = np->clock_divn - 1;
	while (--i >= 0) {
		if (10ul * SYM_CONF_MIN_ASYNC * np->clock_khz > div_10M[i]) {
			++i;
			break;
		}
	}
	np->rv_scntl3 = i+1;

	/*
	 * The C1010 uses hardwired divisors for async.
	 * So, we just throw away, the async. divisor.:-)
	 */
	if (np->features & FE_C10)
		np->rv_scntl3 = 0;

	/*
	 * Minimum synchronous period factor supported by the chip.
	 * Btw, 'period' is in tenths of nanoseconds.
	 */
	period = (4 * div_10M[0] + np->clock_khz - 1) / np->clock_khz;
	if	(period <= 250)		np->minsync = 10;
	else if	(period <= 303)		np->minsync = 11;
	else if	(period <= 500)		np->minsync = 12;
	else				np->minsync = (period + 40 - 1) / 40;

	/*
	 * Check against chip SCSI standard support (SCSI-2,ULTRA,ULTRA2).
	 */
	if	(np->minsync < 25 &&
		 !(np->features & (FE_ULTRA|FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 25;
	else if	(np->minsync < 12 &&
		 !(np->features & (FE_ULTRA2|FE_ULTRA3)))
		np->minsync = 12;

	/*
	 * Maximum synchronous period factor supported by the chip.
	 */
	period = (11 * div_10M[np->clock_divn - 1]) / (4 * np->clock_khz);
	np->maxsync = period > 2540 ? 254 : period / 10;

	/*
	 * If chip is a C1010, guess the sync limits in DT mode.
	 */
	if ((np->features & (FE_C10|FE_ULTRA3)) == (FE_C10|FE_ULTRA3)) {
		if (np->clock_khz == 160000) {
			np->minsync_dt = 9;
			np->maxsync_dt = 50;
		}
	}
	
	/*
	 *  64 bit (53C895A or 53C896) ?
	 */
	if (np->features & FE_64BIT)
#if BITS_PER_LONG > 32
		np->rv_ccntl1	|= (XTIMOD | EXTIBMV);
#else
		np->rv_ccntl1	|= (DDAC);
#endif

	/*
	 *  Phase mismatch handled by SCRIPTS (895A/896/1010) ?
  	 */
	if (np->features & FE_NOPM)
		np->rv_ccntl0	|= (ENPMJ);

 	/*
	 *  C1010 Errata.
	 *  In dual channel mode, contention occurs if internal cycles
	 *  are used. Disable internal cycles.
	 */
	if (np->device_id == PCI_ID_LSI53C1010 && np->revision_id < 0x45)
		np->rv_ccntl0	|=  DILS;

	/*
	 *  Select burst length (dwords)
	 */
	burst_max	= SYM_SETUP_BURST_ORDER;
	if (burst_max == 255)
		burst_max = burst_code(np->sv_dmode, np->sv_ctest4,
				       np->sv_ctest5);
	if (burst_max > 7)
		burst_max = 7;
	if (burst_max > np->maxburst)
		burst_max = np->maxburst;

	/*
	 *  DEL 352 - 53C810 Rev x11 - Part Number 609-0392140 - ITEM 2.
	 *  This chip and the 860 Rev 1 may wrongly use PCI cache line 
	 *  based transactions on LOAD/STORE instructions. So we have 
	 *  to prevent these chips from using such PCI transactions in 
	 *  this driver. The generic ncr driver that does not use 
	 *  LOAD/STORE instructions does not need this work-around.
	 */
	if ((np->device_id == PCI_ID_SYM53C810 &&
	     np->revision_id >= 0x10 && np->revision_id <= 0x11) ||
	    (np->device_id == PCI_ID_SYM53C860 &&
	     np->revision_id <= 0x1))
		np->features &= ~(FE_WRIE|FE_ERL|FE_ERMP);

	/*
	 *  Select all supported special features.
	 *  If we are using on-board RAM for scripts, prefetch (PFEN) 
	 *  does not help, but burst op fetch (BOF) does.
	 *  Disabling PFEN makes sure BOF will be used.
	 */
	if (np->features & FE_ERL)
		np->rv_dmode	|= ERL;		/* Enable Read Line */
	if (np->features & FE_BOF)
		np->rv_dmode	|= BOF;		/* Burst Opcode Fetch */
	if (np->features & FE_ERMP)
		np->rv_dmode	|= ERMP;	/* Enable Read Multiple */
#if 1
	if ((np->features & FE_PFEN) && !np->ram_ba)
#else
	if (np->features & FE_PFEN)
#endif
		np->rv_dcntl	|= PFEN;	/* Prefetch Enable */
	if (np->features & FE_CLSE)
		np->rv_dcntl	|= CLSE;	/* Cache Line Size Enable */
	if (np->features & FE_WRIE)
		np->rv_ctest3	|= WRIE;	/* Write and Invalidate */
	if (np->features & FE_DFS)
		np->rv_ctest5	|= DFS;		/* Dma Fifo Size */

	/*
	 *  Select some other
	 */
	if (SYM_SETUP_PCI_PARITY)
		np->rv_ctest4	|= MPEE; /* Master parity checking */
	if (SYM_SETUP_SCSI_PARITY)
		np->rv_scntl0	|= 0x0a; /*  full arb., ena parity, par->ATN  */

	/*
	 *  Get parity checking, host ID and verbose mode from NVRAM
	 */
	np->myaddr = 255;
	sym_nvram_setup_host (np, nvram);

	/*
	 *  Get SCSI addr of host adapter (set by bios?).
	 */
	if (np->myaddr == 255) {
		np->myaddr = INB(nc_scid) & 0x07;
		if (!np->myaddr)
			np->myaddr = SYM_SETUP_HOST_ID;
	}

	/*
	 *  Prepare initial io register bits for burst length
	 */
	sym_init_burst(np, burst_max);

	/*
	 *  Set SCSI BUS mode.
	 *  - LVD capable chips (895/895A/896/1010) report the 
	 *    current BUS mode through the STEST4 IO register.
	 *  - For previous generation chips (825/825A/875), 
	 *    user has to tell us how to check against HVD, 
	 *    since a 100% safe algorithm is not possible.
	 */
	np->scsi_mode = SMODE_SE;
	if (np->features & (FE_ULTRA2|FE_ULTRA3))
		np->scsi_mode = (np->sv_stest4 & SMODE);
	else if	(np->features & FE_DIFF) {
		if (SYM_SETUP_SCSI_DIFF == 1) {
			if (np->sv_scntl3) {
				if (np->sv_stest2 & 0x20)
					np->scsi_mode = SMODE_HVD;
			}
			else if (nvram->type == SYM_SYMBIOS_NVRAM) {
				if (INB(nc_gpreg) & 0x08)
					np->scsi_mode = SMODE_HVD;
			}
		}
		else if	(SYM_SETUP_SCSI_DIFF == 2)
			np->scsi_mode = SMODE_HVD;
	}
	if (np->scsi_mode == SMODE_HVD)
		np->rv_stest2 |= 0x20;

	/*
	 *  Set LED support from SCRIPTS.
	 *  Ignore this feature for boards known to use a 
	 *  specific GPIO wiring and for the 895A or 896 
	 *  that drive the LED directly.
	 */
	if ((SYM_SETUP_SCSI_LED || nvram->type == SYM_SYMBIOS_NVRAM) &&
	    !(np->features & FE_LEDC) && !(np->sv_gpcntl & 0x01))
		np->features |= FE_LED0;

	/*
	 *  Set irq mode.
	 */
	switch(SYM_SETUP_IRQ_MODE & 3) {
	case 2:
		np->rv_dcntl	|= IRQM;
		break;
	case 1:
		np->rv_dcntl	|= (np->sv_dcntl & IRQM);
		break;
	default:
		break;
	}

	/*
	 *  Configure targets according to driver setup.
	 *  If NVRAM present get targets setup from NVRAM.
	 */
	for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		tcb_p tp = &np->target[i];

		tp->tinfo.user.period = np->minsync;
		tp->tinfo.user.offset = np->maxoffs;
		tp->tinfo.user.width  = np->maxwide ? BUS_16_BIT : BUS_8_BIT;
		tp->usrflags |= (SYM_DISC_ENABLED | SYM_TAGS_ENABLED);
		tp->usrtags = SYM_SETUP_MAX_TAG;

		sym_nvram_setup_target (np, i, nvram);

		if (!tp->usrtags)
			tp->usrflags &= ~SYM_TAGS_ENABLED;
	}

	/*
	 *  Let user know about the settings.
	 */
	i = nvram->type;
	printf("%s: %s NVRAM, ID %d, Fast-%d, %s, %s\n", sym_name(np),
		i  == SYM_SYMBIOS_NVRAM ? "Symbios" :
		(i == SYM_TEKRAM_NVRAM  ? "Tekram" : "No"),
		np->myaddr,
		(np->features & FE_ULTRA3) ? 80 : 
		(np->features & FE_ULTRA2) ? 40 : 
		(np->features & FE_ULTRA)  ? 20 : 10,
		sym_scsi_bus_mode(np->scsi_mode),
		(np->rv_scntl0 & 0xa)	? "parity checking" : "NO parity");
	/*
	 *  Tell him more on demand.
	 */
	if (sym_verbose) {
		printf("%s: %s IRQ line driver%s\n",
			sym_name(np),
			np->rv_dcntl & IRQM ? "totem pole" : "open drain",
			np->ram_ba ? ", using on-chip SRAM" : "");
		if (np->features & FE_NOPM)
			printf("%s: handling phase mismatch from SCRIPTS.\n", 
			       sym_name(np));
	}
	/*
	 *  And still more.
	 */
	if (sym_verbose > 1) {
		printf ("%s: initial SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->sv_scntl3, np->sv_dmode, np->sv_dcntl,
			np->sv_ctest3, np->sv_ctest4, np->sv_ctest5);

		printf ("%s: final   SCNTL3/DMODE/DCNTL/CTEST3/4/5 = "
			"(hex) %02x/%02x/%02x/%02x/%02x/%02x\n",
			sym_name(np), np->rv_scntl3, np->rv_dmode, np->rv_dcntl,
			np->rv_ctest3, np->rv_ctest4, np->rv_ctest5);
	}
	/*
	 *  Let user be aware of targets that have some disable flags set.
	 */
	sym_print_targets_flag(np, SYM_SCAN_BOOT_DISABLED, "SCAN AT BOOT");
	if (sym_verbose)
		sym_print_targets_flag(np, SYM_SCAN_LUNS_DISABLED,
				       "SCAN FOR LUNS");

	return 0;
}

/*
 *  Prepare the next negotiation message if needed.
 *
 *  Fill in the part of message buffer that contains the 
 *  negotiation and the nego_status field of the CCB.
 *  Returns the size of the message in bytes.
 */

static int sym_prepare_nego(hcb_p np, ccb_p cp, int nego, u_char *msgptr)
{
	tcb_p tp = &np->target[cp->target];
	int msglen = 0;

#if 1
	/*
	 *  For now, only use PPR with DT option if period factor = 9.
	 */
	if (tp->tinfo.goal.period == 9) {
		tp->tinfo.goal.width = BUS_16_BIT;
		tp->tinfo.goal.options |= PPR_OPT_DT;
	}
	else
		tp->tinfo.goal.options &= ~PPR_OPT_DT;
#endif
	/*
	 *  Early C1010 chips need a work-around for DT 
	 *  data transfer to work.
	 */
	if (!(np->features & FE_U3EN))
		tp->tinfo.goal.options = 0;
	/*
	 *  negotiate using PPR ?
	 */
	if (tp->tinfo.goal.options & PPR_OPT_MASK)
		nego = NS_PPR;
	/*
	 *  negotiate wide transfers ?
	 */
	else if (tp->tinfo.current.width != tp->tinfo.goal.width)
		nego = NS_WIDE;
	/*
	 *  negotiate synchronous transfers?
	 */
	else if (tp->tinfo.current.period != tp->tinfo.goal.period ||
		 tp->tinfo.current.offset != tp->tinfo.goal.offset)
		nego = NS_SYNC;

	switch (nego) {
	case NS_SYNC:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 3;
		msgptr[msglen++] = M_X_SYNC_REQ;
		msgptr[msglen++] = tp->tinfo.goal.period;
		msgptr[msglen++] = tp->tinfo.goal.offset;
		break;
	case NS_WIDE:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 2;
		msgptr[msglen++] = M_X_WIDE_REQ;
		msgptr[msglen++] = tp->tinfo.goal.width;
		break;
	case NS_PPR:
		msgptr[msglen++] = M_EXTENDED;
		msgptr[msglen++] = 6;
		msgptr[msglen++] = M_X_PPR_REQ;
		msgptr[msglen++] = tp->tinfo.goal.period;
		msgptr[msglen++] = 0;
		msgptr[msglen++] = tp->tinfo.goal.offset;
		msgptr[msglen++] = tp->tinfo.goal.width;
		msgptr[msglen++] = tp->tinfo.goal.options & PPR_OPT_DT;
		break;
	};

	cp->nego_status = nego;

	if (nego) {
		tp->nego_cp = cp; /* Keep track a nego will be performed */
		if (DEBUG_FLAGS & DEBUG_NEGO) {
			sym_print_msg(cp, nego == NS_SYNC ? "sync msgout" :
					  nego == NS_WIDE ? "wide msgout" :
					  "ppr msgout", msgptr);
		};
	};

	return msglen;
}

/*
 *  Insert a job into the start queue.
 */
static void sym_put_start_queue(hcb_p np, ccb_p cp)
{
	u_short	qidx;

#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  If the previously queued CCB is not yet done, 
	 *  set the IARB hint. The SCRIPTS will go with IARB 
	 *  for this job when starting the previous one.
	 *  We leave devices a chance to win arbitration by 
	 *  not using more than 'iarb_max' consecutive 
	 *  immediate arbitrations.
	 */
	if (np->last_cp && np->iarb_count < np->iarb_max) {
		np->last_cp->host_flags |= HF_HINT_IARB;
		++np->iarb_count;
	}
	else
		np->iarb_count = 0;
	np->last_cp = cp;
#endif
	
	/*
	 *  Insert first the idle task and then our job.
	 *  The MB should ensure proper ordering.
	 */
	qidx = np->squeueput + 2;
	if (qidx >= MAX_QUEUE*2) qidx = 0;

	np->squeue [qidx]	   = cpu_to_scr(np->idletask_ba);
	MEMORY_BARRIER();
	np->squeue [np->squeueput] = cpu_to_scr(cp->ccb_ba);

	np->squeueput = qidx;

	if (DEBUG_FLAGS & DEBUG_QUEUE)
		printf ("%s: queuepos=%d.\n", sym_name (np), np->squeueput);

	/*
	 *  Script processor may be waiting for reselect.
	 *  Wake it up.
	 */
	MEMORY_BARRIER();
	OUTB (nc_istat, SIGP|np->istat_sem);
}


/*
 *  Soft reset the chip.
 *
 *  Raising SRST when the chip is running may cause 
 *  problems on dual function chips (see below).
 *  On the other hand, LVD devices need some delay 
 *  to settle and report actual BUS mode in STEST4.
 */
static void sym_chip_reset (hcb_p np)
{
	OUTB (nc_istat, SRST);
	UDELAY (10);
	OUTB (nc_istat, 0);
	UDELAY(2000);	/* For BUS MODE to settle */
}

/*
 *  Soft reset the chip.
 *
 *  Some 896 and 876 chip revisions may hang-up if we set 
 *  the SRST (soft reset) bit at the wrong time when SCRIPTS 
 *  are running.
 *  So, we need to abort the current operation prior to 
 *  soft resetting the chip.
 */
static void sym_soft_reset (hcb_p np)
{
	u_char istat;
	int i;

	OUTB (nc_istat, CABRT);
	for (i = 1000000 ; i ; --i) {
		istat = INB (nc_istat);
		if (istat & SIP) {
			INW (nc_sist);
			continue;
		}
		if (istat & DIP) {
			OUTB (nc_istat, 0);
			INB (nc_dstat);
			break;
		}
	}
	if (!i)
		printf("%s: unable to abort current chip operation.\n",
			sym_name(np));
	sym_chip_reset (np);
}

/*
 *  Start reset process.
 *
 *  The interrupt handler will reinitialize the chip.
 */
static void sym_start_reset(hcb_p np)
{
	(void) sym_reset_scsi_bus(np, 1);
}
 
static int sym_reset_scsi_bus(hcb_p np, int enab_int)
{
	u32 term;
	int retv = 0;

	sym_soft_reset(np);	/* Soft reset the chip */
	if (enab_int)
		OUTW (nc_sien, RST);
	/*
	 *  Enable Tolerant, reset IRQD if present and 
	 *  properly set IRQ mode, prior to resetting the bus.
	 */
	OUTB (nc_stest3, TE);
	OUTB (nc_dcntl, (np->rv_dcntl & IRQM));
	OUTB (nc_scntl1, CRST);
	UDELAY (200);

	if (!SYM_SETUP_SCSI_BUS_CHECK)
		goto out;
	/*
	 *  Check for no terminators or SCSI bus shorts to ground.
	 *  Read SCSI data bus, data parity bits and control signals.
	 *  We are expecting RESET to be TRUE and other signals to be 
	 *  FALSE.
	 */
	term =	INB(nc_sstat0);
	term =	((term & 2) << 7) + ((term & 1) << 17);	/* rst sdp0 */
	term |= ((INB(nc_sstat2) & 0x01) << 26) |	/* sdp1     */
		((INW(nc_sbdl) & 0xff)   << 9)  |	/* d7-0     */
		((INW(nc_sbdl) & 0xff00) << 10) |	/* d15-8    */
		INB(nc_sbcl);	/* req ack bsy sel atn msg cd io    */

	if (!(np->features & FE_WIDE))
		term &= 0x3ffff;

	if (term != (2<<7)) {
		printf("%s: suspicious SCSI data while resetting the BUS.\n",
			sym_name(np));
		printf("%s: %sdp0,d7-0,rst,req,ack,bsy,sel,atn,msg,c/d,i/o = "
			"0x%lx, expecting 0x%lx\n",
			sym_name(np),
			(np->features & FE_WIDE) ? "dp1,d15-8," : "",
			(u_long)term, (u_long)(2<<7));
		if (SYM_SETUP_SCSI_BUS_CHECK == 1)
			retv = 1;
	}
out:
	OUTB (nc_scntl1, 0);
	/* MDELAY(100); */
	return retv;
}

/*
 *  The chip may have completed jobs. Look at the DONE QUEUE.
 */
static int sym_wakeup_done (hcb_p np)
{
	ccb_p cp;
	int i, n;
	u_long dsa;

	n = 0;
	i = np->dqueueget;
	while (1) {
		dsa = scr_to_cpu(np->dqueue[i]);
		if (!dsa)
			break;
		np->dqueue[i] = 0;
		if ((i = i+2) >= MAX_QUEUE*2)
			i = 0;

		cp = sym_ccb_from_dsa(np, dsa);
		if (cp) {
			sym_complete_ok (np, cp);
			++n;
		}
		else
			printf ("%s: bad DSA (%lx) in done queue.\n",
				sym_name(np), dsa);
	}
	np->dqueueget = i;

	return n;
}

/*
 *  Complete all active CCBs with error.
 *  Used on CHIP/SCSI RESET.
 */
static void sym_flush_busy_queue (hcb_p np, int cam_status)
{
	/*
	 *  Move all active CCBs to the COMP queue 
	 *  and flush this queue.
	 */
	sym_que_splice(&np->busy_ccbq, &np->comp_ccbq);
	sym_que_init(&np->busy_ccbq);
	sym_flush_comp_queue(np, cam_status);
}

/*
 *  Start chip.
 *
 *  'reason' means:
 *     0: initialisation.
 *     1: SCSI BUS RESET delivered or received.
 *     2: SCSI BUS MODE changed.
 */
static void sym_init (hcb_p np, int reason)
{
 	int	i;
	u_long	phys;

 	/*
	 *  Reset chip if asked, otherwise just clear fifos.
 	 */
	if (reason == 1)
		sym_soft_reset(np);
	else {
		OUTB (nc_stest3, TE|CSF);
		OUTONB (nc_ctest3, CLF);
	}
 
	/*
	 *  Clear Start Queue
	 */
	phys = vtobus(np->squeue);
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->squeue[i]   = cpu_to_scr(np->idletask_ba);
		np->squeue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->squeue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	/*
	 *  Start at first entry.
	 */
	np->squeueput = 0;
	np->scripth0->startpos[0] = cpu_to_scr(phys);

	/*
	 *  Clear Done Queue
	 */
	phys = vtobus(np->dqueue);
	for (i = 0; i < MAX_QUEUE*2; i += 2) {
		np->dqueue[i]   = 0;
		np->dqueue[i+1] = cpu_to_scr(phys + (i+2)*4);
	}
	np->dqueue[MAX_QUEUE*2-1] = cpu_to_scr(phys);

	/*
	 *  Start at first entry.
	 */
	np->scripth0->done_pos[0] = cpu_to_scr(phys);
	np->dqueueget = 0;

	/*
	 *  Wakeup all pending jobs.
	 */
	sym_flush_busy_queue(np, CAM_SCSI_BUS_RESET);

	/*
	 *  Init chip.
	 */
	OUTB (nc_istat,  0x00   );	/*  Remove Reset, abort */
	UDELAY (2000);	/* The 895 needs time for the bus mode to settle */

	OUTB (nc_scntl0, np->rv_scntl0 | 0xc0);
					/*  full arb., ena parity, par->ATN  */
	OUTB (nc_scntl1, 0x00);		/*  odd parity, and remove CRST!! */

	sym_selectclock(np, np->rv_scntl3);	/* Select SCSI clock */

	OUTB (nc_scid  , RRE|np->myaddr);	/* Adapter SCSI address */
	OUTW (nc_respid, 1ul<<np->myaddr);	/* Id to respond to */
	OUTB (nc_istat , SIGP	);		/*  Signal Process */
	OUTB (nc_dmode , np->rv_dmode);		/* Burst length, dma mode */
	OUTB (nc_ctest5, np->rv_ctest5);	/* Large fifo + large burst */

	OUTB (nc_dcntl , NOCOM|np->rv_dcntl);	/* Protect SFBR */
	OUTB (nc_ctest3, np->rv_ctest3);	/* Write and invalidate */
	OUTB (nc_ctest4, np->rv_ctest4);	/* Master parity checking */

	/* Extended Sreq/Sack filtering not supported on the C10 */
	if (np->features & FE_C10)
		OUTB (nc_stest2, np->rv_stest2);
	else
		OUTB (nc_stest2, EXT|np->rv_stest2);

	OUTB (nc_stest3, TE);			/* TolerANT enable */
	OUTB (nc_stime0, 0x0c);			/* HTH disabled  STO 0.25 sec */

	/*
	 *  C10101 Errata.
	 *  Errant SGE's when in narrow. Write bits 4 & 5 of
	 *  STEST1 register to disable SGE. We probably should do 
	 *  that from SCRIPTS for each selection/reselection, but 
	 *  I just don't want. :)
	 */
	if (np->device_id == PCI_ID_LSI53C1010 && np->revision_id < 0x45)
		OUTB (nc_stest1, INB(nc_stest1) | 0x30);

	/*
	 *  DEL 441 - 53C876 Rev 5 - Part Number 609-0392787/2788 - ITEM 2.
	 *  Disable overlapped arbitration for some dual function devices, 
	 *  regardless revision id (kind of post-chip-design feature. ;-))
	 */
	if (np->device_id == PCI_ID_SYM53C875)
		OUTB (nc_ctest0, (1<<5));
	else if (np->device_id == PCI_ID_SYM53C896)
		np->rv_ccntl0 |= DPR;

	/*
	 *  If 64 bit (895A/896/1010) write CCNTL1 to enable 40 bit 
	 *  address table indirect addressing for MOVE.
	 *  Also write CCNTL0 if 64 bit chip, since this register seems 
	 *  to only be used by 64 bit cores.
	 */
	if (np->features & FE_64BIT) {
		OUTB (nc_ccntl0, np->rv_ccntl0);
		OUTB (nc_ccntl1, np->rv_ccntl1);
	}

	/*
	 *  If phase mismatch handled by scripts (895A/896/1010),
	 *  set PM jump addresses.
	 */
	if (np->features & FE_NOPM) {
		OUTL (nc_pmjad1, SCRIPTH_BA (np, pm_handle));
		OUTL (nc_pmjad2, SCRIPTH_BA (np, pm_handle));
	}

	/*
	 *    Enable GPIO0 pin for writing if LED support from SCRIPTS.
	 *    Also set GPIO5 and clear GPIO6 if hardware LED control.
	 */
	if (np->features & FE_LED0)
		OUTB(nc_gpcntl, INB(nc_gpcntl) & ~0x01);
	else if (np->features & FE_LEDC)
		OUTB(nc_gpcntl, (INB(nc_gpcntl) & ~0x41) | 0x20);

	/*
	 *      enable ints
	 */
	OUTW (nc_sien , STO|HTH|MA|SGE|UDC|RST|PAR);
	OUTB (nc_dien , MDPE|BF|SSI|SIR|IID);

	/*
	 *  For 895/6 enable SBMC interrupt and save current SCSI bus mode.
	 *  Try to eat the spurious SBMC interrupt that may occur when 
	 *  we reset the chip but not the SCSI BUS (at initialization).
	 */
	if (np->features & (FE_ULTRA2|FE_ULTRA3)) {
		OUTONW (nc_sien, SBMC);
		if (reason == 0) {
			MDELAY(100);
			INW (nc_sist);
		}
		np->scsi_mode = INB (nc_stest4) & SMODE;
	}

	/*
	 *  Fill in target structure.
	 *  Reinitialize usrsync.
	 *  Reinitialize usrwide.
	 *  Prepare sync negotiation according to actual SCSI bus mode.
	 */
	for (i=0;i<SYM_CONF_MAX_TARGET;i++) {
		tcb_p tp = &np->target[i];

		tp->to_reset = 0;
		tp->sval    = 0;
		tp->wval    = np->rv_scntl3;
		tp->uval    = 0;

		tp->tinfo.current.period = 0;
		tp->tinfo.current.offset = 0;
		tp->tinfo.current.width  = BUS_8_BIT;
		tp->tinfo.current.options = 0;
	}

	/*
	 *  Download SCSI SCRIPTS to on-chip RAM if present,
	 *  and start script processor.
	 */
	if (np->ram_ba) {
		if (sym_verbose > 1)
			printf ("%s: Downloading SCSI SCRIPTS.\n",
				sym_name(np));
		if (np->ram_ws == 8192) {
			memcpy_to_pci(np->ram_va + 4096,
					np->scripth0, sizeof(struct sym_scrh));
			OUTL (nc_mmws, np->scr_ram_seg);
			OUTL (nc_mmrs, np->scr_ram_seg);
			OUTL (nc_sfs,  np->scr_ram_seg);
			phys = SCRIPTH_BA (np, start64);
		}
		else
			phys = SCRIPT_BA (np, init);
		memcpy_to_pci(np->ram_va,np->script0,sizeof(struct sym_scr));
	}
	else
		phys = SCRIPT_BA (np, init);

	np->istat_sem = 0;

	MEMORY_BARRIER();
	OUTL (nc_dsa, vtobus(np));
	OUTL (nc_dsp, phys);

	/*
	 *  Notify the XPT about the RESET condition.
	 */
	if (reason != 0)
		xpt_async(AC_BUS_RESET, np->path, NULL);
}

/*
 *  Get clock factor and sync divisor for a given 
 *  synchronous factor period.
 */
static int 
sym_getsync(hcb_p np, u_char dt, u_char sfac, u_char *divp, u_char *fakp)
{
	u32	clk = np->clock_khz;	/* SCSI clock frequency in kHz	*/
	int	div = np->clock_divn;	/* Number of divisors supported	*/
	u32	fak;			/* Sync factor in sxfer		*/
	u32	per;			/* Period in tenths of ns	*/
	u32	kpc;			/* (per * clk)			*/
	int	ret;

	/*
	 *  Compute the synchronous period in tenths of nano-seconds
	 */
	if (dt && sfac <= 9)	per = 125;
	else if	(sfac <= 10)	per = 250;
	else if	(sfac == 11)	per = 303;
	else if	(sfac == 12)	per = 500;
	else			per = 40 * sfac;
	ret = per;

	kpc = per * clk;
	if (dt)
		kpc <<= 1;

	/*
	 *  For earliest C10, the extra clocks does not apply 
	 *  to CRC cycles, so it may be safe not to use them.
	 *  Note that this limits the lowest sync data transfer 
	 *  to 5 Mega-transfers per second and may result in
	 *  using higher clock divisors.
	 */
#if 1
	if ((np->features & (FE_C10|FE_U3EN)) == FE_C10) {
		/*
		 *  Look for the lowest clock divisor that allows an 
		 *  output speed not faster than the period.
		 */
		while (div > 0) {
			--div;
			if (kpc > (div_10M[div] << 2)) {
				++div;
				break;
			}
		}
		fak = 0;			/* No extra clocks */
		if (div == np->clock_divn) {	/* Are we too fast ? */
			ret = -1;
		}
		*divp = div;
		*fakp = fak;
		return ret;
	}
#endif

	/*
	 *  Look for the greatest clock divisor that allows an 
	 *  input speed faster than the period.
	 */
	while (div-- > 0)
		if (kpc >= (div_10M[div] << 2)) break;

	/*
	 *  Calculate the lowest clock factor that allows an output 
	 *  speed not faster than the period, and the max output speed.
	 *  If fak >= 1 we will set both XCLKH_ST and XCLKH_DT.
	 *  If fak >= 2 we will also set XCLKS_ST and XCLKS_DT.
	 */
	if (dt) {
		fak = (kpc - 1) / (div_10M[div] << 1) + 1 - 2;
		/* ret = ((2+fak)*div_10M[div])/np->clock_khz; */
	}
	else {
		fak = (kpc - 1) / div_10M[div] + 1 - 4;
		/* ret = ((4+fak)*div_10M[div])/np->clock_khz; */
	}

	/*
	 *  Check against our hardware limits, or bugs :).
	 */
	if (fak < 0)	{fak = 0; ret = -1;}
	if (fak > 2)	{fak = 2; ret = -1;}

	/*
	 *  Compute and return sync parameters.
	 */
	*divp = div;
	*fakp = fak;

	return ret;
}

/*
 *  We received a WDTR.
 *  Let everything be aware of the changes.
 */
static void sym_setwide(hcb_p np, ccb_p cp, u_char wide)
{
	struct	ccb_trans_settings neg;
	union ccb *ccb = cp->cam_ccb;
	tcb_p tp = &np->target[cp->target];

	sym_settrans(np, cp, 0, 0, 0, wide, 0, 0);

	/*
	 *  Tell the SCSI layer about the new transfer parameters.
	 */
	tp->tinfo.goal.width = tp->tinfo.current.width = wide;
	tp->tinfo.current.offset = 0;
	tp->tinfo.current.period = 0;
	tp->tinfo.current.options = 0;
	neg.bus_width = wide ? BUS_16_BIT : BUS_8_BIT;
	neg.sync_period = tp->tinfo.current.period;
	neg.sync_offset = tp->tinfo.current.offset;
	neg.valid = CCB_TRANS_BUS_WIDTH_VALID
		  | CCB_TRANS_SYNC_RATE_VALID
		  | CCB_TRANS_SYNC_OFFSET_VALID;
	xpt_setup_ccb(&neg.ccb_h, ccb->ccb_h.path, /*priority*/1);
	xpt_async(AC_TRANSFER_NEG, ccb->ccb_h.path, &neg);
}

/*
 *  We received a SDTR.
 *  Let everything be aware of the changes.
 */
static void
sym_setsync(hcb_p np, ccb_p cp, u_char ofs, u_char per, u_char div, u_char fak)
{
	struct	ccb_trans_settings neg;
	union ccb *ccb = cp->cam_ccb;
	tcb_p tp = &np->target[cp->target];
	u_char wide = (cp->phys.select.sel_scntl3 & EWS) ? 1 : 0;

	sym_settrans(np, cp, 0, ofs, per, wide, div, fak);

	/*
	 *  Tell the SCSI layer about the new transfer parameters.
	 */
	tp->tinfo.goal.period	= tp->tinfo.current.period  = per;
	tp->tinfo.goal.offset	= tp->tinfo.current.offset  = ofs;
	tp->tinfo.goal.options	= tp->tinfo.current.options = 0;
	neg.sync_period = tp->tinfo.current.period;
	neg.sync_offset = tp->tinfo.current.offset;
	neg.valid = CCB_TRANS_SYNC_RATE_VALID
		  | CCB_TRANS_SYNC_OFFSET_VALID;
	xpt_setup_ccb(&neg.ccb_h, ccb->ccb_h.path, /*priority*/1);
	xpt_async(AC_TRANSFER_NEG, ccb->ccb_h.path, &neg);
}

/*
 *  We received a PPR.
 *  Let everything be aware of the changes.
 */
static void sym_setpprot(hcb_p np, ccb_p cp, u_char dt, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak)
{
	struct	ccb_trans_settings neg;
	union ccb *ccb = cp->cam_ccb;
	tcb_p tp = &np->target[cp->target];

	sym_settrans(np, cp, dt, ofs, per, wide, div, fak);

	/*
	 *  Tell the SCSI layer about the new transfer parameters.
	 */
	tp->tinfo.goal.width	= tp->tinfo.current.width  = wide;
	tp->tinfo.goal.period	= tp->tinfo.current.period = per;
	tp->tinfo.goal.offset	= tp->tinfo.current.offset = ofs;
	tp->tinfo.goal.options	= tp->tinfo.current.options = dt;
	neg.sync_period = tp->tinfo.current.period;
	neg.sync_offset = tp->tinfo.current.offset;
	neg.bus_width = wide ? BUS_16_BIT : BUS_8_BIT;
	neg.valid = CCB_TRANS_BUS_WIDTH_VALID
		  | CCB_TRANS_SYNC_RATE_VALID
		  | CCB_TRANS_SYNC_OFFSET_VALID;
	xpt_setup_ccb(&neg.ccb_h, ccb->ccb_h.path, /*priority*/1);
	xpt_async(AC_TRANSFER_NEG, ccb->ccb_h.path, &neg);
}

/*
 *  Switch trans mode for current job and it's target.
 */
static void sym_settrans(hcb_p np, ccb_p cp, u_char dt, u_char ofs,
			 u_char per, u_char wide, u_char div, u_char fak)
{
	SYM_QUEHEAD *qp;
	union	ccb *ccb;
	tcb_p tp;
	u_char target = INB (nc_sdid) & 0x0f;
	u_char sval, wval, uval;

	assert (cp);
	if (!cp) return;
	ccb = cp->cam_ccb;
	assert (ccb);
	if (!ccb) return;
	assert (target == (cp->target & 0xf));
	tp = &np->target[target];

	sval = tp->sval;
	wval = tp->wval;
	uval = tp->uval;

#if 0
	printf("XXXX sval=%x wval=%x uval=%x (%x)\n", 
		sval, wval, uval, np->rv_scntl3);
#endif
	/*
	 *  Set the offset.
	 */
	if (!(np->features & FE_C10))
		sval = (sval & ~0x1f) | ofs;
	else
		sval = (sval & ~0x3f) | ofs;

	/*
	 *  Set the sync divisor and extra clock factor.
	 */
	if (ofs != 0) {
		wval = (wval & ~0x70) | ((div+1) << 4);
		if (!(np->features & FE_C10))
			sval = (sval & ~0xe0) | (fak << 5);
		else {
			uval = uval & ~(XCLKH_ST|XCLKH_DT|XCLKS_ST|XCLKS_DT);
			if (fak >= 1) uval |= (XCLKH_ST|XCLKH_DT);
			if (fak >= 2) uval |= (XCLKS_ST|XCLKS_DT);
		}
	}

	/*
	 *  Set the bus width.
	 */
	wval = wval & ~EWS;
	if (wide != 0)
		wval |= EWS;

	/*
	 *  Set misc. ultra enable bits.
	 */
	if (np->features & FE_C10) {
		uval = uval & ~U3EN;
		if (dt)	{
			assert(np->features & FE_U3EN);
			uval |= U3EN;
		}
	}
	else {
		wval = wval & ~ULTRA;
		if (per <= 12)	wval |= ULTRA;
	}

	/*
	 *   Stop there if sync parameters are unchanged.
	 */
	if (tp->sval == sval && tp->wval == wval && tp->uval == uval) return;
	tp->sval = sval;
	tp->wval = wval;
	tp->uval = uval;

	/*
	 *  Disable extended Sreq/Sack filtering if per < 50.
	 *  Not supported on the C1010.
	 */
	if (per < 50 && !(np->features & FE_C10))
		OUTOFFB (nc_stest2, EXT);

	/*
	 *  set actual value and sync_status
	 */
	OUTB (nc_sxfer, tp->sval);
	OUTB (nc_scntl3, tp->wval);

	if (np->features & FE_C10) {
		OUTB (nc_scntl4, tp->uval);
	}

	/*
	 *  patch ALL busy ccbs of this target.
	 */
	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp->target != target)
			continue;
		cp->phys.select.sel_scntl3 = tp->wval;
		cp->phys.select.sel_sxfer  = tp->sval;
		if (np->features & FE_C10) {
			cp->phys.select.sel_scntl4 = tp->uval;
		}
	}
}

/*
 *  log message for real hard errors
 *
 *  sym0 targ 0?: ERROR (ds:si) (so-si-sd) (sxfer/scntl3) @ name (dsp:dbc).
 *  	      reg: r0 r1 r2 r3 r4 r5 r6 ..... rf.
 *
 *  exception register:
 *  	ds:	dstat
 *  	si:	sist
 *
 *  SCSI bus lines:
 *  	so:	control lines as driven by chip.
 *  	si:	control lines as seen by chip.
 *  	sd:	scsi data lines as seen by chip.
 *
 *  wide/fastmode:
 *  	sxfer:	(see the manual)
 *  	scntl3:	(see the manual)
 *
 *  current script command:
 *  	dsp:	script adress (relative to start of script).
 *  	dbc:	first word of script command.
 *
 *  First 24 register of the chip:
 *  	r0..rf
 */
static void sym_log_hard_error(hcb_p np, u_short sist, u_char dstat)
{
	u32	dsp;
	int	script_ofs;
	int	script_size;
	char	*script_name;
	u_char	*script_base;
	int	i;

	dsp	= INL (nc_dsp);

	if (dsp > np->script_ba &&
	    dsp <= np->script_ba + sizeof(struct sym_scr)) {
		script_ofs	= dsp - np->script_ba;
		script_size	= sizeof(struct sym_scr);
		script_base	= (u_char *) np->script0;
		script_name	= "script";
	}
	else if (np->scripth_ba < dsp && 
		 dsp <= np->scripth_ba + sizeof(struct sym_scrh)) {
		script_ofs	= dsp - np->scripth_ba;
		script_size	= sizeof(struct sym_scrh);
		script_base	= (u_char *) np->scripth0;
		script_name	= "scripth";
	} else {
		script_ofs	= dsp;
		script_size	= 0;
		script_base	= 0;
		script_name	= "mem";
	}

	printf ("%s:%d: ERROR (%x:%x) (%x-%x-%x) (%x/%x) @ (%s %x:%08x).\n",
		sym_name (np), (unsigned)INB (nc_sdid)&0x0f, dstat, sist,
		(unsigned)INB (nc_socl), (unsigned)INB (nc_sbcl),
		(unsigned)INB (nc_sbdl), (unsigned)INB (nc_sxfer),
		(unsigned)INB (nc_scntl3), script_name, script_ofs,
		(unsigned)INL (nc_dbc));

	if (((script_ofs & 3) == 0) &&
	    (unsigned)script_ofs < script_size) {
		printf ("%s: script cmd = %08x\n", sym_name(np),
			scr_to_cpu((int) *(u32 *)(script_base + script_ofs)));
	}

        printf ("%s: regdump:", sym_name(np));
        for (i=0; i<24;i++)
            printf (" %02x", (unsigned)INB_OFF(i));
        printf (".\n");

	/*
	 *  PCI BUS error, read the PCI ststus register.
	 */
	if (dstat & (MDPE|BF)) {
		u_short pci_sts;
#ifdef FreeBSD_4_Bus
		pci_sts = pci_read_config(np->device, PCIR_STATUS, 2);
#else
		pci_sts = pci_cfgread(np->pci_tag, PCIR_STATUS, 2);
#endif
		if (pci_sts & 0xf900) {
#ifdef FreeBSD_4_Bus
			pci_write_config(np->device, PCIR_STATUS, pci_sts, 2);
#else
			pci_cfgwrite(np->pci_tag, PCIR_STATUS, pci_sts, 2);
#endif
			printf("%s: PCI STATUS = 0x%04x\n",
				sym_name(np), pci_sts & 0xf900);
		}
	}
}

/*
 *  chip interrupt handler
 *
 *  In normal situations, interrupt conditions occur one at 
 *  a time. But when something bad happens on the SCSI BUS, 
 *  the chip may raise several interrupt flags before 
 *  stopping and interrupting the CPU. The additionnal 
 *  interrupt flags are stacked in some extra registers 
 *  after the SIP and/or DIP flag has been raised in the 
 *  ISTAT. After the CPU has read the interrupt condition 
 *  flag from SIST or DSTAT, the chip unstacks the other 
 *  interrupt flags and sets the corresponding bits in 
 *  SIST or DSTAT. Since the chip starts stacking once the 
 *  SIP or DIP flag is set, there is a small window of time 
 *  where the stacking does not occur.
 *
 *  Typically, multiple interrupt conditions may happen in 
 *  the following situations:
 *
 *  - SCSI parity error + Phase mismatch  (PAR|MA)
 *    When an parity error is detected in input phase 
 *    and the device switches to msg-in phase inside a 
 *    block MOV.
 *  - SCSI parity error + Unexpected disconnect (PAR|UDC)
 *    When a stupid device does not want to handle the 
 *    recovery of an SCSI parity error.
 *  - Some combinations of STO, PAR, UDC, ...
 *    When using non compliant SCSI stuff, when user is 
 *    doing non compliant hot tampering on the BUS, when 
 *    something really bad happens to a device, etc ...
 *
 *  The heuristic suggested by SYMBIOS to handle 
 *  multiple interrupts is to try unstacking all 
 *  interrupts conditions and to handle them on some 
 *  priority based on error severity.
 *  This will work when the unstacking has been 
 *  successful, but we cannot be 100 % sure of that, 
 *  since the CPU may have been faster to unstack than 
 *  the chip is able to stack. Hmmm ... But it seems that 
 *  such a situation is very unlikely to happen.
 *
 *  If this happen, for example STO caught by the CPU 
 *  then UDC happenning before the CPU have restarted 
 *  the SCRIPTS, the driver may wrongly complete the 
 *  same command on UDC, since the SCRIPTS didn't restart 
 *  and the DSA still points to the same command.
 *  We avoid this situation by setting the DSA to an 
 *  invalid value when the CCB is completed and before 
 *  restarting the SCRIPTS.
 *
 *  Another issue is that we need some section of our 
 *  recovery procedures to be somehow uninterruptible but 
 *  the SCRIPTS processor does not provides such a 
 *  feature. For this reason, we handle recovery preferently 
 *  from the C code and check against some SCRIPTS critical 
 *  sections from the C code.
 *
 *  Hopefully, the interrupt handling of the driver is now 
 *  able to resist to weird BUS error conditions, but donnot 
 *  ask me for any guarantee that it will never fail. :-)
 *  Use at your own decision and risk.
 */

static void sym_intr1 (hcb_p np)
{
	u_char	istat, istatc;
	u_char	dstat;
	u_short	sist;

	/*
	 *  interrupt on the fly ?
	 */
	istat = INB (nc_istat);
	if (istat & INTF) {
		OUTB (nc_istat, (istat & SIGP) | INTF | np->istat_sem);
#if 1
		istat = INB (nc_istat);		/* DUMMY READ */
#endif
		if (DEBUG_FLAGS & DEBUG_TINY) printf ("F ");
		(void)sym_wakeup_done (np);
	};

	if (!(istat & (SIP|DIP)))
		return;

#if 0	/* We should never get this one */
	if (istat & CABRT)
		OUTB (nc_istat, CABRT);
#endif

	/*
	 *  PAR and MA interrupts may occur at the same time,
	 *  and we need to know of both in order to handle 
	 *  this situation properly. We try to unstack SCSI 
	 *  interrupts for that reason. BTW, I dislike a LOT 
	 *  such a loop inside the interrupt routine.
	 *  Even if DMA interrupt stacking is very unlikely to 
	 *  happen, we also try unstacking these ones, since 
	 *  this has no performance impact.
	 */
	sist	= 0;
	dstat	= 0;
	istatc	= istat;
	do {
		if (istatc & SIP)
			sist  |= INW (nc_sist);
		if (istatc & DIP)
			dstat |= INB (nc_dstat);
		istatc = INB (nc_istat);
		istat |= istatc;
	} while (istatc & (SIP|DIP));

	if (DEBUG_FLAGS & DEBUG_TINY)
		printf ("<%d|%x:%x|%x:%x>",
			(int)INB(nc_scr0),
			dstat,sist,
			(unsigned)INL(nc_dsp),
			(unsigned)INL(nc_dbc));
	/*
	 *  First, interrupts we want to service cleanly.
	 *
	 *  Phase mismatch (MA) is the most frequent interrupt 
	 *  for chip earlier than the 896 and so we have to service 
	 *  it as quickly as possible.
	 *  A SCSI parity error (PAR) may be combined with a phase 
	 *  mismatch condition (MA).
	 *  Programmed interrupts (SIR) are used to call the C code 
	 *  from SCRIPTS.
	 *  The single step interrupt (SSI) is not used in this 
	 *  driver.
	 */
	if (!(sist  & (STO|GEN|HTH|SGE|UDC|SBMC|RST)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & PAR)	sym_int_par (np, sist);
		else if (sist & MA)	sym_int_ma (np);
		else if (dstat & SIR)	sym_int_sir (np);
		else if (dstat & SSI)	OUTONB (nc_dcntl, (STD|NOCOM));
		else			goto unknown_int;
		return;
	};

	/*
	 *  Now, interrupts that donnot happen in normal 
	 *  situations and that we may need to recover from.
	 *
	 *  On SCSI RESET (RST), we reset everything.
	 *  On SCSI BUS MODE CHANGE (SBMC), we complete all 
	 *  active CCBs with RESET status, prepare all devices 
	 *  for negotiating again and restart the SCRIPTS.
	 *  On STO and UDC, we complete the CCB with the corres- 
	 *  ponding status and restart the SCRIPTS.
	 */
	if (sist & RST) {
		xpt_print_path(np->path);
		printf("SCSI BUS reset detected.\n");
		sym_init (np, 1);
		return;
	};

	OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
	OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */

	if (!(sist  & (GEN|HTH|SGE)) &&
	    !(dstat & (MDPE|BF|ABRT|IID))) {
		if	(sist & SBMC)	sym_int_sbmc (np);
		else if (sist & STO)	sym_int_sto (np);
		else if (sist & UDC)	sym_int_udc (np);
		else			goto unknown_int;
		return;
	};

	/*
	 *  Now, interrupts we are not able to recover cleanly.
	 *
	 *  Log message for hard errors.
	 *  Reset everything.
	 */

	sym_log_hard_error(np, sist, dstat);

	if ((sist & (GEN|HTH|SGE)) ||
		(dstat & (MDPE|BF|ABRT|IID))) {
		sym_start_reset(np);
		return;
	};

unknown_int:
	/*
	 *  We just miss the cause of the interrupt. :(
	 *  Print a message. The timeout will do the real work.
	 */
	printf(	"%s: unknown interrupt(s) ignored, "
		"ISTAT=0x%x DSTAT=0x%x SIST=0x%x\n",
		sym_name(np), istat, dstat, sist);
}

static void sym_intr(void *arg)
{
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("[");
	sym_intr1((hcb_p) arg);
	if (DEBUG_FLAGS & DEBUG_TINY) printf ("]");
	return;
}

static void sym_poll(struct cam_sim *sim)
{
	int s = splcam();
	sym_intr(cam_sim_softc(sim));  
	splx(s);
}


/*
 *  generic recovery from scsi interrupt
 *
 *  The doc says that when the chip gets an SCSI interrupt,
 *  it tries to stop in an orderly fashion, by completing 
 *  an instruction fetch that had started or by flushing 
 *  the DMA fifo for a write to memory that was executing.
 *  Such a fashion is not enough to know if the instruction 
 *  that was just before the current DSP value has been 
 *  executed or not.
 *
 *  There are some small SCRIPTS sections that deal with 
 *  the start queue and the done queue that may break any 
 *  assomption from the C code if we are interrupted 
 *  inside, so we reset if this happens. Btw, since these 
 *  SCRIPTS sections are executed while the SCRIPTS hasn't 
 *  started SCSI operations, it is very unlikely to happen.
 *
 *  All the driver data structures are supposed to be 
 *  allocated from the same 4 GB memory window, so there 
 *  is a 1 to 1 relationship between DSA and driver data 
 *  structures. Since we are careful :) to invalidate the 
 *  DSA when we complete a command or when the SCRIPTS 
 *  pushes a DSA into a queue, we can trust it when it 
 *  points to a CCB.
 */
static void sym_recover_scsi_int (hcb_p np, u_char hsts)
{
	u32	dsp	= INL (nc_dsp);
	u32	dsa	= INL (nc_dsa);
	ccb_p cp	= sym_ccb_from_dsa(np, dsa);

	/*
	 *  If we haven't been interrupted inside the SCRIPTS 
	 *  critical pathes, we can safely restart the SCRIPTS 
	 *  and trust the DSA value if it matches a CCB.
	 */
	if ((!(dsp > SCRIPT_BA (np, getjob_begin) &&
	       dsp < SCRIPT_BA (np, getjob_end) + 1)) &&
	    (!(dsp > SCRIPT_BA (np, ungetjob) &&
	       dsp < SCRIPT_BA (np, reselect) + 1)) &&
	    (!(dsp > SCRIPTH_BA (np, sel_for_abort) &&
	       dsp < SCRIPTH_BA (np, sel_for_abort_1) + 1)) &&
	    (!(dsp > SCRIPT_BA (np, done) &&
	       dsp < SCRIPT_BA (np, done_end) + 1))) {
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* clear dma fifo  */
		OUTB (nc_stest3, TE|CSF);		/* clear scsi fifo */
		/*
		 *  If we have a CCB, let the SCRIPTS call us back for 
		 *  the handling of the error with SCRATCHA filled with 
		 *  STARTPOS. This way, we will be able to freeze the 
		 *  device queue and requeue awaiting IOs.
		 */
		if (cp) {
			cp->host_status = hsts;
			OUTL (nc_dsp, SCRIPT_BA (np, complete_error));
		}
		/*
		 *  Otherwise just restart the SCRIPTS.
		 */
		else {
			OUTL (nc_dsa, 0xffffff);
			OUTL (nc_dsp, SCRIPT_BA (np, start));
		}
	}
	else
		goto reset_all;

	return;

reset_all:
	sym_start_reset(np);
}

/*
 *  chip exception handler for selection timeout
 */
void sym_int_sto (hcb_p np)
{
	u32 dsp	= INL (nc_dsp);

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("T");

	if (dsp == SCRIPT_BA (np, wf_sel_done) + 8)
		sym_recover_scsi_int(np, HS_SEL_TIMEOUT);
	else
		sym_start_reset(np);
}

/*
 *  chip exception handler for unexpected disconnect
 */
void sym_int_udc (hcb_p np)
{
	printf ("%s: unexpected disconnect\n", sym_name(np));
	sym_recover_scsi_int(np, HS_UNEXPECTED);
}

/*
 *  chip exception handler for SCSI bus mode change
 *
 *  spi2-r12 11.2.3 says a transceiver mode change must 
 *  generate a reset event and a device that detects a reset 
 *  event shall initiate a hard reset. It says also that a
 *  device that detects a mode change shall set data transfer 
 *  mode to eight bit asynchronous, etc...
 *  So, just reinitializing all except chip should be enough.
 */
static void sym_int_sbmc (hcb_p np)
{
	u_char scsi_mode = INB (nc_stest4) & SMODE;

	/*
	 *  Notify user.
	 */
	xpt_print_path(np->path);
	printf("SCSI BUS mode change from %s to %s.\n",
		sym_scsi_bus_mode(np->scsi_mode), sym_scsi_bus_mode(scsi_mode));

	/*
	 *  Should suspend command processing for a few seconds and 
	 *  reinitialize all except the chip.
	 */
	sym_init (np, 2);
}

/*
 *  chip exception handler for SCSI parity error.
 *
 *  When the chip detects a SCSI parity error and is 
 *  currently executing a (CH)MOV instruction, it does 
 *  not interrupt immediately, but tries to finish the 
 *  transfer of the current scatter entry before 
 *  interrupting. The following situations may occur:
 *
 *  - The complete scatter entry has been transferred 
 *    without the device having changed phase.
 *    The chip will then interrupt with the DSP pointing 
 *    to the instruction that follows the MOV.
 *
 *  - A phase mismatch occurs before the MOV finished 
 *    and phase errors are to be handled by the C code.
 *    The chip will then interrupt with both PAR and MA 
 *    conditions set.
 *
 *  - A phase mismatch occurs before the MOV finished and 
 *    phase errors are to be handled by SCRIPTS.
 *    The chip will load the DSP with the phase mismatch 
 *    JUMP address and interrupt the host processor.
 */
static void sym_int_par (hcb_p np, u_short sist)
{
	u_char	hsts	= INB (HS_PRT);
	u32	dsp	= INL (nc_dsp);
	u32	dbc	= INL (nc_dbc);
	u32	dsa	= INL (nc_dsa);
	u_char	sbcl	= INB (nc_sbcl);
	u_char	cmd	= dbc >> 24;
	int phase	= cmd & 7;
	ccb_p	cp	= sym_ccb_from_dsa(np, dsa);

	printf("%s: SCSI parity error detected: SCR1=%d DBC=%x SBCL=%x\n",
		sym_name(np), hsts, dbc, sbcl);

	/*
	 *  Check that the chip is connected to the SCSI BUS.
	 */
	if (!(INB (nc_scntl1) & ISCON)) {
		sym_recover_scsi_int(np, HS_UNEXPECTED);
		return;
	}

	/*
	 *  If the nexus is not clearly identified, reset the bus.
	 *  We will try to do better later.
	 */
	if (!cp)
		goto reset_all;

	/*
	 *  Check instruction was a MOV, direction was INPUT and 
	 *  ATN is asserted.
	 */
	if ((cmd & 0xc0) || !(phase & 1) || !(sbcl & 0x8))
		goto reset_all;

	/*
	 *  Keep track of the parity error.
	 */
	OUTONB (HF_PRT, HF_EXT_ERR);
	cp->xerr_status |= XE_PARITY_ERR;

	/*
	 *  Prepare the message to send to the device.
	 */
	np->msgout[0] = (phase == 7) ? M_PARITY : M_ID_ERROR;

	/*
	 *  If the old phase was DATA IN phase, we have to deal with
	 *  the 3 situations described above.
	 *  For other input phases (MSG IN and STATUS), the device 
	 *  must resend the whole thing that failed parity checking 
	 *  or signal error. So, jumping to dispatcher should be OK.
	 */
	if (phase == 1) {
		/* Phase mismatch handled by SCRIPTS */
		if (dsp == SCRIPTH_BA (np, pm_handle))
			OUTL (nc_dsp, dsp);
		/* Phase mismatch handled by the C code */
		else if (sist & MA)
			sym_int_ma (np);
		/* No phase mismatch occurred */
		else {
			OUTL (nc_temp, dsp);
			OUTL (nc_dsp, SCRIPT_BA (np, dispatch));
		}
	}
	else 
		OUTL (nc_dsp, SCRIPT_BA (np, clrack));
	return;

reset_all:
	sym_start_reset(np);
	return;
}

/*
 *  chip exception handler for phase errors.
 *
 *  We have to construct a new transfer descriptor,
 *  to transfer the rest of the current block.
 */
static void sym_int_ma (hcb_p np)
{
	u32	dbc;
	u32	rest;
	u32	dsp;
	u32	dsa;
	u32	nxtdsp;
	u32	*vdsp;
	u32	oadr, olen;
	u32	*tblp;
        u32	newcmd;
	u_int	delta;
	u_char	cmd;
	u_char	hflags, hflags0;
	struct	sym_pmc *pm;
	ccb_p	cp;

	dsp	= INL (nc_dsp);
	dbc	= INL (nc_dbc);
	dsa	= INL (nc_dsa);

	cmd	= dbc >> 24;
	rest	= dbc & 0xffffff;
	delta	= 0;

	/*
	 *  locate matching cp if any.
	 */
	cp = sym_ccb_from_dsa(np, dsa);

	/*
	 *  Donnot take into account dma fifo and various buffers in 
	 *  INPUT phase since the chip flushes everything before 
	 *  raising the MA interrupt for interrupted INPUT phases.
	 *  For DATA IN phase, we will check for the SWIDE later.
	 */
	if ((cmd & 7) != 1) {
		u_char ss0, ss2;

		if (np->features & FE_DFBC)
			delta = INW (nc_dfbc);
		else {
			u32 dfifo;

			/*
			 * Read DFIFO, CTEST[4-6] using 1 PCI bus ownership.
			 */
			dfifo = INL(nc_dfifo);

			/*
			 *  Calculate remaining bytes in DMA fifo.
			 *  (CTEST5 = dfifo >> 16)
			 */
			if (dfifo & (DFS << 16))
				delta = ((((dfifo >> 8) & 0x300) |
				          (dfifo & 0xff)) - rest) & 0x3ff;
			else
				delta = ((dfifo & 0xff) - rest) & 0x7f;
		}

		/*
		 *  The data in the dma fifo has not been transfered to
		 *  the target -> add the amount to the rest
		 *  and clear the data.
		 *  Check the sstat2 register in case of wide transfer.
		 */
		rest += delta;
		ss0  = INB (nc_sstat0);
		if (ss0 & OLF) rest++;
		if (!(np->features & FE_C10))
			if (ss0 & ORF) rest++;
		if (cp && (cp->phys.select.sel_scntl3 & EWS)) {
			ss2 = INB (nc_sstat2);
			if (ss2 & OLF1) rest++;
			if (!(np->features & FE_C10))
				if (ss2 & ORF1) rest++;
		};

		/*
		 *  Clear fifos.
		 */
		OUTB (nc_ctest3, np->rv_ctest3 | CLF);	/* dma fifo  */
		OUTB (nc_stest3, TE|CSF);		/* scsi fifo */
	}

	/*
	 *  log the information
	 */
	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_PHASE))
		printf ("P%x%x RL=%d D=%d ", cmd&7, INB(nc_sbcl)&7,
			(unsigned) rest, (unsigned) delta);

	/*
	 *  try to find the interrupted script command,
	 *  and the address at which to continue.
	 */
	vdsp	= 0;
	nxtdsp	= 0;
	if	(dsp >  np->script_ba &&
		 dsp <= np->script_ba + sizeof(struct sym_scr)) {
		vdsp = (u32 *)((char*)np->script0 + (dsp-np->script_ba-8));
		nxtdsp = dsp;
	}
	else if	(dsp >  np->scripth_ba &&
		 dsp <= np->scripth_ba + sizeof(struct sym_scrh)) {
		vdsp = (u32 *)((char*)np->scripth0 + (dsp-np->scripth_ba-8));
		nxtdsp = dsp;
	}

	/*
	 *  log the information
	 */
	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("\nCP=%p DSP=%x NXT=%x VDSP=%p CMD=%x ",
			cp, (unsigned)dsp, (unsigned)nxtdsp, vdsp, cmd);
	};

	if (!vdsp) {
		printf ("%s: interrupted SCRIPT address not found.\n", 
			sym_name (np));
		goto reset_all;
	}

	if (!cp) {
		printf ("%s: SCSI phase error fixup: CCB already dequeued.\n", 
			sym_name (np));
		goto reset_all;
	}

	/*
	 *  get old startaddress and old length.
	 */
	oadr = scr_to_cpu(vdsp[1]);

	if (cmd & 0x10) {	/* Table indirect */
		tblp = (u32 *) ((char*) &cp->phys + oadr);
		olen = scr_to_cpu(tblp[0]);
		oadr = scr_to_cpu(tblp[1]);
	} else {
		tblp = (u32 *) 0;
		olen = scr_to_cpu(vdsp[0]) & 0xffffff;
	};

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		printf ("OCMD=%x\nTBLP=%p OLEN=%x OADR=%x\n",
			(unsigned) (scr_to_cpu(vdsp[0]) >> 24),
			tblp,
			(unsigned) olen,
			(unsigned) oadr);
	};

	/*
	 *  check cmd against assumed interrupted script command.
	 */
	if (cmd != (scr_to_cpu(vdsp[0]) >> 24)) {
		PRINT_ADDR(cp);
		printf ("internal error: cmd=%02x != %02x=(vdsp[0] >> 24)\n",
			(unsigned)cmd, (unsigned)scr_to_cpu(vdsp[0]) >> 24);

		goto reset_all;
	};

	/*
	 *  if old phase not dataphase, leave here.
	 */
	if ((cmd & 5) != (cmd & 7)) {
		PRINT_ADDR(cp);
		printf ("phase change %x-%x %d@%08x resid=%d.\n",
			cmd&7, INB(nc_sbcl)&7, (unsigned)olen,
			(unsigned)oadr, (unsigned)rest);
		goto unexpected_phase;
	};

	/*
	 *  Choose the correct PM save area.
	 *
	 *  Look at the PM_SAVE SCRIPT if you want to understand 
	 *  this stuff. The equivalent code is implemented in 
	 *  SCRIPTS for the 895A and 896 that are able to handle 
	 *  PM from the SCRIPTS processor.
	 */
	hflags0 = INB (HF_PRT);
	hflags = hflags0;

	if (hflags & (HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED)) {
		if (hflags & HF_IN_PM0)
			nxtdsp = scr_to_cpu(cp->phys.pm0.ret);
		else if	(hflags & HF_IN_PM1)
			nxtdsp = scr_to_cpu(cp->phys.pm1.ret);

		if (hflags & HF_DP_SAVED)
			hflags ^= HF_ACT_PM;
	}

	if (!(hflags & HF_ACT_PM)) {
		pm = &cp->phys.pm0;
		newcmd = SCRIPT_BA(np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		newcmd = SCRIPT_BA(np, pm1_data);
	}

	hflags &= ~(HF_IN_PM0 | HF_IN_PM1 | HF_DP_SAVED);
	if (hflags != hflags0)
		OUTB (HF_PRT, hflags);

	/*
	 *  fillin the phase mismatch context
	 */
	pm->sg.addr = cpu_to_scr(oadr + olen - rest);
	pm->sg.size = cpu_to_scr(rest);
	pm->ret     = cpu_to_scr(nxtdsp);

	/*
	 *  If we have a SWIDE,
	 *  - prepare the address to write the SWIDE from SCRIPTS,
	 *  - compute the SCRIPTS address to restart from,
	 *  - move current data pointer context by one byte.
	 */
	nxtdsp = SCRIPT_BA (np, dispatch);
	if ((cmd & 7) == 1 && cp && (cp->phys.select.sel_scntl3 & EWS) &&
	    (INB (nc_scntl2) & WSR)) {
		u32 tmp;
#ifdef	SYM_DEBUG_PM_WITH_WSR
		PRINT_ADDR(cp);
		printf ("MA interrupt with WSR set - "
			"pm->sg.addr=%x - pm->sg.size=%d\n",
			pm->sg.addr, pm->sg.size);
#endif
		/*
		 *  Set up the table indirect for the MOVE
		 *  of the residual byte and adjust the data 
		 *  pointer context.
		 */
		tmp = scr_to_cpu(pm->sg.addr);
		cp->phys.wresid.addr = cpu_to_scr(tmp);
		pm->sg.addr = cpu_to_scr(tmp + 1);
		tmp = scr_to_cpu(pm->sg.size);
		cp->phys.wresid.size = cpu_to_scr((tmp&0xff000000) | 1);
		pm->sg.size = cpu_to_scr(tmp - 1);

		/*
		 *  If only the residual byte is to be moved, 
		 *  no PM context is needed.
		 */
		if ((tmp&0xffffff) == 1)
			newcmd = pm->ret;

		/*
		 *  Prepare the address of SCRIPTS that will 
		 *  move the residual byte to memory.
		 */
		nxtdsp = SCRIPTH_BA (np, wsr_ma_helper);
	}

	if (DEBUG_FLAGS & DEBUG_PHASE) {
		PRINT_ADDR(cp);
		printf ("PM %x %x %x / %x %x %x.\n",
			hflags0, hflags, newcmd,
			(unsigned)scr_to_cpu(pm->sg.addr),
			(unsigned)scr_to_cpu(pm->sg.size),
			(unsigned)scr_to_cpu(pm->ret));
	}

	/*
	 *  Restart the SCRIPTS processor.
	 */
	OUTL (nc_temp, newcmd);
	OUTL (nc_dsp,  nxtdsp);
	return;

	/*
	 *  Unexpected phase changes that occurs when the current phase 
	 *  is not a DATA IN or DATA OUT phase are due to error conditions.
	 *  Such event may only happen when the SCRIPTS is using a 
	 *  multibyte SCSI MOVE.
	 *
	 *  Phase change		Some possible cause
	 *
	 *  COMMAND  --> MSG IN	SCSI parity error detected by target.
	 *  COMMAND  --> STATUS	Bad command or refused by target.
	 *  MSG OUT  --> MSG IN     Message rejected by target.
	 *  MSG OUT  --> COMMAND    Bogus target that discards extended
	 *  			negotiation messages.
	 *
	 *  The code below does not care of the new phase and so 
	 *  trusts the target. Why to annoy it ?
	 *  If the interrupted phase is COMMAND phase, we restart at
	 *  dispatcher.
	 *  If a target does not get all the messages after selection, 
	 *  the code assumes blindly that the target discards extended 
	 *  messages and clears the negotiation status.
	 *  If the target does not want all our response to negotiation,
	 *  we force a SIR_NEGO_PROTO interrupt (it is a hack that avoids 
	 *  bloat for such a should_not_happen situation).
	 *  In all other situation, we reset the BUS.
	 *  Are these assumptions reasonnable ? (Wait and see ...)
	 */
unexpected_phase:
	dsp -= 8;
	nxtdsp = 0;

	switch (cmd & 7) {
	case 2:	/* COMMAND phase */
		nxtdsp = SCRIPT_BA (np, dispatch);
		break;
#if 0
	case 3:	/* STATUS  phase */
		nxtdsp = SCRIPT_BA (np, dispatch);
		break;
#endif
	case 6:	/* MSG OUT phase */
		/*
		 *  If the device may want to use untagged when we want 
		 *  tagged, we prepare an IDENTIFY without disc. granted, 
		 *  since we will not be able to handle reselect.
		 *  Otherwise, we just don't care.
		 */
		if	(dsp == SCRIPT_BA (np, send_ident)) {
			if (cp->tag != NO_TAG && olen - rest <= 3) {
				cp->host_status = HS_BUSY;
				np->msgout[0] = M_IDENTIFY | cp->lun;
				nxtdsp = SCRIPTH_BA (np, ident_break_atn);
			}
			else
				nxtdsp = SCRIPTH_BA (np, ident_break);
		}
		else if	(dsp == SCRIPTH_BA (np, send_wdtr) ||
			 dsp == SCRIPTH_BA (np, send_sdtr) ||
			 dsp == SCRIPTH_BA (np, send_ppr)) {
			nxtdsp = SCRIPTH_BA (np, nego_bad_phase);
		}
		break;
#if 0
	case 7:	/* MSG IN  phase */
		nxtdsp = SCRIPT_BA (np, clrack);
		break;
#endif
	}

	if (nxtdsp) {
		OUTL (nc_dsp, nxtdsp);
		return;
	}

reset_all:
	sym_start_reset(np);
}

/*
 *  Dequeue from the START queue all CCBs that match 
 *  a given target/lun/task condition (-1 means all),
 *  and move them from the BUSY queue to the COMP queue 
 *  with CAM_REQUEUE_REQ status condition.
 *  This function is used during error handling/recovery.
 *  It is called with SCRIPTS not running.
 */
static int
sym_dequeue_from_squeue(hcb_p np, int i, int target, int lun, int task)
{
	int j;
	ccb_p cp;

	/*
	 *  Make sure the starting index is within range.
	 */
	assert((i >= 0) && (i < 2*MAX_QUEUE));

	/*
	 *  Walk until end of START queue and dequeue every job 
	 *  that matches the target/lun/task condition.
	 */
	j = i;
	while (i != np->squeueput) {
		cp = sym_ccb_from_dsa(np, scr_to_cpu(np->squeue[i]));
		assert(cp);
#ifdef SYM_CONF_IARB_SUPPORT
		/* Forget hints for IARB, they may be no longer relevant */
		cp->host_flags &= ~HF_HINT_IARB;
#endif
		if ((target == -1 || cp->target == target) &&
		    (lun    == -1 || cp->lun    == lun)    &&
		    (task   == -1 || cp->tag    == task)) {
			sym_set_cam_status(cp->cam_ccb, CAM_REQUEUE_REQ);
			sym_remque(&cp->link_ccbq);
			sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);
		}
		else {
			if (i != j)
				np->squeue[j] = np->squeue[i];
			if ((j += 2) >= MAX_QUEUE*2) j = 0;
		}
		if ((i += 2) >= MAX_QUEUE*2) i = 0;
	}
	if (i != j)		/* Copy back the idle task if needed */
		np->squeue[j] = np->squeue[i];
	np->squeueput = j;	/* Update our current start queue pointer */

	return (i - j) / 2;
}

/*
 *  Complete all CCBs queued to the COMP queue.
 *
 *  These CCBs are assumed:
 *  - Not to be referenced either by devices or 
 *    SCRIPTS-related queues and datas.
 *  - To have to be completed with an error condition 
 *    or requeued.
 *
 *  The device queue freeze count is incremented 
 *  for each CCB that does not prevent this.
 *  This function is called when all CCBs involved 
 *  in error handling/recovery have been reaped.
 */
static void
sym_flush_comp_queue(hcb_p np, int cam_status)
{
	SYM_QUEHEAD *qp;
	ccb_p cp;

	while ((qp = sym_remque_head(&np->comp_ccbq)) != 0) {
		union ccb *ccb;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
		ccb = cp->cam_ccb;
		if (cam_status)
			sym_set_cam_status(ccb, cam_status);
		sym_free_ccb(np, cp);
		sym_freeze_cam_ccb(ccb);
		sym_xpt_done(np, ccb);
	}
}

/*
 *  chip handler for bad SCSI status condition
 *
 *  In case of bad SCSI status, we unqueue all the tasks 
 *  currently queued to the controller but not yet started 
 *  and then restart the SCRIPTS processor immediately.
 *
 *  QUEUE FULL and BUSY conditions are handled the same way.
 *  Basically all the not yet started tasks are requeued in 
 *  device queue and the queue is frozen until a completion.
 *
 *  For CHECK CONDITION and COMMAND TERMINATED status, we use 
 *  the CCB of the failed command to prepare a REQUEST SENSE 
 *  SCSI command and queue it to the controller queue.
 *
 *  SCRATCHA is assumed to have been loaded with STARTPOS 
 *  before the SCRIPTS called the C code.
 */
static void sym_sir_bad_scsi_status(hcb_p np, int num, ccb_p cp)
{
	tcb_p tp	= &np->target[cp->target];
	u32		startp;
	u_char		s_status = cp->ssss_status;
	u_char		h_flags  = cp->host_flags;
	int		msglen;
	int		nego;
	int		i;

	/*
	 *  Compute the index of the next job to start from SCRIPTS.
	 */
	i = (INL (nc_scratcha) - vtobus(np->squeue)) / 4;

	/*
	 *  The last CCB queued used for IARB hint may be 
	 *  no longer relevant. Forget it.
	 */
#ifdef SYM_CONF_IARB_SUPPORT
	if (np->last_cp)
		np->last_cp = 0;
#endif

	/*
	 *  Now deal with the SCSI status.
	 */
	switch(s_status) {
	case S_BUSY:
	case S_QUEUE_FULL:
		if (sym_verbose >= 2) {
			PRINT_ADDR(cp);
			printf (s_status == S_BUSY ? "BUSY" : "QUEUE FULL\n");
		}
	default:	/* S_INT, S_INT_COND_MET, S_CONFLICT */
		sym_complete_error (np, cp);
		break;
	case S_TERMINATED:
	case S_CHECK_COND:
		/*
		 *  If we get an SCSI error when requesting sense, give up.
		 */
		if (h_flags & HF_SENSE) {
			sym_complete_error (np, cp);
			break;
		}

		/*
		 *  Dequeue all queued CCBs for that device not yet started,
		 *  and restart the SCRIPTS processor immediately.
		 */
		(void) sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);
		OUTL (nc_dsp, SCRIPT_BA (np, start));

 		/*
		 *  Save some info of the actual IO.
		 *  Compute the data residual.
		 */
		cp->sv_scsi_status = cp->ssss_status;
		cp->sv_xerr_status = cp->xerr_status;
		cp->sv_resid = sym_compute_residual(np, cp);

		/*
		 *  Prepare all needed data structures for 
		 *  requesting sense data.
		 */

		/*
		 *  identify message
		 */
		cp->scsi_smsg2[0] = M_IDENTIFY | cp->lun;
		msglen = 1;

		/*
		 *  If we are currently using anything different from 
		 *  async. 8 bit data transfers with that target,
		 *  start a negotiation, since the device may want 
		 *  to report us a UNIT ATTENTION condition due to 
		 *  a cause we currently ignore, and we donnot want 
		 *  to be stuck with WIDE and/or SYNC data transfer.
		 *
		 *  cp->nego_status is filled by sym_prepare_nego().
		 */
		cp->nego_status = 0;
		nego = 0;
		if	(tp->tinfo.current.options & PPR_OPT_MASK)
			nego = NS_PPR;
		else if	(tp->tinfo.current.width != BUS_8_BIT)
			nego = NS_WIDE;
		else if (tp->tinfo.current.offset != 0)
			nego = NS_SYNC;
		if (nego)
			msglen +=
			sym_prepare_nego (np,cp, nego, &cp->scsi_smsg2[msglen]);
		/*
		 *  Message table indirect structure.
		 */
		cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg2));
		cp->phys.smsg.size	= cpu_to_scr(msglen);

		/*
		 *  sense command
		 */
		cp->phys.cmd.addr	= cpu_to_scr(CCB_PHYS (cp, sensecmd));
		cp->phys.cmd.size	= cpu_to_scr(6);

		/*
		 *  patch requested size into sense command
		 */
		cp->sensecmd[0]		= 0x03;
		cp->sensecmd[1]		= cp->lun << 5;
		cp->sensecmd[4]		= cp->cam_ccb->csio.sense_len;
		cp->data_len		= cp->cam_ccb->csio.sense_len;

		/*
		 *  sense data
		 */
		cp->phys.sense.addr	=
			cpu_to_scr(vtobus(&cp->cam_ccb->csio.sense_data));
		cp->phys.sense.size	=
			cpu_to_scr(cp->cam_ccb->csio.sense_len);

		/*
		 *  requeue the command.
		 */
		startp = SCRIPTH_BA (np, sdata_in);

		cp->phys.savep	= cpu_to_scr(startp);
		cp->phys.goalp	= cpu_to_scr(startp + 16);
		cp->phys.lastp	= cpu_to_scr(startp);
		cp->startp	= cpu_to_scr(startp);

		cp->actualquirks = SYM_QUIRK_AUTOSAVE;
		cp->host_status	= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
		cp->ssss_status = S_ILLEGAL;
		cp->host_flags	= HF_SENSE;
		cp->xerr_status = 0;
		cp->phys.extra_bytes = 0;

		cp->phys.go.start =
			cpu_to_scr(SCRIPT_BA (np, select));

		/*
		 *  Requeue the command.
		 */
		sym_put_start_queue(np, cp);

		/*
		 *  Give back to upper layer everything we have dequeued.
		 */
		sym_flush_comp_queue(np, 0);
		break;
	}
}

/*
 *  After a device has accepted some management message 
 *  as BUS DEVICE RESET, ABORT TASK, etc ..., or when 
 *  a device signals a UNIT ATTENTION condition, some 
 *  tasks are thrown away by the device. We are required 
 *  to reflect that on our tasks list since the device 
 *  will never complete these tasks.
 *
 *  This function move from the BUSY queue to the COMP 
 *  queue all disconnected CCBs for a given target that 
 *  match the following criteria:
 *  - lun=-1  means any logical UNIT otherwise a given one.
 *  - task=-1 means any task, otherwise a given one.
 */
static int 
sym_clear_tasks(hcb_p np, int cam_status, int target, int lun, int task)
{
	SYM_QUEHEAD qtmp, *qp;
	int i = 0;
	ccb_p cp;

	/*
	 *  Move the entire BUSY queue to our temporary queue.
	 */
	sym_que_init(&qtmp);
	sym_que_splice(&np->busy_ccbq, &qtmp);
	sym_que_init(&np->busy_ccbq);

	/*
	 *  Put all CCBs that matches our criteria into 
	 *  the COMP queue and put back other ones into 
	 *  the BUSY queue.
	 */
	while ((qp = sym_remque_head(&qtmp)) != 0) {
		union ccb *ccb;
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		ccb = cp->cam_ccb;
		if (cp->host_status != HS_DISCONNECT ||
		    cp->target != target	     ||
		    (lun  != -1 && cp->lun != lun)   ||
		    (task != -1 && 
			(cp->tag != NO_TAG && cp->scsi_smsg[2] != task))) {
			sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);
			continue;
		}
		sym_insque_tail(&cp->link_ccbq, &np->comp_ccbq);

		/* Preserve the software timeout condition */
		if (sym_get_cam_status(ccb) != CAM_CMD_TIMEOUT)
			sym_set_cam_status(ccb, cam_status);
		++i;
#if 0
printf("XXXX TASK @%p CLEARED\n", cp);
#endif
	}
	return i;
}

/*
 *  chip handler for TASKS recovery
 *
 *  We cannot safely abort a command, while the SCRIPTS 
 *  processor is running, since we just would be in race 
 *  with it.
 *
 *  As long as we have tasks to abort, we keep the SEM 
 *  bit set in the ISTAT. When this bit is set, the 
 *  SCRIPTS processor interrupts (SIR_SCRIPT_STOPPED) 
 *  each time it enters the scheduler.
 *
 *  If we have to reset a target, clear tasks of a unit,
 *  or to perform the abort of a disconnected job, we 
 *  restart the SCRIPTS for selecting the target. Once 
 *  selected, the SCRIPTS interrupts (SIR_TARGET_SELECTED).
 *  If it loses arbitration, the SCRIPTS will interrupt again 
 *  the next time it will enter its scheduler, and so on ...
 *
 *  On SIR_TARGET_SELECTED, we scan for the more 
 *  appropriate thing to do:
 *
 *  - If nothing, we just sent a M_ABORT message to the 
 *    target to get rid of the useless SCSI bus ownership.
 *    According to the specs, no tasks shall be affected.
 *  - If the target is to be reset, we send it a M_RESET 
 *    message.
 *  - If a logical UNIT is to be cleared , we send the 
 *    IDENTIFY(lun) + M_ABORT.
 *  - If an untagged task is to be aborted, we send the 
 *    IDENTIFY(lun) + M_ABORT.
 *  - If a tagged task is to be aborted, we send the 
 *    IDENTIFY(lun) + task attributes + M_ABORT_TAG.
 *
 *  Once our 'kiss of death' :) message has been accepted 
 *  by the target, the SCRIPTS interrupts again 
 *  (SIR_ABORT_SENT). On this interrupt, we complete 
 *  all the CCBs that should have been aborted by the 
 *  target according to our message.
 */
static void sym_sir_task_recovery(hcb_p np, int num)
{
	SYM_QUEHEAD *qp;
	ccb_p cp;
	tcb_p tp;
	int target=-1, lun=-1, task;
	int i, k;

	switch(num) {
	/*
	 *  The SCRIPTS processor stopped before starting
	 *  the next command in order to allow us to perform 
	 *  some task recovery.
	 */
	case SIR_SCRIPT_STOPPED:
		/*
		 *  Do we have any target to reset or unit to clear ?
		 */
		for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
			tp = &np->target[i];
			if (tp->to_reset || 
			    (tp->lun0p && tp->lun0p->to_clear)) {
				target = i;
				break;
			}
			if (!tp->lunmp)
				continue;
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					target	= i;
					break;
				}
			}
			if (target != -1)
				break;
		}

		/*
		 *  If not, walk the busy queue for any 
		 *  disconnected CCB to be aborted.
		 */
		if (target == -1) {
			FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
				cp = sym_que_entry(qp,struct sym_ccb,link_ccbq);
				if (cp->host_status != HS_DISCONNECT)
					continue;
				if (cp->to_abort) {
					target = cp->target;
					break;
				}
			}
		}

		/*
		 *  If some target is to be selected, 
		 *  prepare and start the selection.
		 */
		if (target != -1) {
			tp = &np->target[target];
			np->abrt_sel.sel_id	= target;
			np->abrt_sel.sel_scntl3 = tp->wval;
			np->abrt_sel.sel_sxfer  = tp->sval;
			OUTL(nc_dsa, vtobus(np));
			OUTL (nc_dsp, SCRIPTH_BA (np, sel_for_abort));
			return;
		}

		/*
		 *  Now look for a CCB to abort that haven't started yet.
		 *  Btw, the SCRIPTS processor is still stopped, so 
		 *  we are not in race.
		 */
		i = 0;
		cp = 0;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_BUSY &&
			    cp->host_status != HS_NEGOTIATE)
				continue;
			if (!cp->to_abort)
				continue;
#ifdef SYM_CONF_IARB_SUPPORT
			/*
			 *    If we are using IMMEDIATE ARBITRATION, we donnot 
			 *    want to cancel the last queued CCB, since the 
			 *    SCRIPTS may have anticipated the selection.
			 */
			if (cp == np->last_cp) {
				cp->to_abort = 0;
				continue;
			}
#endif
			i = 1;	/* Means we have found some */
			break;
		}
		if (!i) {
			/*
			 *  We are done, so we donnot need 
			 *  to synchronize with the SCRIPTS anylonger.
			 *  Remove the SEM flag from the ISTAT.
			 */
			np->istat_sem = 0;
			OUTB (nc_istat, SIGP);
			break;
		}
		/*
		 *  Compute index of next position in the start 
		 *  queue the SCRIPTS intends to start and dequeue 
		 *  all CCBs for that device that haven't been started.
		 */
		i = (INL (nc_scratcha) - vtobus(np->squeue)) / 4;
		i = sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);

		/*
		 *  Make sure at least our IO to abort has been dequeued.
		 */
		assert(i && sym_get_cam_status(cp->cam_ccb) == CAM_REQUEUE_REQ);

		/*
		 *  Keep track in cam status of the reason of the abort.
		 */
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cam_ccb, CAM_CMD_TIMEOUT);
		else
			sym_set_cam_status(cp->cam_ccb, CAM_REQ_ABORTED);

		/*
		 *  Complete with error everything that we have dequeued.
	 	 */
		sym_flush_comp_queue(np, 0);
		break;
	/*
	 *  The SCRIPTS processor has selected a target 
	 *  we may have some manual recovery to perform for.
	 */
	case SIR_TARGET_SELECTED:
		target = (INB (nc_sdid) & 0xf);
		tp = &np->target[target];

		np->abrt_tbl.addr = vtobus(np->abrt_msg);

		/*
		 *  If the target is to be reset, prepare a 
		 *  M_RESET message and clear the to_reset flag 
		 *  since we donnot expect this operation to fail.
		 */
		if (tp->to_reset) {
			np->abrt_msg[0] = M_RESET;
			np->abrt_tbl.size = 1;
			tp->to_reset = 0;
			break;
		}

		/*
		 *  Otherwise, look for some logical unit to be cleared.
		 */
		if (tp->lun0p && tp->lun0p->to_clear)
			lun = 0;
		else if (tp->lunmp) {
			for (k = 1 ; k < SYM_CONF_MAX_LUN ; k++) {
				if (tp->lunmp[k] && tp->lunmp[k]->to_clear) {
					lun = k;
					break;
				}
			}
		}

		/*
		 *  If a logical unit is to be cleared, prepare 
		 *  an IDENTIFY(lun) + ABORT MESSAGE.
		 */
		if (lun != -1) {
			lcb_p lp = sym_lp(np, tp, lun);
			lp->to_clear = 0; /* We donnot expect to fail here */
			np->abrt_msg[0] = M_IDENTIFY | lun;
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
			break;
		}

		/*
		 *  Otherwise, look for some disconnected job to 
		 *  abort for this target.
		 */
		i = 0;
		cp = 0;
		FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
			cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
			if (cp->host_status != HS_DISCONNECT)
				continue;
			if (cp->target != target)
				continue;
			if (!cp->to_abort)
				continue;
			i = 1;	/* Means we have some */
			break;
		}

		/*
		 *  If we have none, probably since the device has 
		 *  completed the command before we won abitration,
		 *  send a M_ABORT message without IDENTIFY.
		 *  According to the specs, the device must just 
		 *  disconnect the BUS and not abort any task.
		 */
		if (!i) {
			np->abrt_msg[0] = M_ABORT;
			np->abrt_tbl.size = 1;
			break;
		}

		/*
		 *  We have some task to abort.
		 *  Set the IDENTIFY(lun)
		 */
		np->abrt_msg[0] = M_IDENTIFY | cp->lun;

		/*
		 *  If we want to abort an untagged command, we 
		 *  will send a IDENTIFY + M_ABORT.
		 *  Otherwise (tagged command), we will send 
		 *  a IDENTITFY + task attributes + ABORT TAG.
		 */
		if (cp->tag == NO_TAG) {
			np->abrt_msg[1] = M_ABORT;
			np->abrt_tbl.size = 2;
		}
		else {
			np->abrt_msg[1] = cp->scsi_smsg[1];
			np->abrt_msg[2] = cp->scsi_smsg[2];
			np->abrt_msg[3] = M_ABORT_TAG;
			np->abrt_tbl.size = 4;
		}
		/*
		 *  Keep track of software timeout condition, since the 
		 *  peripheral driver may not count retries on abort 
		 *  conditions not due to timeout.
		 */
		if (cp->to_abort == 2)
			sym_set_cam_status(cp->cam_ccb, CAM_CMD_TIMEOUT);
		cp->to_abort = 0; /* We donnot expect to fail here */
		break;

	/*
	 *  The target has accepted our message and switched 
	 *  to BUS FREE phase as we expected.
	 */
	case SIR_ABORT_SENT:
		target = (INB (nc_sdid) & 0xf);
		tp = &np->target[target];
		
		/*
		**  If we didn't abort anything, leave here.
		*/
		if (np->abrt_msg[0] == M_ABORT)
			break;

		/*
		 *  If we sent a M_RESET, then a hardware reset has 
		 *  been performed by the target.
		 *  - Reset everything to async 8 bit
		 *  - Tell ourself to negotiate next time :-)
		 *  - Prepare to clear all disconnected CCBs for 
		 *    this target from our task list (lun=task=-1)
		 */
		lun = -1;
		task = -1;
		if (np->abrt_msg[0] == M_RESET) {
			tp->sval = 0;
			tp->wval = np->rv_scntl3;
			tp->uval = 0;
			tp->tinfo.current.period = 0;
			tp->tinfo.current.offset = 0;
			tp->tinfo.current.width  = BUS_8_BIT;
			tp->tinfo.current.options = 0;
		}

		/*
		 *  Otherwise, check for the LUN and TASK(s) 
		 *  concerned by the cancelation.
		 *  If it is not ABORT_TAG then it is CLEAR_QUEUE 
		 *  or an ABORT message :-)
		 */
		else {
			lun = np->abrt_msg[0] & 0x3f;
			if (np->abrt_msg[1] == M_ABORT_TAG)
				task = np->abrt_msg[2];
		}

		/*
		 *  Complete all the CCBs the device should have 
		 *  aborted due to our 'kiss of death' message.
		 */
		i = (INL (nc_scratcha) - vtobus(np->squeue)) / 4;
		(void) sym_dequeue_from_squeue(np, i, target, lun, -1);
		(void) sym_clear_tasks(np, CAM_REQ_ABORTED, target, lun, task);
		sym_flush_comp_queue(np, 0);

		/*
		 *  If we sent a BDR, make uper layer aware of that.
		 */
		if (np->abrt_msg[0] == M_RESET)
			xpt_async(AC_SENT_BDR, np->path, NULL);
		break;
	}

	/*
	 *  Print to the log the message we intend to send.
	 */
	if (num == SIR_TARGET_SELECTED) {
		PRINT_TARGET(np, target);
		sym_printl_hex("control msgout:", np->abrt_msg,
			      np->abrt_tbl.size);
		np->abrt_tbl.size = cpu_to_scr(np->abrt_tbl.size);
	}

	/*
	 *  Let the SCRIPTS processor continue.
	 */
	OUTONB (nc_dcntl, (STD|NOCOM));
}

/*
 *  Gerard's alchemy:) that deals with with the data 
 *  pointer for both MDP and the residual calculation.
 *
 *  I didn't want to bloat the code by more than 200 
 *  lignes for the handling of both MDP and the residual.
 *  This has been achieved by using a data pointer 
 *  representation consisting in an index in the data 
 *  array (dp_sg) and a negative offset (dp_ofs) that 
 *  have the following meaning:
 *
 *  - dp_sg = SYM_CONF_MAX_SG
 *    we are at the end of the data script.
 *  - dp_sg < SYM_CONF_MAX_SG
 *    dp_sg points to the next entry of the scatter array 
 *    we want to transfer.
 *  - dp_ofs < 0
 *    dp_ofs represents the residual of bytes of the 
 *    previous entry scatter entry we will send first.
 *  - dp_ofs = 0
 *    no residual to send first.
 *
 *  The function sym_evaluate_dp() accepts an arbitray 
 *  offset (basically from the MDP message) and returns 
 *  the corresponding values of dp_sg and dp_ofs.
 */

static int sym_evaluate_dp(hcb_p np, ccb_p cp, u32 scr, int *ofs)
{
	u32	dp_scr;
	int	dp_ofs, dp_sg, dp_sgmin;
	int	tmp;
	struct sym_pmc *pm;

	/*
	 *  Compute the resulted data pointer in term of a script 
	 *  address within some DATA script and a signed byte offset.
	 */
	dp_scr = scr;
	dp_ofs = *ofs;
	if	(dp_scr == SCRIPT_BA (np, pm0_data))
		pm = &cp->phys.pm0;
	else if (dp_scr == SCRIPT_BA (np, pm1_data))
		pm = &cp->phys.pm1;
	else
		pm = 0;

	if (pm) {
		dp_scr  = scr_to_cpu(pm->ret);
		dp_ofs -= scr_to_cpu(pm->sg.size);
	}

	/*
	 *  If we are auto-sensing, then we are done.
	 */
	if (cp->host_flags & HF_SENSE) {
		*ofs = dp_ofs;
		return 0;
	}

	/*
	 *  Deduce the index of the sg entry.
	 *  Keep track of the index of the first valid entry.
	 *  If result is dp_sg = SYM_CONF_MAX_SG, then we are at the 
	 *  end of the data.
	 */
	tmp = scr_to_cpu(cp->phys.goalp);
	dp_sg = SYM_CONF_MAX_SG;
	if (dp_scr != tmp)
		dp_sg -= (tmp - 8 - (int)dp_scr) / (2*4);
	dp_sgmin = SYM_CONF_MAX_SG - cp->segments;

	/*
	 *  Move to the sg entry the data pointer belongs to.
	 *
	 *  If we are inside the data area, we expect result to be:
	 *
	 *  Either,
	 *      dp_ofs = 0 and dp_sg is the index of the sg entry
	 *      the data pointer belongs to (or the end of the data)
	 *  Or,
	 *      dp_ofs < 0 and dp_sg is the index of the sg entry 
	 *      the data pointer belongs to + 1.
	 */
	if (dp_ofs < 0) {
		int n;
		while (dp_sg > dp_sgmin) {
			--dp_sg;
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			n = dp_ofs + (tmp & 0xffffff);
			if (n > 0) {
				++dp_sg;
				break;
			}
			dp_ofs = n;
		}
	}
	else if (dp_ofs > 0) {
		while (dp_sg < SYM_CONF_MAX_SG) {
			tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
			dp_ofs -= (tmp & 0xffffff);
			++dp_sg;
			if (dp_ofs <= 0)
				break;
		}
	}

	/*
	 *  Make sure the data pointer is inside the data area.
	 *  If not, return some error.
	 */
	if	(dp_sg < dp_sgmin || (dp_sg == dp_sgmin && dp_ofs < 0))
		goto out_err;
	else if	(dp_sg > SYM_CONF_MAX_SG ||
		 (dp_sg == SYM_CONF_MAX_SG && dp_ofs > 0))
		goto out_err;

	/*
	 *  Save the extreme pointer if needed.
	 */
	if (dp_sg > cp->ext_sg ||
            (dp_sg == cp->ext_sg && dp_ofs > cp->ext_ofs)) {
		cp->ext_sg  = dp_sg;
		cp->ext_ofs = dp_ofs;
	}

	/*
	 *  Return data.
	 */
	*ofs = dp_ofs;
	return dp_sg;

out_err:
#ifdef	SYM_DEBUG_PM_WITH_WSR
	printf("XXXX dp_sg=%d dp_sgmin=%d dp_ofs=%d, SYM_CONF_MAX_SG=%d\n",
		dp_sg, dp_sgmin, dp_ofs, SYM_CONF_MAX_SG);
#endif

	return -1;
}

/*
 *  chip handler for MODIFY DATA POINTER MESSAGE
 *
 *  We also call this function on IGNORE WIDE RESIDUE 
 *  messages that do not match a SWIDE full condition.
 *  Btw, we assume in that situation that such a message 
 *  is equivalent to a MODIFY DATA POINTER (offset=-1).
 */

static void sym_modify_dp(hcb_p np, tcb_p tp, ccb_p cp, int ofs)
{
	int dp_ofs	= ofs;
	u32	dp_scr	= INL (nc_temp);
	u32	dp_ret;
	u32	tmp;
	u_char	hflags;
	int	dp_sg;
	struct	sym_pmc *pm;

	/*
	 *  Not supported for auto-sense.
	 */
	if (cp->host_flags & HF_SENSE)
		goto out_reject;

	/*
	 *  Apply our alchemy:) (see comments in sym_evaluate_dp()), 
	 *  to the resulted data pointer.
	 */
	dp_sg = sym_evaluate_dp(np, cp, dp_scr, &dp_ofs);
	if (dp_sg < 0)
		goto out_reject;

	/*
	 *  And our alchemy:) allows to easily calculate the data 
	 *  script address we want to return for the next data phase.
	 */
	dp_ret = cpu_to_scr(cp->phys.goalp);
	dp_ret = dp_ret - 8 - (SYM_CONF_MAX_SG - dp_sg) * (2*4);

	/*
	 *  If offset / scatter entry is zero we donnot need 
	 *  a context for the new current data pointer.
	 */
	if (dp_ofs == 0) {
		dp_scr = dp_ret;
		goto out_ok;
	}

	/*
	 *  Get a context for the new current data pointer.
	 */
	hflags = INB (HF_PRT);

	if (hflags & HF_DP_SAVED)
		hflags ^= HF_ACT_PM;

	if (!(hflags & HF_ACT_PM)) {
		pm  = &cp->phys.pm0;
		dp_scr = SCRIPT_BA (np, pm0_data);
	}
	else {
		pm = &cp->phys.pm1;
		dp_scr = SCRIPT_BA (np, pm1_data);
	}

	hflags &= ~(HF_DP_SAVED);

	OUTB (HF_PRT, hflags);

	/*
	 *  Set up the new current data pointer.
	 *  ofs < 0 there, and for the next data phase, we 
	 *  want to transfer part of the data of the sg entry 
	 *  corresponding to index dp_sg-1 prior to returning 
	 *  to the main data script.
	 */
	pm->ret = cpu_to_scr(dp_ret);
	tmp  = scr_to_cpu(cp->phys.data[dp_sg-1].addr);
	tmp += scr_to_cpu(cp->phys.data[dp_sg-1].size) + dp_ofs;
	pm->sg.addr = cpu_to_scr(tmp);
	pm->sg.size = cpu_to_scr(-dp_ofs);

out_ok:
	OUTL (nc_temp, dp_scr);
	OUTL (nc_dsp, SCRIPT_BA (np, clrack));
	return;

out_reject:
	OUTL (nc_dsp, SCRIPTH_BA (np, msg_bad));
}


/*
 *  chip calculation of the data residual.
 *
 *  As I used to say, the requirement of data residual 
 *  in SCSI is broken, useless and cannot be achieved 
 *  without huge complexity.
 *  But most OSes and even the official CAM require it.
 *  When stupidity happens to be so widely spread inside 
 *  a community, it gets hard to convince.
 *
 *  Anyway, I don't care, since I am not going to use 
 *  any software that considers this data residual as 
 *  a relevant information. :)
 */

static int sym_compute_residual(hcb_p np, ccb_p cp)
{
	int dp_sg, dp_sgmin, resid = 0;
	int dp_ofs = 0;

	/*
	 *  Check for some data lost or just thrown away.
	 *  We are not required to be quite accurate in this 
	 *  situation. Btw, if we are odd for output and the 
	 *  device claims some more data, it may well happen 
	 *  than our residual be zero. :-)
	 */
	if (cp->xerr_status & (XE_EXTRA_DATA|XE_SODL_UNRUN|XE_SWIDE_OVRUN)) {
		if (cp->xerr_status & XE_EXTRA_DATA)
			resid -= scr_to_cpu(cp->phys.extra_bytes);
		if (cp->xerr_status & XE_SODL_UNRUN)
			++resid;
		if (cp->xerr_status & XE_SWIDE_OVRUN)
			--resid;
	}

	/*
	 *  If all data has been transferred,
	 *  there is no residual.
	 */
	if (cp->phys.lastp == cp->phys.goalp)
		return resid;

	/*
	 *  If no data transfer occurs, or if the data
	 *  pointer is weird, return full residual.
	 */
	if (cp->startp == cp->phys.lastp ||
	    sym_evaluate_dp(np, cp, scr_to_cpu(cp->phys.lastp), &dp_ofs) < 0) {
		return cp->data_len;
	}

	/*
	 *  If we were auto-sensing, then we are done.
	 */
	if (cp->host_flags & HF_SENSE) {
		return -dp_ofs;
	}

	/*
	 *  We are now full comfortable in the computation 
	 *  of the data residual (2's complement).
	 */
	dp_sgmin = SYM_CONF_MAX_SG - cp->segments;
	resid = -cp->ext_ofs;
	for (dp_sg = cp->ext_sg; dp_sg < SYM_CONF_MAX_SG; ++dp_sg) {
		u_long tmp = scr_to_cpu(cp->phys.data[dp_sg].size);
		resid += (tmp & 0xffffff);
	}

	/*
	 *  Hopefully, the result is not too wrong.
	 */
	return resid;
}

/*
 *  Print out the containt of a SCSI message.
 */

static int sym_show_msg (u_char * msg)
{
	u_char i;
	printf ("%x",*msg);
	if (*msg==M_EXTENDED) {
		for (i=1;i<8;i++) {
			if (i-1>msg[1]) break;
			printf ("-%x",msg[i]);
		};
		return (i+1);
	} else if ((*msg & 0xf0) == 0x20) {
		printf ("-%x",msg[1]);
		return (2);
	};
	return (1);
}

static void sym_print_msg (ccb_p cp, char *label, u_char *msg)
{
	PRINT_ADDR(cp);
	if (label)
		printf ("%s: ", label);

	(void) sym_show_msg (msg);
	printf (".\n");
}

/*
 *  Negotiation for WIDE and SYNCHRONOUS DATA TRANSFER.
 *
 *  We try to negotiate sync and wide transfer only after
 *  a successfull inquire command. We look at byte 7 of the
 *  inquire data to determine the capabilities of the target.
 *
 *  When we try to negotiate, we append the negotiation message
 *  to the identify and (maybe) simple tag message.
 *  The host status field is set to HS_NEGOTIATE to mark this
 *  situation.
 *
 *  If the target doesn't answer this message immediately
 *  (as required by the standard), the SIR_NEGO_FAILED interrupt
 *  will be raised eventually.
 *  The handler removes the HS_NEGOTIATE status, and sets the
 *  negotiated value to the default (async / nowide).
 *
 *  If we receive a matching answer immediately, we check it
 *  for validity, and set the values.
 *
 *  If we receive a Reject message immediately, we assume the
 *  negotiation has failed, and fall back to standard values.
 *
 *  If we receive a negotiation message while not in HS_NEGOTIATE
 *  state, it's a target initiated negotiation. We prepare a
 *  (hopefully) valid answer, set our parameters, and send back 
 *  this answer to the target.
 *
 *  If the target doesn't fetch the answer (no message out phase),
 *  we assume the negotiation has failed, and fall back to default
 *  settings (SIR_NEGO_PROTO interrupt).
 *
 *  When we set the values, we adjust them in all ccbs belonging 
 *  to this target, in the controller's register, and in the "phys"
 *  field of the controller's struct sym_hcb.
 */

/*
 *  chip handler for SYNCHRONOUS DATA TRANSFER REQUEST (SDTR) message.
 */
static void sym_sync_nego(hcb_p np, tcb_p tp, ccb_p cp)
{
	u_char	chg, ofs, per, fak, div;
	int	req = 1;

	/*
	 *  Synchronous request message received.
	 */
	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "sync msgin", np->msgin);
	};

	/*
	 * request or answer ?
	 */
	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_SYNC)
			goto reject_it;
		req = 0;
	}

	/*
	 *  get requested values.
	 */
	chg = 0;
	per = np->msgin[3];
	ofs = np->msgin[4];

	/*
	 *  check values against our limits.
	 */
	if (ofs) {
		if (ofs > np->maxoffs)
			{chg = 1; ofs = np->maxoffs;}
		if (req) {
			if (ofs > tp->tinfo.user.offset)
				{chg = 1; ofs = tp->tinfo.user.offset;}
		}
	}

	if (ofs) {
		if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (req) {
			if (per < tp->tinfo.user.period)
				{chg = 1; per = tp->tinfo.user.period;}
		}
	}

	div = fak = 0;
	if (ofs && sym_getsync(np, 0, per, &div, &fak) < 0)
		goto reject_it;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp);
		printf ("sdtr: ofs=%d per=%d div=%d fak=%d chg=%d.\n",
			ofs, per, div, fak, chg);
	}

	/*
	 *  This was an answer message
	 */
	if (req == 0) {
		if (chg) 	/* Answer wasn't acceptable. */
			goto reject_it;
		sym_setsync (np, cp, ofs, per, div, fak);
		OUTL (nc_dsp, SCRIPT_BA (np, clrack));
		return;
	}

	/*
	 *  It was a request. Set value and
	 *  prepare an answer message
	 */
	sym_setsync (np, cp, ofs, per, div, fak);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 3;
	np->msgout[2] = M_X_SYNC_REQ;
	np->msgout[3] = per;
	np->msgout[4] = ofs;

	cp->nego_status = NS_SYNC;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "sync msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	OUTL (nc_dsp, SCRIPTH_BA (np, sdtr_resp));
	return;
reject_it:
	sym_setsync (np, cp, 0, 0, 0, 0);
	OUTL (nc_dsp, SCRIPTH_BA (np, msg_bad));
}

/*
 *  chip handler for PARALLEL PROTOCOL REQUEST (PPR) message.
 */
static void sym_ppr_nego(hcb_p np, tcb_p tp, ccb_p cp)
{
	u_char	chg, ofs, per, fak, dt, div, wide;
	int	req = 1;

	/*
	 * Synchronous request message received.
	 */
	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "ppr msgin", np->msgin);
	};

	/*
	 * request or answer ?
	 */
	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_PPR)
			goto reject_it;
		req = 0;
	}

	/*
	 *  get requested values.
	 */
	chg  = 0;
	per  = np->msgin[3];
	ofs  = np->msgin[5];
	wide = np->msgin[6];
	dt   = np->msgin[7] & PPR_OPT_DT;

	/*
	 *  check values against our limits.
	 */
	if (wide > np->maxwide)
		{chg = 1; wide = np->maxwide;}
	if (!wide || !(np->features & FE_ULTRA3))
		dt &= ~PPR_OPT_DT;
	if (req) {
		if (wide > tp->tinfo.user.width)
			{chg = 1; wide = tp->tinfo.user.width;}
	}

	if (!(np->features & FE_U3EN))	/* Broken U3EN bit not supported */
		dt &= ~PPR_OPT_DT;

	if (dt != (np->msgin[7] & PPR_OPT_MASK)) chg = 1;

	if (ofs) {
		if (ofs > np->maxoffs)
			{chg = 1; ofs = np->maxoffs;}
		if (req) {
			if (ofs > tp->tinfo.user.offset)
				{chg = 1; ofs = tp->tinfo.user.offset;}
		}
	}

	if (ofs) {
		if (dt) {
			if (per < np->minsync_dt)
				{chg = 1; per = np->minsync_dt;}
		}
		else if (per < np->minsync)
			{chg = 1; per = np->minsync;}
		if (req) {
			if (per < tp->tinfo.user.period)
				{chg = 1; per = tp->tinfo.user.period;}
		}
	}

	div = fak = 0;
	if (ofs && sym_getsync(np, dt, per, &div, &fak) < 0)
		goto reject_it;
	
	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp);
		printf ("ppr: "
			"dt=%x ofs=%d per=%d wide=%d div=%d fak=%d chg=%d.\n",
			dt, ofs, per, wide, div, fak, chg);
	}

	/*
	 *  It was an answer.
	 */
	if (req == 0) {
		if (chg) 	/* Answer wasn't acceptable */
			goto reject_it;
		sym_setpprot (np, cp, dt, ofs, per, wide, div, fak);
		OUTL (nc_dsp, SCRIPT_BA (np, clrack));
		return;
	}

	/*
	 *  It was a request. Set value and
	 *  prepare an answer message
	 */
	sym_setpprot (np, cp, dt, ofs, per, wide, div, fak);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 6;
	np->msgout[2] = M_X_PPR_REQ;
	np->msgout[3] = per;
	np->msgout[4] = 0;
	np->msgout[5] = ofs;
	np->msgout[6] = wide;
	np->msgout[7] = dt;

	cp->nego_status = NS_PPR;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "ppr msgout", np->msgout);
	}

	np->msgin [0] = M_NOOP;

	OUTL (nc_dsp, SCRIPTH_BA (np, ppr_resp));
	return;
reject_it:
	sym_setpprot (np, cp, 0, 0, 0, 0, 0, 0);
	OUTL (nc_dsp, SCRIPTH_BA (np, msg_bad));
}

/*
 *  chip handler for WIDE DATA TRANSFER REQUEST (WDTR) message.
 */
static void sym_wide_nego(hcb_p np, tcb_p tp, ccb_p cp)
{
	u_char	chg, wide;
	int	req = 1;

	/*
	 *  Wide request message received.
	 */
	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "wide msgin", np->msgin);
	};

	/*
	 * Is it an request from the device?
	 */
	if (INB (HS_PRT) == HS_NEGOTIATE) {
		OUTB (HS_PRT, HS_BUSY);
		if (cp->nego_status && cp->nego_status != NS_WIDE)
			goto reject_it;
		req = 0;
	}

	/*
	 *  get requested values.
	 */
	chg  = 0;
	wide = np->msgin[3];

	/*
	 *  check values against driver limits.
	 */
	if (wide > np->maxoffs)
		{chg = 1; wide = np->maxoffs;}
	if (req) {
		if (wide > tp->tinfo.user.width)
			{chg = 1; wide = tp->tinfo.user.width;}
	}

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		PRINT_ADDR(cp);
		printf ("wdtr: wide=%d chg=%d.\n", wide, chg);
	}

	/*
	 * This was an answer message
	 */
	if (req == 0) {
		if (chg)	/*  Answer wasn't acceptable. */
			goto reject_it;
		sym_setwide (np, cp, wide);
#if 1
		/*
		 * Negotiate for SYNC immediately after WIDE response.
		 * This allows to negotiate for both WIDE and SYNC on 
		 * a single SCSI command (Suggested by Justin Gibbs).
		 */
		if (tp->tinfo.goal.offset) {
			np->msgout[0] = M_EXTENDED;
			np->msgout[1] = 3;
			np->msgout[2] = M_X_SYNC_REQ;
			np->msgout[3] = tp->tinfo.goal.period;
			np->msgout[4] = tp->tinfo.goal.offset;

			if (DEBUG_FLAGS & DEBUG_NEGO) {
				sym_print_msg(cp, "sync msgout", np->msgout);
			}

			cp->nego_status = NS_SYNC;
			OUTB (HS_PRT, HS_NEGOTIATE);
			OUTL (nc_dsp, SCRIPTH_BA (np, sdtr_resp));
			return;
		}
#endif
		OUTL (nc_dsp, SCRIPT_BA (np, clrack));
		return;
	};

	/*
	 *  It was a request, set value and
	 *  prepare an answer message
	 */
	sym_setwide (np, cp, wide);

	np->msgout[0] = M_EXTENDED;
	np->msgout[1] = 2;
	np->msgout[2] = M_X_WIDE_REQ;
	np->msgout[3] = wide;

	np->msgin [0] = M_NOOP;

	cp->nego_status = NS_WIDE;

	if (DEBUG_FLAGS & DEBUG_NEGO) {
		sym_print_msg(cp, "wide msgout", np->msgout);
	}

	OUTL (nc_dsp, SCRIPTH_BA (np, wdtr_resp));
	return;
reject_it:
	OUTL (nc_dsp, SCRIPTH_BA (np, msg_bad));
}

/*
 *  Reset SYNC or WIDE to default settings.
 *
 *  Called when a negotiation does not succeed either 
 *  on rejection or on protocol error.
 */
static void sym_nego_default(hcb_p np, tcb_p tp, ccb_p cp)
{
	/*
	 *  any error in negotiation:
	 *  fall back to default mode.
	 */
	switch (cp->nego_status) {
	case NS_PPR:
		sym_setpprot (np, cp, 0, 0, 0, 0, 0, 0);
		break;
	case NS_SYNC:
		sym_setsync (np, cp, 0, 0, 0, 0);
		break;
	case NS_WIDE:
		sym_setwide (np, cp, 0);
		break;
	};
	np->msgin [0] = M_NOOP;
	np->msgout[0] = M_NOOP;
	cp->nego_status = 0;
}

/*
 *  chip handler for MESSAGE REJECT received in response to 
 *  a WIDE or SYNCHRONOUS negotiation.
 */
static void sym_nego_rejected(hcb_p np, tcb_p tp, ccb_p cp)
{
	sym_nego_default(np, tp, cp);
	OUTB (HS_PRT, HS_BUSY);
}

/*
 *  chip exception handler for programmed interrupts.
 */
void sym_int_sir (hcb_p np)
{
	u_char	num	= INB (nc_dsps);
	u_long	dsa	= INL (nc_dsa);
	ccb_p	cp	= sym_ccb_from_dsa(np, dsa);
	u_char	target	= INB (nc_sdid) & 0x0f;
	tcb_p	tp	= &np->target[target];
	int	tmp;

	if (DEBUG_FLAGS & DEBUG_TINY) printf ("I#%d", num);

	switch (num) {
#ifdef	SYM_DEBUG_PM_WITH_WSR
	case SIR_PM_WITH_WSR:
		printf ("%s:%d: HW PM with WSR bit set - ",
			sym_name (np), target);
		tmp =
		(vtobus(&cp->phys.data[SYM_CONF_MAX_SG]) - INL (nc_esa))/8;
		printf("RBC=%d - SEG=%d - SIZE=%d - OFFS=%d\n",
		INL (nc_rbc), cp->segments - tmp,
		cp->phys.data[SYM_CONF_MAX_SG - tmp].size,
		INL (nc_ua) - cp->phys.data[SYM_CONF_MAX_SG - tmp].addr);
		goto out;
#endif
	/*
	 *  Command has been completed with error condition 
	 *  or has been auto-sensed.
	 */
	case SIR_COMPLETE_ERROR:
		sym_complete_error(np, cp);
		return;
	/*
	 *  The C code is currently trying to recover from something.
	 *  Typically, user want to abort some command.
	 */
	case SIR_SCRIPT_STOPPED:
	case SIR_TARGET_SELECTED:
	case SIR_ABORT_SENT:
		sym_sir_task_recovery(np, num);
		return;
	/*
	 *  The device didn't go to MSG OUT phase after having 
	 *  been selected with ATN. We donnot want to handle 
	 *  that.
	 */
	case SIR_SEL_ATN_NO_MSG_OUT:
		printf ("%s:%d: No MSG OUT phase after selection with ATN.\n",
			sym_name (np), target);
		goto out_stuck;
	/*
	 *  The device didn't switch to MSG IN phase after 
	 *  having reseleted the initiator.
	 */
	case SIR_RESEL_NO_MSG_IN:
		printf ("%s:%d: No MSG IN phase after reselection.\n",
			sym_name (np), target);
		goto out_stuck;
	/*
	 *  After reselection, the device sent a message that wasn't 
	 *  an IDENTIFY.
	 */
	case SIR_RESEL_NO_IDENTIFY:
		printf ("%s:%d: No IDENTIFY after reselection.\n",
			sym_name (np), target);
		goto out_stuck;
	/*
	 *  The device reselected a LUN we donnot know about.
	 */
	case SIR_RESEL_BAD_LUN:
		np->msgout[0] = M_RESET;
		goto out;
	/*
	 *  The device reselected for an untagged nexus and we 
	 *  haven't any.
	 */
	case SIR_RESEL_BAD_I_T_L:
		np->msgout[0] = M_ABORT;
		goto out;
	/*
	 *  The device reselected for a tagged nexus that we donnot 
	 *  have.
	 */
	case SIR_RESEL_BAD_I_T_L_Q:
		np->msgout[0] = M_ABORT_TAG;
		goto out;
	/*
	 *  The SCRIPTS let us know that the device has grabbed 
	 *  our message and will abort the job.
	 */
	case SIR_RESEL_ABORTED:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		printf ("%s:%d: message %x sent on bad reselection.\n",
			sym_name (np), target, np->lastmsg);
		goto out;
	/*
	 *  The SCRIPTS let us know that a message has been 
	 *  successfully sent to the device.
	 */
	case SIR_MSG_OUT_DONE:
		np->lastmsg = np->msgout[0];
		np->msgout[0] = M_NOOP;
		/* Should we really care of that */
		if (np->lastmsg == M_PARITY || np->lastmsg == M_ID_ERROR) {
			if (cp) {
				cp->xerr_status &= ~XE_PARITY_ERR;
				if (!cp->xerr_status)
					OUTOFFB (HF_PRT, HF_EXT_ERR);
			}
		}
		goto out;
	/*
	 *  The device didn't send a GOOD SCSI status.
	 *  We may have some work to do prior to allow 
	 *  the SCRIPTS processor to continue.
	 */
	case SIR_BAD_SCSI_STATUS:
		if (!cp)
			goto out;
		sym_sir_bad_scsi_status(np, num, cp);
		return;
	/*
	 *  We are asked by the SCRIPTS to prepare a 
	 *  REJECT message.
	 */
	case SIR_REJECT_TO_SEND:
		sym_print_msg(cp, "M_REJECT to send for ", np->msgin);
		np->msgout[0] = M_REJECT;
		goto out;
	/*
	 *  We have been ODD at the end of a DATA IN 
	 *  transfer and the device didn't send a 
	 *  IGNORE WIDE RESIDUE message.
	 *  It is a data overrun condition.
	 */
	case SIR_SWIDE_OVERRUN:
		if (cp) {
			OUTONB (HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_SWIDE_OVRUN;
		}
		goto out;
	/*
	 *  We have been ODD at the end of a DATA OUT 
	 *  transfer.
	 *  It is a data underrun condition.
	 */
	case SIR_SODL_UNDERRUN:
		if (cp) {
			OUTONB (HF_PRT, HF_EXT_ERR);
			cp->xerr_status |= XE_SODL_UNRUN;
		}
		goto out;
	/*
	 *  We received a message.
	 */
	case SIR_MSG_RECEIVED:
		if (!cp)
			goto out_stuck;
		switch (np->msgin [0]) {
		/*
		 *  We received an extended message.
		 *  We handle MODIFY DATA POINTER, SDTR, WDTR 
		 *  and reject all other extended messages.
		 */
		case M_EXTENDED:
			switch (np->msgin [2]) {
			case M_X_MODIFY_DP:
				if (DEBUG_FLAGS & DEBUG_POINTER)
					sym_print_msg(cp,"modify DP",np->msgin);
				tmp = (np->msgin[3]<<24) + (np->msgin[4]<<16) + 
				      (np->msgin[5]<<8)  + (np->msgin[6]);
				sym_modify_dp(np, tp, cp, tmp);
				return;
			case M_X_SYNC_REQ:
				sym_sync_nego(np, tp, cp);
				return;
			case M_X_PPR_REQ:
				sym_ppr_nego(np, tp, cp);
				return;
			case M_X_WIDE_REQ:
				sym_wide_nego(np, tp, cp);
				return;
			default:
				goto out_reject;
			}
			break;
		/*
		 *  We received a 1/2 byte message not handled from SCRIPTS.
		 *  We are only expecting MESSAGE REJECT and IGNORE WIDE 
		 *  RESIDUE messages that haven't been anticipated by 
		 *  SCRIPTS on SWIDE full condition. Unanticipated IGNORE 
		 *  WIDE RESIDUE messages are aliased as MODIFY DP (-1).
		 */
		case M_IGN_RESIDUE:
			if (DEBUG_FLAGS & DEBUG_POINTER)
				sym_print_msg(cp,"ign wide residue", np->msgin);
			sym_modify_dp(np, tp, cp, -1);
			return;
		case M_REJECT:
			if (INB (HS_PRT) == HS_NEGOTIATE)
				sym_nego_rejected(np, tp, cp);
			else {
				PRINT_ADDR(cp);
				printf ("M_REJECT received (%x:%x).\n",
					scr_to_cpu(np->lastmsg), np->msgout[0]);
			}
			goto out_clrack;
			break;
		default:
			goto out_reject;
		}
		break;
	/*
	 *  We received an unknown message.
	 *  Ignore all MSG IN phases and reject it.
	 */
	case SIR_MSG_WEIRD:
		sym_print_msg(cp, "WEIRD message received", np->msgin);
		OUTL (nc_dsp, SCRIPTH_BA (np, msg_weird));
		return;
	/*
	 *  Negotiation failed.
	 *  Target does not send us the reply.
	 *  Remove the HS_NEGOTIATE status.
	 */
	case SIR_NEGO_FAILED:
		OUTB (HS_PRT, HS_BUSY);
	/*
	 *  Negotiation failed.
	 *  Target does not want answer message.
	 */
	case SIR_NEGO_PROTO:
		sym_nego_default(np, tp, cp);
		goto out;
	};

out:
	OUTONB (nc_dcntl, (STD|NOCOM));
	return;
out_reject:
	OUTL (nc_dsp, SCRIPTH_BA (np, msg_bad));
	return;
out_clrack:
	OUTL (nc_dsp, SCRIPT_BA (np, clrack));
	return;
out_stuck:
}

/*
 *  Acquire a control block
 */
static	ccb_p sym_get_ccb (hcb_p np, u_char tn, u_char ln, u_char tag_order)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = sym_lp(np, tp, ln);
	u_short tag = NO_TAG;
	SYM_QUEHEAD *qp;
	ccb_p cp = (ccb_p) 0;

	/*
	 *  Look for a free CCB
	 */
	if (sym_que_empty(&np->free_ccbq))
		(void) sym_alloc_ccb(np);
	qp = sym_remque_head(&np->free_ccbq);
	if (!qp)
		goto out;
	cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);

	/*
	 *  If the LCB is not yet available and the LUN
	 *  has been probed ok, try to allocate the LCB.
	 */
	if (!lp && sym_is_bit(tp->lun_map, ln)) {
		lp = sym_alloc_lcb(np, tn, ln);
		if (!lp)
			goto out_free;
	}

	/*
	 *  If the LCB is not available here, then the 
	 *  logical unit is not yet discovered. For those 
	 *  ones only accept 1 SCSI IO per logical unit, 
	 *  since we cannot allow disconnections.
	 */
	if (!lp) {
		if (!sym_is_bit(tp->busy0_map, ln))
			sym_set_bit(tp->busy0_map, ln);
		else
			goto out_free;
	} else {
		/*
		 *  If we have been asked for a tagged command.
		 */
		if (tag_order) {
			/*
			 *  Debugging purpose.
			 */
			assert(lp->busy_itl == 0);
			/*
			 *  Allocate resources for tags if not yet.
			 */
			if (!lp->cb_tags) {
				sym_alloc_lcb_tags(np, tn, ln);
				if (!lp->cb_tags)
					goto out_free;
			}
			/*
			 *  Get a tag for this SCSI IO and set up
			 *  the CCB bus address for reselection, 
			 *  and count it for this LUN.
			 *  Toggle reselect path to tagged.
			 */
			if (lp->busy_itlq < SYM_CONF_MAX_TASK) {
				tag = lp->cb_tags[lp->ia_tag];
				if (++lp->ia_tag == SYM_CONF_MAX_TASK)
					lp->ia_tag = 0;
				lp->itlq_tbl[tag] = cpu_to_scr(cp->ccb_ba);
				++lp->busy_itlq;
				lp->resel_sa =
					cpu_to_scr(SCRIPT_BA (np, resel_tag));
			}
			else
				goto out_free;
		}
		/*
		 *  This command will not be tagged.
		 *  If we already have either a tagged or untagged 
		 *  one, refuse to overlap this untagged one.
		 */
		else {
			/*
			 *  Debugging purpose.
			 */
			assert(lp->busy_itl == 0 && lp->busy_itlq == 0);
			/*
			 *  Count this nexus for this LUN.
			 *  Set up the CCB bus address for reselection.
			 *  Toggle reselect path to untagged.
			 */
			if (++lp->busy_itl == 1) {
				lp->itl_task_sa = cpu_to_scr(cp->ccb_ba);
				lp->resel_sa =
					cpu_to_scr(SCRIPT_BA (np,resel_no_tag));
			}
			else
				goto out_free;
		}
	}
	/*
	 *  Put the CCB into the busy queue.
	 */
	sym_insque_tail(&cp->link_ccbq, &np->busy_ccbq);

	/*
	 *  Remember all informations needed to free this CCB.
	 */
	cp->to_abort = 0;
	cp->tag	   = tag;
	cp->target = tn;
	cp->lun    = ln;

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, tn, ln);
		printf ("ccb @%p using tag %d.\n", cp, tag);
	}

out:
	return cp;
out_free:
	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);
	return (ccb_p) 0;
}

/*
 *  Release one control block
 */
static void sym_free_ccb (hcb_p np, ccb_p cp)
{
	tcb_p tp = &np->target[cp->target];
	lcb_p lp = sym_lp(np, tp, cp->lun);

	if (DEBUG_FLAGS & DEBUG_TAGS) {
		PRINT_LUN(np, cp->target, cp->lun);
		printf ("ccb @%p freeing tag %d.\n", cp, cp->tag);
	}

	/*
	 *  If LCB available,
	 */
	if (lp) {
		/*
		 *  If tagged, release the tag, set the relect path 
		 */
		if (cp->tag != NO_TAG) {
			/*
			 *  Free the tag value.
			 */
			lp->cb_tags[lp->if_tag] = cp->tag;
			if (++lp->if_tag == SYM_CONF_MAX_TASK)
				lp->if_tag = 0;
			/*
			 *  Make the reselect path invalid, 
			 *  and uncount this CCB.
			 */
			lp->itlq_tbl[cp->tag] = cpu_to_scr(np->bad_itlq_ba);
			--lp->busy_itlq;
		} else {	/* Untagged */
			/*
			 *  Make the reselect path invalid, 
			 *  and uncount this CCB.
			 */
			lp->itl_task_sa = cpu_to_scr(np->bad_itl_ba);
			--lp->busy_itl;
		}
		/*
		 *  If no JOB active, make the LUN reselect path invalid.
		 */
		if (lp->busy_itlq == 0 && lp->busy_itl == 0)
			lp->resel_sa = cpu_to_scr(SCRIPTH_BA(np,resel_bad_lun));
	}
	/*
	 *  Otherwise, we only accept 1 IO per LUN.
	 *  Clear the bit that keeps track of this IO.
	 */
	else
		sym_clr_bit(tp->busy0_map, cp->lun);

	/*
	 *  We donnot queue more than 1 ccb per target 
	 *  with negotiation at any time. If this ccb was 
	 *  used for negotiation, clear this info in the tcb.
	 */
	if (cp == tp->nego_cp)
		tp->nego_cp = 0;

#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *  If we just complete the last queued CCB,
	 *  clear this info that is no longer relevant.
	 */
	if (cp == np->last_cp)
		np->last_cp = 0;
#endif
	/*
	 *  Make this CCB available.
	 */
	cp->cam_ccb = 0;
	cp->host_status = HS_IDLE;
	sym_remque(&cp->link_ccbq);
	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);
}

/*
 *  Allocate a CCB from memory and initialize its fixed part.
 */
static ccb_p sym_alloc_ccb(hcb_p np)
{
	ccb_p cp = 0;
	int hcode;

	/*
	 *  Prevent from allocating more CCBs than we can 
	 *  queue to the controller.
	 */
	if (np->actccbs >= SYM_CONF_MAX_START)
		return 0;

	/*
	 *  Allocate memory for this CCB.
	 */
	cp = sym_calloc(sizeof(struct sym_ccb), "CCB");
	if (!cp)
		return 0;

	/*
	 *  Count it.
	 */
	np->actccbs++;

	/*
	 *  Compute the bus address of this ccb.
	 */
	cp->ccb_ba = vtobus(cp);

	/*
	 *  Insert this ccb into the hashed list.
	 */
	hcode = CCB_HASH_CODE(cp->ccb_ba);
	cp->link_ccbh = np->ccbh[hcode];
	np->ccbh[hcode] = cp;

	/*
	 *  Initialyze the start and restart actions.
	 */
	cp->phys.go.start   = cpu_to_scr(SCRIPT_BA (np, idle));
	cp->phys.go.restart = cpu_to_scr(SCRIPTH_BA(np, bad_i_t_l));

 	/*
	 *  Initilialyze some other fields.
	 */
	cp->phys.smsg_ext.addr = cpu_to_scr(vtobus(&np->msgin[2]));

	/*
	 *  Chain into free ccb queue.
	 */
	sym_insque_head(&cp->link_ccbq, &np->free_ccbq);

	return cp;
}

/*
 *  Look up a CCB from a DSA value.
 */
static ccb_p sym_ccb_from_dsa(hcb_p np, u_long dsa)
{
	int hcode;
	ccb_p cp;

	hcode = CCB_HASH_CODE(dsa);
	cp = np->ccbh[hcode];
	while (cp) {
		if (cp->ccb_ba == dsa)
			break;
		cp = cp->link_ccbh;
	}

	return cp;
}

/*
 *  Target control block initialisation.
 *  Nothing important to do at the moment.
 */
static void sym_init_tcb (hcb_p np, u_char tn)
{
	/*
	 *  Check some alignments required by the chip.
	 */	
	assert (((offsetof(struct sym_reg, nc_sxfer) ^
		offsetof(struct sym_tcb, sval)) &3) == 0);
	assert (((offsetof(struct sym_reg, nc_scntl3) ^
		offsetof(struct sym_tcb, wval)) &3) == 0);
}

/*
 *  Lun control block allocation and initialization.
 */
static lcb_p sym_alloc_lcb (hcb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = sym_lp(np, tp, ln);

	/*
	 *  Already done, just return.
	 */
	if (lp)
		return lp;
	/*
	 *  Check against some race.
	 */
	assert(!sym_is_bit(tp->busy0_map, ln));

	/*
	 *  Initialize the target control block if not yet.
	 */
	sym_init_tcb (np, tn);

	/*
	 *  Allocate the LCB bus address array.
	 *  Compute the bus address of this table.
	 */
	if (ln && !tp->luntbl) {
		int i;

		tp->luntbl = sym_calloc(256, "LUNTBL");
		if (!tp->luntbl)
			goto fail;
		for (i = 0 ; i < 64 ; i++)
			tp->luntbl[i] = cpu_to_scr(vtobus(&np->badlun_sa));
		tp->luntbl_sa = cpu_to_scr(vtobus(tp->luntbl));
	}

	/*
	 *  Allocate the table of pointers for LUN(s) > 0, if needed.
	 */
	if (ln && !tp->lunmp) {
		tp->lunmp = sym_calloc(SYM_CONF_MAX_LUN * sizeof(lcb_p),
				   "LUNMP");
		if (!tp->lunmp)
			goto fail;
	}

	/*
	 *  Allocate the lcb.
	 *  Make it available to the chip.
	 */
	lp = sym_calloc(sizeof(struct sym_lcb), "LCB");
	if (!lp)
		goto fail;
	if (ln) {
		tp->lunmp[ln] = lp;
		tp->luntbl[ln] = cpu_to_scr(vtobus(lp));
	}
	else {
		tp->lun0p = lp;
		tp->lun0_sa = cpu_to_scr(vtobus(lp));
	}

	/*
	 *  Let the itl task point to error handling.
	 */
	lp->itl_task_sa = cpu_to_scr(np->bad_itl_ba);

	/*
	 *  Set the reselect pattern to our default. :)
	 */
	lp->resel_sa = cpu_to_scr(SCRIPTH_BA(np, resel_bad_lun));

	/*
	 *  Set user capabilities.
	 */
	lp->user_flags = tp->usrflags & (SYM_DISC_ENABLED | SYM_TAGS_ENABLED);

fail:
	return lp;
}

/*
 *  Allocate LCB resources for tagged command queuing.
 */
static void sym_alloc_lcb_tags (hcb_p np, u_char tn, u_char ln)
{
	tcb_p tp = &np->target[tn];
	lcb_p lp = sym_lp(np, tp, ln);
	int i;

	/*
	 *  If LCB not available, try to allocate it.
	 */
	if (!lp && !(lp = sym_alloc_lcb(np, tn, ln)))
		goto fail;

	/*
	 *  Allocate the task table and and the tag allocation 
	 *  circular buffer. We want both or none.
	 */
	lp->itlq_tbl = sym_calloc(SYM_CONF_MAX_TASK*4, "ITLQ_TBL");
	if (!lp->itlq_tbl)
		goto fail;
	lp->cb_tags = sym_calloc(SYM_CONF_MAX_TASK, "CB_TAGS");
	if (!lp->cb_tags) {
		sym_mfree(lp->itlq_tbl, SYM_CONF_MAX_TASK*4, "ITLQ_TBL");
		lp->itlq_tbl = 0;
		goto fail;
	}

	/*
	 *  Initialize the task table with invalid entries.
	 */
	for (i = 0 ; i < SYM_CONF_MAX_TASK ; i++)
		lp->itlq_tbl[i] = cpu_to_scr(np->notask_ba);

	/*
	 *  Fill up the tag buffer with tag numbers.
	 */
	for (i = 0 ; i < SYM_CONF_MAX_TASK ; i++)
		lp->cb_tags[i] = i;

	/*
	 *  Make the task table available to SCRIPTS, 
	 *  And accept tagged commands now.
	 */
	lp->itlq_tbl_sa = cpu_to_scr(vtobus(lp->itlq_tbl));

	return;
fail:
}

/*
 *  Test the pci bus snoop logic :-(
 *
 *  Has to be called with interrupts disabled.
 */
#ifndef SYM_CONF_IOMAPPED
static int sym_regtest (hcb_p np)
{
	register volatile u32 data;
	/*
	 *  chip registers may NOT be cached.
	 *  write 0xffffffff to a read only register area,
	 *  and try to read it back.
	 */
	data = 0xffffffff;
	OUTL_OFF(offsetof(struct sym_reg, nc_dstat), data);
	data = INL_OFF(offsetof(struct sym_reg, nc_dstat));
#if 1
	if (data == 0xffffffff) {
#else
	if ((data & 0xe2f0fffd) != 0x02000080) {
#endif
		printf ("CACHE TEST FAILED: reg dstat-sstat2 readback %x.\n",
			(unsigned) data);
		return (0x10);
	};
	return (0);
}
#endif

static int sym_snooptest (hcb_p np)
{
	u32	sym_rd, sym_wr, sym_bk, host_rd, host_wr, pc;
	int	i, err=0;
#ifndef SYM_CONF_IOMAPPED
	err |= sym_regtest (np);
	if (err) return (err);
#endif
	/*
	 *  init
	 */
	pc  = SCRIPTH0_BA (np, snooptest);
	host_wr = 1;
	sym_wr  = 2;
	/*
	 *  Set memory and register.
	 */
	np->cache = cpu_to_scr(host_wr);
	OUTL (nc_temp, sym_wr);
	/*
	 *  Start script (exchange values)
	 */
	OUTL (nc_dsa, vtobus(np));
	OUTL (nc_dsp, pc);
	/*
	 *  Wait 'til done (with timeout)
	 */
	for (i=0; i<SYM_SNOOP_TIMEOUT; i++)
		if (INB(nc_istat) & (INTF|SIP|DIP))
			break;
	/*
	 *  Save termination position.
	 */
	pc = INL (nc_dsp);
	/*
	 *  Read memory and register.
	 */
	host_rd = scr_to_cpu(np->cache);
	sym_rd  = INL (nc_scratcha);
	sym_bk  = INL (nc_temp);

	/*
	 *  check for timeout
	 */
	if (i>=SYM_SNOOP_TIMEOUT) {
		printf ("CACHE TEST FAILED: timeout.\n");
		return (0x20);
	};
	/*
	 *  Check termination position.
	 */
	if (pc != SCRIPTH0_BA (np, snoopend)+8) {
		printf ("CACHE TEST FAILED: script execution failed.\n");
		printf ("start=%08lx, pc=%08lx, end=%08lx\n", 
			(u_long) SCRIPTH0_BA (np, snooptest), (u_long) pc,
			(u_long) SCRIPTH0_BA (np, snoopend) +8);
		return (0x40);
	};
	/*
	 *  Show results.
	 */
	if (host_wr != sym_rd) {
		printf ("CACHE TEST FAILED: host wrote %d, chip read %d.\n",
			(int) host_wr, (int) sym_rd);
		err |= 1;
	};
	if (host_rd != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, host read %d.\n",
			(int) sym_wr, (int) host_rd);
		err |= 2;
	};
	if (sym_bk != sym_wr) {
		printf ("CACHE TEST FAILED: chip wrote %d, read back %d.\n",
			(int) sym_wr, (int) sym_bk);
		err |= 4;
	};
	return (err);
}

/*
 *  Determine the chip's clock frequency.
 *
 *  This is essential for the negotiation of the synchronous 
 *  transfer rate.
 *
 *  Note: we have to return the correct value.
 *  THERE IS NO SAFE DEFAULT VALUE.
 *
 *  Most NCR/SYMBIOS boards are delivered with a 40 Mhz clock.
 *  53C860 and 53C875 rev. 1 support fast20 transfers but 
 *  do not have a clock doubler and so are provided with a 
 *  80 MHz clock. All other fast20 boards incorporate a doubler 
 *  and so should be delivered with a 40 MHz clock.
 *  The recent fast40 chips (895/896/895A/1010) use a 40 Mhz base 
 *  clock and provide a clock quadrupler (160 Mhz).
 */

/*
 *  Select SCSI clock frequency
 */
static void sym_selectclock(hcb_p np, u_char scntl3)
{
	/*
	 *  If multiplier not present or not selected, leave here.
	 */
	if (np->multiplier <= 1) {
		OUTB(nc_scntl3,	scntl3);
		return;
	}

	if (sym_verbose >= 2)
		printf ("%s: enabling clock multiplier\n", sym_name(np));

	OUTB(nc_stest1, DBLEN);	   /* Enable clock multiplier		  */
	/*
	 *  Wait for the LCKFRQ bit to be set if supported by the chip.
	 *  Otherwise wait 20 micro-seconds.
	 */
	if (np->features & FE_LCKFRQ) {
		int i = 20;
		while (!(INB(nc_stest4) & LCKFRQ) && --i > 0)
			UDELAY (20);
		if (!i)
			printf("%s: the chip cannot lock the frequency\n",
				sym_name(np));
	} else
		UDELAY (20);
	OUTB(nc_stest3, HSC);		/* Halt the scsi clock		*/
	OUTB(nc_scntl3,	scntl3);
	OUTB(nc_stest1, (DBLEN|DBLSEL));/* Select clock multiplier	*/
	OUTB(nc_stest3, 0x00);		/* Restart scsi clock 		*/
}

/*
 *  calculate SCSI clock frequency (in KHz)
 */
static unsigned getfreq (hcb_p np, int gen)
{
	unsigned int ms = 0;
	unsigned int f;

	/*
	 * Measure GEN timer delay in order 
	 * to calculate SCSI clock frequency
	 *
	 * This code will never execute too
	 * many loop iterations (if DELAY is 
	 * reasonably correct). It could get
	 * too low a delay (too high a freq.)
	 * if the CPU is slow executing the 
	 * loop for some reason (an NMI, for
	 * example). For this reason we will
	 * if multiple measurements are to be 
	 * performed trust the higher delay 
	 * (lower frequency returned).
	 */
	OUTW (nc_sien , 0);	/* mask all scsi interrupts */
	(void) INW (nc_sist);	/* clear pending scsi interrupt */
	OUTB (nc_dien , 0);	/* mask all dma interrupts */
	(void) INW (nc_sist);	/* another one, just to be sure :) */
	OUTB (nc_scntl3, 4);	/* set pre-scaler to divide by 3 */
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
	OUTB (nc_stime1, gen);	/* set to nominal delay of 1<<gen * 125us */
	while (!(INW(nc_sist) & GEN) && ms++ < 100000)
		UDELAY (1000);	/* count ms */
	OUTB (nc_stime1, 0);	/* disable general purpose timer */
 	/*
 	 * set prescaler to divide by whatever 0 means
 	 * 0 ought to choose divide by 2, but appears
 	 * to set divide by 3.5 mode in my 53c810 ...
 	 */
 	OUTB (nc_scntl3, 0);

  	/*
 	 * adjust for prescaler, and convert into KHz 
  	 */
	f = ms ? ((1 << gen) * 4340) / ms : 0;

	if (sym_verbose >= 2)
		printf ("%s: Delay (GEN=%d): %u msec, %u KHz\n",
			sym_name(np), gen, ms, f);

	return f;
}

static unsigned sym_getfreq (hcb_p np)
{
	u_int f1, f2;
	int gen = 11;

	(void) getfreq (np, gen);	/* throw away first result */
	f1 = getfreq (np, gen);
	f2 = getfreq (np, gen);
	if (f1 > f2) f1 = f2;		/* trust lower result	*/
	return f1;
}

/*
 *  Get/probe chip SCSI clock frequency
 */
static void sym_getclock (hcb_p np, int mult)
{
	unsigned char scntl3 = np->sv_scntl3;
	unsigned char stest1 = np->sv_stest1;
	unsigned f1;

	/*
	 *  For the C10 core, assume 40 MHz.
	 */
	if (np->features & FE_C10) {
		np->multiplier = mult;
		np->clock_khz = 40000 * mult;
		return;
	}

	np->multiplier = 1;
	f1 = 40000;
	/*
	 *  True with 875/895/896/895A with clock multiplier selected
	 */
	if (mult > 1 && (stest1 & (DBLEN+DBLSEL)) == DBLEN+DBLSEL) {
		if (sym_verbose >= 2)
			printf ("%s: clock multiplier found\n", sym_name(np));
		np->multiplier = mult;
	}

	/*
	 *  If multiplier not found or scntl3 not 7,5,3,
	 *  reset chip and get frequency from general purpose timer.
	 *  Otherwise trust scntl3 BIOS setting.
	 */
	if (np->multiplier != mult || (scntl3 & 7) < 3 || !(scntl3 & 1)) {
		OUTB (nc_stest1, 0);		/* make sure doubler is OFF */
		f1 = sym_getfreq (np);

		if (sym_verbose)
			printf ("%s: chip clock is %uKHz\n", sym_name(np), f1);

		if	(f1 <	45000)		f1 =  40000;
		else if (f1 <	55000)		f1 =  50000;
		else				f1 =  80000;

		if (f1 < 80000 && mult > 1) {
			if (sym_verbose >= 2)
				printf ("%s: clock multiplier assumed\n",
					sym_name(np));
			np->multiplier	= mult;
		}
	} else {
		if	((scntl3 & 7) == 3)	f1 =  40000;
		else if	((scntl3 & 7) == 5)	f1 =  80000;
		else 				f1 = 160000;

		f1 /= np->multiplier;
	}

	/*
	 *  Compute controller synchronous parameters.
	 */
	f1		*= np->multiplier;
	np->clock_khz	= f1;
}

/*
 *  Get/probe PCI clock frequency
 */
static int sym_getpciclock (hcb_p np)
{
	static int f = 0;

	/* For the C10, this will not work */
	if (!f && !(np->features & FE_C10)) {
		OUTB (nc_stest1, SCLK);	/* Use the PCI clock as SCSI clock */
		f = (int) sym_getfreq (np);
		OUTB (nc_stest1, 0);
	}
	return f;
}

/*============= DRIVER ACTION/COMPLETION ====================*/

/*
 *  Print something that tells about extended errors.
 */
static void sym_print_xerr(ccb_p cp, int x_status)
{
	if (x_status & XE_PARITY_ERR) {
		PRINT_ADDR(cp);
		printf ("unrecovered SCSI parity error.\n");
	}
	if (x_status & XE_EXTRA_DATA) {
		PRINT_ADDR(cp);
		printf ("extraneous data discarded.\n");
	}
	if (x_status & XE_BAD_PHASE) {
		PRINT_ADDR(cp);
		printf ("illegal scsi phase (4/5).\n");
	}
	if (x_status & XE_SODL_UNRUN) {
		PRINT_ADDR(cp);
		printf ("ODD transfer in DATA OUT phase.\n");
	}
	if (x_status & XE_SWIDE_OVRUN) {
		PRINT_ADDR(cp);
		printf ("ODD transfer in DATA IN phase.\n");
	}
}

/*
 *  Choose the more appropriate CAM status if 
 *  the IO encountered an extended error.
 */
static int sym_xerr_cam_status(int cam_status, int x_status)
{
	if (x_status) {
		if	(x_status & XE_PARITY_ERR)
			cam_status = CAM_UNCOR_PARITY;
		else if	(x_status &(XE_EXTRA_DATA|XE_SODL_UNRUN|XE_SWIDE_OVRUN))
			cam_status = CAM_DATA_RUN_ERR;
		else if	(x_status & XE_BAD_PHASE)
			cam_status = CAM_REQ_CMP_ERR;
		else
			cam_status = CAM_REQ_CMP_ERR;
	}
	return cam_status;
}

/*
 *  Complete execution of a SCSI command with extented 
 *  error, SCSI status error, or having been auto-sensed.
 *
 *  The SCRIPTS processor is not running there, so we 
 *  can safely access IO registers and remove JOBs from  
 *  the START queue.
 *  SCRATCHA is assumed to have been loaded with STARTPOS 
 *  before the SCRIPTS called the C code.
 */
static void sym_complete_error (hcb_p np, ccb_p cp)
{
	struct ccb_scsiio *csio;
	u_int cam_status;
	int i;

	/*
	 *  Paranoid check. :)
	 */
	if (!cp || !cp->cam_ccb)
		return;

	if (DEBUG_FLAGS & (DEBUG_TINY|DEBUG_RESULT)) {
		printf ("CCB=%lx STAT=%x/%x/%x DEV=%d/%d\n", (unsigned long)cp,
			cp->host_status, cp->ssss_status, cp->host_flags,
			cp->target, cp->lun);
		MDELAY(100);
	}

	/*
	 *  Get command, target and lun pointers.
	 */
	csio = &cp->cam_ccb->csio;

	/*
	 *  Check for extended errors.
	 */
	if (cp->xerr_status) {
		if (sym_verbose)
			sym_print_xerr(cp, cp->xerr_status);
		if (cp->host_status == HS_COMPLETE)
			cp->host_status = HS_COMP_ERR;
	}

	/*
	 *  Calculate the residual.
	 */
	csio->sense_resid = 0;
	csio->resid = sym_compute_residual(np, cp);

	if (!SYM_CONF_RESIDUAL_SUPPORT) {/* If user does not want residuals */
		csio->resid  = 0;	/* throw them away. :)		   */
		cp->sv_resid = 0;
	}

	if (cp->host_flags & HF_SENSE) {		/* Auto sense     */
		csio->scsi_status = cp->sv_scsi_status;	/* Restore status */
		csio->sense_resid = csio->resid;	/* Swap residuals */
		csio->resid       = cp->sv_resid;
		cp->sv_resid	  = 0;
		if (sym_verbose && cp->sv_xerr_status)
			sym_print_xerr(cp, cp->sv_xerr_status);
		if (cp->host_status == HS_COMPLETE &&
		    cp->ssss_status == S_GOOD &&
		    cp->xerr_status == 0) {
			cam_status = sym_xerr_cam_status(CAM_SCSI_STATUS_ERROR,
							 cp->sv_xerr_status);
			cam_status |= CAM_AUTOSNS_VALID;
#if 0
			/*
			 *  If the device reports a UNIT ATTENTION condition 
			 *  due to a RESET condition, we should consider all 
			 *  disconnect CCBs for this unit as aborted.
			 */
			if (1) {
				u_char *p;
				p  = (u_char *) &cp->cam_ccb->csio.sense_data;
				if (p[0]==0x70 && p[2]==0x6 && p[12]==0x29)
					sym_clear_tasks(np, CAM_REQ_ABORTED,
							cp->target,cp->lun, -1);
			}
#endif
		}
		else
			cam_status = CAM_AUTOSENSE_FAIL;
	}
	else if (cp->host_status == HS_COMPLETE) {	/* Bad SCSI status */
		csio->scsi_status = cp->ssss_status;
		cam_status = CAM_SCSI_STATUS_ERROR;
	}
	else if (cp->host_status == HS_SEL_TIMEOUT)	/* Selection timeout */
		cam_status = CAM_SEL_TIMEOUT;
	else if (cp->host_status == HS_UNEXPECTED)	/* Unexpected BUS FREE*/
		cam_status = CAM_UNEXP_BUSFREE;
	else {						/* Extended error */
		if (sym_verbose) {
			PRINT_ADDR(cp);
			printf ("COMMAND FAILED (%x %x %x).\n",
				cp->host_status, cp->ssss_status,
				cp->xerr_status);
		}
		csio->scsi_status = cp->ssss_status;
		/*
		 *  Set the most appropriate value for CAM status.
		 */
		cam_status = sym_xerr_cam_status(CAM_REQ_CMP_ERR,
						 cp->xerr_status);
	}

	/*
	 *  Dequeue all queued CCBs for that device 
	 *  not yet started by SCRIPTS.
	 */
	i = (INL (nc_scratcha) - vtobus(np->squeue)) / 4;
	(void) sym_dequeue_from_squeue(np, i, cp->target, cp->lun, -1);

	/*
	 *  Restart the SCRIPTS processor.
	 */
	OUTL (nc_dsp, SCRIPT_BA (np, start));

	/*
	 *  Add this one to the COMP queue.
	 *  Complete all those commands with either error 
	 *  or requeue condition.
	 */
	sym_set_cam_status((union ccb *) csio, cam_status);
	sym_remque(&cp->link_ccbq);
	sym_insque_head(&cp->link_ccbq, &np->comp_ccbq);
	sym_flush_comp_queue(np, 0);
}

/*
 *  Complete execution of a successful SCSI command.
 *
 *  Only successful commands go to the DONE queue, 
 *  since we need to have the SCRIPTS processor 
 *  stopped on any error condition.
 *  The SCRIPTS processor is running while we are 
 *  completing successful commands.
 */
static void sym_complete_ok (hcb_p np, ccb_p cp)
{
	struct ccb_scsiio *csio;
	tcb_p tp;
	lcb_p lp;

	/*
	 *  Paranoid check. :)
	 */
	if (!cp || !cp->cam_ccb)
		return;
	assert (cp->host_status == HS_COMPLETE);

	/*
	 *  Get command, target and lun pointers.
	 */
	csio = &cp->cam_ccb->csio;
	tp = &np->target[cp->target];
	lp = sym_lp(np, tp, cp->lun);

	/*
	 *  Assume device discovered on first success.
	 */
	if (!lp)
		sym_set_bit(tp->lun_map, cp->lun);

	/*
	 *  If all data have been transferred, given than no
	 *  extended error did occur, there is no residual.
	 */
	csio->resid = 0;
	if (cp->phys.lastp != cp->phys.goalp)
		csio->resid = sym_compute_residual(np, cp);

	/*
	 *  Wrong transfer residuals may be worse than just always 
	 *  returning zero. User can disable this feature from 
	 *  sym_conf.h. Residual support is enabled by default.
	 */
	if (!SYM_CONF_RESIDUAL_SUPPORT)
		csio->resid  = 0;
#ifdef	SYM_DEBUG_PM_WITH_WSR
if (csio->resid) {
	printf("XXXX %d %d %d\n", csio->dxfer_len,  csio->resid,
				  csio->dxfer_len - csio->resid);
	csio->resid = 0;
}
#endif

	/*
	 *  Set status and complete the command.
	 */
	csio->scsi_status = cp->ssss_status;
	sym_set_cam_status((union ccb *) csio, CAM_REQ_CMP);
	sym_free_ccb (np, cp);
	sym_xpt_done(np, (union ccb *) csio);
}

/*
 *  Our timeout handler.
 */
static void sym_timeout1(void *arg)
{
	union ccb *ccb = (union ccb *) arg;
	hcb_p np = ccb->ccb_h.sym_hcb_ptr;

	/*
	 *  Check that the CAM CCB is still queued.
	 */
	if (!np)
		return;

	switch(ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
		(void) sym_abort_scsiio(np, ccb, 1);
		break;
	default:
		break;
	}
}

static void sym_timeout(void *arg)
{
	int s = splcam();
	sym_timeout1(arg);
	splx(s);
}

/*
 *  Abort an SCSI IO.
 */
static int sym_abort_scsiio(hcb_p np, union ccb *ccb, int timed_out)
{
	ccb_p cp;
	SYM_QUEHEAD *qp;

	/*
	 *  Look up our CCB control block.
	 */
	cp = 0;
	FOR_EACH_QUEUED_ELEMENT(&np->busy_ccbq, qp) {
		ccb_p cp2 = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		if (cp2->cam_ccb == ccb) {
			cp = cp2;
			break;
		}
	}
	if (!cp)
		return -1;

	/*
	 *  If a previous abort didn't succeed in time,
	 *  perform a BUS reset.
	 */
	if (cp->to_abort) {
		sym_reset_scsi_bus(np, 1);
		return 0;
	}

	/*
	 *  Mark the CCB for abort and allow time for.
	 */
	cp->to_abort = timed_out ? 2 : 1;
	ccb->ccb_h.timeout_ch = timeout(sym_timeout, (caddr_t) ccb, 10*hz);

	/*
	 *  Tell the SCRIPTS processor to stop and synchronize with us.
	 */
	np->istat_sem = SEM;
	OUTB (nc_istat, SIGP|SEM);
	return 0;
}

/*
 *  Reset a SCSI device (all LUNs of a target).
 */
static void sym_reset_dev(hcb_p np, union ccb *ccb)
{
	tcb_p tp;
	struct ccb_hdr *ccb_h = &ccb->ccb_h;

	if (ccb_h->target_id   == np->myaddr ||
	    ccb_h->target_id   >= SYM_CONF_MAX_TARGET ||
	    ccb_h->target_lun  >= SYM_CONF_MAX_LUN) {
		sym_xpt_done2(np, ccb, CAM_DEV_NOT_THERE);
		return;
	}

	tp = &np->target[ccb_h->target_id];

	tp->to_reset = 1;
	sym_xpt_done2(np, ccb, CAM_REQ_CMP);

	np->istat_sem = SEM;
	OUTB (nc_istat, SIGP|SEM);
	return;
}

/*
 *  SIM action entry point.
 */
static void sym_action(struct cam_sim *sim, union ccb *ccb)
{
	int s = splcam();
	sym_action1(sim, ccb);
	splx(s);
}

static void sym_action1(struct cam_sim *sim, union ccb *ccb)
{
	hcb_p	np;
	tcb_p	tp;
	lcb_p	lp;
	ccb_p	cp;
	int 	tmp;
	u_char	idmsg, *msgptr;
	u_int   msglen;
	struct	ccb_scsiio *csio;
	struct	ccb_hdr  *ccb_h;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("sym_action\n"));

	/*
	 *  Retrieve our controller data structure.
	 */
	np = (hcb_p) cam_sim_softc(sim);

	/*
	 *  The common case is SCSI IO.
	 *  We deal with other ones elsewhere.
	 */
	if (ccb->ccb_h.func_code != XPT_SCSI_IO) {
		sym_action2(sim, ccb);
		return;
	}
	csio  = &ccb->csio;
	ccb_h = &csio->ccb_h;

	/*
	 *  Work around races.
	 */
	if ((ccb_h->status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
		xpt_done(ccb);
		return;
	}

	/*
	 *  Minimal checkings, so that we will not 
	 *  go outside our tables.
	 */
	if (ccb_h->target_id   == np->myaddr ||
	    ccb_h->target_id   >= SYM_CONF_MAX_TARGET ||
	    ccb_h->target_lun  >= SYM_CONF_MAX_LUN) {
		sym_xpt_done2(np, ccb, CAM_DEV_NOT_THERE);
		return;
        }

	/*
	 *  Retreive the target and lun descriptors.
	 */
	tp = &np->target[ccb_h->target_id];
	lp = sym_lp(np, tp, ccb_h->target_lun);

	/*
	 *  Complete the 1st INQUIRY command with error 
	 *  condition if the device is flagged NOSCAN 
	 *  at BOOT in the NVRAM. This may speed up 
	 *  the boot and maintain coherency with BIOS 
	 *  device numbering. Clearing the flag allows 
	 *  user to rescan skipped devices later.
	 *  We also return error for devices not flagged 
	 *  for SCAN LUNS in the NVRAM since some mono-lun 
	 *  devices behave badly when asked for some non 
	 *  zero LUN. Btw, this is an absolute hack.:-)
	 */
	if (!(ccb_h->flags & CAM_CDB_PHYS) &&
	    (0x12 == ((ccb_h->flags & CAM_CDB_POINTER) ?
		  csio->cdb_io.cdb_ptr[0] : csio->cdb_io.cdb_bytes[0]))) {
		if ((tp->usrflags & SYM_SCAN_BOOT_DISABLED) ||
		    ((tp->usrflags & SYM_SCAN_LUNS_DISABLED) && 
		     ccb_h->target_lun != 0)) {
			tp->usrflags &= ~SYM_SCAN_BOOT_DISABLED;
			sym_xpt_done2(np, ccb, CAM_DEV_NOT_THERE);
			return;
		}
	}

	/*
	 *  Get a control block for this IO.
	 */
	tmp = ((ccb_h->flags & CAM_TAG_ACTION_VALID) != 0);
	cp = sym_get_ccb(np, ccb_h->target_id, ccb_h->target_lun, tmp);
	if (!cp) {
		sym_xpt_done2(np, ccb, CAM_RESRC_UNAVAIL);
		return;
	}

	/*
	 *  Enqueue this IO in our pending queue.
	 */
	cp->cam_ccb = ccb;
	sym_enqueue_cam_ccb(np, ccb);

	/*
	 *  Build the IDENTIFY message.
	 */
	idmsg = M_IDENTIFY | cp->lun;
	if (cp->tag != NO_TAG || (lp && (lp->current_flags & SYM_DISC_ENABLED)))
		idmsg |= 0x40;

	msgptr = cp->scsi_smsg;
	msglen = 0;
	msgptr[msglen++] = idmsg;

	/*
	 *  Build the tag message if present.
	 */
	if (cp->tag != NO_TAG) {
		u_char order = csio->tag_action;

		switch(order) {
		case M_ORDERED_TAG:
			break;
		case M_HEAD_TAG:
			break;
		default:
			order = M_SIMPLE_TAG;
		}
		msgptr[msglen++] = order;

		/*
		 *  For less than 128 tags, actual tags are numbered 
		 *  1,3,5,..2*MAXTAGS+1,since we may have to deal 
		 *  with devices that have problems with #TAG 0 or too 
		 *  great #TAG numbers. For more tags (up to 256), 
		 *  we use directly our tag number.
		 */
#if SYM_CONF_MAX_TASK > (512/4)
		msgptr[msglen++] = cp->tag;
#else
		msgptr[msglen++] = (cp->tag << 1) + 1;
#endif
	}

	/*
	 *  Build a negotiation message if needed.
	 *  (nego_status is filled by sym_prepare_nego())
	 */
	cp->nego_status = 0;
	if (tp->tinfo.current.width   != tp->tinfo.goal.width  ||
	    tp->tinfo.current.period  != tp->tinfo.goal.period ||
	    tp->tinfo.current.offset  != tp->tinfo.goal.offset ||
#if 0 /* For now only renegotiate, based on width, period and offset */
	    tp->tinfo.current.options != tp->tinfo.goal.options) {
#else
	    0) {
#endif
		if (!tp->nego_cp && lp)
			msglen += sym_prepare_nego(np, cp, 0, msgptr + msglen);
	}

	/*
	 *  Fill in our ccb
	 */

	/*
	 *  Startqueue
	 */
	cp->phys.go.start   = cpu_to_scr(SCRIPT_BA (np, select));
	cp->phys.go.restart = cpu_to_scr(SCRIPT_BA (np, resel_dsa));

	/*
	 *  select
	 */
	cp->phys.select.sel_id		= cp->target;
	cp->phys.select.sel_scntl3	= tp->wval;
	cp->phys.select.sel_sxfer	= tp->sval;
	cp->phys.select.sel_scntl4	= tp->uval;

	/*
	 *  message
	 */
	cp->phys.smsg.addr	= cpu_to_scr(CCB_PHYS (cp, scsi_smsg));
	cp->phys.smsg.size	= cpu_to_scr(msglen);

	/*
	 *  command
	 */
	if (sym_setup_cdb(np, csio, cp) < 0) {
		sym_free_ccb(np, cp);
		sym_xpt_done(np, ccb);
		return;
	}

	/*
	 *  status
	 */
#if	0	/* Provision */
	cp->actualquirks	= tp->quirks;
#endif
	cp->actualquirks	= SYM_QUIRK_AUTOSAVE;
	cp->host_status		= cp->nego_status ? HS_NEGOTIATE : HS_BUSY;
	cp->ssss_status		= S_ILLEGAL;
	cp->xerr_status		= 0;
	cp->host_flags		= 0;
	cp->phys.extra_bytes	= 0;

	/*
	 *  extreme data pointer.
	 *  shall be positive, so -1 is lower than lowest.:)
	 */
	cp->ext_sg  = -1;
	cp->ext_ofs = 0;

	/*
	 *  Build the data descriptor block 
	 *  and start the IO.
	 */
	if (sym_setup_data(np, csio, cp) < 0) {
		sym_free_ccb(np, cp);
		sym_xpt_done(np, ccb);
		return;
	}
}

/*
 *  How complex it gets to deal with the CDB in CAM.
 *  I bet, physical CDBs will never be used on the planet.
 */
static int sym_setup_cdb(hcb_p np, struct ccb_scsiio *csio, ccb_p cp)
{
	struct ccb_hdr *ccb_h;
	u32	cmd_ba;
	int	cmd_len;
	
	ccb_h = &csio->ccb_h;

	/*
	 *  CDB is 16 bytes max.
	 */
	if (csio->cdb_len > 16) {
		sym_set_cam_status(cp->cam_ccb, CAM_REQ_INVALID);
		return -1;
	}
	cmd_len = csio->cdb_len;

	if (ccb_h->flags & CAM_CDB_POINTER) {
		/* CDB is a pointer */
		if (!(ccb_h->flags & CAM_CDB_PHYS)) {
			/* CDB pointer is virtual */
			cmd_ba = vtobus(csio->cdb_io.cdb_ptr);
		} else {
			/* CDB pointer is physical */
#if 0
			cmd_ba = ((u32)csio->cdb_io.cdb_ptr) & 0xffffffff;
#else
			sym_set_cam_status(cp->cam_ccb, CAM_REQ_INVALID);
			return -1;
#endif
		}
	} else {
		/* CDB is in the ccb (buffer) */
		cmd_ba = vtobus(csio->cdb_io.cdb_bytes);
	}

	cp->phys.cmd.addr	= cpu_to_scr(cmd_ba);
	cp->phys.cmd.size	= cpu_to_scr(cmd_len);

	return 0;
}

/*
 *  How complex it gets to deal with the data in CAM.
 *  I bet physical data will never be used in our galaxy.
 */
static int sym_setup_data(hcb_p np, struct ccb_scsiio *csio, ccb_p cp)
{
	struct ccb_hdr *ccb_h;
	int dir, retv;
	u32 lastp, goalp;
	
	ccb_h = &csio->ccb_h;

	/*
	 *  Now deal with the data.
	 */
	cp->data_len = 0;
	cp->segments = 0;

	/*
	 *  No direction means no data.
	 */
	dir = (ccb_h->flags & CAM_DIR_MASK);
	if (dir == CAM_DIR_NONE)
		goto end_scatter;

	if (!(ccb_h->flags & CAM_SCATTER_VALID)) {
		/* Single buffer */
		if (!(ccb_h->flags & CAM_DATA_PHYS)) {
			/* Buffer is virtual */
			retv = sym_scatter_virtual(np, cp,
						(vm_offset_t) csio->data_ptr, 
						(vm_size_t) csio->dxfer_len);
		} else {
			/* Buffer is physical */
			retv = sym_scatter_physical(np, cp,
						(vm_offset_t) csio->data_ptr, 
						(vm_size_t) csio->dxfer_len);
		}
		if (retv < 0)
			goto too_big;
	} else {
		/* Scatter/gather list */
		int i;
		struct bus_dma_segment *segs;
		segs = (struct bus_dma_segment *)csio->data_ptr;

		if ((ccb_h->flags & CAM_SG_LIST_PHYS) != 0) {
			/* The SG list pointer is physical */
			sym_set_cam_status(cp->cam_ccb, CAM_REQ_INVALID);
			return -1;
		}
		retv = 0;
		if (!(ccb_h->flags & CAM_DATA_PHYS)) {
			/* SG buffer pointers are virtual */
			for (i = csio->sglist_cnt - 1 ;  i >= 0 ; --i) {
				retv = sym_scatter_virtual(np, cp, 
							   segs[i].ds_addr,
							   segs[i].ds_len);
				if (retv < 0)
					break;
			}
		} else {
			/* SG buffer pointers are physical */
			for (i = csio->sglist_cnt - 1 ;  i >= 0 ; --i) {
				retv = sym_scatter_physical(np, cp,
							    segs[i].ds_addr,
							    segs[i].ds_len);
				if (retv < 0)
					break;
			}
		}
		if (retv < 0)
			goto too_big;
	}

end_scatter:
	/*
	 *  No segments means no data.
	 */
	if (!cp->segments)
		dir = CAM_DIR_NONE;

	/*
	 *  Set the data pointer.
	 */
	switch(dir) {
	case CAM_DIR_OUT:
		goalp = SCRIPT_BA (np, data_out2) + 8;
		lastp = goalp - 8 - (cp->segments * (2*4));
		break;
	case CAM_DIR_IN:
		goalp = SCRIPT_BA (np, data_in2) + 8;
		lastp = goalp - 8 - (cp->segments * (2*4));
		break;
	case CAM_DIR_NONE:
	default:
		lastp = goalp = SCRIPTH_BA (np, no_data);
		break;
	}

	cp->phys.lastp = cpu_to_scr(lastp);
	cp->phys.goalp = cpu_to_scr(goalp);
	cp->phys.savep = cpu_to_scr(lastp);
	cp->startp     = cp->phys.savep;

	/*
	 *  Activate this job.
	 */
	sym_put_start_queue(np, cp);

	/*
	 *  Command is successfully queued.
	 */
	return 0;
too_big:
	sym_set_cam_status(cp->cam_ccb, CAM_REQ_TOO_BIG);
	return -1;
}

/*
 *  Scatter a virtual buffer into bus addressable chunks.
 */
static int
sym_scatter_virtual(hcb_p np, ccb_p cp, vm_offset_t vaddr, vm_size_t len)
{
	u_long	pe, pn;
	u_long	n, k; 
	int s;
#ifdef	SYM_DEBUG_PM_WITH_WSR
	int k0 = 0;
#endif

	cp->data_len += len;

	pe = vaddr + len;
	n  = len;
	s  = SYM_CONF_MAX_SG - 1 - cp->segments;

	while (n && s >= 0) {
		pn = (pe - 1) & ~PAGE_MASK;
		k = pe - pn;
#ifdef	SYM_DEBUG_PM_WITH_WSR
		if (len < 20 && k >= 2) {
			k = (k0&1) ? 1 : 2;
			pn = pe - k;
			++k0;
			if (k0 == 1) printf("[%d]:", (int)len);
		}
#if 0
		if (len > 512 && len < 515 && k > 512) {
			k = 512;
			pn = pe - k;
			++k0;
			if (k0 == 1) printf("[%d]:", (int)len);
		}
#endif
#endif
		if (k > n) {
			k  = n;
			pn = pe - n;
		}
		if (DEBUG_FLAGS & DEBUG_SCATTER) {
			printf ("%s scatter: va=%lx pa=%lx siz=%lx\n",
				sym_name(np), pn, (u_long) vtobus(pn), k);
		}
		cp->phys.data[s].addr = cpu_to_scr(vtobus(pn));
		cp->phys.data[s].size = cpu_to_scr(k);
		pe = pn;
		n -= k;
		--s;
#ifdef	SYM_DEBUG_PM_WITH_WSR
		if (k0)
			printf(" %d", (int)k);
#endif
	}
	cp->segments = SYM_CONF_MAX_SG - 1 - s;

#ifdef	SYM_DEBUG_PM_WITH_WSR
	if (k0)
		printf("\n");
#endif
	return n ? -1 : 0;
}

/*
 *  Will stay so forever, in my opinion.
 */
static int
sym_scatter_physical(hcb_p np, ccb_p cp, vm_offset_t vaddr, vm_size_t len)
{
	return -1;
}

/*
 *  SIM action for non performance critical stuff.
 */
static void sym_action2(struct cam_sim *sim, union ccb *ccb)
{
	hcb_p	np;
	tcb_p	tp;
	lcb_p	lp;
	struct	ccb_hdr  *ccb_h;

	/*
	 *  Retrieve our controller data structure.
	 */
	np = (hcb_p) cam_sim_softc(sim);

	ccb_h = &ccb->ccb_h;

	switch (ccb_h->func_code) {
	case XPT_SET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts;

		cts  = &ccb->cts;
		tp = &np->target[ccb_h->target_id];

		/*
		 *  Update our transfer settings (basically WIDE/SYNC). 
		 *  These features are to be handled in a per target 
		 *  basis according to SCSI specifications.
		 */
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0)
			sym_update_trans(np, tp, &tp->tinfo.user, cts);

		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
			sym_update_trans(np, tp, &tp->tinfo.goal, cts);

		/*
		 *  Update our disconnect and tag settings.
		 *  SCSI requires CmdQue feature to be handled in a per 
		 *  device (logical unit) basis.
		 */
		lp = sym_lp(np, tp, ccb_h->target_lun);
		if (lp) {
			if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0)
				sym_update_dflags(np, &lp->user_flags, cts);
			if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0)
				sym_update_dflags(np, &lp->current_flags, cts);
		}

		sym_xpt_done2(np, ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts;
		struct sym_trans *tip;
		u_char dflags;

		cts = &ccb->cts;
		tp = &np->target[ccb_h->target_id];
		lp = sym_lp(np, tp, ccb_h->target_lun);

		if ((cts->flags & CCB_TRANS_CURRENT_SETTINGS) != 0) {
			tip = &tp->tinfo.current;
			dflags = lp ? lp->current_flags : 0;
		}
		else {
			tip = &tp->tinfo.user;
			dflags = lp ? lp->user_flags : tp->usrflags;
		}
		
		cts->sync_period = tip->period;
		cts->sync_offset = tip->offset;
		cts->bus_width   = tip->width;

		cts->valid = CCB_TRANS_SYNC_RATE_VALID
			   | CCB_TRANS_SYNC_OFFSET_VALID
			   | CCB_TRANS_BUS_WIDTH_VALID;

		if (lp) {
			cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);

			if (dflags & SYM_DISC_ENABLED)
				cts->flags |= CCB_TRANS_DISC_ENB;

			if (dflags & SYM_TAGS_ENABLED)
				cts->flags |= CCB_TRANS_TAG_ENB;

			cts->valid |= CCB_TRANS_DISC_VALID;
			cts->valid |= CCB_TRANS_TQ_VALID;
		}

		sym_xpt_done2(np, ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg;
		u32 size_mb;
		u32 secs_per_cylinder;
		int extended;

		/*
		 *  Silly DOS geometry.  
		 */
		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		extended = 1;
		
		if (size_mb > 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		sym_xpt_done2(np, ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		cpi->version_num = 1;
		cpi->hba_inquiry = PI_MDP_ABLE|PI_SDTR_ABLE|PI_TAG_ABLE;
		if ((np->features & FE_WIDE) != 0)
			cpi->hba_inquiry |= PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		if (np->usrflags & SYM_SCAN_TARGETS_HILO)
			cpi->hba_misc |= PIM_SCANHILO;
		if (np->usrflags & SYM_AVOID_BUS_RESET)
			cpi->hba_misc |= PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = (np->features & FE_WIDE) ? 15 : 7;
		/* Semantic problem:)LUN number max = max number of LUNs - 1 */
		cpi->max_lun = SYM_CONF_MAX_LUN-1;
		if (SYM_SETUP_MAX_LUN < SYM_CONF_MAX_LUN)
			cpi->max_lun = SYM_SETUP_MAX_LUN-1;
		cpi->bus_id = cam_sim_bus(sim);
		cpi->initiator_id = np->myaddr;
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Symbios", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		sym_xpt_done2(np, ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_ABORT:
	{
		union ccb *abort_ccb = ccb->cab.abort_ccb;
		switch(abort_ccb->ccb_h.func_code) {
		case XPT_SCSI_IO:
			if (sym_abort_scsiio(np, abort_ccb, 0) == 0) {
				sym_xpt_done2(np, ccb, CAM_REQ_CMP);
				break;
			}
		default:
			sym_xpt_done2(np, ccb, CAM_UA_ABORT);
			break;
		}
		break;
	}
	case XPT_RESET_DEV:
	{
		sym_reset_dev(np, ccb);
		break;
	}
	case XPT_RESET_BUS:
	{
		sym_reset_scsi_bus(np, 0);
		if (sym_verbose) {
			xpt_print_path(np->path);
			printf("SCSI BUS reset delivered.\n");
		}
		sym_init (np, 1);
		sym_xpt_done2(np, ccb, CAM_REQ_CMP);
		break;
	}
	case XPT_ACCEPT_TARGET_IO:
	case XPT_CONT_TARGET_IO:
	case XPT_EN_LUN:
	case XPT_NOTIFY_ACK:
	case XPT_IMMED_NOTIFY:
	case XPT_TERM_IO:
	default:
		sym_xpt_done2(np, ccb, CAM_REQ_INVALID);
		break;
	}
}

/*
 *  Update transfer settings of a target.
 */
static void sym_update_trans(hcb_p np, tcb_p tp, struct sym_trans *tip,
			    struct ccb_trans_settings *cts)
{
	/*
	 *  Update the infos.
	 */
	if ((cts->valid & CCB_TRANS_BUS_WIDTH_VALID) != 0)
		tip->width = cts->bus_width;
	if ((cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) != 0)
		tip->offset = cts->sync_offset;
	if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) != 0)
		tip->period = cts->sync_period;

	/*
	 *  Scale against out limits.
	 */
	if (tip->width  > SYM_SETUP_MAX_WIDE)	tip->width  =SYM_SETUP_MAX_WIDE;
	if (tip->width  > np->maxwide)		tip->width  = np->maxwide;
	if (tip->offset > SYM_SETUP_MAX_OFFS)	tip->offset =SYM_SETUP_MAX_OFFS;
	if (tip->offset > np->maxoffs)		tip->offset = np->maxoffs;
	if (tip->period) {
		if (tip->period < SYM_SETUP_MIN_SYNC)
			tip->period = SYM_SETUP_MIN_SYNC;
		if (np->features & FE_ULTRA3) {
			if (tip->period < np->minsync_dt)
				tip->period = np->minsync_dt;
		}
		else {
			if (tip->period < np->minsync)
				tip->period = np->minsync;
		}
		if (tip->period > np->maxsync)
			tip->period = np->maxsync;
	}
}

/*
 *  Update flags for a device (logical unit).
 */
static void 
sym_update_dflags(hcb_p np, u_char *flags, struct ccb_trans_settings *cts)
{
	if ((cts->valid & CCB_TRANS_DISC_VALID) != 0) {
		if ((cts->flags & CCB_TRANS_DISC_ENB) != 0)
			*flags |= SYM_DISC_ENABLED;
		else
			*flags &= ~SYM_DISC_ENABLED;
	}

	if ((cts->valid & CCB_TRANS_TQ_VALID) != 0) {
		if ((cts->flags & CCB_TRANS_TAG_ENB) != 0)
			*flags |= SYM_TAGS_ENABLED;
		else
			*flags &= ~SYM_TAGS_ENABLED;
	}
}


/*============= DRIVER INITIALISATION ==================*/

#ifdef FreeBSD_4_Bus

static device_method_t sym_pci_methods[] = {
	DEVMETHOD(device_probe,	 sym_pci_probe),
	DEVMETHOD(device_attach, sym_pci_attach),
	{ 0, 0 }
};

static driver_t sym_pci_driver = {
	"sym",
	sym_pci_methods,
	sizeof(struct sym_hcb)
};

static devclass_t sym_devclass;

DRIVER_MODULE(sym, pci, sym_pci_driver, sym_devclass, 0, 0);

#else	/* Pre-FreeBSD_4_Bus */

static u_long sym_unit;

static struct	pci_device sym_pci_driver = {
	"sym",
	sym_pci_probe,
	sym_pci_attach,
	&sym_unit,
	NULL
}; 

DATA_SET (pcidevice_set, sym_pci_driver);

#endif /* FreeBSD_4_Bus */

static struct sym_pci_chip sym_pci_dev_table[] = {
 {PCI_ID_SYM53C810, 0x0f, "810", 4, 8, 4, 0,
 FE_ERL}
 ,
 {PCI_ID_SYM53C810, 0xff, "810a", 4,  8, 4, 1,
 FE_CACHE_SET|FE_LDSTR|FE_PFEN|FE_BOF}
 ,
 {PCI_ID_SYM53C825, 0x0f, "825", 6,  8, 4, 0,
 FE_WIDE|FE_BOF|FE_ERL|FE_DIFF}
 ,
 {PCI_ID_SYM53C825, 0xff, "825a", 6,  8, 4, 2,
 FE_WIDE|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|FE_RAM|FE_DIFF}
 ,
 {PCI_ID_SYM53C860, 0xff, "860", 4,  8, 5, 1,
 FE_ULTRA|FE_CLK80|FE_CACHE_SET|FE_BOF|FE_LDSTR|FE_PFEN}
 ,
 {PCI_ID_SYM53C875, 0x01, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_CLK80|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF}
 ,
 {PCI_ID_SYM53C875, 0xff, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF}
 ,
 {PCI_ID_SYM53C875_2, 0xff, "875", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF}
 ,
 {PCI_ID_SYM53C885, 0xff, "885", 6, 16, 5, 2,
 FE_WIDE|FE_ULTRA|FE_DBLR|FE_CACHE0_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_DIFF}
 ,
 {PCI_ID_SYM53C895, 0xff, "895", 6, 31, 7, 2,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_LCKFRQ}
 ,
 {PCI_ID_SYM53C896, 0xff, "896", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_ID_SYM53C895A, 0xff, "895a", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_IO256|FE_NOPM|FE_LEDC|FE_LCKFRQ}
 ,
 {PCI_ID_LSI53C1010, 0x00, "1010", 6, 62, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_IO256|FE_NOPM|FE_LEDC|FE_PCI66|FE_CRC|
 FE_C10}
 ,
 {PCI_ID_LSI53C1010, 0xff, "1010", 6, 62, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_IO256|FE_NOPM|FE_LEDC|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_ID_LSI53C1010_2, 0xff, "1010", 6, 62, 7, 8,
 FE_WIDE|FE_ULTRA3|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFBC|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_RAM8K|FE_64BIT|FE_IO256|FE_NOPM|FE_LEDC|FE_PCI66|FE_CRC|
 FE_C10|FE_U3EN}
 ,
 {PCI_ID_LSI53C1510D, 0xff, "1510d", 6, 31, 7, 4,
 FE_WIDE|FE_ULTRA2|FE_QUAD|FE_CACHE_SET|FE_BOF|FE_DFS|FE_LDSTR|FE_PFEN|
 FE_RAM|FE_IO256|FE_LEDC}
};

#define sym_pci_num_devs \
	(sizeof(sym_pci_dev_table) / sizeof(sym_pci_dev_table[0]))

/*
 *  Look up the chip table.
 *
 *  Return a pointer to the chip entry if found, 
 *  zero otherwise.
 */
static struct sym_pci_chip *
#ifdef FreeBSD_4_Bus
sym_find_pci_chip(device_t dev)
#else
sym_find_pci_chip(pcici_t pci_tag)
#endif
{
	struct	sym_pci_chip *chip;
	int	i;
	u_short	device_id;
	u_char	revision;

#ifdef FreeBSD_4_Bus
	if (pci_get_vendor(dev) != PCI_VENDOR_NCR)
		return 0;

	device_id = pci_get_device(dev);
	revision  = pci_get_revid(dev);
#else
	if (pci_cfgread(pci_tag, PCIR_VENDOR, 2) != PCI_VENDOR_NCR)
		return 0;

	device_id = pci_cfgread(pci_tag, PCIR_DEVICE, 2);
	revision  = pci_cfgread(pci_tag, PCIR_REVID,  1);
#endif

	for (i = 0; i < sym_pci_num_devs; i++) {
		chip = &sym_pci_dev_table[i];
		if (device_id != chip->device_id)
			continue;
		if (revision > chip->revision_id)
			continue;
		if (FE_LDSTR & chip->features)
			return chip;
		break;
	}

	return 0;
}

/*
 *  Tell upper layer if the chip is supported.
 */
#ifdef FreeBSD_4_Bus
static int
sym_pci_probe(device_t dev)
{
	struct	sym_pci_chip *chip;

	chip = sym_find_pci_chip(dev);
	if (chip) {
		device_set_desc(dev, chip->name);
		return (chip->lp_probe_bit & SYM_SETUP_LP_PROBE_MAP)? -2000 : 0;
	}
	return ENXIO;
}
#else /* Pre-FreeBSD_4_Bus */
static const char *
sym_pci_probe(pcici_t pci_tag, pcidi_t type)
{
	struct	sym_pci_chip *chip;

	chip = sym_find_pci_chip(pci_tag);
#if NNCR > 0
	/* Only claim chips we are allowed to take precedence over the ncr */
	if (chip && !(chip->lp_probe_bit & SYM_SETUP_LP_PROBE_MAP))
#else
	if (chip)
#endif
		return chip->name;
	return 0;
}
#endif

/*
 *  Attach a sym53c8xx device.
 */
#ifdef FreeBSD_4_Bus
static int
sym_pci_attach(device_t dev)
#else
static void
sym_pci_attach(pcici_t pci_tag, int unit)
{
	int err = sym_pci_attach2(pci_tag, unit);
	if (err)
		printf("sym: failed to attach unit %d - err=%d.\n", unit, err);
}
static int
sym_pci_attach2(pcici_t pci_tag, int unit)
#endif
{
	struct	sym_pci_chip *chip;
	u_short	command;
	u_char	cachelnsz;
	struct	sym_hcb *np = 0;
	struct	sym_nvram nvram;
	int 	i;

	/*
	 *  Only probed devices should be attached.
	 *  We just enjoy being paranoid. :)
	 */
#ifdef FreeBSD_4_Bus
	chip = sym_find_pci_chip(dev);
#else
	chip = sym_find_pci_chip(pci_tag);
#endif
	if (chip == NULL)
		return (ENXIO);

	/*
	 *  Allocate immediately the host control block, 
	 *  since we are only expecting to succeed. :)
	 *  We keep track in the HCB of all the resources that 
	 *  are to be released on error.
	 */
	np = sym_calloc(sizeof(*np), "HCB");
	if (!np)
		goto attach_failed;

	/*
	 *  Copy some useful infos to the HCB.
	 */
	np->verbose	 = bootverbose;
#ifdef FreeBSD_4_Bus
	np->device	 = dev;
	np->unit	 = device_get_unit(dev);
	np->device_id	 = pci_get_device(dev);
	np->revision_id  = pci_get_revid(dev);
#else
	np->pci_tag	 = pci_tag;
	np->unit	 = unit;
	np->device_id	 = pci_cfgread(pci_tag, PCIR_DEVICE, 2);
	np->revision_id  = pci_cfgread(pci_tag, PCIR_REVID,  1);
#endif
	np->features	 = chip->features;
	np->clock_divn	 = chip->nr_divisor;
	np->maxoffs	 = chip->offset_max;
	np->maxburst	 = chip->burst_max;

	/*
	 * Edit its name.
	 */
	snprintf(np->inst_name, sizeof(np->inst_name), "sym%d", np->unit);

	/*
	 *  Read and apply some fix-ups to the PCI COMMAND 
	 *  register. We want the chip to be enabled for:
	 *  - BUS mastering
	 *  - PCI parity checking (reporting would also be fine)
	 *  - Write And Invalidate.
	 */
#ifdef FreeBSD_4_Bus
	command = pci_read_config(dev, PCIR_COMMAND, 2);
#else
	command = pci_cfgread(pci_tag, PCIR_COMMAND, 2);
#endif
	command |= PCIM_CMD_BUSMASTEREN;
	command |= PCIM_CMD_PERRESPEN;
	command |= /* PCIM_CMD_MWIEN */ 0x0010;
#ifdef FreeBSD_4_Bus
	pci_write_config(dev, PCIR_COMMAND, command, 2);
#else
	pci_cfgwrite(pci_tag, PCIR_COMMAND, command, 2);
#endif

	/*
	 *  Let the device know about the cache line size, 
	 *  if it doesn't yet.
	 */
#ifdef FreeBSD_4_Bus
	cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
#else
	cachelnsz = pci_cfgread(pci_tag, PCIR_CACHELNSZ, 1);
#endif
	if (!cachelnsz) {
		cachelnsz = 8;
#ifdef FreeBSD_4_Bus
		pci_write_config(dev, PCIR_CACHELNSZ, cachelnsz, 1);
#else
		pci_cfgwrite(pci_tag, PCIR_CACHELNSZ, cachelnsz, 1);
#endif
	}

	/*
	 *  Alloc/get/map/retrieve everything that deals with MMIO.
	 */
#ifdef FreeBSD_4_Bus
	if ((command & PCIM_CMD_MEMEN) != 0) {
		int regs_id = SYM_PCI_MMIO;
		np->mmio_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &regs_id,
						  0, ~0, 1, RF_ACTIVE);
	}
	if (!np->mmio_res) {
		device_printf(dev, "failed to allocate MMIO resources\n");
		goto attach_failed;
	}
	np->mmio_bsh = rman_get_bushandle(np->mmio_res);
	np->mmio_tag = rman_get_bustag(np->mmio_res);
	np->mmio_pa  = rman_get_start(np->mmio_res);
	np->mmio_va  = (vm_offset_t) rman_get_virtual(np->mmio_res);
	np->mmio_ba  = np->mmio_pa;
#else
	if ((command & PCIM_CMD_MEMEN) != 0) {
		vm_offset_t vaddr, paddr;
		if (!pci_map_mem(pci_tag, SYM_PCI_MMIO, &vaddr, &paddr)) {
			printf("%s: failed to map MMIO window\n", sym_name(np));
			goto attach_failed;
		}
		np->mmio_va = vaddr;
		np->mmio_pa = paddr;
		np->mmio_ba = paddr;
	}
#endif

	/*
	 *  Allocate the IRQ.
	 */
#ifdef FreeBSD_4_Bus
	i = 0;
	np->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &i,
					 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!np->irq_res) {
		device_printf(dev, "failed to allocate IRQ resource\n");
		goto attach_failed;
	}
#endif

#ifdef	SYM_CONF_IOMAPPED
	/*
	 *  User want us to use normal IO with PCI.
	 *  Alloc/get/map/retrieve everything that deals with IO.
	 */
#ifdef FreeBSD_4_Bus
	if ((command & PCI_COMMAND_IO_ENABLE) != 0) {
		int regs_id = SYM_PCI_IO;
		np->io_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &regs_id,
						0, ~0, 1, RF_ACTIVE);
	}
	if (!np->io_res) {
		device_printf(dev, "failed to allocate IO resources\n");
		goto attach_failed;
	}
	np->io_bsh  = rman_get_bushandle(np->io_res);
	np->io_tag  = rman_get_bustag(np->io_res);
	np->io_port = rman_get_start(np->io_res);
#else
	if ((command & PCI_COMMAND_IO_ENABLE) != 0) {
		pci_port_t io_port;
		if (!pci_map_port (pci_tag, SYM_PCI_IO, &io_port)) {
			printf("%s: failed to map IO window\n", sym_name(np));
			goto attach_failed;
		}
		np->io_port = io_port;
	}
#endif

#endif /* SYM_CONF_IOMAPPED */

	/*
	 *  If the chip has RAM.
	 *  Alloc/get/map/retrieve the corresponding resources.
	 */
	if ((np->features & (FE_RAM|FE_RAM8K)) &&
	    (command & PCIM_CMD_MEMEN) != 0) {
#ifdef FreeBSD_4_Bus
		int regs_id = SYM_PCI_RAM;
		if (np->features & FE_64BIT)
			regs_id = SYM_PCI_RAM64;
		np->ram_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &regs_id,
						 0, ~0, 1, RF_ACTIVE);
		if (!np->ram_res) {
			device_printf(dev,"failed to allocate RAM resources\n");
			goto attach_failed;
		}
		np->ram_id  = regs_id;
		np->ram_bsh = rman_get_bushandle(np->ram_res);
		np->ram_tag = rman_get_bustag(np->ram_res);
		np->ram_pa  = rman_get_start(np->ram_res);
		np->ram_va  = (vm_offset_t) rman_get_virtual(np->ram_res);
		np->ram_ba  = np->ram_pa;
#else
		vm_offset_t vaddr, paddr;
		int regs_id = SYM_PCI_RAM;
		if (np->features & FE_64BIT)
			regs_id = SYM_PCI_RAM64;
		if (!pci_map_mem(pci_tag, regs_id, &vaddr, &paddr)) {
			printf("%s: failed to map RAM window\n", sym_name(np));
			goto attach_failed;
		}
		np->ram_va = vaddr;
		np->ram_pa = paddr;
		np->ram_ba = paddr;
#endif
	}

	/*
	 *  Save setting of some IO registers, so we will 
	 *  be able to probe specific implementations.
	 */
	sym_save_initial_setting (np);

	/*
	 *  Reset the chip now, since it has been reported 
	 *  that SCSI clock calibration may not work properly 
	 *  if the chip is currently active.
	 */
	sym_chip_reset (np);

	/*
	 *  Try to read the user set-up.
	 */
	(void) sym_read_nvram(np, &nvram);

	/*
	 *  Prepare controller and devices settings, according 
	 *  to chip features, user set-up and driver set-up.
	 */
	(void) sym_prepare_setting(np, &nvram);

	/*
	 *  Check the PCI clock frequency.
	 *  Must be performed after prepare_setting since it destroys 
	 *  STEST1 that is used to probe for the clock doubler.
	 */
	i = sym_getpciclock(np);
	if (i > 37000)
#ifdef FreeBSD_4_Bus
		device_printf(dev, "PCI BUS clock seems too high: %u KHz.\n",i);
#else
		printf("%s: PCI BUS clock seems too high: %u KHz.\n",
			sym_name(np), i);
#endif

	/*
	 *  Allocate the start queue.
	 */
	np->squeue = (u32 *) sym_calloc(sizeof(u32)*(MAX_QUEUE*2), "SQUEUE");
	if (!np->squeue)
		goto attach_failed;

	/*
	 *  Allocate the done queue.
	 */
	np->dqueue = (u32 *) sym_calloc(sizeof(u32)*(MAX_QUEUE*2), "DQUEUE");
	if (!np->dqueue)
		goto attach_failed;

	/*
	 *  Allocate the target bus address array.
	 */
	np->targtbl = (u32 *) sym_calloc(256, "TARGTBL");
	if (!np->targtbl)
		goto attach_failed;

	/*
	 *  Allocate SCRIPTS areas.
	 */
	np->script0  = (struct sym_scr *)
			sym_calloc(sizeof(struct sym_scr), "SCRIPT0");
	np->scripth0 = (struct sym_scrh *)
			sym_calloc(sizeof(struct sym_scrh), "SCRIPTH0");
	if (!np->script0 || !np->scripth0)
		goto attach_failed;

	/*
	 *  Initialyze the CCB free and busy queues.
	 *  Allocate some CCB. We need at least ONE.
	 */
	sym_que_init(&np->free_ccbq);
	sym_que_init(&np->busy_ccbq);
	sym_que_init(&np->comp_ccbq);
	if (!sym_alloc_ccb(np))
		goto attach_failed;

	/*
	 * Initialyze the CAM CCB pending queue.
	 */
	sym_que_init(&np->cam_ccbq);

	/*
	 *  Fill-up variable-size parts of the SCRIPTS.
	 */
	sym_fill_scripts(&script0, &scripth0);

	/*
	 *  Calculate BUS addresses where we are going 
	 *  to load the SCRIPTS.
	 */
	np->script_ba	= vtobus(np->script0);
	np->scripth_ba	= vtobus(np->scripth0);
	np->scripth0_ba	= np->scripth_ba;

	if (np->ram_ba) {
		np->script_ba	= np->ram_ba;
		if (np->features & FE_RAM8K) {
			np->ram_ws = 8192;
			np->scripth_ba = np->script_ba + 4096;
#if BITS_PER_LONG > 32
			np->scr_ram_seg = cpu_to_scr(np->script_ba >> 32);
#endif
		}
		else
			np->ram_ws = 4096;
	}

	/*
	 *  Bind SCRIPTS with physical addresses usable by the 
	 *  SCRIPTS processor (as seen from the BUS = BUS addresses).
	 */
	sym_bind_script(np, (u32 *) &script0,
			    (u32 *) np->script0, sizeof(struct sym_scr));
	sym_bind_script(np, (u32 *) &scripth0,
			    (u32 *) np->scripth0, sizeof(struct sym_scrh));

	/*
	 *  Patch some variables in SCRIPTS.
	 *  These ones are loaded by the SCRIPTS processor.
	 */
	np->scripth0->pm0_data_addr[0] = cpu_to_scr(SCRIPT_BA(np,pm0_data));
	np->scripth0->pm1_data_addr[0] = cpu_to_scr(SCRIPT_BA(np,pm1_data));


	/*
	 *  Still some for LED support.
	 */
	if (np->features & FE_LED0) {
		np->script0->idle[0]  =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_OR,  0x01));
		np->script0->reselected[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
		np->script0->start[0] =
				cpu_to_scr(SCR_REG_REG(gpreg, SCR_AND, 0xfe));
	}

	/*
	 *  Load SCNTL4 on reselection for the C10.
	 */
	if (np->features & FE_C10) {
		np->script0->resel_scntl4[0] =
				cpu_to_scr(SCR_LOAD_REL (scntl4, 1));
		np->script0->resel_scntl4[1] =
				cpu_to_scr(offsetof(struct sym_tcb, uval));
	}

#ifdef SYM_CONF_IARB_SUPPORT
	/*
	 *    If user does not want to use IMMEDIATE ARBITRATION
	 *    when we are reselected while attempting to arbitrate,
	 *    patch the SCRIPTS accordingly with a SCRIPT NO_OP.
	 */
	if (!SYM_CONF_SET_IARB_ON_ARB_LOST)
		np->script0->ungetjob[0] = cpu_to_scr(SCR_NO_OP);

	/*
	 *    If user wants IARB to be set when we win arbitration 
	 *    and have other jobs, compute the max number of consecutive 
	 *    settings of IARB hints before we leave devices a chance to 
	 *    arbitrate for reselection.
	 */
#ifdef	SYM_SETUP_IARB_MAX
	np->iarb_max = SYM_SETUP_IARB_MAX;
#else
	np->iarb_max = 4;
#endif
#endif

	/*
	 *  Prepare the idle and invalid task actions.
	 */
	np->idletask.start	= cpu_to_scr(SCRIPT_BA(np, idle));
	np->idletask.restart	= cpu_to_scr(SCRIPTH_BA(np, bad_i_t_l));
	np->idletask_ba		= vtobus(&np->idletask);

	np->notask.start	= cpu_to_scr(SCRIPT_BA(np, idle));
	np->notask.restart	= cpu_to_scr(SCRIPTH_BA(np, bad_i_t_l));
	np->notask_ba		= vtobus(&np->notask);

	np->bad_itl.start	= cpu_to_scr(SCRIPT_BA(np, idle));
	np->bad_itl.restart	= cpu_to_scr(SCRIPTH_BA(np, bad_i_t_l));
	np->bad_itl_ba		= vtobus(&np->bad_itl);

	np->bad_itlq.start	= cpu_to_scr(SCRIPT_BA(np, idle));
	np->bad_itlq.restart	= cpu_to_scr(SCRIPTH_BA (np,bad_i_t_l_q));
	np->bad_itlq_ba		= vtobus(&np->bad_itlq);

	/*
	 *  Allocate and prepare the lun JUMP table that is used 
	 *  for a target prior the probing of devices (bad lun table).
	 *  A private table will be allocated for the target on the 
	 *  first INQUIRY response received.
	 */
	np->badluntbl = sym_calloc(256, "BADLUNTBL");
	if (!np->badluntbl)
		goto attach_failed;

	np->badlun_sa = cpu_to_scr(SCRIPTH_BA(np, resel_bad_lun));
	for (i = 0 ; i < 64 ; i++)	/* 64 luns/target, no less */
		np->badluntbl[i] = cpu_to_scr(vtobus(&np->badlun_sa));

	/*
	 *  Prepare the bus address array that contains the bus 
	 *  address of each target control bloc.
	 *  For now, assume all logical unit are wrong. :)
	 */
	np->scripth0->targtbl[0] = cpu_to_scr(vtobus(np->targtbl));
	for (i = 0 ; i < SYM_CONF_MAX_TARGET ; i++) {
		np->targtbl[i] = cpu_to_scr(vtobus(&np->target[i]));
		np->target[i].luntbl_sa = cpu_to_scr(vtobus(np->badluntbl));
		np->target[i].lun0_sa = cpu_to_scr(vtobus(&np->badlun_sa));
	}

	/*
	 *  Now check the cache handling of the pci chipset.
	 */
	if (sym_snooptest (np)) {
#ifdef FreeBSD_4_Bus
		device_printf(dev, "CACHE INCORRECTLY CONFIGURED.\n");
#else
		printf("%s: CACHE INCORRECTLY CONFIGURED.\n", sym_name(np));
#endif
		goto attach_failed;
	};

	/*
	 *  Now deal with CAM.
	 *  Hopefully, we will succeed with that one.:)
	 */
	if (!sym_cam_attach(np))
		goto attach_failed;

	/*
	 *  Sigh! we are done.
	 */
	return 0;

	/*
	 *  We have failed.
	 *  We will try to free all the resources we have 
	 *  allocated, but if we are a boot device, this 
	 *  will not help that much.;)
	 */
attach_failed:
	if (np)
		sym_pci_free(np);
	return ENXIO;
}

/*
 *  Free everything that have been allocated for this device.
 */
static void sym_pci_free(hcb_p np)
{
	SYM_QUEHEAD *qp;
	ccb_p cp;
	tcb_p tp;
	lcb_p lp;
	int target, lun;
	int s;

	/*
	 *  First free CAM resources.
	 */
	s = splcam();
	sym_cam_free(np);
	splx(s);

	/*
	 *  Now every should be quiet for us to 
	 *  free other resources.
	 */
#ifdef FreeBSD_4_Bus
	if (np->ram_res)
		bus_release_resource(np->device, SYS_RES_MEMORY, 
				     np->ram_id, np->ram_res);
	if (np->mmio_res)
		bus_release_resource(np->device, SYS_RES_MEMORY, 
				     SYM_PCI_MMIO, np->mmio_res);
	if (np->io_res)
		bus_release_resource(np->device, SYS_RES_IOPORT, 
				     SYM_PCI_IO, np->io_res);
	if (np->irq_res)
		bus_release_resource(np->device, SYS_RES_IRQ, 
				     0, np->irq_res);
#else
	/*
	 *  YEAH!!!
	 *  It seems there is no means to free MMIO resources.
	 */
#endif

	if (np->scripth0)
		sym_mfree(np->scripth0, sizeof(struct sym_scrh), "SCRIPTH0");
	if (np->script0)
		sym_mfree(np->script0, sizeof(struct sym_scr), "SCRIPT0");
	if (np->squeue)
		sym_mfree(np->squeue, sizeof(u32)*(MAX_QUEUE*2), "SQUEUE");
	if (np->dqueue)
		sym_mfree(np->dqueue, sizeof(u32)*(MAX_QUEUE*2), "DQUEUE");

	while ((qp = sym_remque_head(&np->free_ccbq)) != 0) {
		cp = sym_que_entry(qp, struct sym_ccb, link_ccbq);
		sym_mfree(cp, sizeof(*cp), "CCB");
	}

	if (np->badluntbl)
		sym_mfree(np->badluntbl, 256,"BADLUNTBL");

	for (target = 0; target < SYM_CONF_MAX_TARGET ; target++) {
		tp = &np->target[target];
		for (lun = 0 ; lun < SYM_CONF_MAX_LUN ; lun++) {
			lp = sym_lp(np, tp, lun);
			if (!lp)
				continue;
			if (lp->itlq_tbl)
				sym_mfree(lp->itlq_tbl, SYM_CONF_MAX_TASK*4,
				       "ITLQ_TBL");
			if (lp->cb_tags)
				sym_mfree(lp->cb_tags, SYM_CONF_MAX_TASK,
				       "CB_TAGS");
			sym_mfree(lp, sizeof(*lp), "LCB");
		}
#if SYM_CONF_MAX_LUN > 1
		if (tp->lunmp)
			sym_mfree(tp->lunmp, SYM_CONF_MAX_LUN*sizeof(lcb_p),
			       "LUNMP");
#endif 
	}

	sym_mfree(np, sizeof(*np), "HCB");
}

/*
 *  Allocate CAM resources and register a bus to CAM.
 */
int sym_cam_attach(hcb_p np)
{
	struct cam_devq *devq = 0;
	struct cam_sim *sim = 0;
	struct cam_path *path = 0;
	int err, s;

	s = splcam();

	/*
	 *  Establish our interrupt handler.
	 */
#ifdef FreeBSD_4_Bus
	err = bus_setup_intr(np->device, np->irq_res, INTR_TYPE_CAM,
			     sym_intr, np, &np->intr);
	if (err) {
		device_printf(np->device, "bus_setup_intr() failed: %d\n",
			      err);
		goto fail;
	}
#else
	if (!pci_map_int (np->pci_tag, sym_intr, np, &cam_imask)) {
		printf("%s: failed to map interrupt\n", sym_name(np));
		goto fail;
	}
#endif

	/*
	 *  Create the device queue for our sym SIM.
	 */
	devq = cam_simq_alloc(SYM_CONF_MAX_START);
	if (!devq)
		goto fail;

	/*
	 *  Construct our SIM entry.
	 */
	sim = cam_sim_alloc(sym_action, sym_poll, "sym", np, np->unit,
			    1, SYM_SETUP_MAX_TAG, devq);
	if (!sim)
		goto fail;
	devq = 0;

	if (xpt_bus_register(sim, 0) != CAM_SUCCESS)
		goto fail;
	np->sim = sim;
	sim = 0;

	if (xpt_create_path(&path, 0,
			    cam_sim_path(np->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		goto fail;
	}
	np->path = path;

	/*
	 *  Hmmm... This should be useful, but I donnot want to 
	 *  know about.
	 */
#if 	__FreeBSD_version < 400000
#ifdef	__alpha__
#ifdef	FreeBSD_4_Bus
	alpha_register_pci_scsi(pci_get_bus(np->device),
				pci_get_slot(np->device), np->sim);
#else
	alpha_register_pci_scsi(pci_tag->bus, pci_tag->slot, np->sim);
#endif
#endif
#endif

#if 0
	/*
	 *  Establish our async notification handler.
	 */
	{
	struct ccb_setasync csa;
	xpt_setup_ccb(&csa.ccb_h, np->path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable    = AC_LOST_DEVICE;
	csa.callback	    = sym_async;
	csa.callback_arg    = np->sim;
	xpt_action((union ccb *)&csa);
	}
#endif
	/*
	 *  Start the chip now, without resetting the BUS, since  
	 *  it seems that this must stay under control of CAM.
	 *  With LVD/SE capable chips and BUS in SE mode, we may 
	 *  get a spurious SMBC interrupt.
	 */
	sym_init (np, 0);

	splx(s);
	return 1;
fail:
	if (sim)
		cam_sim_free(sim, FALSE);
	if (devq)
		cam_simq_free(devq);

	sym_cam_free(np);

	splx(s);
	return 0;
}

/*
 *  Free everything that deals with CAM.
 */
void sym_cam_free(hcb_p np)
{
#ifdef FreeBSD_4_Bus
	if (np->intr)
		bus_teardown_intr(np->device, np->irq_res, np->intr);
#else
	/* pci_unmap_int(np->pci_tag); */	/* Does nothing */
#endif
	
	if (np->sim) {
		xpt_bus_deregister(cam_sim_path(np->sim));
		cam_sim_free(np->sim, /*free_devq*/ TRUE);
	}
	if (np->path)
		xpt_free_path(np->path);
}

/*============ OPTIONNAL NVRAM SUPPORT =================*/

/*
 *  Get host setup from NVRAM.
 */
static void sym_nvram_setup_host (hcb_p np, struct sym_nvram *nvram)
{
#ifdef SYM_CONF_NVRAM_SUPPORT
	/*
	 *  Get parity checking, host ID, verbose mode 
	 *  and miscellaneous host flags from NVRAM.
	 */
	switch(nvram->type) {
	case SYM_SYMBIOS_NVRAM:
		if (!(nvram->data.Symbios.flags & SYMBIOS_PARITY_ENABLE))
			np->rv_scntl0  &= ~0x0a;
		np->myaddr = nvram->data.Symbios.host_id & 0x0f;
		if (nvram->data.Symbios.flags & SYMBIOS_VERBOSE_MSGS)
			np->verbose += 1;
		if (nvram->data.Symbios.flags1 & SYMBIOS_SCAN_HI_LO)
			np->usrflags |= SYM_SCAN_TARGETS_HILO;
		if (nvram->data.Symbios.flags2 & SYMBIOS_AVOID_BUS_RESET)
			np->usrflags |= SYM_AVOID_BUS_RESET;
		break;
	case SYM_TEKRAM_NVRAM:
		np->myaddr = nvram->data.Tekram.host_id & 0x0f;
		break;
	default:
		break;
	}
#endif
}

/*
 *  Get target setup from NVRAM.
 */
#ifdef SYM_CONF_NVRAM_SUPPORT
static void sym_Symbios_setup_target(hcb_p np,int target, Symbios_nvram *nvram);
static void sym_Tekram_setup_target(hcb_p np,int target, Tekram_nvram *nvram);
#endif

static void
sym_nvram_setup_target (hcb_p np, int target, struct sym_nvram *nvp)
{
#ifdef SYM_CONF_NVRAM_SUPPORT
	switch(nvp->type) {
	case SYM_SYMBIOS_NVRAM:
		sym_Symbios_setup_target (np, target, &nvp->data.Symbios);
		break;
	case SYM_TEKRAM_NVRAM:
		sym_Tekram_setup_target (np, target, &nvp->data.Tekram);
		break;
	default:
		break;
	}
#endif
}

#ifdef SYM_CONF_NVRAM_SUPPORT
/*
 *  Get target set-up from Symbios format NVRAM.
 */
static void
sym_Symbios_setup_target(hcb_p np, int target, Symbios_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	Symbios_target *tn = &nvram->target[target];

	tp->tinfo.user.period = tn->sync_period ? (tn->sync_period + 3) / 4 : 0;
	tp->tinfo.user.width  = tn->bus_width == 0x10 ? BUS_16_BIT : BUS_8_BIT;
	tp->usrtags =
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? SYM_SETUP_MAX_TAG : 0;

	if (!(tn->flags & SYMBIOS_DISCONNECT_ENABLE))
		tp->usrflags &= ~SYM_DISC_ENABLED;
	if (!(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME))
		tp->usrflags |= SYM_SCAN_BOOT_DISABLED;
	if (!(tn->flags & SYMBIOS_SCAN_LUNS))
		tp->usrflags |= SYM_SCAN_LUNS_DISABLED;
}

/*
 *  Get target set-up from Tekram format NVRAM.
 */
static void
sym_Tekram_setup_target(hcb_p np, int target, Tekram_nvram *nvram)
{
	tcb_p tp = &np->target[target];
	struct Tekram_target *tn = &nvram->target[target];
	int i;

	if (tn->flags & TEKRAM_SYNC_NEGO) {
		i = tn->sync_index & 0xf;
		tp->tinfo.user.period = Tekram_sync[i];
	}

	tp->tinfo.user.width =
		(tn->flags & TEKRAM_WIDE_NEGO) ? BUS_16_BIT : BUS_8_BIT;

	if (tn->flags & TEKRAM_TAGGED_COMMANDS) {
		tp->usrtags = 2 << nvram->max_tags_index;
	}

	if (tn->flags & TEKRAM_DISCONNECT_ENABLE)
		tp->usrflags |= SYM_DISC_ENABLED;
 
	/* If any device does not support parity, we will not use this option */
	if (!(tn->flags & TEKRAM_PARITY_CHECK))
		np->rv_scntl0  &= ~0x0a; /* SCSI parity checking disabled */
}

#ifdef	SYM_CONF_DEBUG_NVRAM
/*
 *  Dump Symbios format NVRAM for debugging purpose.
 */
void sym_display_Symbios_nvram(hcb_p np, Symbios_nvram *nvram)
{
	int i;

	/* display Symbios nvram host data */
	printf("%s: HOST ID=%d%s%s%s%s%s%s\n",
		sym_name(np), nvram->host_id & 0x0f,
		(nvram->flags  & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags  & SYMBIOS_PARITY_ENABLE)	? " PARITY"	:"",
		(nvram->flags  & SYMBIOS_VERBOSE_MSGS)	? " VERBOSE"	:"", 
		(nvram->flags  & SYMBIOS_CHS_MAPPING)	? " CHS_ALT"	:"", 
		(nvram->flags2 & SYMBIOS_AVOID_BUS_RESET)?" NO_RESET"	:"",
		(nvram->flags1 & SYMBIOS_SCAN_HI_LO)	? " HI_LO"	:"");

	/* display Symbios nvram drive data */
	for (i = 0 ; i < 15 ; i++) {
		struct Symbios_target *tn = &nvram->target[i];
		printf("%s-%d:%s%s%s%s WIDTH=%d SYNC=%d TMO=%d\n",
		sym_name(np), i,
		(tn->flags & SYMBIOS_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & SYMBIOS_SCAN_AT_BOOT_TIME)	? " SCAN_BOOT"	: "",
		(tn->flags & SYMBIOS_SCAN_LUNS)		? " SCAN_LUNS"	: "",
		(tn->flags & SYMBIOS_QUEUE_TAGS_ENABLED)? " TCQ"	: "",
		tn->bus_width,
		tn->sync_period / 4,
		tn->timeout);
	}
}

/*
 *  Dump TEKRAM format NVRAM for debugging purpose.
 */
static u_char Tekram_boot_delay[7] __initdata = {3, 5, 10, 20, 30, 60, 120};
void sym_display_Tekram_nvram(hcb_p np, Tekram_nvram *nvram)
{
	int i, tags, boot_delay;
	char *rem;

	/* display Tekram nvram host data */
	tags = 2 << nvram->max_tags_index;
	boot_delay = 0;
	if (nvram->boot_delay_index < 6)
		boot_delay = Tekram_boot_delay[nvram->boot_delay_index];
	switch((nvram->flags & TEKRAM_REMOVABLE_FLAGS) >> 6) {
	default:
	case 0:	rem = "";			break;
	case 1: rem = " REMOVABLE=boot device";	break;
	case 2: rem = " REMOVABLE=all";		break;
	}

	printf("%s: HOST ID=%d%s%s%s%s%s%s%s%s%s BOOT DELAY=%d tags=%d\n",
		sym_name(np), nvram->host_id & 0x0f,
		(nvram->flags1 & SYMBIOS_SCAM_ENABLE)	? " SCAM"	:"",
		(nvram->flags & TEKRAM_MORE_THAN_2_DRIVES) ? " >2DRIVES"	:"",
		(nvram->flags & TEKRAM_DRIVES_SUP_1GB)	? " >1GB"	:"",
		(nvram->flags & TEKRAM_RESET_ON_POWER_ON) ? " RESET"	:"",
		(nvram->flags & TEKRAM_ACTIVE_NEGATION)	? " ACT_NEG"	:"",
		(nvram->flags & TEKRAM_IMMEDIATE_SEEK)	? " IMM_SEEK"	:"",
		(nvram->flags & TEKRAM_SCAN_LUNS)	? " SCAN_LUNS"	:"",
		(nvram->flags1 & TEKRAM_F2_F6_ENABLED)	? " F2_F6"	:"",
		rem, boot_delay, tags);

	/* display Tekram nvram drive data */
	for (i = 0; i <= 15; i++) {
		int sync, j;
		struct Tekram_target *tn = &nvram->target[i];
		j = tn->sync_index & 0xf;
		sync = Tekram_sync[j];
		printf("%s-%d:%s%s%s%s%s%s PERIOD=%d\n",
		sym_name(np), i,
		(tn->flags & TEKRAM_PARITY_CHECK)	? " PARITY"	: "",
		(tn->flags & TEKRAM_SYNC_NEGO)		? " SYNC"	: "",
		(tn->flags & TEKRAM_DISCONNECT_ENABLE)	? " DISC"	: "",
		(tn->flags & TEKRAM_START_CMD)		? " START"	: "",
		(tn->flags & TEKRAM_TAGGED_COMMANDS)	? " TCQ"	: "",
		(tn->flags & TEKRAM_WIDE_NEGO)		? " WIDE"	: "",
		sync);
	}
}
#endif	/* SYM_CONF_DEBUG_NVRAM */
#endif	/* SYM_CONF_NVRAM_SUPPORT */


/*
 *  Try reading Symbios or Tekram NVRAM
 */
#ifdef SYM_CONF_NVRAM_SUPPORT
static int sym_read_Symbios_nvram (hcb_p np, Symbios_nvram *nvram);
static int sym_read_Tekram_nvram  (hcb_p np, Tekram_nvram *nvram);
#endif

int sym_read_nvram(hcb_p np, struct sym_nvram *nvp)
{
#ifdef SYM_CONF_NVRAM_SUPPORT
	/*
	 *  Try to read SYMBIOS nvram.
	 *  Try to read TEKRAM nvram if Symbios nvram not found.
	 */
	if	(SYM_SETUP_SYMBIOS_NVRAM &&
		 !sym_read_Symbios_nvram (np, &nvp->data.Symbios))
		nvp->type = SYM_SYMBIOS_NVRAM;
	else if	(SYM_SETUP_TEKRAM_NVRAM &&
		 !sym_read_Tekram_nvram (np, &nvp->data.Tekram))
		nvp->type = SYM_TEKRAM_NVRAM;
	else
		nvp->type = 0;
#else
	nvp->type = 0;
#endif
	return nvp->type;
}


#ifdef SYM_CONF_NVRAM_SUPPORT
/*
 *  24C16 EEPROM reading.
 *
 *  GPOI0 - data in/data out
 *  GPIO1 - clock
 *  Symbios NVRAM wiring now also used by Tekram.
 */

#define SET_BIT 0
#define CLR_BIT 1
#define SET_CLK 2
#define CLR_CLK 3

/*
 *  Set/clear data/clock bit in GPIO0
 */
static void S24C16_set_bit(hcb_p np, u_char write_bit, u_char *gpreg, 
			  int bit_mode)
{
	UDELAY (5);
	switch (bit_mode){
	case SET_BIT:
		*gpreg |= write_bit;
		break;
	case CLR_BIT:
		*gpreg &= 0xfe;
		break;
	case SET_CLK:
		*gpreg |= 0x02;
		break;
	case CLR_CLK:
		*gpreg &= 0xfd;
		break;

	}
	OUTB (nc_gpreg, *gpreg);
	UDELAY (5);
}

/*
 *  Send START condition to NVRAM to wake it up.
 */
static void S24C16_start(hcb_p np, u_char *gpreg)
{
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZzzzz!!
 */
static void S24C16_stop(hcb_p np, u_char *gpreg)
{
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	S24C16_set_bit(np, 1, gpreg, SET_BIT);
}

/*
 *  Read or write a bit to the NVRAM,
 *  read if GPIO0 input else write if GPIO0 output
 */
static void S24C16_do_bit(hcb_p np, u_char *read_bit, u_char write_bit, 
			 u_char *gpreg)
{
	S24C16_set_bit(np, write_bit, gpreg, SET_BIT);
	S24C16_set_bit(np, 0, gpreg, SET_CLK);
	if (read_bit)
		*read_bit = INB (nc_gpreg);
	S24C16_set_bit(np, 0, gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, gpreg, CLR_BIT);
}

/*
 *  Output an ACK to the NVRAM after reading,
 *  change GPIO0 to output and when done back to an input
 */
static void S24C16_write_ack(hcb_p np, u_char write_bit, u_char *gpreg, 
			    u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl & 0xfe);
	S24C16_do_bit(np, 0, write_bit, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  Input an ACK from NVRAM after writing,
 *  change GPIO0 to input and when done back to an output
 */
static void S24C16_read_ack(hcb_p np, u_char *read_bit, u_char *gpreg, 
			   u_char *gpcntl)
{
	OUTB (nc_gpcntl, *gpcntl | 0x01);
	S24C16_do_bit(np, read_bit, 1, gpreg);
	OUTB (nc_gpcntl, *gpcntl);
}

/*
 *  WRITE a byte to the NVRAM and then get an ACK to see it was accepted OK,
 *  GPIO0 must already be set as an output
 */
static void S24C16_write_byte(hcb_p np, u_char *ack_data, u_char write_data, 
			     u_char *gpreg, u_char *gpcntl)
{
	int x;
	
	for (x = 0; x < 8; x++)
		S24C16_do_bit(np, 0, (write_data >> (7 - x)) & 0x01, gpreg);
		
	S24C16_read_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  READ a byte from the NVRAM and then send an ACK to say we have got it,
 *  GPIO0 must already be set as an input
 */
static void S24C16_read_byte(hcb_p np, u_char *read_data, u_char ack_data, 
			    u_char *gpreg, u_char *gpcntl)
{
	int x;
	u_char read_bit;

	*read_data = 0;
	for (x = 0; x < 8; x++) {
		S24C16_do_bit(np, &read_bit, 1, gpreg);
		*read_data |= ((read_bit & 0x01) << (7 - x));
	}

	S24C16_write_ack(np, ack_data, gpreg, gpcntl);
}

/*
 *  Read 'len' bytes starting at 'offset'.
 */
static int sym_read_S24C16_nvram (hcb_p np, int offset, u_char *data, int len)
{
	u_char	gpcntl, gpreg;
	u_char	old_gpcntl, old_gpreg;
	u_char	ack_data;
	int	retv = 1;
	int	x;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);
	gpcntl		= old_gpcntl & 0xfc;

	/* set up GPREG & GPCNTL to set GPIO0 and GPIO1 in to known state */
	OUTB (nc_gpreg,  old_gpreg);
	OUTB (nc_gpcntl, gpcntl);

	/* this is to set NVRAM into a known state with GPIO0/1 both low */
	gpreg = old_gpreg;
	S24C16_set_bit(np, 0, &gpreg, CLR_CLK);
	S24C16_set_bit(np, 0, &gpreg, CLR_BIT);
		
	/* now set NVRAM inactive with GPIO0/1 both high */
	S24C16_stop(np, &gpreg);
	
	/* activate NVRAM */
	S24C16_start(np, &gpreg);

	/* write device code and random address MSB */
	S24C16_write_byte(np, &ack_data,
		0xa0 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* write random address LSB */
	S24C16_write_byte(np, &ack_data,
		offset & 0xff, &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* regenerate START state to set up for reading */
	S24C16_start(np, &gpreg);
	
	/* rewrite device code and address MSB with read bit set (lsb = 0x01) */
	S24C16_write_byte(np, &ack_data,
		0xa1 | ((offset >> 7) & 0x0e), &gpreg, &gpcntl);
	if (ack_data & 0x01)
		goto out;

	/* now set up GPIO0 for inputting data */
	gpcntl |= 0x01;
	OUTB (nc_gpcntl, gpcntl);
		
	/* input all requested data - only part of total NVRAM */
	for (x = 0; x < len; x++) 
		S24C16_read_byte(np, &data[x], (x == (len-1)), &gpreg, &gpcntl);

	/* finally put NVRAM back in inactive mode */
	gpcntl &= 0xfe;
	OUTB (nc_gpcntl, gpcntl);
	S24C16_stop(np, &gpreg);
	retv = 0;
out:
	/* return GPIO0/1 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

#undef SET_BIT 0
#undef CLR_BIT 1
#undef SET_CLK 2
#undef CLR_CLK 3

/*
 *  Try reading Symbios NVRAM.
 *  Return 0 if OK.
 */
static int sym_read_Symbios_nvram (hcb_p np, Symbios_nvram *nvram)
{
	static u_char Symbios_trailer[6] = {0xfe, 0xfe, 0, 0, 0, 0};
	u_char *data = (u_char *) nvram;
	int len  = sizeof(*nvram);
	u_short	csum;
	int x;

	/* probe the 24c16 and read the SYMBIOS 24c16 area */
	if (sym_read_S24C16_nvram (np, SYMBIOS_NVRAM_ADDRESS, data, len))
		return 1;

	/* check valid NVRAM signature, verify byte count and checksum */
	if (nvram->type != 0 ||
	    bcmp(nvram->trailer, Symbios_trailer, 6) ||
	    nvram->byte_count != len - 12)
		return 1;

	/* verify checksum */
	for (x = 6, csum = 0; x < len - 6; x++)
		csum += data[x];
	if (csum != nvram->checksum)
		return 1;

	return 0;
}

/*
 *  93C46 EEPROM reading.
 *
 *  GPOI0 - data in
 *  GPIO1 - data out
 *  GPIO2 - clock
 *  GPIO4 - chip select
 *
 *  Used by Tekram.
 */

/*
 *  Pulse clock bit in GPIO0
 */
static void T93C46_Clk(hcb_p np, u_char *gpreg)
{
	OUTB (nc_gpreg, *gpreg | 0x04);
	UDELAY (2);
	OUTB (nc_gpreg, *gpreg);
}

/* 
 *  Read bit from NVRAM
 */
static void T93C46_Read_Bit(hcb_p np, u_char *read_bit, u_char *gpreg)
{
	UDELAY (2);
	T93C46_Clk(np, gpreg);
	*read_bit = INB (nc_gpreg);
}

/*
 *  Write bit to GPIO0
 */
static void T93C46_Write_Bit(hcb_p np, u_char write_bit, u_char *gpreg)
{
	if (write_bit & 0x01)
		*gpreg |= 0x02;
	else
		*gpreg &= 0xfd;
		
	*gpreg |= 0x10;
		
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send STOP condition to NVRAM - puts NVRAM to sleep... ZZZzzz!!
 */
static void T93C46_Stop(hcb_p np, u_char *gpreg)
{
	*gpreg &= 0xef;
	OUTB (nc_gpreg, *gpreg);
	UDELAY (2);

	T93C46_Clk(np, gpreg);
}

/*
 *  Send read command and address to NVRAM
 */
static void T93C46_Send_Command(hcb_p np, u_short write_data, 
				u_char *read_bit, u_char *gpreg)
{
	int x;

	/* send 9 bits, start bit (1), command (2), address (6)  */
	for (x = 0; x < 9; x++)
		T93C46_Write_Bit(np, (u_char) (write_data >> (8 - x)), gpreg);

	*read_bit = INB (nc_gpreg);
}

/*
 *  READ 2 bytes from the NVRAM
 */
static void T93C46_Read_Word(hcb_p np, u_short *nvram_data, u_char *gpreg)
{
	int x;
	u_char read_bit;

	*nvram_data = 0;
	for (x = 0; x < 16; x++) {
		T93C46_Read_Bit(np, &read_bit, gpreg);

		if (read_bit & 0x01)
			*nvram_data |=  (0x01 << (15 - x));
		else
			*nvram_data &= ~(0x01 << (15 - x));
	}
}

/*
 *  Read Tekram NvRAM data.
 */
static int T93C46_Read_Data(hcb_p np, u_short *data,int len,u_char *gpreg)
{
	u_char	read_bit;
	int	x;

	for (x = 0; x < len; x++)  {

		/* output read command and address */
		T93C46_Send_Command(np, 0x180 | x, &read_bit, gpreg);
		if (read_bit & 0x01)
			return 1; /* Bad */
		T93C46_Read_Word(np, &data[x], gpreg);
		T93C46_Stop(np, gpreg);
	}

	return 0;
}

/*
 *  Try reading 93C46 Tekram NVRAM.
 */
static int sym_read_T93C46_nvram (hcb_p np, Tekram_nvram *nvram)
{
	u_char gpcntl, gpreg;
	u_char old_gpcntl, old_gpreg;
	int retv = 1;

	/* save current state of GPCNTL and GPREG */
	old_gpreg	= INB (nc_gpreg);
	old_gpcntl	= INB (nc_gpcntl);

	/* set up GPREG & GPCNTL to set GPIO0/1/2/4 in to known state, 0 in,
	   1/2/4 out */
	gpreg = old_gpreg & 0xe9;
	OUTB (nc_gpreg, gpreg);
	gpcntl = (old_gpcntl & 0xe9) | 0x09;
	OUTB (nc_gpcntl, gpcntl);

	/* input all of NVRAM, 64 words */
	retv = T93C46_Read_Data(np, (u_short *) nvram,
				sizeof(*nvram) / sizeof(short), &gpreg);
	
	/* return GPIO0/1/2/4 to original states after having accessed NVRAM */
	OUTB (nc_gpcntl, old_gpcntl);
	OUTB (nc_gpreg,  old_gpreg);

	return retv;
}

/*
 *  Try reading Tekram NVRAM.
 *  Return 0 if OK.
 */
static int sym_read_Tekram_nvram (hcb_p np, Tekram_nvram *nvram)
{
	u_char *data = (u_char *) nvram;
	int len = sizeof(*nvram);
	u_short	csum;
	int x;

	switch (np->device_id) {
	case PCI_ID_SYM53C885:
	case PCI_ID_SYM53C895:
	case PCI_ID_SYM53C896:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		break;
	case PCI_ID_SYM53C875:
		x = sym_read_S24C16_nvram(np, TEKRAM_24C16_NVRAM_ADDRESS,
					  data, len);
		if (!x)
			break;
	default:
		x = sym_read_T93C46_nvram(np, nvram);
		break;
	}
	if (x)
		return 1;

	/* verify checksum */
	for (x = 0, csum = 0; x < len - 1; x += 2)
		csum += data[x] + (data[x+1] << 8);
	if (csum != 0x1234)
		return 1;

	return 0;
}

#endif	/* SYM_CONF_NVRAM_SUPPORT */
