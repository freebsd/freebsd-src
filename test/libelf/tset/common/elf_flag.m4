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
 * $Id: elf_flag.m4 1412 2011-02-05 10:09:22Z jkoshy $
 */

/*
 * M4 macros for use with the elf_flag*() APIs.
 */

divert(-1)

define(`_TP_FLAG_FN',`
void
$1(void)
{
	int result;
$2
$3
$4
	tet_result(result);
}')

define(`TP_FLAG_NULL',`_TP_FLAG_FN(`tcArgsNull',`
	int error, ret;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("A NULL first parameter returns zero.");',`
	result = TET_PASS;
	if ((ret = $1(NULL, ELF_C_SET, ELF_F_DIRTY)) != 0 ||
	    (error = elf_errno()) != ELF_E_NONE)
		TP_FAIL("ret=%d, error=%d \"%s\".", ret, error,
		    elf_errmsg(error));',`')')

/*
 * TP_FLAG_ILLEGAL_CMD(FN,ARG)
 *
 * Check that illegal `cmd' values are rejected.
 */
define(`TP_FLAG_ILLEGAL_CMD',`_TP_FLAG_FN(`tcArgsIllegalCmd',`
	int error, ret;
	Elf_Cmd cmd;
	_TP_DECLARATIONS

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("Illegal CMD values are rejected.");

	_TP_PROLOGUE',`
	result = TET_PASS;
	for (cmd = ELF_C_NULL-1; cmd <= ELF_C_NUM; cmd++) {
		if (cmd == ELF_C_CLR || cmd == ELF_C_SET)
			continue;
		if ((ret = $1($2, ELF_C_NUM, ELF_F_DIRTY)) != 0 ||
		    (error = elf_errno()) != ELF_E_ARGUMENT) {
			TP_FAIL("cmd=%d ret=%d, error=%d \"%s\".", cmd, ret,
			    error, elf_errmsg(error));
			goto done;
		}
	}',`_TP_EPILOGUE')')

/*
 * TP_FLAG_SET(FN,ARG)
 *
 * Check that an ELF_C_SET works.
 */
define(`TP_FLAG_SET',`_TP_FLAG_FN(`tcArgsSet',`
	int error, flag;
	_TP_DECLARATIONS

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("ELF_C_SET works correctly.");

	_TP_PROLOGUE',`
	result = TET_PASS;

	if ((flag = $1($2, ELF_C_SET, ELF_F_DIRTY)) != ELF_F_DIRTY) {
		error = elf_errno();
		TP_FAIL("flag=0x%x, expected 0x%x, error=%d \"%s\".", flag,
		    ELF_F_DIRTY, error, elf_errmsg(error));
		goto done;
	}',`_TP_EPILOGUE')')

/*
 * TP_FLAG_CLR(FN,ARG)
 *
 * Check that an ELF_C_CLR works.
 */
define(`TP_FLAG_CLR',`_TP_FLAG_FN(`tcArgsClr',`
	int error, flag;
	_TP_DECLARATIONS

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("ELF_C_CLR works correctly.");

	_TP_PROLOGUE',`
	result = TET_PASS;

	(void) $1($2, ELF_C_SET, ELF_F_DIRTY);
	if ((flag = $1($2, ELF_C_CLR, ELF_F_DIRTY)) != 0) {
		error = elf_errno();
		TP_FAIL("flag=0x%x, error=%d \"%s\".", flag, error,
		    elf_errmsg(error));
		goto done;
	}',`_TP_EPILOGUE')')

/*
 * TP_FLAG_ILLEGAL_CMD(FN, ARG, LEGALFLAGS)
 *
 * Check that all flag values other than those in LEGALFLAGS are
 * rejected with ELF_E_ARGUMENT.
 */
define(`TP_FLAG_ILLEGAL_FLAG',`_TP_FLAG_FN(`tcArgsIllegalFlags',`
	int error, ret;
	unsigned int flags;

	_TP_DECLARATIONS

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("Illegal flag values are rejected.");

	_TP_PROLOGUE',`
	result = TET_PASS;
	for (flags = 0x1; flags; flags <<= 1) {
		if (flags & ($3))
			continue;
		if ((ret = $1($2, ELF_C_SET, flags)) != 0 ||
		    (error = elf_errno()) != ELF_E_ARGUMENT) {
			TP_FAIL("ret=%d, error=%d \"%s\".", ret, error,
			    elf_errmsg(error));
			goto done;
		}
	}',`_TP_EPILOGUE')')

/*
 * TP_FLAG_NON_ELF(FN,ARG)
 *
 * Check that a non-elf file is rejected.
 */
define(`TP_FLAG_NON_ELF',`
char *rawdata = "This is not an ELF file.";
_TP_FLAG_FN(`tcArgsNonElf',`
	int error, ret;
	Elf *e;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("Non-ELF files are rejected.");

	TS_OPEN_MEMORY(e, rawdata);',`
	result = TET_PASS;
	if ((ret = $1(e, ELF_C_SET, ELF_F_DIRTY)) != 0 ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("ret=%d, error=%d \"%s\".", ret, error,
		    elf_errmsg(error));
	}',`')')

divert(0)
