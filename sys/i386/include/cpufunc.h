/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: cpufunc.h,v 1.15 1994/08/11 02:26:08 jkh Exp $
 */

/*
 * Functions to provide access to special i386 instructions.
 * XXX - bezillions more are defined in locore.s but are not declared anywhere.
 */

#ifndef _MACHINE_CPUFUNC_H_
#define _MACHINE_CPUFUNC_H_ 1

#include <sys/cdefs.h>
#include <sys/types.h>

#include <machine/spl.h>

#ifdef	__GNUC__

static inline int bdb(void)
{
	extern int bdb_exists;

	if (!bdb_exists)
		return (0);
	__asm("int $3");
	return (1);
}

static inline void
disable_intr(void)
{
	__asm __volatile("cli");
}

static inline void
enable_intr(void)
{
	__asm __volatile("sti");
}

/*
 * This roundabout method of returning a u_char helps stop gcc-1.40 from
 * generating unnecessary movzbl's.
 */
#define	inb(port)	((u_char) u_int_inb(port))

static inline u_int
u_int_inb(u_int port)
{
	u_char	data;
	/*
	 * We use %%dx and not %1 here because i/o is done at %dx and not at
	 * %edx, while gcc-2.2.2 generates inferior code (movw instead of movl)
	 * if we tell it to load (u_short) port.
	 */
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return data;
}

static inline void
outb(u_int port, u_char data)
{
	u_char	al;

	al = data;		/* help gcc-1.40's register allocator */
	__asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

static inline void
tlbflush()
{
	__asm __volatile("movl %%cr3, %%eax; movl %%eax, %%cr3" : : : "ax");
}

static inline u_long
rcr2()
{
	u_long	data;
	__asm __volatile("movl %%cr2,%%eax" : "=a" (data));
	return data;
}

struct quehead {
	struct quehead *qh_link;
	struct quehead *qh_rlink;
};

static inline void
insque(void *a, void *b)
{
	register struct quehead *element = a, *head = b;
	element->qh_link = head->qh_link;
	head->qh_link = (struct quehead *)element;
	element->qh_rlink = (struct quehead *)head;
	((struct quehead *)(element->qh_link))->qh_rlink
	  = (struct quehead *)element;
}

static inline void
remque(void *a)
{
	register struct quehead *element = a;
	((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
	((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
	element->qh_rlink = 0;
}

#else /* not __GNUC__ */
extern	void insque __P((void *, void *));
extern	void remque __P((void *));

int	bdb		__P((void));
void	disable_intr	__P((void));
void	enable_intr	__P((void));
u_char	inb		__P((u_int port));
void	outb		__P((u_int port, u_int data));	/* XXX - incompat */

#endif	/* __GNUC__ */

void	load_cr0	__P((u_int cr0));
u_int	rcr0	__P((void));
void load_cr3(u_long);
u_long rcr3(void);

void	setidt	__P((int, void (*)(), int, int));
extern u_long kvtop(void *);
extern void outw(int /*u_short*/, int /*u_short*/); /* XXX inline!*/
extern void outsb(int /*u_short*/, void *, size_t);
extern void outsw(int /*u_short*/, void *, size_t);
extern void insw(int /*u_short*/, void *, size_t);
extern void fillw(int /*u_short*/, void *, size_t);
extern void filli(int, void *, size_t);

#endif /* _MACHINE_CPUFUNC_H_ */
