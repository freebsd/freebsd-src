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
 * $Id: ehdr_template.m4 3174 2015-03-27 17:13:41Z emaste $
 */

include(`elfts.m4')

/*
 * Boilerplate for testing the *_getehdr and *_newehdr APIs.
 *
 * This template is to be used as follows:
 *
 * `define(`TS_EHDRFUNC',`_getehdr')'	(or `_newehdr')
 * `define(`TS_EHDRSZ',`32')'		(or `64')
 * `include(`ehdr_template.m4')'
 */

ifdef(`TS_EHDRFUNC',`',`errprint(`TS_EHDRFUNC was not defined')m4exit(1)')
ifdef(`TS_EHDRSZ',`',`errprint(`TS_EHDRSZ was not defined')m4exit(1)')
define(`TS_OTHERSIZE',`ifelse(TS_EHDRSZ,32,64,32)')

define(`TS_ICFUNC',`elf'TS_EHDRSZ`'TS_EHDRFUNC)
define(`TS_EHDR',`Elf'TS_EHDRSZ`_Ehdr')
define(`TS_ICNAME',TS_ICFUNC)
define(`TS_ELFCLASS',`ELFCLASS'TS_EHDRSZ)

IC_REQUIRES_VERSION_INIT();

/*
 * Checks for the contents of an Ehdr structure. The values here must
 * match that in the "ehdr.yaml" file in the test case directory.
 */

#define	CHECK_SIGFIELD(E,I,V)	do {					\
		if ((E)->e_ident[EI_##I] != (V)) 			\
			TP_FAIL(#I " value 0x%x != "			\
			    "expected 0x%x.", (E)->e_ident[EI_##I],	\
			    (V));					\
	} while (0)

#define	CHECK_SIG(E,ED,EC,EV,EABI,EABIVER)		do {		\
		if ((E)->e_ident[EI_MAG0] != ELFMAG0 ||			\
		    (E)->e_ident[EI_MAG1] != ELFMAG1 ||			\
		    (E)->e_ident[EI_MAG2] != ELFMAG2 ||			\
		    (E)->e_ident[EI_MAG3] != ELFMAG3) 			\
			TP_FAIL("incorrect ELF signature "		\
			    "(%x %x %x %x).", (E)->e_ident[EI_MAG0],	\
			    (E)->e_ident[EI_MAG1], (E)->e_ident[EI_MAG2],\
			    (E)->e_ident[EI_MAG3]);			\
		CHECK_SIGFIELD(E,CLASS,	EC);				\
		CHECK_SIGFIELD(E,DATA,	ED);				\
		CHECK_SIGFIELD(E,VERSION, EV);				\
		CHECK_SIGFIELD(E,OSABI,	EABI);				\
		CHECK_SIGFIELD(E,ABIVERSION, EABIVER);			\
	} while (0)


#define	CHECK_FIELD(E,FIELD,VALUE)	do {				\
		if ((E)->e_##FIELD != (VALUE)) 				\
			TP_FAIL("field \"%s\" actual 0x%jx "		\
			    "!= expected 0x%jx.", #FIELD, 		\
			    (uintmax_t) (E)->e_##FIELD,			\
			    (uintmax_t) (VALUE));			\
	} while (0)

#define	CHECK_EHDR(E,ED,EC)	do {		\
		CHECK_SIG(E,ED,EC,EV_CURRENT,ELFOSABI_FREEBSD,1);	\
		CHECK_FIELD(E,type,	ET_REL);	\
		CHECK_FIELD(E,machine,	0x42);		\
		CHECK_FIELD(E,version,	EV_CURRENT);	\
		CHECK_FIELD(E,entry,	0xF0F0F0F0);	\
		CHECK_FIELD(E,phoff,	0x0E0E0E0E);	\
		CHECK_FIELD(E,shoff,	0xD0D0D0D0);	\
		CHECK_FIELD(E,flags,	64+8+2+1);	\
		CHECK_FIELD(E,ehsize,	0x0A0A);	\
		CHECK_FIELD(E,phentsize,0xB0B0);	\
		CHECK_FIELD(E,phnum,	0x0C0C);	\
		CHECK_FIELD(E,shentsize,0xD0D0);	\
		CHECK_FIELD(E,shnum,	0x0E0E);	\
		CHECK_FIELD(E,shstrndx,	0xF0F0);	\
	} while (0)

/*
 * Check behaviour when passed a NULL argument.
 */

void
tcNullArgument(void)
{
	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'(NULL) fails with ELF_E_ARGUMENT.");

	if (TS_ICFUNC`'(NULL) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}

/*
 * Check behaviour when passed a pointer to a non-ELF object.
 */

static char data[] = "This isn't an ELF file.";

void
tcNonElfData(void)
{
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'(non-ELF) fails with ELF_E_ARGUMENT.");

	TS_OPEN_MEMORY(e, data);

	if (TS_ICFUNC`'(e) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}


/*
 * Check behaviour when an object with a malformed ELF header.
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

/*
 * Verify that the version number is checked before other kinds
 * of errors.
 */

void
tcBadElfVersion(void)
{
	int err, result;
	Elf *e;
	TS_EHDR *eh;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'() with an unsupported version "
	    "fails with ELF_E_VERSION.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));

	badelf[EI_VERSION] = EV_NONE;
	badelf[EI_CLASS] = TS_ELFCLASS;

	TS_OPEN_MEMORY(e, badelf);

	result = TET_PASS;

	if ((eh = TS_ICFUNC`'(e)) != NULL ||
	    (err = elf_errno()) != ELF_E_VERSION)
		TP_FAIL("error=%d eh=%p.", err, (void *) eh);

	(void) elf_end(e);

	tet_result(result);
}

void
tcBadElf(void)
{
	int err, result;
	Elf *e;
	TS_EHDR *eh;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'() on a malformed ELF file "
	    "fails with ELF_E_HEADER.");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));
	badelf[EI_VERSION] = EV_CURRENT;
	badelf[EI_CLASS]   = TS_ELFCLASS;

	TS_OPEN_MEMORY(e, badelf);

	result = TET_PASS;
	if ((eh = TS_ICFUNC`'(e)) != NULL ||
	    (err = elf_errno()) != ELF_E_HEADER)
		TP_FAIL("error=%d eh=%p.", err, (void *) eh);

	(void) elf_end(e);

	tet_result(result);
}

/*
 * Verify non-NULL return for a legal ELF object.
 */

undefine(`FN')
define(`FN',`
void
tcValidElfNonNull$1(void)
{
	int fd;
	Elf *e;
	TS_EHDR *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'($1) on valid EHDR returns non-NULL.");

	TS_OPEN_FILE(e,"ehdr.TOLOWER($1)`'TS_EHDRSZ",ELF_C_READ,fd);

	if ((eh = TS_ICFUNC`'(e)) == NULL)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
	(void) close(fd);
}')

FN(`LSB')
FN(`MSB')

/*
 * Verify accuracy of the return header.
 */

define(`FN',`
void
tcValidElf$1(void)
{
	int fd, result;
	Elf *e;
	TS_EHDR *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'($1) returns the correct $1 ehdr.");

	TS_OPEN_FILE(e,"ehdr.TOLOWER($1)`'TS_EHDRSZ",ELF_C_READ,fd);

	if ((eh = TS_ICFUNC`'(e)) == NULL) {
		TP_UNRESOLVED("TS_ICNAME`'() failed.");
		goto done;
	}

	result = TET_PASS;

	CHECK_EHDR(eh, ELFDATA2$1, TS_ELFCLASS);

done:
	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);

}')

FN(`LSB')
FN(`MSB')

/*
 * Verify duplicate handling.
 */

undefine(`FN')
define(`FN',`
void
tcElfDup$1(void)
{
	int fd, result;
	Elf *e;
	TS_EHDR *eh1, *eh2;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("Successful calls to TS_ICNAME`'($1) return "
	    "identical pointers.");

	TS_OPEN_FILE(e,"ehdr.TOLOWER($1)`'TS_EHDRSZ",ELF_C_READ,fd);

	if ((eh1 = TS_ICFUNC`'(e)) == NULL ||
	    (eh2 = TS_ICFUNC`'(e)) == NULL) {
		TP_UNRESOLVED("TS_ICNAME`'() failed.");
		tet_result(result);
		return;
	}

	tet_result(eh1 == eh2 ? TET_PASS : TET_FAIL);

	(void) elf_end(e);
	(void) close(fd);
}')

FN(`LSB')
FN(`MSB')

/*
 * Verify the error reported for incorrectly sized ELF objects.
 */

undefine(`FN')
define(`FN',`
void
tcElfWrongSize$1(void)
{
	int error, fd, result;
	Elf *e;
	char *fn;
	TS_EHDR *eh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TS_ICNAME`'($1.TS_OTHERSIZE) fails with ELF_E_CLASS.");

	result = TET_PASS;

	fn = "ehdr.TOLOWER($1)`'TS_OTHERSIZE";
	TS_OPEN_FILE(e,fn,ELF_C_READ,fd);
	if ((eh = TS_ICFUNC`'(e)) != NULL ||
	    (error = elf_errno()) != ELF_E_CLASS)
		TP_FAIL("\"%s\" opened (error %d).", fn, error);

	(void) elf_end(e);
	(void) close(fd);

	tet_result(result);
	
}')

FN(`LSB')
FN(`MSB')
