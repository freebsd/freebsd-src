/* $Id: p1275.c,v 1.22 2001/10/18 09:40:00 davem Exp $
 * p1275.c: Sun IEEE 1275 PROM low level interface routines
 *
 * Copyright (C) 1996,1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/spinlock.h>

#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/system.h>
#include <asm/spitfire.h>
#include <asm/pstate.h>

struct {
	long prom_callback;			/* 0x00 */
	void (*prom_cif_handler)(long *);	/* 0x08 */
	unsigned long prom_cif_stack;		/* 0x10 */
	unsigned long prom_args [23];		/* 0x18 */
	char prom_buffer [3000];
} p1275buf;

extern void prom_world(int);

void prom_cif_interface (void)
{
	__asm__ __volatile__ (
"	mov	%0, %%o0\n"
"	ldx	[%%o0 + 0x010], %%o1	! prom_cif_stack\n"
"	save	%%o1, -0x190, %%sp\n"
"	ldx	[%%i0 + 0x008], %%l2	! prom_cif_handler\n"
"	rdpr	%%pstate, %%l4\n"
"	wrpr	%%g0, 0x15, %%pstate	! save alternate globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x0b0]\n"
"	stx	%%g2, [%%sp + 2047 + 0x0b8]\n"
"	stx	%%g3, [%%sp + 2047 + 0x0c0]\n"
"	stx	%%g4, [%%sp + 2047 + 0x0c8]\n"
"	stx	%%g5, [%%sp + 2047 + 0x0d0]\n"
"	stx	%%g6, [%%sp + 2047 + 0x0d8]\n"
"	stx	%%g7, [%%sp + 2047 + 0x0e0]\n"
"	wrpr	%%g0, 0x814, %%pstate	! save interrupt globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x0e8]\n"
"	stx	%%g2, [%%sp + 2047 + 0x0f0]\n"
"	stx	%%g3, [%%sp + 2047 + 0x0f8]\n"
"	stx	%%g4, [%%sp + 2047 + 0x100]\n"
"	stx	%%g5, [%%sp + 2047 + 0x108]\n"
"	stx	%%g6, [%%sp + 2047 + 0x110]\n"
"	stx	%%g7, [%%sp + 2047 + 0x118]\n"
"	wrpr	%%g0, 0x14, %%pstate	! save normal globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x120]\n"
"	stx	%%g2, [%%sp + 2047 + 0x128]\n"
"	stx	%%g3, [%%sp + 2047 + 0x130]\n"
"	stx	%%g4, [%%sp + 2047 + 0x138]\n"
"	stx	%%g5, [%%sp + 2047 + 0x140]\n"
"	stx	%%g6, [%%sp + 2047 + 0x148]\n"
"	stx	%%g7, [%%sp + 2047 + 0x150]\n"
"	wrpr	%%g0, 0x414, %%pstate	! save mmu globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x158]\n"
"	stx	%%g2, [%%sp + 2047 + 0x160]\n"
"	stx	%%g3, [%%sp + 2047 + 0x168]\n"
"	stx	%%g4, [%%sp + 2047 + 0x170]\n"
"	stx	%%g5, [%%sp + 2047 + 0x178]\n"
"	stx	%%g6, [%%sp + 2047 + 0x180]\n"
"	stx	%%g7, [%%sp + 2047 + 0x188]\n"
"	mov	%%g1, %%l0		! also save to locals, so we can handle\n"
"	mov	%%g2, %%l1		! tlb faults later on, when accessing\n"
"	mov	%%g3, %%l3		! the stack.\n"
"	mov	%%g7, %%l5\n"
"	wrpr	%%l4, %1, %%pstate	! turn off interrupts\n"
"	call	%%l2\n"
"	 add	%%i0, 0x018, %%o0	! prom_args\n"
"	wrpr	%%g0, 0x414, %%pstate	! restore mmu globals\n"
"	mov	%%l0, %%g1\n"
"	mov	%%l1, %%g2\n"
"	mov	%%l3, %%g3\n"
"	mov	%%l5, %%g7\n"
"	wrpr	%%g0, 0x14, %%pstate	! restore normal globals\n"
"	ldx	[%%sp + 2047 + 0x120], %%g1\n"
"	ldx	[%%sp + 2047 + 0x128], %%g2\n"
"	ldx	[%%sp + 2047 + 0x130], %%g3\n"
"	ldx	[%%sp + 2047 + 0x138], %%g4\n"
"	ldx	[%%sp + 2047 + 0x140], %%g5\n"
"	ldx	[%%sp + 2047 + 0x148], %%g6\n"
"	ldx	[%%sp + 2047 + 0x150], %%g7\n"
"	wrpr	%%g0, 0x814, %%pstate	! restore interrupt globals\n"
"	ldx	[%%sp + 2047 + 0x0e8], %%g1\n"
"	ldx	[%%sp + 2047 + 0x0f0], %%g2\n"
"	ldx	[%%sp + 2047 + 0x0f8], %%g3\n"
"	ldx	[%%sp + 2047 + 0x100], %%g4\n"
"	ldx	[%%sp + 2047 + 0x108], %%g5\n"
"	ldx	[%%sp + 2047 + 0x110], %%g6\n"
"	ldx	[%%sp + 2047 + 0x118], %%g7\n"
"	wrpr	%%g0, 0x15, %%pstate	! restore alternate globals\n"
"	ldx	[%%sp + 2047 + 0x0b0], %%g1\n"
"	ldx	[%%sp + 2047 + 0x0b8], %%g2\n"
"	ldx	[%%sp + 2047 + 0x0c0], %%g3\n"
"	ldx	[%%sp + 2047 + 0x0c8], %%g4\n"
"	ldx	[%%sp + 2047 + 0x0d0], %%g5\n"
"	ldx	[%%sp + 2047 + 0x0d8], %%g6\n"
"	ldx	[%%sp + 2047 + 0x0e0], %%g7\n"
"	wrpr	%%l4, 0, %%pstate	! restore original pstate\n"
"	ret\n"
"	 restore\n"
"	" : : "r" (&p1275buf), "i" (PSTATE_IE));
}

void prom_cif_callback(void)
{
	__asm__ __volatile__ (
"	mov	%0, %%o1\n"
"	save	%%sp, -0x270, %%sp\n"
"	rdpr	%%pstate, %%l4\n"
"	wrpr	%%g0, 0x15, %%pstate	! save PROM alternate globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x0b0]\n"
"	stx	%%g2, [%%sp + 2047 + 0x0b8]\n"
"	stx	%%g3, [%%sp + 2047 + 0x0c0]\n"
"	stx	%%g4, [%%sp + 2047 + 0x0c8]\n"
"	stx	%%g5, [%%sp + 2047 + 0x0d0]\n"
"	stx	%%g6, [%%sp + 2047 + 0x0d8]\n"
"	stx	%%g7, [%%sp + 2047 + 0x0e0]\n"
"					! restore Linux alternate globals\n"
"	ldx	[%%sp + 2047 + 0x190], %%g1\n"
"	ldx	[%%sp + 2047 + 0x198], %%g2\n"
"	ldx	[%%sp + 2047 + 0x1a0], %%g3\n"
"	ldx	[%%sp + 2047 + 0x1a8], %%g4\n"
"	ldx	[%%sp + 2047 + 0x1b0], %%g5\n"
"	ldx	[%%sp + 2047 + 0x1b8], %%g6\n"
"	ldx	[%%sp + 2047 + 0x1c0], %%g7\n"
"	wrpr	%%g0, 0x814, %%pstate	! save PROM interrupt globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x0e8]\n"
"	stx	%%g2, [%%sp + 2047 + 0x0f0]\n"
"	stx	%%g3, [%%sp + 2047 + 0x0f8]\n"
"	stx	%%g4, [%%sp + 2047 + 0x100]\n"
"	stx	%%g5, [%%sp + 2047 + 0x108]\n"
"	stx	%%g6, [%%sp + 2047 + 0x110]\n"
"	stx	%%g7, [%%sp + 2047 + 0x118]\n"
"					! restore Linux interrupt globals\n"
"	ldx	[%%sp + 2047 + 0x1c8], %%g1\n"
"	ldx	[%%sp + 2047 + 0x1d0], %%g2\n"
"	ldx	[%%sp + 2047 + 0x1d8], %%g3\n"
"	ldx	[%%sp + 2047 + 0x1e0], %%g4\n"
"	ldx	[%%sp + 2047 + 0x1e8], %%g5\n"
"	ldx	[%%sp + 2047 + 0x1f0], %%g6\n"
"	ldx	[%%sp + 2047 + 0x1f8], %%g7\n"
"	wrpr	%%g0, 0x14, %%pstate	! save PROM normal globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x120]\n"
"	stx	%%g2, [%%sp + 2047 + 0x128]\n"
"	stx	%%g3, [%%sp + 2047 + 0x130]\n"
"	stx	%%g4, [%%sp + 2047 + 0x138]\n"
"	stx	%%g5, [%%sp + 2047 + 0x140]\n"
"	stx	%%g6, [%%sp + 2047 + 0x148]\n"
"	stx	%%g7, [%%sp + 2047 + 0x150]\n"
"					! restore Linux normal globals\n"
"	ldx	[%%sp + 2047 + 0x200], %%g1\n"
"	ldx	[%%sp + 2047 + 0x208], %%g2\n"
"	ldx	[%%sp + 2047 + 0x210], %%g3\n"
"	ldx	[%%sp + 2047 + 0x218], %%g4\n"
"	ldx	[%%sp + 2047 + 0x220], %%g5\n"
"	ldx	[%%sp + 2047 + 0x228], %%g6\n"
"	ldx	[%%sp + 2047 + 0x230], %%g7\n"
"	wrpr	%%g0, 0x414, %%pstate	! save PROM mmu globals\n"
"	stx	%%g1, [%%sp + 2047 + 0x158]\n"
"	stx	%%g2, [%%sp + 2047 + 0x160]\n"
"	stx	%%g3, [%%sp + 2047 + 0x168]\n"
"	stx	%%g4, [%%sp + 2047 + 0x170]\n"
"	stx	%%g5, [%%sp + 2047 + 0x178]\n"
"	stx	%%g6, [%%sp + 2047 + 0x180]\n"
"	stx	%%g7, [%%sp + 2047 + 0x188]\n"
"					! restore Linux mmu globals\n"
"	ldx	[%%sp + 2047 + 0x238], %%o0\n"
"	ldx	[%%sp + 2047 + 0x240], %%o1\n"
"	ldx	[%%sp + 2047 + 0x248], %%l2\n"
"	ldx	[%%sp + 2047 + 0x250], %%l3\n"
"	ldx	[%%sp + 2047 + 0x258], %%l5\n"
"	ldx	[%%sp + 2047 + 0x260], %%l6\n"
"	ldx	[%%sp + 2047 + 0x268], %%l7\n"
"					! switch to Linux tba\n"
"	sethi	%%hi(sparc64_ttable_tl0), %%l1\n"
"	rdpr	%%tba, %%l0		! save PROM tba\n"
"	mov	%%o0, %%g1\n"
"	mov	%%o1, %%g2\n"
"	mov	%%l2, %%g3\n"
"	mov	%%l3, %%g4\n"
"	mov	%%l5, %%g5\n"
"	mov	%%l6, %%g6\n"
"	mov	%%l7, %%g7\n"
"	wrpr	%%l1, %%tba		! install Linux tba\n"
"	wrpr	%%l4, 0, %%pstate	! restore PSTATE\n"
"	call	prom_world\n"
"	 mov	%%g0, %%o0\n"
"	ldx	[%%i1 + 0x000], %%l2\n"
"	call	%%l2\n"
"	 mov	%%i0, %%o0\n"
"	mov	%%o0, %%l1\n"
"	call	prom_world\n"
"	 or	%%g0, 1, %%o0\n"
"	wrpr	%%g0, 0x14, %%pstate	! interrupts off\n"
"					! restore PROM mmu globals\n"
"	ldx	[%%sp + 2047 + 0x158], %%o0\n"
"	ldx	[%%sp + 2047 + 0x160], %%o1\n"
"	ldx	[%%sp + 2047 + 0x168], %%l2\n"
"	ldx	[%%sp + 2047 + 0x170], %%l3\n"
"	ldx	[%%sp + 2047 + 0x178], %%l5\n"
"	ldx	[%%sp + 2047 + 0x180], %%l6\n"
"	ldx	[%%sp + 2047 + 0x188], %%l7\n"
"	wrpr	%%g0, 0x414, %%pstate	! restore PROM mmu globals\n"
"	mov	%%o0, %%g1\n"
"	mov	%%o1, %%g2\n"
"	mov	%%l2, %%g3\n"
"	mov	%%l3, %%g4\n"
"	mov	%%l5, %%g5\n"
"	mov	%%l6, %%g6\n"
"	mov	%%l7, %%g7\n"
"	wrpr	%%l0, %%tba		! restore PROM tba\n"
"	wrpr	%%g0, 0x14, %%pstate	! restore PROM normal globals\n"
"	ldx	[%%sp + 2047 + 0x120], %%g1\n"
"	ldx	[%%sp + 2047 + 0x128], %%g2\n"
"	ldx	[%%sp + 2047 + 0x130], %%g3\n"
"	ldx	[%%sp + 2047 + 0x138], %%g4\n"
"	ldx	[%%sp + 2047 + 0x140], %%g5\n"
"	ldx	[%%sp + 2047 + 0x148], %%g6\n"
"	ldx	[%%sp + 2047 + 0x150], %%g7\n"
"	wrpr	%%g0, 0x814, %%pstate	! restore PROM interrupt globals\n"
"	ldx	[%%sp + 2047 + 0x0e8], %%g1\n"
"	ldx	[%%sp + 2047 + 0x0f0], %%g2\n"
"	ldx	[%%sp + 2047 + 0x0f8], %%g3\n"
"	ldx	[%%sp + 2047 + 0x100], %%g4\n"
"	ldx	[%%sp + 2047 + 0x108], %%g5\n"
"	ldx	[%%sp + 2047 + 0x110], %%g6\n"
"	ldx	[%%sp + 2047 + 0x118], %%g7\n"
"	wrpr	%%g0, 0x15, %%pstate	! restore PROM alternate globals\n"
"	ldx	[%%sp + 2047 + 0x0b0], %%g1\n"
"	ldx	[%%sp + 2047 + 0x0b8], %%g2\n"
"	ldx	[%%sp + 2047 + 0x0c0], %%g3\n"
"	ldx	[%%sp + 2047 + 0x0c8], %%g4\n"
"	ldx	[%%sp + 2047 + 0x0d0], %%g5\n"
"	ldx	[%%sp + 2047 + 0x0d8], %%g6\n"
"	ldx	[%%sp + 2047 + 0x0e0], %%g7\n"
"	wrpr	%%l4, 0, %%pstate\n"
"	ret\n"
"	 restore %%l1, 0, %%o0\n"
"	" : : "r" (&p1275buf), "i" (PSTATE_PRIV));
}

/*
 * This provides SMP safety on the p1275buf. prom_callback() drops this lock
 * to allow recursuve acquisition.
 */
spinlock_t prom_entry_lock = SPIN_LOCK_UNLOCKED;

long p1275_cmd (char *service, long fmt, ...)
{
	char *p, *q;
	unsigned long flags;
	int nargs, nrets, i;
	va_list list;
	long attrs, x;
	long ctx = 0;
	
	p = p1275buf.prom_buffer;
	ctx = spitfire_get_primary_context ();
	if (ctx) {
		flushw_user ();
		spitfire_set_primary_context (0);
	}

	spin_lock_irqsave(&prom_entry_lock, flags);

	p1275buf.prom_args[0] = (unsigned long)p;		/* service */
	strcpy (p, service);
	p = (char *)(((long)(strchr (p, 0) + 8)) & ~7);
	p1275buf.prom_args[1] = nargs = (fmt & 0x0f);		/* nargs */
	p1275buf.prom_args[2] = nrets = ((fmt & 0xf0) >> 4); 	/* nrets */
	attrs = fmt >> 8;
	va_start(list, fmt);
	for (i = 0; i < nargs; i++, attrs >>= 3) {
		switch (attrs & 0x7) {
		case P1275_ARG_NUMBER:
			p1275buf.prom_args[i + 3] =
						(unsigned)va_arg(list, long);
			break;
		case P1275_ARG_IN_64B:
			p1275buf.prom_args[i + 3] =
				va_arg(list, unsigned long);
			break;
		case P1275_ARG_IN_STRING:
			strcpy (p, va_arg(list, char *));
			p1275buf.prom_args[i + 3] = (unsigned long)p;
			p = (char *)(((long)(strchr (p, 0) + 8)) & ~7);
			break;
		case P1275_ARG_OUT_BUF:
			(void) va_arg(list, char *);
			p1275buf.prom_args[i + 3] = (unsigned long)p;
			x = va_arg(list, long);
			i++; attrs >>= 3;
			p = (char *)(((long)(p + (int)x + 7)) & ~7);
			p1275buf.prom_args[i + 3] = x;
			break;
		case P1275_ARG_IN_BUF:
			q = va_arg(list, char *);
			p1275buf.prom_args[i + 3] = (unsigned long)p;
			x = va_arg(list, long);
			i++; attrs >>= 3;
			memcpy (p, q, (int)x);
			p = (char *)(((long)(p + (int)x + 7)) & ~7);
			p1275buf.prom_args[i + 3] = x;
			break;
		case P1275_ARG_OUT_32B:
			(void) va_arg(list, char *);
			p1275buf.prom_args[i + 3] = (unsigned long)p;
			p += 32;
			break;
		case P1275_ARG_IN_FUNCTION:
			p1275buf.prom_args[i + 3] =
					(unsigned long)prom_cif_callback;
			p1275buf.prom_callback = va_arg(list, long);
			break;
		}
	}
	va_end(list);

	prom_world(1);
	prom_cif_interface();
	prom_world(0);

	attrs = fmt >> 8;
	va_start(list, fmt);
	for (i = 0; i < nargs; i++, attrs >>= 3) {
		switch (attrs & 0x7) {
		case P1275_ARG_NUMBER:
			(void) va_arg(list, long);
			break;
		case P1275_ARG_IN_STRING:
			(void) va_arg(list, char *);
			break;
		case P1275_ARG_IN_FUNCTION:
			(void) va_arg(list, long);
			break;
		case P1275_ARG_IN_BUF:
			(void) va_arg(list, char *);
			(void) va_arg(list, long);
			i++; attrs >>= 3;
			break;
		case P1275_ARG_OUT_BUF:
			p = va_arg(list, char *);
			x = va_arg(list, long);
			memcpy (p, (char *)(p1275buf.prom_args[i + 3]), (int)x);
			i++; attrs >>= 3;
			break;
		case P1275_ARG_OUT_32B:
			p = va_arg(list, char *);
			memcpy (p, (char *)(p1275buf.prom_args[i + 3]), 32);
			break;
		}
	}
	va_end(list);
	x = p1275buf.prom_args [nargs + 3];

	spin_unlock_irqrestore(&prom_entry_lock, flags);

	if (ctx)
		spitfire_set_primary_context (ctx);

	return x;
}

void prom_cif_init(void *cif_handler, void *cif_stack)
{
	p1275buf.prom_cif_handler = (void (*)(long *))cif_handler;
	p1275buf.prom_cif_stack = (unsigned long)cif_stack;
}
