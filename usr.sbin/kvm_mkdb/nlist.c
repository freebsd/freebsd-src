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
#if 0
static char sccsid[] = "from: @(#)nlist.c	8.1 (Berkeley) 6/6/93";
#else
static char *rcsid = "$Id$";
#endif
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

#ifdef DO_ELF
#include <elf.h>
#endif

typedef struct nlist NLIST;
#define	_strx	n_un.n_strx
#define	_name	n_un.n_name

#define	badfmt(str)	errx(1, "%s: %s: %s", kfile, str, strerror(EFTYPE))
static char *kfile;

#if defined(DO_AOUT)

int
__aout_knlist(name, db)
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


#endif /* DO_AOUT */

#ifdef DO_ELF
int
__elf_knlist(name, db)
	char *name;
	DB *db;
{
	register caddr_t strtab;
	register off_t symstroff, symoff;
	register u_long symsize;
	register u_long kernvma, kernoffs;
	register int i;
	Elf32_Sym *sbuf;
	size_t symstrsize;
	char *shstr, buf[1024];
	Elf32_Ehdr *eh;
	Elf32_Shdr *sh = NULL;
	DBT data, key;
	NLIST nbuf;
	int fd;
	u_char *filep;
	struct stat sst;

	kfile = name;
	if ((fd = open(name, O_RDONLY, 0)) < 0)
		err(1, "%s", name);

	fstat(fd, &sst);

	/* Check for files too large to mmap. */
	/* XXX is this really possible? */
	if (sst.st_size > SIZE_T_MAX) {
		badfmt("corrupt file");
	}
	filep = (u_char*)mmap(0, sst.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (filep == (u_char*)MAP_FAILED)
		err(1, "mmap failed");

	/* Read in exec structure. */
	eh = (Elf32_Ehdr *) filep;

	if (!IS_ELF(*eh))
		return(-1);

	sh = (Elf32_Shdr *)&filep[eh->e_shoff];

	shstr = (char *)&filep[sh[eh->e_shstrndx].sh_offset];

	for (i = 0; i < eh->e_shnum; i++) {
		if (strcmp (shstr + sh[i].sh_name, ".strtab") == 0) {
			symstroff = sh[i].sh_offset;
			symstrsize = sh[i].sh_size;
		}
		else if (strcmp (shstr + sh[i].sh_name, ".symtab") == 0) {
			symoff = sh[i].sh_offset;
			symsize = sh[i].sh_size;
		}
		else if (strcmp (shstr + sh[i].sh_name, ".text") == 0) {
			kernvma = sh[i].sh_addr;
			kernoffs = sh[i].sh_offset;
		}
	}

	strtab = (char *)&filep[symstroff];

	data.data = (u_char *)&nbuf;
	data.size = sizeof(NLIST);

	/* Read each symbol and enter it into the database. */
	for (i = 0; symsize > 0; i++, symsize -= sizeof(Elf32_Sym)) {

		sbuf = (Elf32_Sym *)&filep[symoff + i * sizeof(*sbuf)];
		if (!sbuf->st_name)
			continue;

		nbuf.n_value = sbuf->st_value;

		/*XXX type conversion is pretty rude... */
		switch (ELF32_ST_TYPE(sbuf->st_info)) {
		case STT_NOTYPE:
			nbuf.n_type = N_UNDF;
			break;
		case STT_FUNC:
			nbuf.n_type = N_TEXT;
			break;
		case STT_OBJECT:
			nbuf.n_type = N_DATA;
			break;
		}
		if (ELF32_ST_BIND(sbuf->st_info) == STB_LOCAL)
			nbuf.n_type = N_EXT;

		key.data = (u_char *)(strtab + sbuf->st_name);
		key.size = strlen((char *)key.data);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		/* also put in name prefixed with _ */
		*buf = '_';
		strcpy(buf + 1, strtab + sbuf->st_name);
		key.data = (u_char *)buf;
		key.size = strlen((char *)key.data);
		if (db->put(db, &key, &data, 0))
			err(1, "record enter");

		/* Special processing for "_version" (depends on above) */
		if (strcmp((char *)key.data, VRS_SYM) == 0) {
			char *vp;

			key.data = (u_char *)VRS_KEY;
			key.size = sizeof(VRS_KEY) - 1;
			/* Find the version string, relative to its section */
			data.data = strdup(&filep[nbuf.n_value -
			    sh[sbuf->st_shndx].sh_addr +
			    sh[sbuf->st_shndx].sh_offset]);
			/* assumes newline terminates version. */
			if ((vp = strchr(data.data, '\n')) != NULL)
				*vp = '\0';
			data.size = strlen((char *)data.data);

			if (db->put(db, &key, &data, 0))
				err(1, "record enter");

			/* Restore to original values. */
			data.data = (u_char *)&nbuf;
			data.size = sizeof(NLIST);
		}
	}
	munmap(filep, sst.st_size);
	(void)close(fd);
	return(0);
}
#endif /* DO_ELF */

static struct knlist_handlers {
	int	(*fn) __P((char *name, DB *db));
} nlist_fn[] = {
#ifdef DO_ELF
	{ __elf_knlist },
#endif
#ifdef DO_AOUT
	{ __aout_knlist },
#endif
};

void
create_knlist(name, db)
	char *name;
	DB *db;
{
	int n, i;

	for (i = 0; i < sizeof(nlist_fn)/sizeof(nlist_fn[0]); i++) {
		n = (nlist_fn[i].fn)(name, db);
		if (n != -1)
			break;
	}
}
