/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)nlist.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>

#include <errno.h>
#include <a.out.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int
nlist(name, list)
	const char *name;
	struct nlist *list;
{
	int fd, n;

	fd = open(name, O_RDONLY, 0);
	if (fd < 0)
		return (-1);
	n = __fdnlist(fd, list);
	(void)close(fd);
	return (n);
}

#define	ISLAST(p)	(p->n_un.n_name == 0 || p->n_un.n_name[0] == 0)

int
__fdnlist(fd, list)
	register int fd;
	register struct nlist *list;
{
	register struct nlist *p, *s;
	register caddr_t strtab;
	register off_t stroff, symoff;
	register u_long symsize;
	register int nent, cc;
	size_t strsize;
	struct nlist nbuf[1024];
	struct exec exec;
	struct stat st;

	if (lseek(fd, (off_t)0, SEEK_SET) == -1 ||
	    read(fd, &exec, sizeof(exec)) != sizeof(exec) ||
	    N_BADMAG(exec) || fstat(fd, &st) < 0)
		return (-1);

	symoff = N_SYMOFF(exec);
	symsize = exec.a_syms;
	stroff = symoff + symsize;

	/* Check for files too large to mmap. */
	if (st.st_size - stroff > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}
	/*
	 * Map string table into our address space.  This gives us
	 * an easy way to randomly access all the strings, without
	 * making the memory allocation permanent as with malloc/free
	 * (i.e., munmap will return it to the system).
	 */
	strsize = st.st_size - stroff;
	strtab = mmap(NULL, (size_t)strsize, PROT_READ, 0, fd, stroff);
	if (strtab == (char *)-1)
		return (-1);
	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 *
	 * XXX clearing anything other than n_type and n_value violates
	 * the semantics given in the man page.
	 */
	nent = 0;
	for (p = list; !ISLAST(p); ++p) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		++nent;
	}
	if (lseek(fd, symoff, SEEK_SET) == -1)
		return (-1);

	while (symsize > 0) {
		cc = MIN(symsize, sizeof(nbuf));
		if (read(fd, nbuf, cc) != cc)
			break;
		symsize -= cc;
		for (s = nbuf; cc > 0; ++s, cc -= sizeof(*s)) {
			register int soff = s->n_un.n_strx;

			if (soff == 0 || (s->n_type & N_STAB) != 0)
				continue;
			for (p = list; !ISLAST(p); p++)
				if (!strcmp(&strtab[soff], p->n_un.n_name)) {
					p->n_value = s->n_value;
					p->n_type = s->n_type;
					p->n_desc = s->n_desc;
					p->n_other = s->n_other;
					if (--nent <= 0)
						break;
				}
		}
	}
	munmap(strtab, strsize);
	return (nent);
}
