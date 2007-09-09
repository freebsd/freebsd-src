/*-
 * Copyright (c) 2006,2007 Joseph Koshy
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
		unsigned char *const _p = (unsigned char *) (P);	\
		_p[0]		= (unsigned char) (X);			\
		(P)		= _p + 1;				\
	} while (0)
#define	WRITE_HALF(P,X)	do {						\
		uint16_t _t	= (X);					\
		unsigned char *const _p	= (unsigned char *) (P);	\
		unsigned const char *const _q = (unsigned char *) &_t;	\
		_p[0]		= _q[0];				\
		_p[1]		= _q[1];				\
		(P) 		= _p + 2;				\
	} while (0)
#define	WRITE_WORD(P,X)	do {						\
		uint32_t _t	= (X);					\
		unsigned char *const _p	= (unsigned char *) (P);	\
		unsigned const char *const _q = (unsigned char *) &_t;	\
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
		unsigned char *const _p	= (unsigned char *) (P);	\
		unsigned const char *const _q = (unsigned char *) &_t;	\
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
 * unaligned.  Values are read in in native byte order.  The source
 * pointer is incremented appropriately.
 */

#define	READ_BYTE(P,X)	do {						\
		const unsigned char *const _p =				\
			(const unsigned char *) (P);			\
		(X)		= _p[0];				\
		(P)		= (P) + 1;				\
	} while (0)
#define	READ_HALF(P,X)	do {						\
		uint16_t _t;						\
		unsigned char *const _q = (unsigned char *) &_t;	\
		const unsigned char *const _p =				\
			(const unsigned char *) (P);			\
		_q[0]		= _p[0];				\
		_q[1]		= _p[1];				\
		(P)		= (P) + 2;				\
		(X)		= _t;					\
	} while (0)
#define	READ_WORD(P,X)	do {						\
		uint32_t _t;						\
		unsigned char *const _q = (unsigned char *) &_t;	\
		const unsigned char *const _p =				\
			(const unsigned char *) (P);			\
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
		unsigned char *const _q = (unsigned char *) &_t;	\
		const unsigned char *const _p =				\
			(const unsigned char *) (P);			\
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

define(IGNORE_BYTE,		1)	/* 'lator, leave 'em bytes alone */
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
static void
libelf_cvt_$1$3_tof(char *dst, char *src, size_t count, int byteswap)
{
	Elf$4_$2 t, *s = (Elf$4_$2 *) (uintptr_t) src;
	size_t c;

	if (dst == src && !byteswap)
		return;

	if (!byteswap) {
		(void) memcpy(dst, src, count * sizeof(*s));
		return;
	}

	for (c = 0; c < count; c++) {
		t = *s++;
		SWAP_$1$3(t);
		WRITE_$1$3(dst,t);
	}
}
')

define(`MAKEPRIM_TO_M',`
static void
libelf_cvt_$1$3_tom(char *dst, char *src, size_t count, int byteswap)
{
	Elf$4_$2 t, *d = (Elf$4_$2 *) (uintptr_t) dst;
	size_t c;

	if (dst == src && !byteswap)
		return;

	if (!byteswap) {
		(void) memcpy(dst, src, count * sizeof(*d));
		return;
	}

	for (c = 0; c < count; c++) {
		READ_$1$3(src,t);
		SWAP_$1$3(t);
		*d++ = t;
	}
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
static void
libelf_cvt$3_$1_tof(char *dst, char *src, size_t count, int byteswap)
{
	Elf$3_$2	t, *s;
	size_t c;

	s = (Elf$3_$2 *) (uintptr_t) src;
	for (c = 0; c < count; c++) {
		t = *s++;
		if (byteswap) {
			SWAP_STRUCT($2,$3)
		}
		WRITE_STRUCT($2,$3)
	}
}
')')

define(`MAKE_TO_M',
  `ifdef(`IGNORE_'$1$3,`',`
static void
libelf_cvt$3_$1_tom(char *dst, char *src, size_t count, int byteswap)
{
	Elf$3_$2	 t, *d;
	unsigned char	*s,*s0;
	size_t		fsz;

	fsz = elf$3_fsize(ELF_T_$1, (size_t) 1, EV_CURRENT);
	d   = ((Elf$3_$2 *) (uintptr_t) dst) + (count - 1);
	s0  = (unsigned char *) src + (count - 1) * fsz;

	while (count--) {
		s = s0;
		READ_STRUCT($2,$3)
		if (byteswap) {
			SWAP_STRUCT($2,$3)
		}
		*d-- = t; s0 -= fsz;
	}
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

static void
libelf_cvt_BYTE_tox(char *dst, char *src, size_t count, int byteswap)
{
	(void) byteswap;
	if (dst != src)
		(void) memcpy(dst, src, count);
}

/*
 * Elf_Note structures comprise a fixed size header followed by variable
 * length strings.  The fixed size header needs to be byte swapped, but
 * not the strings.
 *
 * Argument `count' denotes the total number of bytes to be converted.
 */
static void
libelf_cvt_NOTE_tom(char *dst, char *src, size_t count, int byteswap)
{
	uint32_t namesz, descsz, type;
	Elf_Note *en;
	size_t sz;

	if (dst == src && !byteswap)
		return;

	if (!byteswap) {
		(void) memcpy(dst, src, count);
		return;
	}

	while (count > sizeof(Elf_Note)) {

		READ_WORD(src, namesz);
		READ_WORD(src, descsz);
		READ_WORD(src, type);

		if (byteswap) {
			SWAP_WORD(namesz);
			SWAP_WORD(descsz);
			SWAP_WORD(type);
		}

		en = (Elf_Note *) (uintptr_t) dst;
		en->n_namesz = namesz;
		en->n_descsz = descsz;
		en->n_type = type;

		dst += sizeof(Elf_Note);

		ROUNDUP2(namesz, 4);
		ROUNDUP2(descsz, 4);

		sz = namesz + descsz;

		if (count < sz)
			sz = count;

		(void) memcpy(dst, src, sz);

		src += sz;
		dst += sz;
		count -= sz;
	}
}

static void
libelf_cvt_NOTE_tof(char *dst, char *src, size_t count, int byteswap)
{
	uint32_t namesz, descsz, type;
	Elf_Note *en;
	size_t sz;

	if (dst == src && !byteswap)
		return;

	if (!byteswap) {
		(void) memcpy(dst, src, count);
		return;
	}

	while (count > sizeof(Elf_Note)) {

		en = (Elf_Note *) (uintptr_t) src;
		namesz = en->n_namesz;
		descsz = en->n_descsz;
		type = en->n_type;

		if (byteswap) {
			SWAP_WORD(namesz);
			SWAP_WORD(descsz);
			SWAP_WORD(type);
		}


		WRITE_WORD(dst, namesz);
		WRITE_WORD(dst, descsz);
		WRITE_WORD(dst, type);

		src += sizeof(Elf_Note);

		ROUNDUP2(namesz, 4);
		ROUNDUP2(descsz, 4);

		sz = namesz + descsz;

		if (count < sz)
			sz = count;

		(void) memcpy(dst, src, sz);

		src += sz;
		dst += sz;
		count -= sz;
	}
}

MAKE_TYPE_CONVERTERS(ELF_TYPE_LIST)

struct converters {
	void	(*tof32)(char *dst, char *src, size_t cnt, int byteswap);
	void	(*tom32)(char *dst, char *src, size_t cnt, int byteswap);
	void	(*tof64)(char *dst, char *src, size_t cnt, int byteswap);
	void	(*tom64)(char *dst, char *src, size_t cnt, int byteswap);
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
	[ELF_T_NOTE] = {
		.tof32 = libelf_cvt_NOTE_tof,
		.tom32 = libelf_cvt_NOTE_tom,
		.tof64 = libelf_cvt_NOTE_tof,
		.tom64 = libelf_cvt_NOTE_tom
	}
};

void (*_libelf_get_translator(Elf_Type t, int direction, int elfclass))
 (char *_dst, char *_src, size_t _cnt, int _byteswap)
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
