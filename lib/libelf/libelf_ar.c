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
#include <ctype.h>
#include <libelf.h>
#include <stdlib.h>
#include <string.h>

#include "_libelf.h"

#define	LIBELF_NALLOC_SIZE	16

/*
 * `ar' archive handling.
 *
 * `ar' archives start with signature `ARMAG'.  Each archive member is
 * preceded by a header containing meta-data for the member.  This
 * header is described in <ar.h> (struct ar_hdr).  The header always
 * starts on an even address.  File data is padded with "\n"
 * characters to keep this invariant.
 *
 * Special considerations for `ar' archives:
 *
 * The `ar' header only has space for a 16 character file name.  File
 * names are terminated with a '/', so this effectively leaves 15
 * characters for the actual file name.  In order to accomodate longer
 * file names, names may be stored in a separate 'string table' and
 * referenced indirectly by a member header.  The string table itself
 * appears as an archive member with name "// ".  An indirect file name
 * in an `ar' header matches the pattern "/[0-9]*". The digits form a
 * decimal number that corresponds to a byte offset into the string
 * table where the actual file name of the object starts.  Strings in
 * the string table are padded to start on even addresses.
 *
 * Archives may also have a symbol table (see ranlib(1)), mapping
 * program symbols to object files inside the archive.  A symbol table
 * uses a file name of "/ " in its archive header.  The symbol table
 * is structured as:
 *  - a 4-byte count of entries stored as a binary value, MSB first
 *  - 'n' 4-byte offsets, stored as binary values, MSB first
 *  - 'n' NUL-terminated strings, for ELF symbol names, stored unpadded.
 *
 * If the symbol table and string table are is present in an archive
 * they must be the very first objects and in that order.
 */

/*
 * Convert a string bounded by `start' and `start+sz' (exclusive) to a
 * number in the specified base.
 */
static int
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
		if (v >= base)		/* illegal digit */
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
static char *
_libelf_ar_get_string(char *buf, size_t bufsize, int rawname)
{
	char *q, *r;

	q = buf + bufsize;

	/* Skip trailing blanks. */
	for (q = buf + bufsize - 1; q > buf && *q == ' '; --q)
		;

	if (rawname == 0 && *q == '/')
		q--;

	if (q <= buf)
		return (NULL);

	if ((r = malloc(q - buf + 2)) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, 0);
		return (NULL);
	}

	(void) strlcpy(r, buf, q - buf + 2);

	return (r);
}

/*
 * Retrieve the full name of the archive member.
 */
static char *
_libelf_ar_get_name(char *buf, size_t bufsize, Elf *e)
{
	char *q, *r, *s;
	size_t len;
	size_t offset;

	assert(e->e_kind == ELF_K_AR);

	if (buf[0] == '/') {
		/*
		 * The value in field ar_name is a decimal offset into
		 * the archive string table where the actual name
		 * resides.
		 */
		if (_libelf_ar_get_number(buf + 1, bufsize - 1, 10, &offset) == 0) {
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

		(void) strlcpy(s, q, len);

		return (s);
	}

	/*
	 * Normal 'name'
	 */
	return (_libelf_ar_get_string(buf, bufsize, 0));
}


Elf_Arhdr *
_libelf_ar_gethdr(Elf *e)
{
	Elf *parent;
	struct ar_hdr *arh;
	Elf_Arhdr *eh;
	size_t n;

	if ((parent = e->e_parent) == NULL) {
		LIBELF_SET_ERROR(ARGUMENT, 0);
		return (NULL);
	}

	arh = (struct ar_hdr *) ((uintptr_t) e->e_rawfile - sizeof(struct ar_hdr));

	assert((uintptr_t) arh >= (uintptr_t) parent->e_rawfile + SARMAG);
	assert((uintptr_t) arh <= (uintptr_t) parent->e_rawfile + parent->e_rawsize -
	    sizeof(struct ar_hdr));

	if ((eh = malloc(sizeof(Elf_Arhdr))) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, 0);
		return (NULL);
	}

	e->e_arhdr = eh;
	eh->ar_name = eh->ar_rawname = NULL;

	if ((eh->ar_name = _libelf_ar_get_name(arh->ar_name, sizeof(arh->ar_name),
		 parent)) == NULL)
		goto error;

	if (_libelf_ar_get_number(arh->ar_uid, sizeof(arh->ar_uid), 10, &n) == 0)
		goto error;
	eh->ar_uid = (uid_t) n;

	if (_libelf_ar_get_number(arh->ar_gid, sizeof(arh->ar_gid), 10, &n) == 0)
		goto error;
	eh->ar_gid = (gid_t) n;

	if (_libelf_ar_get_number(arh->ar_mode, sizeof(arh->ar_mode), 8, &n) == 0)
		goto error;
	eh->ar_mode = (mode_t) n;

	if (_libelf_ar_get_number(arh->ar_size, sizeof(arh->ar_size), 10, &n) == 0)
		goto error;
	eh->ar_size = n;

	if ((eh->ar_rawname = _libelf_ar_get_string(arh->ar_name,
		 sizeof(arh->ar_name), 1)) == NULL)
		goto error;

	return (eh);

 error:
	if (eh) {
		if (eh->ar_name)
			free(eh->ar_name);
		if (eh->ar_rawname)
			free(eh->ar_rawname);
		free(eh);
	}
	e->e_arhdr = NULL;

	return (NULL);
}

Elf *
_libelf_ar_open_member(int fd, Elf_Cmd c, Elf *elf)
{
	Elf *e;
	off_t next;
	struct ar_hdr *arh;
	size_t sz;

	assert(elf->e_kind == ELF_K_AR);

	next = elf->e_u.e_ar.e_next;

	/*
	 * `next' is only set to zero by elf_next() when the last
	 * member of an archive is processed.
	 */
	if (next == (off_t) 0)
		return (NULL);

	assert((next & 1) == 0);

	arh = (struct ar_hdr *) (elf->e_rawfile + next);

	if (_libelf_ar_get_number(arh->ar_size, sizeof(arh->ar_size), 10, &sz) == 0) {
		LIBELF_SET_ERROR(ARCHIVE, 0);
		return (NULL);
	}

	assert(sz > 0);

	arh++;	/* skip over archive member header */

	if ((e = elf_memory((char *) arh, sz)) == NULL)
		return (NULL);

	e->e_fd = fd;
	e->e_cmd = c;

	elf->e_u.e_ar.e_nchildren++;
	e->e_parent = elf;

	return (e);
}

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

		if (_libelf_ar_get_number(arh.ar_size, sizeof(arh.ar_size), 10, &sz) == 0) {
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

/*
 * An ar(1) symbol table has the following layout:
 *
 * The first 4 bytes are a binary count of the number of entries in the
 * symbol table, stored MSB-first.
 *
 * Then there are 'n' 4-byte binary offsets, also stored MSB first.
 *
 * Following this, there are 'n' null-terminated strings.
 */

#define	GET_WORD(P, V) do {			\
		(V) = 0;			\
		(V) = (P)[0]; (V) <<= 8;	\
		(V) += (P)[1]; (V) <<= 8;	\
		(V) += (P)[2]; (V) <<= 8;	\
		(V) += (P)[3];			\
	} while (0)

#define	INTSZ	4

Elf_Arsym *
_libelf_ar_process_symtab(Elf *e, size_t *count)
{
	size_t n, nentries, off;
	Elf_Arsym *symtab, *sym;
	unsigned char  *p, *s, *end;

	assert(e != NULL);
	assert(count != NULL);

	if (e->e_u.e_ar.e_rawsymtabsz < INTSZ) {
		LIBELF_SET_ERROR(ARCHIVE, 0);
		return (NULL);
	}

	p = (unsigned char *) e->e_u.e_ar.e_rawsymtab;
	end = p + e->e_u.e_ar.e_rawsymtabsz;

	GET_WORD(p, nentries);
	p += INTSZ;

	if (nentries == 0 || p + nentries * INTSZ >= end) {
		LIBELF_SET_ERROR(ARCHIVE, 0);
		return (NULL);
	}

	/* Allocate space for a nentries + a sentinel. */
	if ((symtab = malloc(sizeof(Elf_Arsym) * (nentries+1))) == NULL) {
		LIBELF_SET_ERROR(RESOURCE, 0);
		return (NULL);
	}

	s = p + (nentries * INTSZ); /* start of the string table. */

	for (n = nentries, sym = symtab; n > 0; n--) {
		off = 0;

		GET_WORD(p, off);

		sym->as_off = off;
		sym->as_hash = elf_hash(s);
		sym->as_name = s;

		p += INTSZ;
		sym++;

		for (; s < end && *s++ != '\0';) /* skip to next string */
			;
		if (s > end) {
			LIBELF_SET_ERROR(ARCHIVE, 0);
			free(symtab);
			return (NULL);
		}
	}

	/* Fill up the sentinel entry. */
	sym->as_name = NULL;
	sym->as_hash = ~0UL;
	sym->as_off = (off_t) 0;

	*count = e->e_u.e_ar.e_symtabsz = nentries + 1;
	e->e_u.e_ar.e_symtab = symtab;

	return (symtab);
}
