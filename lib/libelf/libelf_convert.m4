/*-
 * Copyright (c) 2006-2008 Joseph Koshy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/elf32.h>
#include <sys/elf64.h>

#include <assert.h>
#include <libelf.h>
#include <osreldate.h>
#include <string.h>

#include "_libelf.h"

/* WARNING: GENERATED FROM __file__. */

/*
 * Macros to swap various integral quantities.
 */

#define	SWAP_HALF(X) 	do {						\
		uint16_t _x = (uint16_t) (X);				\
		uint16_t _t = _x & 0xFF;				\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		(X) = _t;						\
	} while (0)
#define	SWAP_WORD(X) 	do {						\
		uint32_t _x = (uint32_t) (X);				\
		uint32_t _t = _x & 0xFF;				\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		(X) = _t;						\
	} while (0)
#define	SWAP_ADDR32(X)	SWAP_WORD(X)
#define	SWAP_OFF32(X)	SWAP_WORD(X)
#define	SWAP_SWORD(X)	SWAP_WORD(X)
#define	SWAP_WORD64(X)	do {						\
		uint64_t _x = (uint64_t) (X);				\
		uint64_t _t = _x & 0xFF;				\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		_t <<= 8; _x >>= 8; _t |= _x & 0xFF;			\
		(X) = _t;						\
	} while (0)
#define	SWAP_ADDR64(X)	SWAP_WORD64(X)
#define	SWAP_LWORD(X)	SWAP_WORD64(X)
#define	SWAP_OFF64(X)	SWAP_WORD64(X)
#define	SWAP_SXWORD(X)	SWAP_WORD64(X)
#define	SWAP_XWORD(X)	SWAP_WORD64(X)

/*
 * Write out various integral values.  The destination pointer could
 * be unaligned.  Values are written out in native byte order.  The
 * destination pointer is incremented after the write.
 */
#define	WRITE_BYTE(P,X) do {						\
		char *const _p = (char *) (P);	\
		_p[0]		= (char) (X);			\
		(P)		= _p + 1;				\
	} while (0)
#define	WRITE_HALF(P,X)	do {						\
		uint16_t _t	= (X);					\
		char *const _p	= (char *) (P);	\
		const char *const _q = (char *) &_t;	\
		_p[0]		= _q[0];				\
		_p[1]		= _q[1];				\
		(P) 		= _p + 2;				\
	} while (0)
#define	WRITE_WORD(P,X)	do {						\
		uint32_t _t	= (X);					\
		char *const _p	= (char *) (P);	\
		const char *const _q = (char *) &_t;	\
		_p[0]		= _q[0];				\
		_p[1]		= _q[1];				\
		_p[2]		= _q[2];				\
		_p[3]		= _q[3];				\
		(P)		= _p + 4;				\
	} while (0)
#define	WRITE_ADDR32(P,X)	WRITE_WORD(P,X)
#define	WRITE_OFF32(P,X)	WRITE_WORD(P,X)
#define	WRITE_SWORD(P,X)	WRITE_WORD(P,X)
#define	WRITE_WORD64(P,X)	do {					\
		uint64_t _t	= (X);					\
		char *const _p	= (char *) (P);	\
		const char *const _q = (char *) &_t;	\
		_p[0]		= _q[0];				\
		_p[1]		= _q[1];				\
		_p[2]		= _q[2];				\
		_p[3]		= _q[3];				\
		_p[4]		= _q[4];				\
		_p[5]		= _q[5];				\
		_p[6]		= _q[6];				\
		_p[7]		= _q[7];				\
		(P)		= _p + 8;				\
	} while (0)
#define	WRITE_ADDR64(P,X)	WRITE_WORD64(P,X)
#define	WRITE_LWORD(P,X)	WRITE_WORD64(P,X)
#define	WRITE_OFF64(P,X)	WRITE_WORD64(P,X)
#define	WRITE_SXWORD(P,X)	WRITE_WORD64(P,X)
#define	WRITE_XWORD(P,X)	WRITE_WORD64(P,X)
#define	WRITE_IDENT(P,X)	do {					\
		(void) memcpy((P), (X), sizeof((X)));			\
		(P)		= (P) + EI_NIDENT;			\
	} while (0)

/*
 * Read in various integral values.  The source pointer could be
 * unaligned.  Values are read in native byte order.  The source
 * pointer is incremented appropriately.
 */

#define	READ_BYTE(P,X)	do {						\
		const char *const _p =				\
			(const char *) (P);			\
		(X)		= _p[0];				\
		(P)		= (P) + 1;				\
	} while (0)
#define	READ_HALF(P,X)	do {						\
		uint16_t _t;						\
		char *const _q = (char *) &_t;	\
		const char *const _p =				\
			(const char *) (P);			\
		_q[0]		= _p[0];				\
		_q[1]		= _p[1];				\
		(P)		= (P) + 2;				\
		(X)		= _t;					\
	} while (0)
#define	READ_WORD(P,X)	do {						\
		uint32_t _t;						\
		char *const _q = (char *) &_t;	\
		const char *const _p =				\
			(const char *) (P);			\
		_q[0]		= _p[0];				\
		_q[1]		= _p[1];				\
		_q[2]		= _p[2];				\
		_q[3]		= _p[3];				\
		(P)		= (P) + 4;				\
		(X)		= _t;					\
	} while (0)
#define	READ_ADDR32(P,X)	READ_WORD(P,X)
#define	READ_OFF32(P,X)		READ_WORD(P,X)
#define	READ_SWORD(P,X)		READ_WORD(P,X)
#define	READ_WORD64(P,X)	do {					\
		uint64_t _t;						\
		char *const _q = (char *) &_t;	\
		const char *const _p =				\
			(const char *) (P);			\
		_q[0]		= _p[0];				\
		_q[1]		= _p[1];				\
		_q[2]		= _p[2];				\
		_q[3]		= _p[3];				\
		_q[4]		= _p[4];				\
		_q[5]		= _p[5];				\
		_q[6]		= _p[6];				\
		_q[7]		= _p[7];				\
		(P)		= (P) + 8;				\
		(X)		= _t;					\
	} while (0)
#define	READ_ADDR64(P,X)	READ_WORD64(P,X)
#define	READ_LWORD(P,X)		READ_WORD64(P,X)
#define	READ_OFF64(P,X)		READ_WORD64(P,X)
#define	READ_SXWORD(P,X)	READ_WORD64(P,X)
#define	READ_XWORD(P,X)		READ_WORD64(P,X)
#define	READ_IDENT(P,X)		do {					\
		(void) memcpy((X), (P), sizeof((X)));			\
		(P)		= (P) + EI_NIDENT;			\
	} while (0)

#define	ROUNDUP2(V,N)	(V) = ((((V) + (N) - 1)) & ~((N) - 1))

divert(-1)

/*
 * Generate conversion routines for converting between in-memory and
 * file representations of Elf data structures.
 *
 * `In-memory' representations of an Elf data structure use natural
 * alignments and native byte ordering.  This allows arithmetic and
 * casting to work as expected.  On the other hand the `file'
 * representation of an ELF data structure could be packed tighter
 * than its `in-memory' representation, and could be of a differing
 * byte order.  An additional complication is that `ar' only pads data
 * to even addresses and so ELF archive member data being read from
 * inside an `ar' archive could end up at misaligned memory addresses.
 *
 * Consequently, casting the `char *' pointers that point to memory
 * representations (i.e., source pointers for the *_tof() functions
 * and the destination pointers for the *_tom() functions), is safe,
 * as these pointers should be correctly aligned for the memory type
 * already.  However, pointers to file representations have to be
 * treated as being potentially unaligned and no casting can be done.
 */

include(SRCDIR`/elf_types.m4')

/*
 * `IGNORE'_* flags turn off generation of template code.
 */

define(`IGNORE',
  `define(IGNORE_$1`'32,	1)
   define(IGNORE_$1`'64,	1)')

IGNORE(MOVEP)
IGNORE(NOTE)
IGNORE(GNUHASH)

define(IGNORE_BYTE,		1)	/* 'lator, leave 'em bytes alone */
define(IGNORE_GNUHASH,		1)
define(IGNORE_NOTE,		1)
define(IGNORE_SXWORD32,		1)
define(IGNORE_XWORD32,		1)

/*
 * `BASE'_XXX flags cause class agnostic template functions
 * to be generated.
 */

define(`BASE_BYTE',	1)
define(`BASE_HALF',	1)
define(`BASE_NOTE',	1)
define(`BASE_WORD',	1)
define(`BASE_LWORD',	1)
define(`BASE_SWORD',	1)
define(`BASE_XWORD',	1)
define(`BASE_SXWORD',	1)

/*
 * `SIZEDEP'_XXX flags cause 32/64 bit variants to be generated
 * for each primitive type.
 */

define(`SIZEDEP_ADDR',	1)
define(`SIZEDEP_OFF',	1)

/*
 * `Primitive' ELF types are those that are an alias for an integral
 * type.  They have no internal structure. These can be copied using
 * a `memcpy()', and byteswapped in straightforward way.
 *
 * Macro use:
 * `$1': Name of the ELF type.
 * `$2': C structure name suffix
 * `$3': ELF class specifier for symbols, one of [`', `32', `64']
 * `$4': ELF class specifier for types, one of [`32', `64']
 */
define(`MAKEPRIM_TO_F',`
static int
libelf_cvt_$1$3_tof(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	Elf$4_$2 t, *s = (Elf$4_$2 *) (uintptr_t) src;
	size_t c;

	(void) dsz;

	if (!byteswap) {
		(void) memcpy(dst, src, count * sizeof(*s));
		return (1);
	}

	for (c = 0; c < count; c++) {
		t = *s++;
		SWAP_$1$3(t);
		WRITE_$1$3(dst,t);
	}

	return (1);
}
')

define(`MAKEPRIM_TO_M',`
static int
libelf_cvt_$1$3_tom(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	Elf$4_$2 t, *d = (Elf$4_$2 *) (uintptr_t) dst;
	size_t c;

	if (dsz < count * sizeof(Elf$4_$2))
		return (0);

	if (!byteswap) {
		(void) memcpy(dst, src, count * sizeof(*d));
		return (1);
	}

	for (c = 0; c < count; c++) {
		READ_$1$3(src,t);
		SWAP_$1$3(t);
		*d++ = t;
	}

	return (1);
}
')

define(`SWAP_FIELD',
  `ifdef(`IGNORE_'$2,`',
    `ifelse(BASE_$2,1,
      `SWAP_$2(t.$1);
			',
      `ifelse($2,BYTE,`',
        `ifelse($2,IDENT,`',
          `SWAP_$2'SZ()`(t.$1);
			')')')')')
define(`SWAP_MEMBERS',
  `ifelse($#,1,`/**/',
     `SWAP_FIELD($1)SWAP_MEMBERS(shift($@))')')

define(`SWAP_STRUCT',
  `pushdef(`SZ',$2)/* Swap an Elf$2_$1 */
			SWAP_MEMBERS(Elf$2_$1_DEF)popdef(`SZ')')

define(`WRITE_FIELD',
  `ifelse(BASE_$2,1,
    `WRITE_$2(dst,t.$1);
		',
    `ifelse($2,IDENT,
      `WRITE_$2(dst,t.$1);
		',
      `WRITE_$2'SZ()`(dst,t.$1);
		')')')
define(`WRITE_MEMBERS',
  `ifelse($#,1,`/**/',
    `WRITE_FIELD($1)WRITE_MEMBERS(shift($@))')')

define(`WRITE_STRUCT',
  `pushdef(`SZ',$2)/* Write an Elf$2_$1 */
		WRITE_MEMBERS(Elf$2_$1_DEF)popdef(`SZ')')

define(`READ_FIELD',
  `ifelse(BASE_$2,1,
    `READ_$2(s,t.$1);
		',
    `ifelse($2,IDENT,
      `READ_$2(s,t.$1);
		',
      `READ_$2'SZ()`(s,t.$1);
		')')')

define(`READ_MEMBERS',
  `ifelse($#,1,`/**/',
    `READ_FIELD($1)READ_MEMBERS(shift($@))')')

define(`READ_STRUCT',
  `pushdef(`SZ',$2)/* Read an Elf$2_$1 */
		READ_MEMBERS(Elf$2_$1_DEF)popdef(`SZ')')

/*
 * Converters for non-integral ELF data structures.
 *
 * When converting data to file representation, the source pointer
 * will be naturally aligned for a data structure's in-memory
 * representation.  When converting data to memory, the destination
 * pointer will be similarly aligned.
 *
 * For in-place conversions, when converting to file representations,
 * the source buffer is large enough to hold `file' data.  When
 * converting from file to memory, we need to be careful to work
 * `backwards', to avoid overwriting unconverted data.
 *
 * Macro use:
 * `$1': Name of the ELF type.
 * `$2': C structure name suffix.
 * `$3': ELF class specifier, one of [`', `32', `64']
 */

define(`MAKE_TO_F',
  `ifdef(`IGNORE_'$1$3,`',`
static int
libelf_cvt$3_$1_tof(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	Elf$3_$2	t, *s;
	size_t c;

	(void) dsz;

	s = (Elf$3_$2 *) (uintptr_t) src;
	for (c = 0; c < count; c++) {
		t = *s++;
		if (byteswap) {
			SWAP_STRUCT($2,$3)
		}
		WRITE_STRUCT($2,$3)
	}

	return (1);
}
')')

define(`MAKE_TO_M',
  `ifdef(`IGNORE_'$1$3,`',`
static int
libelf_cvt$3_$1_tom(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	Elf$3_$2	 t, *d;
	char		*s,*s0;
	size_t		fsz;

	fsz = elf$3_fsize(ELF_T_$1, (size_t) 1, EV_CURRENT);
	d   = ((Elf$3_$2 *) (uintptr_t) dst) + (count - 1);
	s0  = (char *) src + (count - 1) * fsz;

	if (dsz < count * sizeof(Elf$3_$2))
		return (0);

	while (count--) {
		s = s0;
		READ_STRUCT($2,$3)
		if (byteswap) {
			SWAP_STRUCT($2,$3)
		}
		*d-- = t; s0 -= fsz;
	}

	return (1);
}
')')

/*
 * Make type convertor functions from the type definition
 * of the ELF type:
 * - if the type is a base (i.e., `primitive') type:
 *   - if it is marked as to be ignored (i.e., `IGNORE_'TYPE)
 *     is defined, we skip the code generation step.
 *   - if the type is declared as `SIZEDEP', then 32 and 64 bit
 *     variants of the conversion functions are generated.
 *   - otherwise a 32 bit variant is generated.
 * - if the type is a structure type, we generate 32 and 64 bit
 *   variants of the conversion functions.
 */

define(`MAKE_TYPE_CONVERTER',
  `#if	__FreeBSD_version >= $3 /* $1 */
ifdef(`BASE'_$1,
    `ifdef(`IGNORE_'$1,`',
      `MAKEPRIM_TO_F($1,$2,`',64)
       MAKEPRIM_TO_M($1,$2,`',64)')',
    `ifdef(`SIZEDEP_'$1,
      `MAKEPRIM_TO_F($1,$2,32,32)dnl
       MAKEPRIM_TO_M($1,$2,32,32)dnl
       MAKEPRIM_TO_F($1,$2,64,64)dnl
       MAKEPRIM_TO_M($1,$2,64,64)',
      `MAKE_TO_F($1,$2,32)dnl
       MAKE_TO_F($1,$2,64)dnl
       MAKE_TO_M($1,$2,32)dnl
       MAKE_TO_M($1,$2,64)')')
#endif /* $1 */
')

define(`MAKE_TYPE_CONVERTERS',
  `ifelse($#,1,`',
    `MAKE_TYPE_CONVERTER($1)MAKE_TYPE_CONVERTERS(shift($@))')')

divert(0)

/*
 * Sections of type ELF_T_BYTE are never byteswapped, consequently a
 * simple memcpy suffices for both directions of conversion.
 */

static int
libelf_cvt_BYTE_tox(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	(void) byteswap;
	if (dsz < count)
		return (0);
	if (dst != src)
		(void) memcpy(dst, src, count);
	return (1);
}

MAKE_TYPE_CONVERTERS(ELF_TYPE_LIST)

#if	__FreeBSD_version >= 800062
/*
 * Sections of type ELF_T_GNUHASH start with a header containing 4 32-bit
 * words.  Bloom filter data comes next, followed by hash buckets and the
 * hash chain.
 *
 * Bloom filter words are 64 bit wide on ELFCLASS64 objects and are 32 bit
 * wide on ELFCLASS32 objects.  The other objects in this section are 32
 * bits wide.
 *
 * Argument `srcsz' denotes the number of bytes to be converted.  In the
 * 32-bit case we need to translate `srcsz' to a count of 32-bit words.
 */

static int
libelf_cvt32_GNUHASH_tom(char *dst, size_t dsz, char *src, size_t srcsz,
    int byteswap)
{
	return (libelf_cvt_WORD_tom(dst, dsz, src, srcsz / sizeof(uint32_t),
	        byteswap));
}

static int
libelf_cvt32_GNUHASH_tof(char *dst, size_t dsz, char *src, size_t srcsz,
    int byteswap)
{
	return (libelf_cvt_WORD_tof(dst, dsz, src, srcsz / sizeof(uint32_t),
	        byteswap));
}

static int
libelf_cvt64_GNUHASH_tom(char *dst, size_t dsz, char *src, size_t srcsz,
    int byteswap)
{
	size_t sz;
	uint64_t t64, *bloom64;
	Elf_GNU_Hash_Header *gh;
	uint32_t n, nbuckets, nchains, maskwords, shift2, symndx, t32;
	uint32_t *buckets, *chains;

	sz = 4 * sizeof(uint32_t);	/* File header is 4 words long. */
	if (dsz < sizeof(Elf_GNU_Hash_Header) || srcsz < sz)
		return (0);

	/* Read in the section header and byteswap if needed. */
	READ_WORD(src, nbuckets);
	READ_WORD(src, symndx);
	READ_WORD(src, maskwords);
	READ_WORD(src, shift2);

	srcsz -= sz;

	if (byteswap) {
		SWAP_WORD(nbuckets);
		SWAP_WORD(symndx);
		SWAP_WORD(maskwords);
		SWAP_WORD(shift2);
	}

	/* Check source buffer and destination buffer sizes. */
	sz = nbuckets * sizeof(uint32_t) + maskwords * sizeof(uint64_t);
	if (srcsz < sz || dsz < sz + sizeof(Elf_GNU_Hash_Header))
		return (0);

	gh = (Elf_GNU_Hash_Header *) (uintptr_t) dst;
	gh->gh_nbuckets  = nbuckets;
	gh->gh_symndx    = symndx;
	gh->gh_maskwords = maskwords;
	gh->gh_shift2    = shift2;
	
	dsz -= sizeof(Elf_GNU_Hash_Header);
	dst += sizeof(Elf_GNU_Hash_Header);

	bloom64 = (uint64_t *) (uintptr_t) dst;

	/* Copy bloom filter data. */
	for (n = 0; n < maskwords; n++) {
		READ_XWORD(src, t64);
		if (byteswap)
			SWAP_XWORD(t64);
		bloom64[n] = t64;
	}

	/* The hash buckets follows the bloom filter. */
	dst += maskwords * sizeof(uint64_t);
	buckets = (uint32_t *) (uintptr_t) dst;

	for (n = 0; n < nbuckets; n++) {
		READ_WORD(src, t32);
		if (byteswap)
			SWAP_WORD(t32);
		buckets[n] = t32;
	}

	dst += nbuckets * sizeof(uint32_t);

	/* The hash chain follows the hash buckets. */
	dsz -= sz;
	srcsz -= sz;

	if (dsz < srcsz)	/* Destination lacks space. */
	        return (0);

	nchains = srcsz / sizeof(uint32_t);
	chains = (uint32_t *) (uintptr_t) dst;

	for (n = 0; n < nchains; n++) {
		READ_WORD(src, t32);
		if (byteswap)
			SWAP_WORD(t32);
		*chains++ = t32;
	}

	return (1);
}

static int
libelf_cvt64_GNUHASH_tof(char *dst, size_t dsz, char *src, size_t srcsz,
    int byteswap)
{
	uint32_t *s32;
	size_t sz, hdrsz;
	uint64_t *s64, t64;
	Elf_GNU_Hash_Header *gh;
	uint32_t maskwords, n, nbuckets, nchains, t0, t1, t2, t3, t32;

	hdrsz = 4 * sizeof(uint32_t);	/* Header is 4x32 bits. */
	if (dsz < hdrsz || srcsz < sizeof(Elf_GNU_Hash_Header))
		return (0);

	gh = (Elf_GNU_Hash_Header *) (uintptr_t) src;

	t0 = nbuckets = gh->gh_nbuckets;
	t1 = gh->gh_symndx;
	t2 = maskwords = gh->gh_maskwords;
	t3 = gh->gh_shift2;

	src   += sizeof(Elf_GNU_Hash_Header);
	srcsz -= sizeof(Elf_GNU_Hash_Header);
	dsz   -= hdrsz;

	sz = gh->gh_nbuckets * sizeof(uint32_t) + gh->gh_maskwords *
	    sizeof(uint64_t);

	if (srcsz < sz || dsz < sz)
		return (0);

 	/* Write out the header. */
	if (byteswap) {
		SWAP_WORD(t0);
		SWAP_WORD(t1);
		SWAP_WORD(t2);
		SWAP_WORD(t3);
	}

	WRITE_WORD(dst, t0);
	WRITE_WORD(dst, t1);
	WRITE_WORD(dst, t2);
	WRITE_WORD(dst, t3);

	/* Copy the bloom filter and the hash table. */
	s64 = (uint64_t *) (uintptr_t) src;
	for (n = 0; n < maskwords; n++) {
		t64 = *s64++;
		if (byteswap)
			SWAP_XWORD(t64);
		WRITE_WORD64(dst, t64);
	}

	s32 = (uint32_t *) s64;
	for (n = 0; n < nbuckets; n++) {
		t32 = *s32++;
		if (byteswap)
			SWAP_WORD(t32);
		WRITE_WORD(dst, t32);
	}

	srcsz -= sz;
	dsz   -= sz;

	/* Copy out the hash chains. */
	if (dsz < srcsz)
		return (0);

	nchains = srcsz / sizeof(uint32_t);
	for (n = 0; n < nchains; n++) {
		t32 = *s32++;
		if (byteswap)
			SWAP_WORD(t32);
		WRITE_WORD(dst, t32);
	}

	return (1);
}
#endif

/*
 * Elf_Note structures comprise a fixed size header followed by variable
 * length strings.  The fixed size header needs to be byte swapped, but
 * not the strings.
 *
 * Argument `count' denotes the total number of bytes to be converted.
 * The destination buffer needs to be at least `count' bytes in size.
 */
static int
libelf_cvt_NOTE_tom(char *dst, size_t dsz, char *src, size_t count, 
    int byteswap)
{
	uint32_t namesz, descsz, type;
	Elf_Note *en;
	size_t sz, hdrsz;

	if (dsz < count)	/* Destination buffer is too small. */
		return (0);

	hdrsz = 3 * sizeof(uint32_t);
	if (count < hdrsz)		/* Source too small. */
		return (0);

	if (!byteswap) {
		(void) memcpy(dst, src, count);
		return (1);
	}

	/* Process all notes in the section. */
	while (count > hdrsz) {
		/* Read the note header. */
		READ_WORD(src, namesz);
		READ_WORD(src, descsz);
		READ_WORD(src, type);

		/* Translate. */
		SWAP_WORD(namesz);
		SWAP_WORD(descsz);
		SWAP_WORD(type);

		/* Copy out the translated note header. */
		en = (Elf_Note *) (uintptr_t) dst;
		en->n_namesz = namesz;
		en->n_descsz = descsz;
		en->n_type = type;

		dsz -= sizeof(Elf_Note);
		dst += sizeof(Elf_Note);
		count -= hdrsz;

		ROUNDUP2(namesz, 4);
		ROUNDUP2(descsz, 4);

		sz = namesz + descsz;

		if (count < sz || dsz < sz)	/* Buffers are too small. */
			return (0);

		(void) memcpy(dst, src, sz);

		src += sz;
		dst += sz;

		count -= sz;
		dsz -= sz;
	}

	return (1);
}

static int
libelf_cvt_NOTE_tof(char *dst, size_t dsz, char *src, size_t count,
    int byteswap)
{
	uint32_t namesz, descsz, type;
	Elf_Note *en;
	size_t sz;

	if (dsz < count)
		return (0);

	if (!byteswap) {
		(void) memcpy(dst, src, count);
		return (1);
	}

	while (count > sizeof(Elf_Note)) {

		en = (Elf_Note *) (uintptr_t) src;
		namesz = en->n_namesz;
		descsz = en->n_descsz;
		type = en->n_type;

		sz = namesz;
		ROUNDUP2(sz, 4);
		sz += descsz;
		ROUNDUP2(sz, 4);

		SWAP_WORD(namesz);
		SWAP_WORD(descsz);
		SWAP_WORD(type);

		WRITE_WORD(dst, namesz);
		WRITE_WORD(dst, descsz);
		WRITE_WORD(dst, type);

		src += sizeof(Elf_Note);
		count -= sizeof(Elf_Note);

		if (count < sz)
			sz = count;

		(void) memcpy(dst, src, sz);

		src += sz;
		dst += sz;
		count -= sz;
	}

	return (1);
}

struct converters {
	int	(*tof32)(char *dst, size_t dsz, char *src, size_t cnt,
		    int byteswap);
	int	(*tom32)(char *dst, size_t dsz, char *src, size_t cnt,
		    int byteswap);
	int	(*tof64)(char *dst, size_t dsz, char *src, size_t cnt,
		    int byteswap);
	int	(*tom64)(char *dst, size_t dsz, char *src, size_t cnt,
		    int byteswap);
};

divert(-1)
define(`CONV',
  `ifdef(`IGNORE_'$1$2,
    `.$3$2 = NULL',
    `ifdef(`BASE_'$1,
      `.$3$2 = libelf_cvt_$1_$3',
      `ifdef(`SIZEDEP_'$1,
        `.$3$2 = libelf_cvt_$1$2_$3',
        `.$3$2 = libelf_cvt$2_$1_$3')')')')

define(`CONVERTER_NAME',
  `ifdef(`IGNORE_'$1,`',
    `#if	__FreeBSD_version >= $3
    [ELF_T_$1] = {
        CONV($1,32,tof), CONV($1,32,tom),
        CONV($1,64,tof), CONV($1,64,tom) },
#endif
')')

define(`CONVERTER_NAMES',
  `ifelse($#,1,`',
    `CONVERTER_NAME($1)CONVERTER_NAMES(shift($@))')')

undefine(`IGNORE_BYTE32', `IGNORE_BYTE64')
divert(0)

static struct converters cvt[ELF_T_NUM] = {
CONVERTER_NAMES(ELF_TYPE_LIST)

	/*
	 * Types that needs hand-coded converters follow.
	 */

	[ELF_T_BYTE] = {
		.tof32 = libelf_cvt_BYTE_tox,
		.tom32 = libelf_cvt_BYTE_tox,
		.tof64 = libelf_cvt_BYTE_tox,
		.tom64 = libelf_cvt_BYTE_tox
	},

#if	__FreeBSD_version >= 800062
	[ELF_T_GNUHASH] = {
		.tof32 = libelf_cvt32_GNUHASH_tof,
		.tom32 = libelf_cvt32_GNUHASH_tom,
		.tof64 = libelf_cvt64_GNUHASH_tof,
		.tom64 = libelf_cvt64_GNUHASH_tom
	},
#endif

	[ELF_T_NOTE] = {
		.tof32 = libelf_cvt_NOTE_tof,
		.tom32 = libelf_cvt_NOTE_tom,
		.tof64 = libelf_cvt_NOTE_tof,
		.tom64 = libelf_cvt_NOTE_tom
	}
};

int (*_libelf_get_translator(Elf_Type t, int direction, int elfclass))
 (char *_dst, size_t dsz, char *_src, size_t _cnt, int _byteswap)
{
	assert(elfclass == ELFCLASS32 || elfclass == ELFCLASS64);
	assert(direction == ELF_TOFILE || direction == ELF_TOMEMORY);

	if (t >= ELF_T_NUM ||
	    (elfclass != ELFCLASS32 && elfclass != ELFCLASS64) ||
	    (direction != ELF_TOFILE && direction != ELF_TOMEMORY))
		return (NULL);

	return ((elfclass == ELFCLASS32) ?
	    (direction == ELF_TOFILE ? cvt[t].tof32 : cvt[t].tom32) :
	    (direction == ELF_TOFILE ? cvt[t].tof64 : cvt[t].tom64));
}
