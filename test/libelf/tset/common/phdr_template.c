/*-
 * Copyright (c) 2006 Joseph Koshy
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
 * $Id: phdr_template.c 3174 2015-03-27 17:13:41Z emaste $
 */

/*
 * Boilerplate for testing the *_getphdr and *_newphdr APIs.
 *
 * This template is to be used as follows:
 *
 * #define	TS_PHDRFUNC		_getphdr	(or _newphdr)
 * #define	TS_PHDRSZ		32	(or 64)
 * #include "phdr_template.c"
 */

/* Variations of __CONCAT and __STRING which expand their arguments. */
#define __XCONCAT(x,y)	__CONCAT(x,y)
#ifndef __XSTRING
#define __XSTRING(x)	__STRING(x)
#endif

#define	TS_ICFUNC	__XCONCAT(elf,__XCONCAT(TS_PHDRSZ,TS_PHDRFUNC))
#define	TS_PHDR		__XCONCAT(Elf,__XCONCAT(TS_PHDRSZ,_Phdr))
#define	TS_ICNAME	__XSTRING(TS_ICFUNC)
#define	TS_ELFCLASS	__XCONCAT(ELFCLASS,TS_PHDRSZ)

#define	TS_GETEHDR	__XCONCAT(elf,__XCONCAT(TS_PHDRSZ,_getehdr))
#define	TS_EHDR		__XCONCAT(Elf,__XCONCAT(TS_PHDRSZ,_Ehdr))

#define	TS_NPHDR	3	/* should match "phdr.yaml" */

IC_REQUIRES_VERSION_INIT();

/*
 * Reference data for the contents of an Phdr structure. The values
 * here must match that in the "phdr.yaml" file.
 */

static TS_PHDR phdr_testdata[TS_NPHDR] = {
	{
		.p_type = PT_NULL,
		.p_offset = 1,
		.p_vaddr = 2,
		.p_paddr = 3,
		.p_filesz = 4,
		.p_memsz = 5,
		.p_flags = PF_X,
		.p_align = 1
	},
	{
		.p_type = PT_NULL,
		.p_offset = 6,
		.p_vaddr = 7,
		.p_paddr = 8,
		.p_filesz = 9,
		.p_memsz = 10,
		.p_flags = PF_R,
		.p_align = 4
	},
	{
		.p_type = PT_INTERP,
		.p_offset = 11,
		.p_vaddr = 12,
		.p_paddr = 13,
		.p_filesz = 14,
		.p_memsz = 15,
		.p_flags = PF_W,
		.p_align = 8
	}
};

static int
check_phdr(TS_PHDR *p)
{
	int i, result;
	TS_PHDR *pt;

	result = TET_PASS;
	for (pt = phdr_testdata, i = 0; i < TS_NPHDR; i++) {

#define	CHECK_PH_FIELD(FIELD)	do {					\
		if (p->p_##FIELD != pt->p_##FIELD) {			\
			tet_printf("fail: [%d] field \"%s\" actual "	\
			    "0x%jx != expected 0x%jx.", i, #FIELD, 	\
			    (uintmax_t) p->p_##FIELD,			\
			    (uintmax_t) pt->p_##FIELD);			\
			result = TET_FAIL;				\
		} \
	} while (0)

		CHECK_PH_FIELD(type);
		CHECK_PH_FIELD(offset);
		CHECK_PH_FIELD(vaddr);
		CHECK_PH_FIELD(paddr);
		CHECK_PH_FIELD(filesz);
		CHECK_PH_FIELD(memsz);
		CHECK_PH_FIELD(flags);
		CHECK_PH_FIELD(align);

		if (result != TET_PASS)
			return (result);
	}

	return (result);
}

void
tcNull_tpGet(void)
{
	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "(NULL) fails with "
	    "ELF_E_ARGUMENT.");

	if (TS_ICFUNC(NULL) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}

static char data[] = "This isn't an ELF file.";

void
tcData_tpElf(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "(E) for non-ELF (E) fails with "
	    "ELF_E_ARGUMENT.");

	TS_OPEN_MEMORY(e, data);

	if (TS_ICFUNC(e) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}


/*
 * A malformed (too short) ELF header.
 */

static char badelftemplate[EI_NIDENT+1] = {
	[EI_MAG0] = '\177',
	[EI_MAG1] = 'E',
	[EI_MAG2] = 'L',
	[EI_MAG3] = 'F',
	[EI_CLASS] = ELFCLASS64,
	[EI_DATA]  = ELFDATA2MSB,
	[EI_NIDENT] = '@'
};

void
tcBadElfVersion_tpElf(void)
{
	int err;
	Elf *e;
	TS_PHDR *ph;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "() with an unsupported version "
	    "fails with ELF_E_VERSION.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));

	badelf[EI_VERSION] = EV_NONE;
	badelf[EI_CLASS] = TS_ELFCLASS;

	TS_OPEN_MEMORY(e, badelf);

	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (err = elf_errno()) != ELF_E_VERSION) {
		tet_printf("fail: error=%d ph=%p.", err, (void *) ph);
		tet_result(TET_FAIL);
	} else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

void
tcBadElf_tpElf(void)
{
	int err;
	Elf *e;
	TS_PHDR *ph;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "() on a malformed ELF file "
	    "fails with ELF_E_HEADER.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));
	badelf[EI_VERSION] = EV_CURRENT;
	badelf[EI_CLASS]   = TS_ELFCLASS;

	TS_OPEN_MEMORY(e, badelf);

	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (err = elf_errno()) != ELF_E_HEADER) {
		tet_printf("fail: error=%d ph=%p.", err, (void *) ph);
		tet_result(TET_FAIL);
	} else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

void
tcElf_tpCorruptEhdr(void)
{
	int err, fd, result;
	char *fn;
	Elf *e;
	TS_PHDR *ph;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "(E) with corrupt phdr values "
	    "the header returns E_HEADER.");

	fn = "ehdr.msb" __XSTRING(TS_PHDRSZ);
	TS_OPEN_FILE(e, fn, ELF_C_READ, fd);

	result = TET_PASS;

	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (err = (elf_errno() != ELF_E_HEADER))) {
		tet_printf("fail: \"%s\" (ph %p, error %d)", fn, (void *) ph,
		    err);
		result = TET_FAIL;
	}
	(void) elf_end(e);
	(void) close(fd);

	if (result != TET_PASS) {
		tet_result(result);
		return;
	}

	fn = "ehdr.lsb" __XSTRING(TS_PHDRSZ);
	TS_OPEN_FILE(e, fn, ELF_C_READ, fd);

	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (err = (elf_errno() != ELF_E_HEADER))) {
		tet_printf("fail: \"%s\" (ph %p, error %d)", fn, (void *) ph,
		    err);
		result = TET_FAIL;
	}
	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}

void
tcElf_tpElfLSB(void)
{
	int fd;
	Elf *e;
	TS_PHDR *ph;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: " TS_ICNAME "(E) returns the correct LSB phdr.");

	TS_OPEN_FILE(e,"phdr.lsb" __XSTRING(TS_PHDRSZ),ELF_C_READ,fd);

	if ((ph = TS_ICFUNC(e)) == NULL) {
		tet_infoline("fail: " TS_ICNAME "() failed.");
		tet_result(TET_FAIL);
		return;
	}

	tet_result(check_phdr(ph));

	(void) elf_end(e);
	(void) close(fd);
}

void
tcElf_tpElfMSB(void)
{
	int fd;
	Elf *e;
	TS_PHDR *ph;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion:" TS_ICNAME "(E) returns the correct MSB phdr.");

	TS_OPEN_FILE(e,"phdr.msb" __XSTRING(TS_PHDRSZ),ELF_C_READ,fd);

	if ((ph = TS_ICFUNC(e)) == NULL) {
		tet_infoline("fail: " TS_ICNAME "() failed.");
		tet_result(TET_FAIL);
		return;
	}

	tet_result(check_phdr(ph));

	(void) elf_end(e);
	(void) close(fd);
}

void
tcElf_tpElfDup(void)
{
	int fd;
	Elf *e;
	TS_PHDR *ph1, *ph2;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: successful calls to " TS_ICNAME "() return "
	    "identical pointers.");

	TS_OPEN_FILE(e,"phdr.msb" __XSTRING(TS_PHDRSZ),ELF_C_READ,fd);

	if ((ph1 = TS_ICFUNC(e)) == NULL ||
	    (ph2 = TS_ICFUNC(e)) == NULL) {
		tet_infoline("unresolved: " TS_ICNAME "() failed.");
		tet_result(TET_UNRESOLVED);
		return;
	}

	tet_result(ph1 == ph2 ? TET_PASS : TET_FAIL);

	(void) elf_end(e);
	(void) close(fd);
}

#if	TS_PHDRSZ == 32
#define	TS_OTHERSIZE	64
#else
#define	TS_OTHERSIZE	32
#endif

void
tcElf_tpElfWrongSize(void)
{
	int error, fd, result;
	Elf *e;
	char *fn;
	TS_PHDR *ph;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: a call to " TS_ICNAME "() and a mismatched "
	    "ELF class fails with ELF_E_CLASS.");

	result = TET_PASS;

	fn = "phdr.msb" __XSTRING(TS_OTHERSIZE);
	TS_OPEN_FILE(e,fn,ELF_C_READ,fd);

	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (error = elf_errno()) != ELF_E_CLASS) {
		tet_printf("fail: \"%s\" opened (error %d).", fn, error);
		result = TET_FAIL;
	}

	(void) elf_end(e);
	(void) close(fd);

	if (result != TET_PASS) {
		tet_result(result);
		return;
	}

	fn = "phdr.lsb" __XSTRING(TS_OTHERSIZE);
	TS_OPEN_FILE(e,fn,ELF_C_READ,fd);
	if ((ph = TS_ICFUNC(e)) != NULL ||
	    (error = elf_errno()) != ELF_E_CLASS) {
		tet_printf("fail: \"%s\" opened (error %d).", fn, error);
		result = TET_FAIL;
	}

	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
}
