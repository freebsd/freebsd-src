/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)nlist.c	5.8 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/file.h>
#include <a.out.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name
#define	ISVALID(p)	(p->_name && p->_name[0])

int
nlist(name, list)
	const char *name;
	NLIST *list;
{
	register NLIST *p, *s;
	struct exec ebuf;
	FILE *fstr, *fsym;
	NLIST nbuf;
	off_t strings_offset, symbol_offset, symbol_size, lseek();
	int entries, len, maxlen;
	char sbuf[256];

	entries = -1;

	if (!(fsym = fopen(name, "r")))
		return(-1);
	if (fread((char *)&ebuf, sizeof(struct exec), 1, fsym) != 1 ||
	    N_BADMAG(ebuf))
		goto done1;

	symbol_offset = N_SYMOFF(ebuf);
	symbol_size = ebuf.a_syms;
	strings_offset = symbol_offset + symbol_size;
	if (fseek(fsym, symbol_offset, SEEK_SET))
		goto done1;

	if (!(fstr = fopen(name, "r")))
		goto done1;

	/*
	 * clean out any left-over information for all valid entries.
	 * Type and value defined to be 0 if not found; historical
	 * versions cleared other and desc as well.  Also figure out
	 * the largest string length so don't read any more of the
	 * string table than we have to.
	 */
	for (p = list, entries = maxlen = 0; ISVALID(p); ++p, ++entries) {
		p->n_type = 0;
		p->n_other = 0;
		p->n_desc = 0;
		p->n_value = 0;
		if ((len = strlen(p->_name)) > maxlen)
			maxlen = len;
	}
	if (++maxlen > sizeof(sbuf)) {		/* for the NULL */
		(void)fprintf(stderr, "nlist: symbol too large.\n");
		entries = -1;
		goto done2;
	}

	for (s = &nbuf; symbol_size; symbol_size -= sizeof(NLIST)) {
		if (fread((char *)s, sizeof(NLIST), 1, fsym) != 1)
			goto done2;
		if (!s->_strx || s->n_type&N_STAB)
			continue;
		if (fseek(fstr, strings_offset + s->_strx, SEEK_SET))
			goto done2;
		(void)fread(sbuf, sizeof(sbuf[0]), maxlen, fstr);
		for (p = list; ISVALID(p); p++)
			if (!strcmp(p->_name, sbuf)) {
				p->n_value = s->n_value;
				p->n_type = s->n_type;
				p->n_desc = s->n_desc;
				p->n_other = s->n_other;
				if (!--entries)
					goto done2;
			}
	}
done2:	(void)fclose(fstr);
done1:	(void)fclose(fsym);
	return(entries);
}
