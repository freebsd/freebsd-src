/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)build.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <a.out.h>
#include <ar.h>
#include <dirent.h>
#include <fcntl.h>
#include <ranlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"

extern int tmp __P(( void ));
extern void error __P(( char * ));
extern void badfmt __P(( void ));
extern void settime __P(( int ));

extern CHDR chdr;			/* converted header */
extern char *archive;			/* archive name */
extern char *tname;			/* temporary file "name" */

typedef struct _rlib {
	struct _rlib *next;		/* next structure */
	off_t pos;			/* offset of defining archive file */
	char *sym;			/* symbol */
	int symlen;			/* strlen(sym) */
} RLIB;
RLIB *rhead, **pnext;

FILE *fp;

long symcnt;				/* symbol count */
long tsymlen;				/* total string length */

static void rexec __P((int, int));
static void symobj __P((void));

int
build(void)
{
	CF cf;
	int afd, tfd;
	off_t size;

	afd = open_archive(O_RDWR);
	fp = fdopen(afd, "r+");
	tfd = tmp();

	SETCF(afd, archive, tfd, tname, RPAD|WPAD);

	/* Read through the archive, creating list of symbols. */
	pnext = &rhead;
	symcnt = tsymlen = 0;
	while(get_arobj(afd)) {
		if (!strcmp(chdr.name, RANLIBMAG)) {
			skip_arobj(afd);
			continue;
		}
		rexec(afd, tfd);
		put_arobj(&cf, (struct stat *)NULL);
	}
	*pnext = NULL;

	/* Create the symbol table. */
	symobj();

	/* Copy the saved objects into the archive. */
	size = lseek(tfd, (off_t)0, SEEK_CUR);
	(void)lseek(tfd, (off_t)0, SEEK_SET);
	SETCF(tfd, tname, afd, archive, WPAD);
	copy_ar(&cf, size);
	(void)ftruncate(afd, lseek(afd, (off_t)0, SEEK_CUR));
	(void)close(tfd);

	/* Set the time. */
	settime(afd);
	close_archive(afd);
	return(0);
}

/*
 * rexec
 *	Read the exec structure; ignore any files that don't look
 *	exactly right.
 */
static void
rexec(rfd, wfd)
	register int rfd;
	int wfd;
{
	register RLIB *rp;
	register long nsyms;
	register int nr, symlen;
	register char *strtab = 0, *sym;
	struct exec ebuf;
	struct nlist nl;
	off_t r_off, w_off;
	long strsize;
	void *emalloc();

	/* Get current offsets for original and tmp files. */
	r_off = lseek(rfd, (off_t)0, SEEK_CUR);
	w_off = lseek(wfd, (off_t)0, SEEK_CUR);

	/* Read in exec structure. */
	nr = read(rfd, &ebuf, sizeof(struct exec));
	if (nr != sizeof(struct exec))
		goto badread;

	/* Check magic number and symbol count. */
	if (N_BADMAG(ebuf) || ebuf.a_syms == 0)
		goto bad1;

	/* Seek to string table. */
	if (lseek(rfd, r_off + N_STROFF(ebuf), SEEK_SET) == (off_t)-1)
		error(archive);

	/* Read in size of the string table. */
	nr = read(rfd, &strsize, sizeof(strsize));
	if (nr != sizeof(strsize))
		goto badread;

	/* Read in the string table. */
	strsize -= sizeof(strsize);
	strtab = emalloc(strsize);
	nr = read(rfd, strtab, strsize);
	if (nr != strsize) {
badread:	if (nr < 0)
			error(archive);
		goto bad2;
	}

	/* Seek to symbol table. */
	if (fseek(fp, (long)r_off + N_SYMOFF(ebuf), SEEK_SET))
		goto bad2;

	/* For each symbol read the nlist entry and save it as necessary. */
	nsyms = ebuf.a_syms / sizeof(struct nlist);
	while (nsyms--) {
		if (!fread(&nl, sizeof(struct nlist), 1, fp)) {
			if (feof(fp))
				badfmt();
			error(archive);
		}

		/* Ignore if no name or local. */
		if (!nl.n_un.n_strx || !(nl.n_type & N_EXT))
			continue;

		/*
		 * If the symbol is an undefined external and the n_value
		 * field is non-zero, keep it.
		 */
		if ((nl.n_type & N_TYPE) == N_UNDF && !nl.n_value)
			continue;

		/* First four bytes are the table size. */
		sym = strtab + nl.n_un.n_strx - sizeof(long);
		symlen = strlen(sym) + 1;

		rp = (RLIB *)emalloc(sizeof(RLIB));
		rp->sym = (char *)emalloc(symlen);
		bcopy(sym, rp->sym, symlen);
		rp->symlen = symlen;
		rp->pos = w_off;

		/* Build in forward order for "ar -m" command. */
		*pnext = rp;
		pnext = &rp->next;

		++symcnt;
		tsymlen += symlen;
	}

bad2:	free(strtab);
bad1:	(void)lseek(rfd, r_off, SEEK_SET);
}

/*
 * symobj --
 *	Write the symbol table into the archive, computing offsets as
 *	writing.
 */
static void
symobj()
{
	register RLIB *rp, *rpnext;
	struct ranlib rn;
	off_t ransize;
	long size, stroff;
	char hb[sizeof(struct ar_hdr) + 1], pad;

	/* Rewind the archive, leaving the magic number. */
	if (fseek(fp, (long)SARMAG, SEEK_SET))
		error(archive);

	/* Size of the ranlib archive file, pad if necessary. */
	ransize = sizeof(long) +
	    symcnt * sizeof(struct ranlib) + sizeof(long) + tsymlen;
	if (ransize & 01) {
		++ransize;
		pad = '\n';
	} else
		pad = '\0';

	/* Put out the ranlib archive file header. */
#define	DEFMODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
	(void)sprintf(hb, HDR2, RANLIBMAG, 0L, getuid(), getgid(),
	    DEFMODE & ~umask(0), ransize, ARFMAG);
	if (!fwrite(hb, sizeof(struct ar_hdr), 1, fp))
		error(tname);

	/* First long is the size of the ranlib structure section. */
	size = symcnt * sizeof(struct ranlib);
	if (!fwrite(&size, sizeof(size), 1, fp))
		error(tname);

	/* Offset of the first archive file. */
	size = SARMAG + sizeof(struct ar_hdr) + ransize;

	/*
	 * Write out the ranlib structures.  The offset into the string
	 * table is cumulative, the offset into the archive is the value
	 * set in rexec() plus the offset to the first archive file.
	 */
	for (rp = rhead, stroff = 0; rp; rp = rp->next) {
		rn.ran_un.ran_strx = stroff;
		stroff += rp->symlen;
		rn.ran_off = size + rp->pos;
		if (!fwrite(&rn, sizeof(struct ranlib), 1, fp))
			error(archive);
	}

	/* Second long is the size of the string table. */
	if (!fwrite(&tsymlen, sizeof(tsymlen), 1, fp))
		error(tname);

	/* Write out the string table. */
	for (rp = rhead; rp; rp = rpnext) {
		if (!fwrite(rp->sym, rp->symlen, 1, fp))
			error(tname);
		rpnext = rp->next;
		free(rp->sym);
		free(rp);
	}
	rhead = NULL;

	if (pad && !fwrite(&pad, sizeof(pad), 1, fp))
		error(tname);

	(void)fflush(fp);
}
