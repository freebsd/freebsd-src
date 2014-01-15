/*-
 * Copyright (c) 2006,2011 Joseph Koshy
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: newehdr_template.m4 1376 2011-01-23 04:15:51Z jkoshy $
 */

#include <stdlib.h>

/*
 * Boilerplate for testing newehdr{32,64} behaviour that is not
 * common to the getehdr{32,64} functions.
 */

define(`TS_NEWELF',`"new.elf"')

ifdef(`TS_ICNAME',`',
  `errprint(`File included before "ehdr_template.m4".')m4exit(1)')

#define	CHECK_NEWEHDR(E,VER) 	do {					\
		if ((E)->e_ident[EI_MAG0] != ELFMAG0 ||			\
		    (E)->e_ident[EI_MAG1] != ELFMAG1 ||			\
		    (E)->e_ident[EI_MAG2] != ELFMAG2 ||			\
		    (E)->e_ident[EI_MAG3] != ELFMAG3 ||			\
		    (E)->e_ident[EI_CLASS] != TS_ELFCLASS ||		\
		    (E)->e_ident[EI_DATA] != ELFDATANONE ||		\
		    (E)->e_ident[EI_VERSION] != (VER) || 		\
		    (E)->e_machine != EM_NONE ||			\
		    (E)->e_type != ELF_K_NONE ||			\
		    (E)->e_version != (VER))				\
			TP_FAIL("TS_ICNAME`'() header mismatch.");				\
	} while (0)

/*
 * Verify that a new ehdr has the appropriate defaults.
 */

void
tcAllocateCheckDefaults(void)
{
	TS_EHDR	*eh;
	Elf	*e;
	int fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'() allocates an ehdr with the "
	    "documented defaults.");

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	result = TET_PASS;

	if ((eh = TS_ICFUNC`'(e)) == NULL) {
		TP_FAIL("TS_ICNAME`'() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	CHECK_NEWEHDR(eh,EV_CURRENT);

 done:
	(void) elf_end(e);
	(void) close(fd);
	(void) unlink(TS_NEWELF);

	tet_result(result);
}

/*
 * Verify that a new ehdr is marked `dirty'.   This test uses extended
 * functionality in libelf.
 */

void
tcAllocateFlagDirty(void)
{
	TS_EHDR *eh;
	Elf *e;
	int fd, flags, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'() marks the new Ehdr as \"dirty\".");

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	if ((eh = TS_ICFUNC`'(e)) == NULL) {
		TP_UNRESOLVED("TS_ICNAME`'() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	flags = elf_flagehdr(e, ELF_C_CLR, 0); /* Our extension */

	tet_result((flags & ELF_F_DIRTY) == 0 ? TET_FAIL : TET_PASS);

 done:
	(void) unlink(TS_NEWELF);
	(void) elf_end(e);
	(void) close(fd);
}

/* Declare fixed sizes associated with an ELF header. */
ifelse(`TS_EHDRSZ',`32',`
#define	TS_EHSIZE	52
#define	TS_PHENTSIZE	32
#define	TS_SHENTSIZE	40
',`
#define	TS_EHSIZE	64
#define	TS_PHENTSIZE	56
#define	TS_SHENTSIZE	64
')

define(`TS_REFELF',`newehdr')

/*
 * Verify that the correct header is written out.
 */

define(`FN',`
void
tcUpdate$1`'TS_EHDRSZ`'(void)
{
	TS_EHDR *eh;
	Elf *e;
	int fd, reffd, result;
	off_t offset;
	size_t fsz;
	void *t, *tref;
	char *ref = "TS_REFELF.TOLOWER($1)`'TS_EHDRSZ";

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("the contents of the Ehdr for byteorder $1 are correct.");

	t = tref = NULL;
	fd = reffd = -1;

	TS_OPEN_FILE(e, TS_NEWELF, ELF_C_WRITE, fd);

	result = TET_UNRESOLVED;

	if ((eh = TS_ICFUNC`'(e)) == NULL) {
		TP_UNRESOLVED("TS_ICNAME`'() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2$1;

	/* Write out the new ehdr. */
	if ((offset = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	/* check that the correct number of bytes were written out. */
	fsz = elf`'TS_EHDRSZ`'_fsize(ELF_T_EHDR, 1, EV_CURRENT);

	if (offset != fsz) {
		TP_FAIL("elf_update() -> %d, expected %d.", offset, fsz);
		goto done;
	}

	(void) elf_end(e);	e = NULL;
	(void) close(fd);	fd = -1;

	if ((t = malloc(fsz)) == NULL) {
		TP_UNRESOLVED("malloc %d bytes failed: %s.", fsz,
		    strerror(errno));
		goto done;
	}

	if ((fd = open(TS_NEWELF, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open() failed: %s.", strerror(errno));
		goto done;
	}

	if (read(fd, t, fsz) != fsz) {
		TP_UNRESOLVED("read %d bytes failed: %s.", fsz,
		    strerror(errno));
		goto done;
	}

	if ((reffd = open(ref, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open(%s) failed: %s.", ref,
		    strerror(errno));
		goto done;
	}

	if ((tref = malloc(fsz)) == NULL) {
		TP_UNRESOLVED("malloc %d bytes failed: %s.", fsz,
		    strerror(errno));
		goto done;
	}

	if (read(reffd, tref, fsz) != fsz) {
		TP_UNRESOLVED("unresolved: read \"%s\" failed: %s.", ref,
		    strerror(errno));
		goto done;
	}

	/* Read it back in */
	result = TET_PASS;
	if (memcmp(t, tref, fsz) != 0)
		TP_FAIL("memcmp(" TS_NEWELF ",%s) failed.", ref);

 done:
	(void) unlink(TS_NEWELF);
	if (e)
		(void) elf_end(e);
	if (tref)
		free(tref);
	if (t)
		free(t);
	if (fd != -1)
		(void) close(fd);
	if (reffd != -1)
		(void) close(reffd);
	tet_result(result);
}')

FN(`LSB')
FN(`MSB')
