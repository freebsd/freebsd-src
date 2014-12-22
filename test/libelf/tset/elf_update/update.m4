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
 * $Id: update.m4 3081 2014-07-28 08:53:14Z jkoshy $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"

#include "tet_api.h"

include(`elfts.m4')
define(`TS_OFFSET_SHDR',512)
define(`MAKE_EM',
    `ifelse($1,32,
        ifelse($2,msb,EM_SPARC,EM_386),
        ifelse($2,msb,EM_SPARCV9,EM_X86_64))')

/*
 * Tests for the `elf_update' API.
 */

IC_REQUIRES_VERSION_INIT();

static char rawdata[] = "This is not an ELF file.";

/*
 * A NULL Elf argument returns ELF_E_ARGUMENT.
 */

void
tcArgsNull(void)
{
	int error, result;
	off_t offset;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_update(NULL,*) fails with ELF_E_ARGUMENT.");

	if ((offset = elf_update(NULL, 0)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("elf_update() did not fail with ELF_E_ARGUMENT; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	tet_result(result);
}

/*
 * Illegal values for argument `cmd' are rejected.
 */

void
tcArgsBadCmd(void)
{
	Elf *e;
	Elf_Cmd c;
	int error, result;
	off_t offset;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("illegal cmd values are rejected with ELF_E_ARGUMENT.");

	TS_OPEN_MEMORY(e, rawdata);

	result = TET_PASS;
	for (c = ELF_C_NULL-1; result == TET_PASS && c < ELF_C_NUM; c++) {
		if (c == ELF_C_WRITE || c == ELF_C_NULL) /* legal values */
			continue;
		if ((offset = elf_update(e, c)) != (off_t) -1)
			TP_FAIL("elf_update() succeeded unexpectedly; "
			    "offset=%jd.", (intmax_t) offset);
		else if ((error = elf_errno()) != ELF_E_ARGUMENT)
			TP_FAIL("elf_update() did not fail with "
			    "ELF_E_ARGUMENT; error=%d \"%s\".", error,
			    elf_errmsg(error));
	}

	(void) elf_end(e);
	tet_result(result);
}

/*
 * Non-ELF descriptors are rejected by elf_update().
 */
undefine(`FN')
define(`FN',`
void
tcArgsNonElf$1(void)
{
	Elf *e;
	int error, fd, result;
	off_t offset;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_update(non-elf,ELF_C_$1) returns ELF_E_ARGUMENT.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_WRITE_FILE(TS_NEWFILE,rawdata,sizeof(rawdata),goto done;);
	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_READ, fd, goto done;);

	if ((offset = elf_update(e, ELF_C_$1)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("elf_update() did not fail with ELF_E_ARGUMENT; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(`NULL')
FN(`WRITE')

/*
 * In-memory (i.e., non-writeable) ELF objects are rejected for
 * ELF_C_WRITE with error ELF_E_MODE.
 */

undefine(`FN')
define(`FN',`
void
tcMemElfWrite$1$2(void)
{
	Elf *e;
	off_t offset;
	int error, result;
	char elf[sizeof(Elf64_Ehdr)]; /* larger of the Ehdr variants */

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: ELF_C_WRITE with in-memory objects "
	    "returns ELF_E_MODE.");

	result = TET_UNRESOLVED;
	e = NULL;

	_TS_READ_FILE("newehdr.$2$1", elf, sizeof(elf), goto done;);

	TS_OPEN_MEMORY(e, elf);

	if ((offset = elf_update(e, ELF_C_WRITE)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	 if ((error = elf_errno()) != ELF_E_MODE) {
		TP_FAIL("elf_update() did not fail with ELF_E_MODE; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(`32', `lsb')
FN(`32', `msb')
FN(`64', `lsb')
FN(`64', `msb')

/*
 * In-memory ELF objects are updateable with command ELF_C_NULL.
 */

undefine(`FN')
define(`FN',`
void
tcMemElfNull$1$2(void)
{
	Elf *e;
	int result;
	size_t fsz;
	off_t offset;
	char elf[sizeof(Elf64_Ehdr)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: ELF_C_NULL updates in-memory objects.");

	result = TET_UNRESOLVED;

	_TS_READ_FILE("newehdr.$2$1", elf, sizeof(elf), goto done;);

	TS_OPEN_MEMORY(e, elf);

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("elf$2_fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if ((offset = elf_update(e, ELF_C_NULL)) != fsz)
		TP_FAIL("offset=%jd != %d, error=%d \"%s\".",
		    (intmax_t) offset, fsz, elf_errmsg(-1));

 done:
	(void) elf_end(e);
	tet_result(result);
}')

FN(`32', `lsb')
FN(`32', `msb')
FN(`64', `lsb')
FN(`64', `msb')

/*
 * A mismatched class in the Ehdr returns an ELF_E_CLASS error.
 */

undefine(`FN')
define(`FN',`
void
tcClassMismatch$1$2(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf$1_Ehdr *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: a class-mismatch is detected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: %s", elf_errmsg(-1));
		goto done;
	}

	/* change the class */
	eh->e_ident[EI_CLASS] = ELFCLASS`'ifelse($1,32,64,32);

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_CLASS) {
		TP_FAIL("elf_update() did not fail with ELF_E_CLASS; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(`32', `lsb')
FN(`32', `msb')
FN(`64', `lsb')
FN(`64', `msb')

/*
 * Changing the byte order of an ELF file on the fly is not allowed.
 */

undefine(`FN')
define(`FN',`
void
tcByteOrderChange$1$2(void)
{
	int error, fd, result;
	Elf *e;
	off_t offset;
	Elf$1_Ehdr *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: byte order changes are rejected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2`'ifelse($2,`lsb',`MSB',`LSB');

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_HEADER) {
		TP_FAIL("elf_update() did not fail with ELF_E_HEADER; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(`32', `lsb')
FN(`32', `msb')
FN(`64', `lsb')
FN(`64', `msb')

/*
 * An unsupported ELF version is rejected with ELF_E_VERSION.
 */

undefine(`FN')
define(`FN',`
void
tcUnsupportedVersion$1$2(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf$1_Ehdr *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: version changes are rejected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	eh->e_version = EV_CURRENT+1;

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_VERSION) {
		TP_FAIL("elf_update() did not fail with ELF_E_VERSION; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(`32', `lsb')
FN(`32', `msb')
FN(`64', `lsb')
FN(`64', `msb')

/*
 * Invoking an elf_cntl(ELF_C_FDDONE) causes a subsequent elf_update()
 * to fail with ELF_E_SEQUENCE.
 */
undefine(`FN')
define(`FN',`
void
tcSequenceFdDoneWrite$1(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf$1_Ehdr *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("*$1: elf_update(ELF_C_WRITE) after an elf_cntl(FDDONE) "
	    "is rejected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if (elf_cntl(e, ELF_C_FDDONE) != 0) {
		TP_UNRESOLVED("elf_cntl() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((offset = elf_update(e, ELF_C_WRITE)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_SEQUENCE) {
		TP_FAIL("elf_update() did not fail with ELF_E_SEQUENCE; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(32)
FN(64)

/*
 * Invoking an elf_cntl(ELF_C_FDDONE) causes a subsequent
 * elf_update(ELF_C_NULL) to succeed.
 */

undefine(`FN')
define(`FN',`
void
tcSequenceFdDoneNull$1(void)
{
	int fd, result;
	off_t offset;
	size_t fsz;
	Elf *e;
	Elf$1_Ehdr *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_update(ELF_C_NULL) after an elf_cntl(FDDONE) "
	    "succeeds.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if (elf_cntl(e, ELF_C_FDDONE) != 0) {
		TP_UNRESOLVED("elf_cntl() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((offset = elf_update(e, ELF_C_NULL)) != fsz) {
		TP_FAIL("elf_update()->%jd, (expected %d).",
		    (intmax_t) offset, fsz);
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')

FN(32)
FN(64)

/*
 * Check that elf_update() can create a legal ELF file.
 */

const char strtab[] = {
	'\0',
	'.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', '\0'
};

#define	INIT_PHDR(P)	do {				\
		(P)->p_type   = PT_NULL;		\
		(P)->p_offset = 0x0F0F0F0F;		\
		(P)->p_vaddr  = 0xA0A0A0A0;		\
		(P)->p_filesz = 0x1234;			\
		(P)->p_memsz  = 0x5678;			\
		(P)->p_flags  = PF_X | PF_R;		\
		(P)->p_align  = 64;			\
	} while (0)

#define	INIT_SHDR(S,O)	do {				\
		(S)->sh_name = 1;			\
		(S)->sh_type = SHT_STRTAB;		\
		(S)->sh_flags = 0;			\
		(S)->sh_addr = 0;			\
		(S)->sh_offset = (O);			\
		(S)->sh_size = sizeof(strtab);		\
		(S)->sh_link = 0;			\
		(S)->sh_info = 0;			\
		(S)->sh_addralign = 1;			\
		(S)->sh_entsize = 0;			\
	} while (0)

undefine(`FN')
define(`FN',`
void
tcUpdate$1$2(void)
{
	int fd, result;
	off_t offset;
	size_t esz, fsz, psz, roundup, ssz;
	Elf$1_Shdr *sh;
	Elf$1_Ehdr *eh;
	Elf$1_Phdr *ph;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() creates a legal ELF file.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Set the version and endianness */
	eh->e_version = EV_CURRENT;
	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($2);
	eh->e_type = ET_REL;

	if ((esz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0 ||
	    (psz = elf$1_fsize(ELF_T_PHDR, 1, EV_CURRENT)) == 0 ||
	    (ssz = elf$1_fsize(ELF_T_SHDR, 2, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("elf$1_fsize() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((ph = elf$1_newphdr(e,1)) == NULL) {
		TP_UNRESOLVED("elf$1_newphdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	INIT_PHDR(ph);

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_shstrndx = elf_ndxscn(scn);

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_buf  = (char *) strtab;
	d->d_size = sizeof(strtab);
	d->d_off  = (off_t) 0;

	INIT_SHDR(sh, esz+psz);

	fsz = esz + psz + sizeof(strtab);
	roundup = ifelse($1,32,4,8);
	fsz = (fsz + roundup - 1) & ~(roundup - 1);

	fsz += ssz;

	if ((offset = elf_update(e, ELF_C_WRITE)) != fsz) {
		TP_FAIL("ret=%jd != %d [elferror=\"%s\"]",
		    (intmax_t) offset, fsz, elf_errmsg(-1));
		goto done;
	}

	(void) elf_end(e);	e = NULL;
	(void) close(fd);	fd = -1;

	result = elfts_compare_files("u1.$2$1", TS_NEWFILE);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * An unsupported section type should be rejected.
 */
undefine(`FN')
define(`FN',`
void
tcSectionType$2$1(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf_Scn *scn;
	Elf$1_Shdr *sh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: unsupported section types are rejected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd, goto done;);

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type = SHT_LOOS - 1;
	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_SECTION) {
		TP_FAIL("elf_update() did not fail with ELF_E_SECTION; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd)
		(void) close(fd);
	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Verify that sections with unrecognized sh_type values in the
 * range [SHT_LOUSER,SHT_HIUSER], [SHT_LOPROC,SHT_HIPROC],
 * and [SHT_LOOS,SHT_HIOS] are accepted.
 */

define(`ADD_SECTION',`
	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}
	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}
	sh->sh_type = $2;
	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}
	d->d_align = 1;
	d->d_buf = NULL;
	d->d_size = 0;
	d->d_off = (off_t) 0;
	(void) elf_flagdata(d, ELF_C_SET, ELF_F_DIRTY);
	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);')
undefine(`FN')
define(`FN',`
void
tcSectionTypeOSUserProcDefined_$2$1(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf$1_Shdr *sh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: user, OS and processor specific "
	    "section types are accepted.") ;

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd, goto done;);

	/*
	 * Create two new sections, one of type SHT_LOOS (0x60000000UL),
	 * and the other of type SHT_HIUSER (0xFFFFFFFFUL).  These
	 * should be accepted as valid sections.
	 */
	ADD_SECTION($1,`SHT_LOOS')
	ADD_SECTION($1,`SHT_HIUSER')

	if ((offset = elf_update(e, ELF_C_NULL)) == (off_t) -1) {
		TP_FAIL("elf_update() failed.");
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd)
		(void) close(fd);
	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

undefine(`ADD_SECTION')

/*
 * An Elf_Data descriptor that is malformed in various ways
 * should be rejected.
 */

undefine(`FN')
define(`FN',`
void
tc$3_$2$1(void)
{
	int error, fd, result;
	off_t offset;
	Elf *e;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf$1_Shdr *sh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: data descriptors with " $6
	    " are rejected.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, "newehdr.$2$1", ELF_C_READ, fd, goto done;);

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type = SHT_SYMTAB;
	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Setup defaults for the test. */
	d->d_buf  = (char *) NULL;
	d->d_size = sizeof(Elf$1_Sym);
	d->d_type = ELF_T_SYM;
	d->d_align = 1;

	/* Override, on a per test case basis. */
	$4

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_$5) {
		TP_FAIL("elf_update() did not fail with ELF_E_$5; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd)
		(void) close(fd);
	tet_result(result);
}')

define(`MKFN',`
FN(32,`lsb',$1,$2,$3,$4)
FN(32,`msb',$1,$2,$3,$4)
FN(64,`lsb',$1,$2,$3,$4)
FN(64,`msb',$1,$2,$3,$4)
')

MKFN(IllegalAlignment, `d->d_align = 3;', DATA, "incorrect alignments")
MKFN(UnsupportedVersion, `d->d_version = EV_CURRENT+1;', VERSION,
    "an unknown version")
MKFN(UnknownElfType, `d->d_type = ELF_T_NUM;', DATA, "an unknown type")
MKFN(IllegalSize, `d->d_size = 1;', DATA, "an illegal size")


/*
 * Ensure that updating the section header on an ELF object opened
 * in ELF_C_RDWR mode in an idempotent manner leaves the object
 * in a sane state.  See ticket #269.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrShdrIdempotent$2$1(void)
{
	Elf *e;
	off_t fsz;
	struct stat sb;
	size_t strtabidx;
	Elf_Scn *strtabscn;
	int error, fd, tfd, result;
	GElf_Shdr strtabshdr;
	char *srcfile = "newscn.$2$1", *tfn;
	char *reffile = "newscn2.$2$1";

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: (liblayout) a no-op update of section "
	    "headers works as expected");

	result = TET_UNRESOLVED;
	e = NULL;
	tfn = NULL;
	fd = tfd = -1;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".", srcfile,
		    strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, tfd, goto done;);

	if (stat(reffile, &sb) < 0) {
		TP_UNRESOLVED("stat() failed: \"%s\".", strerror(errno));
		goto done;
	}

	/* Retrieve the index of the section name string table. */
	if (elf_getshdrstrndx(e, &strtabidx) != 0) {
		TP_UNRESOLVED("elf_getshdrstrndx() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/*
	 * Retrieve the section descriptor for the section name string table.
	 */
	if ((strtabscn = elf_getscn(e, strtabidx)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	/* Read the section header ... */
	if (gelf_getshdr(strtabscn, &strtabshdr) == NULL) {
		TP_UNRESOLVED("gelf_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* ... and write it back. */
	if (gelf_update_shdr(strtabscn, &strtabshdr) == 0) {
		TP_UNRESOLVED("gelf_update_shdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Update the underlying ELF object. */
	if ((fsz = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if (fsz != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() returned %d.", error);
		goto done;
	}

	e = NULL;
	/* Compare against the original. */
	result = elfts_compare_files(reffile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfd != -1)
		(void) close(tfd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Ensure that updating the section header table on an ELF object opened
 * in ELF_C_RDWR mode in an idempotent manner leaves the object
 * in a sane state.  See ticket #269.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrShdrIdempotentAppLayout$2$1(void)
{
	Elf *e;
	off_t fsz;
	struct stat sb;
	size_t strtabidx;
	Elf_Scn *strtabscn;
	unsigned int flags;
	int error, fd, tfd, result;
	GElf_Shdr strtabshdr;
	char *srcfile = "newscn.$2$1", *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: (applayout) a no-op update of section "
	    "headers works as expected");

	result = TET_UNRESOLVED;
	e = NULL;
	tfn = NULL;
	fd = tfd = -1;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".", srcfile,
		    strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, tfd, goto done;);

	flags = elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);
	if ((flags & ELF_F_LAYOUT) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fstat(tfd, &sb) < 0) {
		TP_UNRESOLVED("fstat() failed: \"%s\".",
		    strerror(errno));
		goto done;
	}

	/* Retrieve the index of the section name string table. */
	if (elf_getshdrstrndx(e, &strtabidx) != 0) {
		TP_UNRESOLVED("elf_getshdrstrndx() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/*
	 * Retrieve the section descriptor for the section name string table.
	 */
	if ((strtabscn = elf_getscn(e, strtabidx)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	/* Read the section header ... */
	if (gelf_getshdr(strtabscn, &strtabshdr) == NULL) {
		TP_UNRESOLVED("gelf_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* ... and write it back. */
	if (gelf_update_shdr(strtabscn, &strtabshdr) == 0) {
		TP_UNRESOLVED("gelf_update_shdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Update the underlying ELF object. */
	if ((fsz = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if (fsz != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() returned %d.", error);
		goto done;
	}

	e = NULL;
	/* Compare against the original. */
	result = elfts_compare_files(srcfile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfd != -1)
		(void) close(tfd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Test handling of sections with buffers of differing Elf_Data types.
 */

/*
 * The contents of the first Elf_Data buffer for section ".foo"
 * (ELF_T_WORD, align 4).
 */
uint32_t hash_words[] = {
	0x01234567,
	0x89abcdef,
	0xdeadc0de
};

/*
 * The contents of the second Elf_Data buffer for section ".foo"
 * (ELF_T_BYTE, align 1)
 */
char data_string[] = "helloworld";

/*
 * The contents of the third Elf_Data buffer for section ".foo"
 * (ELF_T_WORD, align 4)
 */
uint32_t checksum[] = {
	0xffffeeee
};

/*
 * The contents of the ".shstrtab" section.
 */
char string_table[] = {
	/* Offset 0 */   '\0',
	/* Offset 1 */   '.', 'f' ,'o', 'o', '\0',
	/* Offset 6 */   '.', 's' , 'h' , 's' , 't',
	'r', 't', 'a', 'b', '\0'
};

undefine(`FN')
define(`FN',`
void
tcMixedBuffer_$2$1(void)
{
	Elf *e;
	Elf_Scn *scn, *strscn;
	int error, fd, result;
	Elf$1_Ehdr *ehdr;
	Elf$1_Shdr *shdr, *strshdr;
	Elf_Data *data1, *data2, *data3, *data4;
	char *reffile = "mixedscn.$2$1", *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: sections with mixed data work "
	    "as expected");

	result = TET_UNRESOLVED;
	e = NULL;
	tfn = NULL;
	fd = -1;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((ehdr = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	ehdr->e_ident[EI_DATA] = `ELFDATA2'TOUPPER($2);
	ehdr->e_machine = MAKE_EM($1,$2);
	ehdr->e_type = ET_REL;

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((data1 = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata(data1) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	data1->d_align = 4;
	data1->d_off = 0;
	data1->d_buf = hash_words;
	data1->d_type = ELF_T_WORD;
	data1->d_size = sizeof(hash_words);
	data1->d_version = EV_CURRENT;

	if ((data2 = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata(data2) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	data2->d_align = 1;
	data2->d_off = 0;
	data2->d_buf = data_string;
	data2->d_type = ELF_T_BYTE;
	data2->d_size = sizeof(data_string);
	data2->d_version = EV_CURRENT;


	if ((data3 = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata(data3) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	data3->d_align = 4;
	data3->d_off = 0;
	data3->d_buf = checksum;
	data3->d_type = ELF_T_WORD;
	data3->d_size = sizeof(checksum);
	data3->d_version = EV_CURRENT;

	if ((shdr = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	shdr->sh_name = 1; /* offset of ".foo" */
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_entsize = 0;
	shdr->sh_addralign = 4;

	/*
	 * Create the .shstrtab section.
	 */
	if ((strscn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((data4 = elf_newdata(strscn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	data4->d_align = 1;
	data4->d_off = 0;
	data4->d_buf = string_table;
	data4->d_type = ELF_T_BYTE;
	data4->d_size = sizeof(string_table);
	data4->d_version = EV_CURRENT;

	if ((strshdr = elf$1_getshdr(strscn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	strshdr->sh_name = 6; /* \0 + strlen(".foo") + \0 */
	strshdr->sh_type = SHT_STRTAB;
	strshdr->sh_flags = SHF_STRINGS | SHF_ALLOC;
	strshdr->sh_entsize = 0;

	ehdr->e_shstrndx = elf_ndxscn(strscn);

	if (elf_update(e, ELF_C_WRITE) < 0) {
		TP_FAIL("elf_update() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Compare files here. */
	TP_UNRESOLVED("Verification is yet to be implemented.");

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Test that a call to elf_update() without any changes flagged
 * leaves the ELF object unchanged.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrModeNoOp_$1$2(void)
{
	struct stat sb;
	int error, fd, result;
	Elf *e;
	Elf$1_Ehdr *eh;
	const char *srcfile = "rdwr.$2$1";
	off_t fsz1, fsz2;
	char *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() without flagged changes "
	    "is a no-op");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	tfn = NULL;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".",
		    srcfile, strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, fd, goto done;);

	if (fstat(fd, &sb) < 0) {
		TP_UNRESOLVED("fstat() failed: \"%s\".",
		    strerror(errno));
		goto done;
	}

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_getehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz1 = elf_update(e, ELF_C_NULL)) < 0) {
		TP_FAIL("elf_update(NULL) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz1);
		goto done;
	}

	if ((fsz2 = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_FAIL("elf_update(WRITE) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != fsz2) {
		TP_FAIL("fsz1 (%d) != fsz2 (%d)", fsz1, fsz2);
		goto done;
	}

	e = NULL;
	(void) close(fd);

	/* compare against the original */
	result = elfts_compare_files(srcfile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}')

FN(32,lsb)
FN(32,msb)
FN(64,lsb)
FN(64,msb)

/*
 * Test that a call to elf_update() without a change to underlying
 * data for the object is a no-op.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrModeNoDataChange_$1$2(void)
{
	int error, fd, result;
	Elf *e;
	Elf_Scn *scn;
	const char *srcfile = "rdwr.$2$1";
	off_t fsz1, fsz2;
	struct stat sb;
	char *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() with no data changes "
	    "is a no-op");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	tfn = NULL;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".",
		    srcfile, strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, fd, goto done;);

	if (fstat(fd, &sb) < 0) {
		TP_UNRESOLVED("fstat() failed: \"%s\".",
		    strerror(errno));
		goto done;
	}

	if ((scn = elf_getscn(e, 1)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY) != ELF_F_DIRTY) {
		TP_UNRESOLVED("elf_flagscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz1 = elf_update(e, ELF_C_NULL)) < 0) {
		TP_FAIL("elf_update(NULL) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz1);
		goto done;
	}

	if ((fsz2 = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_FAIL("elf_update(WRITE) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != fsz2) {
		TP_FAIL("fsz1 (%d) != fsz2 (%d)", fsz1, fsz2);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	e = NULL;
	(void) close(fd);

	/* compare against the original */
	result = elfts_compare_files(srcfile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}')

FN(32,lsb)
FN(32,msb)
FN(64,lsb)
FN(64,msb)

/*
 * Test that a call to elf_update() with a changed ehdr causes the
 * underlying file to change.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrModeEhdrChange_$1$2(void)
{
	int error, fd, result;
	unsigned int flag;
	struct stat sb;
	Elf *e;
	Elf$1_Ehdr *eh;
	const char *srcfile = "rdwr.$2$1";
	const char *reffile = "rdwr1.$2$1";
	off_t fsz1, fsz2;
	char *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() updates a changed "
	    "header correctly");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	tfn = NULL;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".",
		    srcfile, strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, fd, goto done;);

	if (fstat(fd, &sb) < 0) {
		TP_UNRESOLVED("fstat() failed: \"%s\".",
		    strerror(errno));
		goto done;
	}

	if ((eh = elf$1_getehdr(e)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Change the ELFCLASS of the object. */
	eh->e_type = ET_DYN;

	flag = elf_flagehdr(e, ELF_C_SET, ELF_F_DIRTY);
	if ((flag & ELF_F_DIRTY) == 0) {
		TP_UNRESOLVED("elf_flagehdr failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz1 = elf_update(e, ELF_C_NULL)) < 0) {
		TP_FAIL("elf_update(NULL) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz1);
		goto done;
	}

	if ((fsz2 = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_FAIL("elf_update(WRITE) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != fsz2) {
		TP_FAIL("fsz1 (%d) != fsz2 (%d)", fsz1, fsz2);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	e = NULL;
	(void) close(fd);

	/* compare against the reference */
	result = elfts_compare_files(reffile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}
')

FN(32,lsb)
FN(32,msb)
FN(64,lsb)
FN(64,msb)

/*
 * Test extending a section.
 */

static char *base_data = "hello world";
static char *extra_data = "goodbye world";

undefine(`FN')
define(`FN',`
void
tcRdWrExtendSection_$1$2(void)
{
	int error, fd, result;
	unsigned int flag;
	struct stat sb;
	Elf *e;
	Elf_Scn *scn;
	Elf_Data *d;
	const char *srcfile = "rdwr.$2$1";
	const char *reffile = "rdwr2.$2$1";
	off_t fsz1, fsz2;
	char *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() deals with an "
	    "extended section correctly");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	tfn = NULL;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".",
		    srcfile, strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, fd, goto done;);

	if (stat(reffile, &sb) < 0) {
		TP_UNRESOLVED("stat() failed: \"%s\".", strerror(errno));
		goto done;
	}

	/* Retrieve section 1 and extend it. */

	if ((scn = elf_getscn(e, 1)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_buf = extra_data;
	d->d_size = strlen(extra_data);

	if (elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY) != ELF_F_DIRTY) {
		TP_UNRESOLVED("elf_flagscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz1 = elf_update(e, ELF_C_NULL)) < 0) {
		TP_FAIL("elf_update(NULL) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz1);
		goto done;
	}

	if ((fsz2 = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_FAIL("elf_update(WRITE) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != fsz2) {
		TP_FAIL("fsz1 (%d) != fsz2 (%d)", fsz1, fsz2);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	e = NULL;
	(void) close(fd);

	/* compare against the reference */
	result = elfts_compare_files(reffile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}
')

FN(32,lsb)
FN(32,msb)
FN(64,lsb)
FN(64,msb)

/*
 * Test shrinking a section.
 */

undefine(`FN')
define(`FN',`
void
tcRdWrShrinkSection_$1$2(void)
{
	int error, fd, result;
	unsigned int flag;
	struct stat sb;
	Elf *e;
	Elf_Scn *scn;
	Elf_Data *d;
	const char *srcfile = "rdwr2.$2$1";
	const char *reffile = "rdwr.$2$1";
	off_t fsz1, fsz2;
	char *tfn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_update() deals with an "
	    "shrunk section correctly");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	tfn = NULL;

	/* Make a copy of the reference object. */
	if ((tfn = elfts_copy_file(srcfile, &error)) < 0) {
		TP_UNRESOLVED("elfts_copyfile(%s) failed: \"%s\".",
		    srcfile, strerror(error));
		goto done;
	}

	/* Open the copied object in RDWR mode. */
	_TS_OPEN_FILE(e, tfn, ELF_C_RDWR, fd, goto done;);

	if (stat(reffile, &sb) < 0) {
		TP_UNRESOLVED("stat() failed: \"%s\".", strerror(errno));
		goto done;
	}

	/* Retrieve section 1 and shrink it. */

	if ((scn = elf_getscn(e, 1)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_getdata(scn, NULL)) == NULL) {
		TP_UNRESOLVED("elf_getdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_size = strlen(base_data);

	if (elf_flagdata(d, ELF_C_SET, ELF_F_DIRTY) != ELF_F_DIRTY) {
		TP_UNRESOLVED("elf_flagdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (elf_flagscn(scn, ELF_C_SET, ELF_F_DIRTY) != ELF_F_DIRTY) {
		TP_UNRESOLVED("elf_flagscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz1 = elf_update(e, ELF_C_NULL)) < 0) {
		TP_FAIL("elf_update(NULL) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != sb.st_size) {
		TP_FAIL("Size error: expected=%d, elf_update()=%d",
		    sb.st_size, fsz1);
		goto done;
	}

	if ((fsz2 = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_FAIL("elf_update(WRITE) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if (fsz1 != fsz2) {
		TP_FAIL("fsz1 (%d) != fsz2 (%d)", fsz1, fsz2);
		goto done;
	}

	/* Close the temporary file. */
	if ((error = elf_end(e)) != 0) {
		TP_UNRESOLVED("elf_end() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	e = NULL;
	(void) close(fd);

	/* compare against the reference */
	result = elfts_compare_files(reffile, tfn);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	if (tfn != NULL)
		(void) unlink(tfn);

	tet_result(result);
}
')

FN(32,lsb)
FN(32,msb)
FN(64,lsb)
FN(64,msb)

/*
 * Test cases rejecting malformed ELF files created with the
 * ELF_F_LAYOUT flag set.
 */

undefine(`FN')
define(`FN',`
void
tcEhdrPhdrCollision$1$2(void)
{
	int error, fd, result, flags;
	off_t offset;
	size_t fsz, psz, roundup, ssz;
	Elf$1_Ehdr *eh;
	Elf$1_Phdr *ph;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: an overlap of the ehdr and phdr is "
	    "detected.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	flags = elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);
	if ((flags & ELF_F_LAYOUT) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Fill in sane values for the Ehdr. */
	eh->e_type = ET_REL;
	eh->e_shoff = 0;
	eh->e_ident[EI_CLASS] = ELFCLASS`'$1;
	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($2);

	if ((ph = elf$1_newphdr(e, 1)) == NULL) {
		TP_UNRESOLVED("elf$1_newphdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	/* Make the phdr table overlap with the ehdr. */
	eh->e_phoff = fsz - 1;

	/* Check the return values from elf_update(). */
	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_LAYOUT) {
		TP_FAIL("elf_update() did not fail with ELF_E_LAYOUT, "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}
')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

undefine(`FN')
define(`FN',`
void
tcShdrPhdrCollision$1$2(void)
{
	int error, fd, result, flags;
	off_t offset;
	size_t fsz, psz, roundup, ssz;
	Elf$1_Ehdr *eh;
	Elf$1_Phdr *ph;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: an overlap of the shdr and phdr is "
	    "detected.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	flags = elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);
	if ((flags & ELF_F_LAYOUT) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Fill in sane values for the Ehdr. */
	eh->e_type = ET_REL;
	eh->e_ident[EI_CLASS] = ELFCLASS`'$1;
	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($2);

	if ((ph = elf$1_newphdr(e, 1)) == NULL) {
		TP_UNRESOLVED("elf$1_newphdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	/* Make the PHDR and SHDR tables overlap. */
	eh->e_phoff = fsz;
	eh->e_shoff = fsz;

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_LAYOUT) {
		TP_FAIL("elf_update() did not fail with ELF_E_LAYOUT; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}
')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Verify that an overlap between a section's data and the SHDR
 * table is detected.
 */

undefine(`FN')
define(`FN',`
void
tcShdrSectionCollision$1$2(void)
{
	int error, fd, result, flags;
	off_t offset;
	size_t fsz, psz, roundup, ssz;
	Elf$1_Ehdr *eh;
	Elf$1_Shdr *sh;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: an overlap of the shdr and a section is "
	    "detected.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	flags = elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);
	if ((flags & ELF_F_LAYOUT) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Fill in sane values for the Ehdr. */
	eh->e_type = ET_REL;
	eh->e_ident[EI_CLASS] = ELFCLASS`'$1;
	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($2);

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	eh->e_shoff = fsz;

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	/* Fill in application-specified fields. */
	sh->sh_type = SHT_PROGBITS;
	sh->sh_addralign = 1;
	sh->sh_size = 1;
	sh->sh_entsize = 1;

	/* Make this section overlap with the section header. */
	sh->sh_offset = fsz;

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_LAYOUT) {
		TP_FAIL("elf_update() did not fail with ELF_E_LAYOUT; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}
')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')

/*
 * Check that overlapping sections are rejected when ELF_F_LAYOUT is set.
 */

undefine(`FN')
define(`FN',`
void
tcSectionOverlap$1$2(void)
{
	int error, fd, result, flags;
	off_t offset;
	size_t fsz, psz, roundup, ssz;
	Elf$1_Ehdr *eh;
	Elf$1_Shdr *sh;
	Elf_Data *d;
	Elf_Scn *scn;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: an overlap between two sections is "
	    "detected.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	flags = elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT);
	if ((flags & ELF_F_LAYOUT) == 0) {
		TP_UNRESOLVED("elf_flagelf() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Fill in sane values for the Ehdr. */
	eh->e_type = ET_REL;
	eh->e_ident[EI_CLASS] = ELFCLASS`'$1;
	eh->e_ident[EI_DATA] = ELFDATA2`'TOUPPER($2);
	eh->e_shoff = TS_OFFSET_SHDR;

	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("fsize() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	/*
	 * Build the first section.
	 */

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_type = ELF_T_BYTE;
	d->d_off = 0;
	d->d_buf = base_data;
	d->d_size = strlen(base_data);

	/* Fill in application-specified fields. */
	sh->sh_type = SHT_PROGBITS;
	sh->sh_addralign = 1;
	sh->sh_size = 1;
	sh->sh_entsize = 1;
	sh->sh_offset = fsz;

	/*
	 * Build the second section.
	 */

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: %s.", elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_buf = base_data;
	d->d_size = strlen(base_data);

	/* Fill in application-specified fields. */
	sh->sh_type = SHT_PROGBITS;
	sh->sh_addralign = 1;
	sh->sh_size = 1;
	sh->sh_entsize = 1;

	sh->sh_offset = fsz + 1; /* Overlap with the first section. */

	if ((offset = elf_update(e, ELF_C_NULL)) != (off_t) -1) {
		TP_FAIL("elf_update() succeeded unexpectedly; offset=%jd.",
		    (intmax_t) offset);
		goto done;
	}

	if ((error = elf_errno()) != ELF_E_LAYOUT) {
		TP_FAIL("elf_update() did not fail with ELF_E_LAYOUT; "
		    "error=%d \"%s\".", error, elf_errmsg(error));
		goto done;
	}

	result = TET_PASS;

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}
')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')
