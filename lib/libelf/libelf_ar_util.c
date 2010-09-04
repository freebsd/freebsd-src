/*-
 * Copyright (c) 2006,2009 Joseph Koshy
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS' AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <ar.h>
#include <assert.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>

#include "_libelf.h"

/*
 * Convert a string bounded by `start' and `start+sz' (exclusive) to a
 * number in the specified base.
 */
int
_libelf_ar_get_number(char *s, size_t sz, int base, size_t *ret)
{
	int c, v;
	size_t r;
	char *e;

	assert(base <= 10);

	e = s + sz;

	/* skip leading blanks */
	for (;s < e && (c = *s) == ' '; s++)
		;

	r = 0L;
	for (;s < e; s++) {
		if ((c = *s) == ' ')
			break;
		if (c < '0' || c > '9')
			return (0);
		v = c - '0';
		if (v >= base)		/* Illegal digit. */
			break;
		r *= base;
		r += v;
	}

	*ret = r;

	return (1);
}

/*
 * Retrieve a string from a name field.  If `rawname' is set, leave
 * ar(1) control characters in.
 */
char *
_libelf_ar_get_string(const char *buf, size_t bufsize, int rawname)
{
	const char *q;
	char *r;
	size_t sz;

	if (rawname)
		sz = bufsize + 1;
	else {
		/* Skip back over trailing blanks. */
		for (q = buf + bufsize - 1; q >= buf && *q == ' '; --q)
			;

		if (q < buf) {
			/*
			 * If the input buffer only had blanks in it,
			 * return a zero-length string.
			 */
			buf = "";
			sz = 1;
		} else {
			/*
			 * Remove the trailing '/' character, but only
			 * if the name isn't one of the special names
			 * "/" and "//".
			 */
			if (q > buf + 1 ||
			    (q == (buf + 1) && *buf != '/'))
				q--;

			sz = q - buf + 2; /* Space for a trailing NUL. */
		}
	}

	if ((r = malloc(sz)) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, 0);
		return (NULL);
	}

	(void) strncpy(r, buf, sz);
	r[sz - 1] = '\0';

	return (r);
}

/*
 * Retrieve the full name of the archive member.
 */
char *
_libelf_ar_get_name(char *buf, size_t bufsize, Elf *e)
{
	char c, *q, *r, *s;
	size_t len;
	size_t offset;

	assert(e->e_kind == ELF_K_AR);

	if (buf[0] == '/' && (c = buf[1]) >= '0' && c <= '9') {
		/*
		 * The value in field ar_name is a decimal offset into
		 * the archive string table where the actual name
		 * resides.
		 */
		if (_libelf_ar_get_number(buf + 1, bufsize - 1, 10,
		    &offset) == 0) {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			return (NULL);
		}

		if (offset > e->e_u.e_ar.e_rawstrtabsz) {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			return (NULL);
		}

		s = q = e->e_u.e_ar.e_rawstrtab + offset;
		r = e->e_u.e_ar.e_rawstrtab + e->e_u.e_ar.e_rawstrtabsz;

		for (s = q; s < r && *s != '/'; s++)
			;
		len = s - q + 1; /* space for the trailing NUL */

		if ((s = malloc(len)) == NULL) {
			LIBELF_SET_ERROR(RESOURCE, 0);
			return (NULL);
		}

		(void) strncpy(s, q, len);
		s[len - 1] = '\0';

		return (s);
	}

	/*
	 * Normal 'name'
	 */
	return (_libelf_ar_get_string(buf, bufsize, 0));
}

/*
 * Open an 'ar' archive.
 */
Elf *
_libelf_ar_open(Elf *e)
{
	int i;
	char *s, *end;
	size_t sz;
	struct ar_hdr arh;

	e->e_kind = ELF_K_AR;
	e->e_u.e_ar.e_nchildren = 0;
	e->e_u.e_ar.e_next = (off_t) -1;

	/*
	 * Look for special members.
	 */

	s = e->e_rawfile + SARMAG;
	end = e->e_rawfile + e->e_rawsize;

	assert(e->e_rawsize > 0);

	/*
	 * Look for magic names "/ " and "// " in the first two entries
	 * of the archive.
	 */
	for (i = 0; i < 2; i++) {

		if (s + sizeof(arh) > end) {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			return (NULL);
		}

		(void) memcpy(&arh, s, sizeof(arh));

		if (arh.ar_fmag[0] != '`' || arh.ar_fmag[1] != '\n') {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			return (NULL);
		}

		if (arh.ar_name[0] != '/')	/* not a special symbol */
			break;

		if (_libelf_ar_get_number(arh.ar_size, sizeof(arh.ar_size),
			10, &sz) == 0) {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			return (NULL);
		}

		assert(sz > 0);

		s += sizeof(arh);

		if (arh.ar_name[1] == ' ') {	/* "/ " => symbol table */

			e->e_u.e_ar.e_rawsymtab = s;
			e->e_u.e_ar.e_rawsymtabsz = sz;

		} else if (arh.ar_name[1] == '/' && arh.ar_name[2] == ' ') {

			/* "// " => string table for long file names */
			e->e_u.e_ar.e_rawstrtab = s;
			e->e_u.e_ar.e_rawstrtabsz = sz;
		}

		sz = LIBELF_ADJUST_AR_SIZE(sz);

		s += sz;
	}

	e->e_u.e_ar.e_next = (off_t) (s - e->e_rawfile);

	return (e);
}
