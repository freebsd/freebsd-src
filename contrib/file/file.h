/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * file.h - definitions for file(1) program
 * @(#)$Id: file.h,v 1.61 2004/05/12 14:53:01 christos Exp $
 */

#ifndef __file_h__
#define __file_h__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>	/* Include that here, to make sure __P gets defined */
#include <errno.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
/* Do this here and now, because struct stat gets re-defined on solaris */
#include <sys/stat.h>

#ifndef MAGIC
#define MAGIC "/etc/magic"
#endif

#ifdef __EMX__
#define PATHSEP	';'
#else
#define PATHSEP	':'
#endif

#define private static
#ifndef protected
#define protected
#endif
#define public

#ifndef HOWMANY
# define HOWMANY 65536		/* how much of the file to look at */
#endif
#define MAXMAGIS 4096		/* max entries in /etc/magic */
#define MAXDESC	64		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

#define MAGICNO		0xF11E041C
#define VERSIONNO	2
#define FILE_MAGICSIZE	(32 * 4)

#define	FILE_LOAD	0
#define FILE_CHECK	1
#define FILE_COMPILE	2

struct magic {
	/* Word 1 */
	uint16_t cont_level;	/* level of ">" */
	uint8_t nospflag;	/* supress space character */
	uint8_t flag;
#define INDIR	1		/* if '>(...)' appears,  */
#define	UNSIGNED 2		/* comparison is unsigned */
#define OFFADD	4		/* if '>&' appears,  */
	/* Word 2 */
	uint8_t reln;		/* relation (0=eq, '>'=gt, etc) */
	uint8_t vallen;		/* length of string value, if any */
	uint8_t type;		/* int, short, long or string. */
	uint8_t in_type;	/* type of indirrection */
#define 			FILE_BYTE	1
#define				FILE_SHORT	2
#define				FILE_LONG	4
#define				FILE_STRING	5
#define				FILE_DATE	6
#define				FILE_BESHORT	7
#define				FILE_BELONG	8
#define				FILE_BEDATE	9
#define				FILE_LESHORT	10
#define				FILE_LELONG	11
#define				FILE_LEDATE	12
#define				FILE_PSTRING	13
#define				FILE_LDATE	14
#define				FILE_BELDATE	15
#define				FILE_LELDATE	16
#define				FILE_REGEX	17

#define				FILE_FORMAT_NAME	\
/* 0 */ 			"invalid 0",		\
/* 1 */				"byte",			\
/* 2 */ 			"short",		\
/* 3 */ 			"invalid 3",		\
/* 4 */ 			"long",			\
/* 5 */ 			"string",		\
/* 6 */ 			"date",			\
/* 7 */ 			"beshort",		\
/* 8 */ 			"belong",		\
/* 9 */ 			"bedate"		\
/* 10 */ 			"leshort",		\
/* 11 */ 			"lelong",		\
/* 12 */ 			"ledate",		\
/* 13 */ 			"pstring",		\
/* 14 */ 			"ldate",		\
/* 15 */ 			"beldate",		\
/* 16 */ 			"leldate",		\
/* 17 */ 			"regex",

#define	FILE_FMT_NUM	"cduxXi"
#define FILE_FMT_STR	"s"	

#define				FILE_FORMAT_STRING	\
/* 0 */ 			NULL,			\
/* 1 */				FILE_FMT_NUM,		\
/* 2 */ 			FILE_FMT_NUM,		\
/* 3 */ 			NULL,			\
/* 4 */ 			FILE_FMT_NUM,		\
/* 5 */ 			FILE_FMT_STR,		\
/* 6 */ 			FILE_FMT_STR,		\
/* 7 */ 			FILE_FMT_NUM,		\
/* 8 */ 			FILE_FMT_NUM,		\
/* 9 */ 			FILE_FMT_STR,		\
/* 10 */ 			FILE_FMT_NUM,		\
/* 11 */ 			FILE_FMT_NUM,		\
/* 12 */ 			FILE_FMT_STR,		\
/* 13 */ 			FILE_FMT_STR,		\
/* 14 */ 			FILE_FMT_STR,		\
/* 15 */ 			FILE_FMT_STR,		\
/* 16 */ 			FILE_FMT_STR,		\
/* 17 */ 			FILE_FMT_STR,

	/* Word 3 */
	uint8_t in_op;		/* operator for indirection */
	uint8_t mask_op;	/* operator for mask */
	uint8_t dummy1;	
	uint8_t dummy2;	
#define				FILE_OPS	"&|^+-*/%"
#define				FILE_OPAND	0
#define				FILE_OPOR	1
#define				FILE_OPXOR	2
#define				FILE_OPADD	3
#define				FILE_OPMINUS	4
#define				FILE_OPMULTIPLY	5
#define				FILE_OPDIVIDE	6
#define				FILE_OPMODULO	7
#define				FILE_OPINVERSE	0x80
	/* Word 4 */
	uint32_t offset;	/* offset to magic number */
	/* Word 5 */
	uint32_t in_offset;	/* offset from indirection */
	/* Word 6 */
	uint32_t mask;	/* mask before comparison with value */
	/* Word 7 */
	uint32_t dummy3;
	/* Word 8 */
	uint32_t dummp4;
	/* Words 9-16 */
	union VALUETYPE {
		uint8_t b;
		uint16_t h;
		uint32_t l;
		char s[MAXstring];
		char *buf;
		uint8_t hs[2];	/* 2 bytes of a fixed-endian "short" */
		uint8_t hl[4];	/* 4 bytes of a fixed-endian "long" */
	} value;		/* either number or string */
	/* Words 17..31 */
	char desc[MAXDESC];	/* description */
};

#define BIT(A)   (1 << (A))
#define STRING_IGNORE_LOWERCASE		BIT(0)
#define STRING_COMPACT_BLANK		BIT(1)
#define STRING_COMPACT_OPTIONAL_BLANK	BIT(2)
#define CHAR_IGNORE_LOWERCASE		'c'
#define CHAR_COMPACT_BLANK		'B'
#define CHAR_COMPACT_OPTIONAL_BLANK	'b'


/* list of magic entries */
struct mlist {
	struct magic *magic;		/* array of magic entries */
	uint32_t nmagic;			/* number of entries in array */
	int mapped;  /* allocation type: 0 => apprentice_file
		      *                  1 => apprentice_map + malloc
		      *                  2 => apprentice_map + mmap */
	struct mlist *next, *prev;
};

struct magic_set {
    struct mlist *mlist;
    struct cont {
	size_t len;
	int32_t *off;
    } c;
    struct out {
	/* Accumulation buffer */
	char *buf;
	char *ptr;
	size_t len;
	size_t size;
	/* Printable buffer */
	char *pbuf;
	size_t psize;
    } o;
    int error;
    int flags;
    int haderr;
};

struct stat;
protected char *file_fmttime(uint32_t, int);
protected int file_buffer(struct magic_set *, const void *, size_t);
protected int file_fsmagic(struct magic_set *, const char *, struct stat *);
protected int file_pipe2file(struct magic_set *, int, const void *, size_t);
protected int file_printf(struct magic_set *, const char *, ...);
protected int file_reset(struct magic_set *);
protected int file_tryelf(struct magic_set *, int, const unsigned char *, size_t);
protected int file_zmagic(struct magic_set *, const unsigned char *, size_t);
protected int file_ascmagic(struct magic_set *, const unsigned char *, size_t);
protected int file_is_tar(struct magic_set *, const unsigned char *, size_t);
protected int file_softmagic(struct magic_set *, const unsigned char *, size_t);
protected struct mlist *file_apprentice(struct magic_set *, const char *, int);
protected uint32_t file_signextend(struct magic_set *, struct magic *, uint32_t);
protected void file_delmagic(struct magic *, int type, size_t entries);
protected void file_badread(struct magic_set *);
protected void file_badseek(struct magic_set *);
protected void file_oomem(struct magic_set *);
protected void file_error(struct magic_set *, int, const char *, ...);
protected void file_magwarn(const char *, ...);
protected void file_mdump(struct magic *);
protected void file_showstr(FILE *, const char *, size_t);
protected size_t file_mbswidth(const char *);
protected const char *file_getbuffer(struct magic_set *);

#ifndef HAVE_STRERROR
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(e) \
	(((e) >= 0 && (e) < sys_nerr) ? sys_errlist[(e)] : "Unknown error")
#endif

#ifndef HAVE_STRTOUL
#define strtoul(a, b, c)	strtol(a, b, c)
#endif

#if defined(HAVE_MMAP) && defined(HAVE_SYS_MMAN_H) && !defined(QUICK)
#define QUICK
#endif

#define FILE_RCSID(id) \
static const char *rcsid(const char *p) { \
	return rcsid(p = id); \
}
#else

#endif /* __file_h__ */
