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
 * $Id: fill.m4 1696 2011-08-06 09:12:19Z jkoshy $
 */

#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

#include "elfts.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

define(`TS_ALIGN', 512)
define(`TS_FILLCHAR', `0xAB')
define(`TS_OFFSET_1', 512)
define(`TS_OFFSET_2', 1024)
define(`TS_SHDR_OFFSET', 2048)

/*
 * Test the `elf_fill' entry point.
 */

static char testdata[] = {
	0xAA, 0xBB, 0xCC, 0xDD,
	0xEE, 0xFF, 0x99, 0x88
};

/*
 * Check that gaps in the file are correctly filled with the
 * specified fill character.
 */
undefine(`FN')
define(`FN',`
void
tcDefaultLayout$2$1(void)
{
	Elf *e;
	Elf$1_Ehdr *eh;
	Elf$1_Shdr *sh;
	Elf_Scn *scn;
	Elf_Data *d;
	int fd, result;
	size_t fsz;
	off_t rc;
	unsigned char *p, *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("$2$1: elf_fill()/lib-layout fills gaps.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	r = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	/*
 	 * We create an ELF file with a section with a 1K and 2K alignments
	 * and verify that the gaps are filled appropriately.
	*/

	elf_fill(TS_FILLCHAR);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2$2;

	/*
	 * Create the first section.
	 */
	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_scn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type  = SHT_PROGBITS;

	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	d->d_align = TS_ALIGN;
	d->d_buf = testdata;
	d->d_size = sizeof(testdata);
	d->d_type = ELF_T_BYTE;

	/*
 	 * Create the second section.
	 */

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_scn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type  = SHT_PROGBITS;

	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	d->d_align = TS_ALIGN;
	d->d_buf = testdata;
	d->d_size = sizeof(testdata);
	d->d_type = ELF_T_BYTE;

	if ((rc = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	(void) elf_end(e); e = NULL;
	(void) close(fd); fd = -1;

	/*
	 * Mmap() in the file and check that the contents match.
	 */

	if ((fd = open(TS_NEWFILE, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open() failed: %s.", strerror(errno));
		goto done;
	}

	if ((r = mmap(NULL, (size_t) rc, PROT_READ, MAP_SHARED, fd,
	    (off_t) 0)) == MAP_FAILED) {
		TP_UNRESOLVED("mmap() failed: %s.", strerror(errno));
		goto done;
	}

	/* Check the first gap */
	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("elf$1_fsize(ELF_T_EHDR) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/* Section data would be placed at the next alignment boundary. */
	for (p = r + fsz; p < r + TS_ALIGN; p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	/* Check whether valid contents exist at first section offset. */
	if (memcmp(r + TS_ALIGN, testdata, sizeof(testdata)) != 0) {
		TP_FAIL("memcmp(first) failed.");
		goto done;
	}

	/* Check the between sections.  */
	for (p = r + TS_ALIGN + sizeof(testdata); p < r + 2*TS_ALIGN; p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	/* Check whether valid contents exist at second section offset. */
	if (memcmp(r + 2*TS_ALIGN, testdata, sizeof(testdata)) != 0) {
		TP_FAIL("memcmp(second) failed.");
		goto done;
	}

	result = TET_PASS;
 done:
	if (r)
		(void) munmap(r, rc);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	unlink(TS_NEWFILE);
	tet_result(result);
}')

FN(32,LSB)
FN(32,MSB)
FN(64,LSB)
FN(64,MSB)


/*
 * Check that regions are filled correctly, for application specified
 * layouts.
 */
define(`FN',`
void
tcAppLayout$2$1(void)
{
	Elf *e;
	Elf$1_Ehdr *eh;
	Elf$1_Shdr *sh;
	Elf_Scn *scn;
	Elf_Data *d;
	int fd, result;
	size_t fsz;
	off_t rc;
	unsigned char *p, *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("$2$1: elf_fill()/app-layout fills gaps.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	r = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	/*
 	 * We create an ELF file with a section with a 1K and 2K alignments
	 * and verify that the gaps are filled appropriately.
	*/

	elf_fill(TS_FILLCHAR);
	if (elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT) != ELF_F_LAYOUT) {
		TP_UNRESOLVED("elf_flagdata() failed: \"%s\".",
		   elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2$2;

	/*
	 * Create the first section.
	 */
	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type  = SHT_PROGBITS;
	sh->sh_offset = TS_OFFSET_2;
	sh->sh_addralign = 1;
	sh->sh_size = sizeof(testdata);

	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);
	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	d->d_align = 1;
	d->d_off = 0;
	d->d_buf = testdata;
	d->d_size = sizeof(testdata);
	d->d_type = ELF_T_BYTE;

	/*
 	 * Create the second section, physically located BEFORE the
	 * first section.
	 */
	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_scn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_UNRESOLVED("elf$1_getshdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	sh->sh_type  = SHT_PROGBITS;
	sh->sh_offset = TS_OFFSET_1;
	sh->sh_addralign = 1;
	sh->sh_size = sizeof(testdata);

	(void) elf_flagshdr(scn, ELF_C_SET, ELF_F_DIRTY);

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	d->d_align = 1;
	d->d_off = 0;
	d->d_buf = testdata;
	d->d_size = sizeof(testdata);
	d->d_type = ELF_T_BYTE;

	/*
	 * Position the section header after section data.
	 */
	eh->e_shoff = TS_SHDR_OFFSET;

	if ((rc = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	(void) elf_end(e); e = NULL;
	(void) close(fd); fd = -1;

	/*
	 * Mmap() in the file and check that the contents match.
	 */

	if ((fd = open(TS_NEWFILE, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open() failed: %s.", strerror(errno));
		goto done;
	}

	if ((r = mmap(NULL, (size_t) rc, PROT_READ, MAP_SHARED, fd,
	    (off_t) 0)) == MAP_FAILED) {
		TP_UNRESOLVED("mmap() failed: %s.", strerror(errno));
		goto done;
	}

	/* Check the first gap */
	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("elf$1_fsize(ELF_T_EHDR) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	for (p = r + fsz; p < r + TS_OFFSET_1; p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	/* Check whether valid contents exist at first section offset. */
	if (memcmp(r + TS_OFFSET_1, testdata, sizeof(testdata)) != 0) {
		TP_FAIL("memcmp(first) failed.");
		goto done;
	}

	/* Check the second gap. */
	for (p = r + TS_OFFSET_1 + sizeof(testdata); p < r + TS_OFFSET_2; p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	/* Check whether valid contents exist at second section offset. */
	if (memcmp(r + TS_OFFSET_2, testdata, sizeof(testdata)) != 0) {
		TP_FAIL("memcmp(second) failed.");
		goto done;
	}

	/* Check the gap till the shdr table. */
	for (p = r + TS_OFFSET_2 + sizeof(testdata);
	     p < r + TS_SHDR_OFFSET;
	     p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	result = TET_PASS;

 done:
	if (r)
		(void) munmap(r, rc);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	unlink(TS_NEWFILE);
	tet_result(result);
}')

FN(32,LSB)
FN(32,MSB)
FN(64,LSB)
FN(64,MSB)

/*
 * Check that the region between the Ehdr and Phdr is filled correctly,
 * when the application specifies the file layout.
 */
define(`FN',`
void
tcAppLayoutEhdrPhdrGap$2$1(void)
{
	Elf *e;
	Elf$1_Ehdr *eh;
	Elf$1_Phdr *ph;
	int fd, result;
	size_t fsz;
	off_t rc;
	unsigned char *p, *r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("$2$1: elf_fill()/app-layout fills gaps.");

	result = TET_UNRESOLVED;
	e = NULL;
	fd = -1;
	r = NULL;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	/*
	 * We create an ELF file with the PHDR placed an offset away
	 * from the EHDR.
	 */
	elf_fill(TS_FILLCHAR);
	if (elf_flagelf(e, ELF_C_SET, ELF_F_LAYOUT) != ELF_F_LAYOUT) {
		TP_UNRESOLVED("elf_flagdata() failed: \"%s\".",
		   elf_errmsg(-1));
		goto done;
	}

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	eh->e_ident[EI_DATA] = ELFDATA2$2;

	/*
	 * Create the PHDR.
	 */
	if ((ph = elf$1_newphdr(e, 1)) == NULL) {
		TP_UNRESOLVED("elf_newphdr() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	/*
	 * Position the PHDR.
	 */
	eh->e_phoff = TS_OFFSET_1;

	/*
	 * Update the ELF object.
	 */
	if ((rc = elf_update(e, ELF_C_WRITE)) < 0) {
		TP_UNRESOLVED("elf_update() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	(void) elf_end(e); e = NULL;
	(void) close(fd); fd = -1;

	/*
	 * Mmap() in the file and check that the contents match.
	 */

	if ((fd = open(TS_NEWFILE, O_RDONLY, 0)) < 0) {
		TP_UNRESOLVED("open() failed: %s.", strerror(errno));
		goto done;
	}

	if ((r = mmap(NULL, (size_t) rc, PROT_READ, MAP_SHARED, fd,
	    (off_t) 0)) == MAP_FAILED) {
		TP_UNRESOLVED("mmap() failed: %s.", strerror(errno));
		goto done;
	}

	/* Check the gap between the EHDR and the PHDR. */
	if ((fsz = elf$1_fsize(ELF_T_EHDR, 1, EV_CURRENT)) == 0) {
		TP_UNRESOLVED("elf$1_fsize(ELF_T_EHDR) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	for (p = r + fsz; p < r + TS_OFFSET_1; p++)
		if (*p != TS_FILLCHAR) {
			TP_FAIL("offset 0x%x [%d] != %d", p - r, *p,
			    TS_FILLCHAR);
			goto done;
		}

	result = TET_PASS;

 done:
	if (r)
		(void) munmap(r, rc);
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	unlink(TS_NEWFILE);
	tet_result(result);
}')

FN(32,LSB)
FN(32,MSB)
FN(64,LSB)
FN(64,MSB)
