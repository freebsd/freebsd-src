/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: sbrk.c,v 1.1 1993/12/11 21:06:36 jkh Exp $
 */

#include <machine/vmparam.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#ifndef BSD
#define MAP_COPY	MAP_PRIVATE
#define MAP_FILE	0
#define MAP_ANON	0
#endif
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <machine/param.h>

#ifndef BSD		/* Need do better than this */
#define NEED_DEV_ZERO	1
#endif

caddr_t
sbrk(incr)
int incr;
{
	int	fd = -1;
	caddr_t curbrk;

	/* Round-up increment to page size */
	incr = ((incr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

#if DEBUG
xprintf("sbrk: incr = %#x\n", incr);
#endif

#ifdef NEED_DEV_ZERO
	fd = open("/dev/zero", O_RDWR, 0);
	if (fd == -1)
		perror("/dev/zero");
#endif

	if ((curbrk = mmap(0, incr,
			PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_COPY, fd, 0)) == (caddr_t)-1) {
		xprintf("Cannot map anonymous memory");
		_exit(1);
	}

#ifdef DEBUG
xprintf("sbrk: curbrk = %#x\n", curbrk);
#endif

#ifdef NEED_DEV_ZERO
	close(fd);
#endif

	return(curbrk);
}

