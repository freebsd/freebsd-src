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
	register struct nlist *p, *symtab;
	register caddr_t strtab, a_out_mmap;
	register off_t stroff, symoff;
	register u_long symsize;
	register int nent;
	struct exec * exec;
	struct stat st;

	/* check that file is at least as large as struct exec! */
	if ((fstat(fd, &st) < 0) || (st.st_size < sizeof(struct exec)))
		return (-1);

	/* Check for files too large to mmap. */
	if (st.st_size > SIZE_T_MAX) {
		errno = EFBIG;
		return (-1);
	}

	/*
	 * Map the whole a.out file into our address space.
	 * We then find the string table withing this area.
	 * We do not just mmap the string table, as it probably
	 * does not start at a page boundary - we save ourselves a
	 * lot of nastiness by mmapping the whole file.
	 *
	 * This gives us an easy way to randomly access all the strings,
	 * without making the memory allocation permanent as with
	 * malloc/free (i.e., munmap will return it to the system).
	 */
	a_out_mmap = mmap(NULL, (size_t)st.st_size, PROT_READ, 0, fd, (off_t)0);
	if (a_out_mmap == (char *)-1)
		return (-1);

	exec = (struct exec *)a_out_mmap;
	if (N_BADMAG(*exec)) {
		munmap(a_out_mmap, (size_t)st.st_size);
		return (-1);
	}

	symoff = N_SYMOFF(*exec);
	symsize = exec->a_syms;
	stroff = symoff + symsize;

	/* find the string table in our mmapped area */
	strtab = a_out_mmap + stroff;
	symtab = (struct nlist *)(a_out_mmap + symoff);

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

	while (symsize > 0) {
		register int soff;

		symsize-= sizeof(struct nlist);
		soff = symtab->n_un.n_strx;


		if (soff != 0 && (symtab->n_type & N_STAB) == 0)
			for (p = list; !ISLAST(p); p++)
				if (!strcmp(&strtab[soff], p->n_un.n_name)) {
					p->n_value = symtab->n_value;
					p->n_type = symtab->n_type;
					p->n_desc = symtab->n_desc;
					p->n_other = symtab->n_other;
					if (--nent <= 0)
						break;
				}
		symtab++;
	}
	munmap(a_out_mmap, (size_t)st.st_size);
	return (nent);
}
