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
 * $Id: getshdr.m4 223 2008-08-10 15:40:06Z jkoshy $
 */

/*
 * TP_NULL(CLASS)
 *
 * Check that a NULL argument returns ELF_E_ARGUMENT.
 */

define(`TP_NULL',`
void
tcNull_tpNull$1(void)
{
	int error, result;
	Elf$1_Shdr *sh;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf$1_getshdr(NULL) fails with ELF_E_ARGUMENT.");

	result = TET_PASS;
	if ((sh = elf$1_getshdr(NULL)) != NULL ||
	   (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("sh=%p error=%d \"%s\".", (void *) sh,
		    error, elf_errmsg(error));

	tet_result(result);
}')

/* TP_CHECK_FIELD(I, SH,REF,FIELD) */
define(`TP_CHECK_FIELD',`do {
		if ($2->$4 != $3->$4) {
			TP_FAIL("field[%d] \"$4\" %jd != ref %jd.", $1,
			    (uintmax_t) $2->$4, (uintmax_t) $3->$4);
			goto done;
		}
	} while (0)')
/* TP_CHECK_SHDR(IND, SH, REF) */
define(`TP_CHECK_SHDR',`do {
		TP_CHECK_FIELD($1,$2,$3,sh_name);
		TP_CHECK_FIELD($1,$2,$3,sh_type);
		TP_CHECK_FIELD($1,$2,$3,sh_flags);
		TP_CHECK_FIELD($1,$2,$3,sh_addr);
		TP_CHECK_FIELD($1,$2,$3,sh_offset);
		TP_CHECK_FIELD($1,$2,$3,sh_size);
		TP_CHECK_FIELD($1,$2,$3,sh_link);
		TP_CHECK_FIELD($1,$2,$3,sh_info);
		TP_CHECK_FIELD($1,$2,$3,sh_addralign);
		TP_CHECK_FIELD($1,$2,$3,sh_entsize);
	} while (0)')

/*
 * TC_MAKE_REF_SHDR(CLASS)
 *
 * This must match the values in "shdr.yaml".
 */
define(`TC_MAKE_REF_SHDR',`
static Elf$1_Shdr RefShdr$1[] = {
	/* index 0 */
	{ .sh_type = SHT_NULL },
	/* index 1 : .shstrtab */
	{ .sh_name = 1, .sh_type = SHT_STRTAB, .sh_flags = SHF_ALLOC | SHF_STRINGS,
	  .sh_offset = 256, .sh_link = ~0, .sh_info = ~0, .sh_addralign = 1,
	  .sh_entsize = 0, .sh_size = 38 },
	/* index 2 : SHT_PROGBITS */
	{ .sh_name = 11, .sh_type = SHT_PROGBITS, .sh_flags = SHF_ALLOC, .sh_offset = 128,
	  .sh_link = 0xdeadc0de, .sh_info = 0xcafebabe, .sh_addralign = 8,
	  .sh_entsize = 0 }
};

#define	NSHDR	(sizeof(RefShdr$1)/sizeof(RefShdr$1[0]))
')

/*
 * TP_SHDR(CLASS,ENDIANNESS)
 *
 * Check that the Shdrs returned are valid.
 */

define(`TP_SHDR',`
void
tcShdr_tpValid$1`'TOUPPER($2)(void)
{
	int i, fd;
	Elf *e;
	Elf$1_Shdr *sh, *rs;
	Elf_Scn *scn;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: Check shdr contents.");

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	_TS_OPEN_FILE(e, "shdr.$2$1", ELF_C_READ, fd, goto done;);

	i = SHN_UNDEF;
	rs = RefShdr$1;

	if ((scn = elf_getscn(e, i)) == NULL) {
		TP_UNRESOLVED("elf_getscn() failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((sh = elf$1_getshdr(scn)) == NULL) {
		TP_FAIL("elf$1_getshdr() failed: \"%s\".",
		     elf_errmsg(-1));
		goto done;
	}

	TP_CHECK_SHDR(i, sh, rs);

	while ((scn = elf_nextscn(e, scn)) != NULL) {

		i++; rs++;

		if (i >= NSHDR) {
			TP_UNRESOLVED("Too many (%d) sections.", i);
			goto done;
		}

		if ((sh = elf$1_getshdr(scn)) == NULL) {
			TP_FAIL("elf$1_getshdr() failed: \"%s\".",
			    elf_errmsg(-1));
			goto done;
		}

		TP_CHECK_SHDR(i, sh, rs);

	}

	result = TET_PASS;
	if ((error = elf_errno()) != ELF_E_NONE)
		TP_UNRESOLVED("error=%d \"%s\".", error, elf_errmsg(error));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}')
