/*-
 * Copyright (c) 1990, 1993
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

#ifndef lint
static char sccsid[] = "@(#)nlist.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <a.out.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name

#define	badfmt(str)	errx(1, "%s: %s: %s", kfile, str, strerror(EFTYPE))

static char *kfile;

void
create_knlist(name, db)
	char *name;
	DB *db;
{
	register int nsyms;
	struct exec *ebuf;
	NLIST *nbuf;
	DBT data, key;
	int fd;
	char *strtab;
	u_char *filep;
	char *vp;
	struct stat sst;
	long cur_off, voff;

	kfile = name;
	if ((fd = open(name, O_RDONLY, 0)) < 0)
		err(1, "%s", name);

	fstat(fd,&sst);

	filep = (u_char*)mmap(0, sst.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (filep == (u_char*)MAP_FAILED)
		err(1, "mmap failed");

	/* Read in exec structure. */
	ebuf = (struct exec *) filep;

	/* Check magic number and symbol count. */
	if (N_BADMAG(*ebuf))
		badfmt("bad magic number");
	if (!ebuf->a_syms)
		badfmt("stripped");

	strtab = filep + N_STROFF(*ebuf) + sizeof (int);

	/* Seek to symbol table. */
	cur_off = N_SYMOFF(*ebuf);

	/* Read each symbol and enter it into the database. */
	nsyms = ebuf->a_syms / sizeof(struct nlist);
	while (nsyms--) {

		nbuf = (NLIST *)(filep + cur_off);
		cur_off += sizeof(NLIST);

		if (!nbuf->_strx || nbuf->n_type&N_STAB)
			continue;

		key.data = (u_char *)strtab + nbuf->_strx - sizeof(long);
		key.size = strlen((char *)key.data);
		data.data = (u_char *)nbuf;
		data.size = sizeof(NLIST);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		if (1 && strcmp((char *)key.data, VRS_SYM) == 0) {
#ifndef KERNTEXTOFF
/*
 * XXX
 * The FreeBSD bootloader loads the kernel at the a_entry address, meaning
 * that this is where the kernel starts.  (not at KERNBASE)
 *
 * This may be introducing an i386 dependency.
 */
#if defined(__FreeBSD__)
#define KERNTEXTOFF ebuf->a_entry
#else
#define KERNTEXTOFF KERNBASE
#endif
#endif
			/*
			 * Calculate offset relative to a normal (non-kernel)
			 * a.out.  KERNTEXTOFF is where the kernel is really
			 * loaded; N_TXTADDR is where a normal file is loaded.
			 * From there, locate file offset in text or data.
			 */
			voff = nbuf->n_value - KERNTEXTOFF + N_TXTADDR(*ebuf);
			if ((nbuf->n_type & N_TYPE) == N_TEXT)
				voff += N_TXTOFF(*ebuf) - N_TXTADDR(*ebuf);
			else
				voff += N_DATOFF(*ebuf) - N_DATADDR(*ebuf);

			vp = filep + voff;

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			data.data = vp;
			data.size = strchr(vp, '\n') - vp + 1;

			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.size = sizeof(NLIST);
		}
	}
}
