/*-
 * Copyright (c) 1990 The Regents of the University of California.
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

#ifndef lint
/*static char sccsid[] = "from: @(#)nlist.c	5.4 (Berkeley) 4/27/91";*/
static char rcsid[] = "$Id";
#endif /* not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <limits.h>
#include <a.out.h>
#include <db.h>
#include <errno.h>
#include <unistd.h>
#include <kvm.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name

static char *kfile;

create_knlist(name, db)
	char *name;
	DB *db;
{
	register int nsyms;
	struct exec ebuf;
	FILE *fp;
	NLIST nbuf;
	DBT data, key;
	int fd, nr, strsize;
	char *strtab, buf[1024];

	kfile = name;
	if ((fd = open(name, O_RDONLY, 0)) < 0)
		error(name);

	/* Read in exec structure. */
	nr = read(fd, (char *)&ebuf, sizeof(struct exec));
	if (nr != sizeof(struct exec))
		badfmt(nr, "no exec header");

	/* Check magic number and symbol count. */
	if (N_BADMAG(ebuf))
		badfmt("bad magic number");
	if (!ebuf.a_syms)
		badfmt("stripped");

	/* Seek to string table. */
	if (lseek(fd, N_STROFF(ebuf), SEEK_SET) == -1)
		badfmt("corrupted string table");

	/* Read in the size of the symbol table. */
	nr = read(fd, (char *)&strsize, sizeof(strsize));
	if (nr != sizeof(strsize))
		badread(nr, "no symbol table");

	/* Read in the string table. */
	strsize -= sizeof(strsize);
	if (!(strtab = (char *)malloc(strsize)))
		error(name);
	if ((nr = read(fd, strtab, strsize)) != strsize)
		badread(nr, "corrupted symbol table");

	/* Seek to symbol table. */
	if (!(fp = fdopen(fd, "r")))
		error(name);
	if (fseek(fp, N_SYMOFF(ebuf), SEEK_SET) == -1)
		error(name);
	
	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	/* Read each symbol and enter it into the database. */
	nsyms = ebuf.a_syms / sizeof(struct nlist);
	while (nsyms--) {
		if (fread((char *)&nbuf, sizeof (NLIST), 1, fp) != 1) {
			if (feof(fp))
				badfmt("corrupted symbol table");
			error(name);
		}
		if (!nbuf._strx || nbuf.n_type&N_STAB)
			continue;

		key.data = (u_char *)strtab + nbuf._strx - sizeof(long);
		key.size = strlen((char *)key.data);
		if ((db->put)(db, &key, &data, 0))
			error("put");

		if (!strncmp((char *)key.data, VRS_SYM, sizeof(VRS_SYM) - 1)) {
			off_t cur_off, rel_off, vers_off;

			/* Offset relative to start of text image in VM. */
#ifdef hp300
			rel_off = nbuf.n_value;
#endif
#ifdef tahoe
			/*
			 * On tahoe, first 0x800 is reserved for communication
			 * with the console processor.
			 */
			rel_off = ((nbuf.n_value & ~KERNBASE) - 0x800);
#endif
#ifdef vax
			rel_off = nbuf.n_value & ~KERNBASE;
#endif
#ifdef i386
			rel_off = nbuf.n_value - ebuf.a_entry + CLBYTES;
#endif
			/*
			 * When loaded, data is rounded to next page cluster
			 * after text, but not in file.
			 */
			rel_off -= CLBYTES - (ebuf.a_text % CLBYTES);
			vers_off = N_TXTOFF(ebuf) + rel_off;

			cur_off = ftell(fp);
			if (fseek(fp, vers_off, SEEK_SET) == -1)
				badfmt("corrupted string table");

			/*
			 * Read version string up to, and including newline.
			 * This code assumes that a newline terminates the
			 * version line.
			 */
			if (fgets(buf, sizeof(buf), fp) == NULL)
				badfmt("corrupted string table");

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = (u_char *)buf;
			data.size = strlen(buf);
			if ((db->put)(db, &key, &data, 0))
				error("put");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
			if (fseek(fp, cur_off, SEEK_SET) == -1)
				badfmt("corrupted string table");
		}
	}
	(void)fclose(fp);
}

badread(nr, p)
	int nr;
	char *p;
{
	if (nr < 0)
		error(kfile);
	badfmt(p);
}

badfmt(p)
	char *p;
{
	(void)fprintf(stderr,
	    "kvm_mkdb: %s: %s: %s\n", kfile, p, strerror(EFTYPE));
	exit(1);
}
