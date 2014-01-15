pushdef(`_DIVNUM',divnum)divert(-1)
/*-
 * Copyright (c) 2006,2010 Joseph Koshy
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
 * $Id: elfts.m4 2844 2012-12-31 03:30:20Z jkoshy $
 */

dnl `mkstemp' is a built-in in GNU m4.
ifdef(`mkstemp',`undefine(`mkstemp')')

/*
 * Macros for use with `m4'.
 */

/*
 * TOUPPER/TOLOWER: Convert $1 to upper-case or lower-case respectively.
 * CAPITALIZE: Make $1 capitalized.
 */

define(`TOUPPER',`translit($1,`abcdefghijklmnopqrstuvwxyz',`ABCDEFGHIJKLMNOPQRSTUVWXYZ')')
define(`TOLOWER',`translit($1,`ABCDEFGHIJKLMNOPQRSTUVWXYZ',`abcdefghijklmnopqrstuvwxyz')')
define(`CAPITALIZE',`TOUPPER(substr($1,0,1))`'TOLOWER(substr($1,1))')

/*
 * TP_ANNOUNCE: Announce a test purpose.
 * Usage:
 *	TP_ANNOUNCE()	--  Announce the function name.
 *      TP_ANNOUNCE(fmt, args...) -- Announce the function and print arguments.
 */
define(`TP_ANNOUNCE',`TP_FUNCTION();
	tet_printf("A: " $*)');

/*
 * TP_FUNCTION: Print the current function name to the TET log.
 */
define(`TP_FUNCTION',`tet_printf("N: %s", __func__)')

/*
 * TP_UNRESOLVED/T_FAIL: Print an appropriate message to the log, and set the
 * `result' variable.
 */
define(`TP_UNRESOLVED',
	`do { tet_printf("U: " $*); result = TET_UNRESOLVED; } while (0)')
define(`TP_FAIL',
	`do { tet_printf("F: " $*); result = TET_FAIL; } while (0)')

/*
 * TP_SET_VERSION: set elf_version() or fail.
 */
define(`TP_SET_VERSION',`do {
		if (elf_version(EV_CURRENT) != EV_CURRENT) {
			TP_UNRESOLVED("elf_version() failed: \"%s\".",
			    elf_errmsg(-1));
			goto done;
		}
	} while (0)')

divert(_DIVNUM)popdef(`_DIVNUM')
