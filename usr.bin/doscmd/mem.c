/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI mem.c,v 2.2 1996/04/08 19:32:57 bostic Exp
 *
 * $FreeBSD: src/usr.bin/doscmd/mem.c,v 1.2 1999/08/28 01:00:19 peter Exp $
 */

#include <stdio.h>
#include "doscmd.h"

#define	Mark(x)	(*(char *) (x))
#define Owner(x) (*(u_short *) ((char *)(x)+1))
#define Size(x) (*(u_short *) ((char *)(x)+3))
#define Next(x) ((char *)(x) + (Size(x)+1)*16)

/* exports */
char	*dosmem;

/* locals */
static int	dosmem_size;

static char	*next_p = (char *)0;
static char	*end_p  = (char *)0xB0000L;

static char *
core_alloc(int *size)
{
    char *ret;
    if (*size) {
	if (*size & 0xfff) {
	    *size = (*size & ~0xfff) + 0x1000;
	}
    } else {
	*size = end_p - next_p;
    }
    
    if (next_p + *size > end_p) {
	return NULL;
    }

    ret = next_p;
    next_p += *size;
    return ret;
}	

void
mem_free_owner(int owner)
{
    char *mp;

    debug(D_MEMORY, "    : freeow(%04x)\n", owner);
    
    for (mp = dosmem; ; mp = Next(mp)) {
	if (Owner(mp) == owner)
	    Owner(mp) = 0;
	
	if (Mark(mp) != 'M')
	    break;
    }
}

static void
mem_print(void)
{
    char *mp;

    for (mp = dosmem; ; mp = Next(mp)) {
	debug(D_ALWAYS, "%05x: mark %c owner %04x size %04x\n",
	      mp, Mark(mp), Owner(mp), Size(mp));

	if (Mark(mp) != 'M')
	    break;
    }
}

void	
mem_change_owner(int addr, int owner)
{
    char *mp;

    debug(D_MEMORY, "%04x: owner (%04x)\n", addr, owner);
    addr <<= 4;

    for (mp = dosmem; ; mp = Next(mp)) {
	if ((int)(mp + 16) == addr)
	    goto found;

	if (Mark(mp) != 'M')
	    break;
    }

    debug(D_ALWAYS, "%05x: illegal block in change owner\n", addr);
    mem_print();
    return;

found:
    Owner(mp) = owner;
}

void
mem_init(void)
{
    int base, avail_memory;

    base = 0x600;
    core_alloc(&base);

    avail_memory = MAX_AVAIL_SEG * 16 - base;
    dosmem = core_alloc(&avail_memory);

    if (!dosmem || dosmem != (char *)base)
	fatal("internal memory error\n");

    dosmem_size = avail_memory / 16;

    debug(D_MEMORY, "dosmem = 0x%x base = 0x%x avail = 0x%x (%dK)\n",
	  dosmem, base, dosmem_size, avail_memory / 1024);

    Mark(dosmem) = 'Z';
    Owner(dosmem) = 0;
    Size(dosmem) = dosmem_size - 1;
}

static void
mem_unsplit(char *mp, int size)
{
    char *nmp;

    while (Mark(mp) == 'M' && Size(mp) < size) {
	nmp = Next(mp);

	if (Owner(nmp) != 0)
	    break;

	Size(mp) += Size(nmp) + 1;
	Mark(mp) = Mark(nmp);
    }
}

static void
mem_split(char *mp, int size)
{
    char *nmp;
    int rest;

    rest = Size(mp) - size;
    Size(mp) = size;
    nmp = Next(mp);
    Mark(nmp) = Mark(mp);
    Mark(mp) = 'M';
    Owner(nmp) = 0;
    Size(nmp) = rest - 1;
}

int
mem_alloc(int size, int owner, int *biggestp)
{
    char *mp;
    int biggest;

    biggest = 0;
    for (mp = dosmem; ; mp = Next(mp)) {
	if (Owner(mp) == 0) {
	    if (Size(mp) < size)
		mem_unsplit(mp, size);
	    if (Size(mp) >= size)
		goto got;

	    if (Size(mp) > biggest)
		biggest = Size(mp);
	}

	if (Mark(mp) != 'M')
	    break;
    }

    debug(D_MEMORY, "%04x: alloc(%04x, owner %04x) failed -> %d\n",
	  0, size, owner, biggest);

    if (biggestp)
	*biggestp = biggest;
    return 0;

got:
    if (Size(mp) > size)
	mem_split(mp, size);
    Owner(mp) = owner;
    debug(D_MEMORY, "%04x: alloc(%04x, owner %04x)\n",
	  (int)mp/16 + 1, size, owner);

    if (biggestp)
	*biggestp = size;
    return (int)mp/16 + 1;
}

int
mem_adjust(int addr, int size, int *availp)
{
    char *mp;
    int delta, nxtsiz;

    debug(D_MEMORY, "%04x: adjust(%05x)\n", addr, size);
    addr <<= 4;

    for (mp = dosmem; ; mp = Next(mp)) {
	if ((int)(mp + 16) == addr)
	    goto found;

	if (Mark(mp) != 'M')
	    break;
    }

    debug(D_ALWAYS, "%05x: illegal block in adjust\n", addr);
    mem_print();
    return -2;

found:
    if (Size(mp) < size)
	mem_unsplit(mp, size);
    if (Size(mp) >= size)
	goto got;

    debug(D_MEMORY, "%04x: adjust(%04x) failed -> %d\n",
	  (int)mp/16 + 1, size, Size(mp));

    if (availp)
	*availp = Size(mp);
    return -1;

got:
    if (Size(mp) > size)
	mem_split(mp, size);
    debug(D_MEMORY, "%04x: adjust(%04x)\n",
	  (int)mp/16 + 1, size);

    if (availp)
	*availp = size;
    return 0;
}

		

#ifdef	MEM_TEST
mem_check ()
{
    struct mem_block *mp;
    for (mp = mem_blocks.next; mp != &mem_blocks; mp = mp->next) {
	if (mp->addr + mp->size != mp->next->addr)
	    break;
	if (mp->inuse && mp->size == 0)
	    return (-1);
    }

    if (mp->next != &mem_blocks)
	return (-1);
    return (0);
}	

char *blocks[10];

main ()
{
    int i;
    int n;
    int newsize;

    mem_init (0, 300);

    for (i = 0; i < 100000; i++) {
	n = random () % 10;

	if (blocks[n]) {
	    newsize = random () % 20;
	    if ((newsize & 1) == 0)
		newsize = 0;
			
	    if (0)
		printf ("adjust %d %x %d\n",
			n, blocks[n], newsize);
	    mem_adjust (blocks[n], newsize, NULL);
	    if (newsize == 0)
		blocks[n] = NULL;
	} else {
	    while ((newsize = random () % 20) == 0)
		;
	    if (0)
		printf ("alloc %d %d\n", n, newsize);
	    blocks[n] = mem_alloc (newsize, NULL);
	}
	if (mem_check () < 0) {
	    printf ("==== %d\n", i);
	    mem_print ();
	}
    }

    mem_print ();
}
#endif /* MEM_TEST */
