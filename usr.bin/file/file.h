/*
 * file.h - definitions for file(1) program
 * @(#)file.h,v 1.2 1993/06/10 00:38:09 jtc Exp
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#define HOWMANY	1024		/* how much of the file to look at */
#define MAXMAGIS 1000		/* max entries in /etc/magic */
#define MAXDESC	50		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

struct magic {
	short flag;		
#define INDIR	1		/* if '>(...)' appears,  */
	short cont_level;	/* level of ">" */
	struct {
		char type;	/* byte short long */
		long offset;	/* offset from indirection */
	} in;
	long offset;		/* offset to magic number */
#define	MASK	0200		/* this is a masked op, like & v1 = v2 */
	unsigned char reln;	/* relation (0=eq, '>'=gt, etc) */
	char type;		/* int, short, long or string. */
	char vallen;		/* length of string value, if any */
#define 			BYTE	1
#define				SHORT	2
#define				LONG	4
#define				STRING	5
#define				DATE	6
#define				BESHORT	7
#define				BELONG	8
#define				BEDATE	9
#define				LESHORT	10
#define				LELONG	11
#define				LEDATE	12
	union VALUETYPE {
		char b;
		short h;
		long l;
		char s[MAXstring];
		unsigned char hs[2];	/* 2 bytes of a fixed-endian "short" */
		unsigned char hl[4];	/* 2 bytes of a fixed-endian "long" */
	} value;		/* either number or string */
	long mask;		/* mask before comparison with value */
	char nospflag;		/* supress space character */
	char desc[MAXDESC];	/* description */
};

#include <stdio.h>	/* Include that here, to make sure __P gets defined */

#ifndef __P
# if __STDC__ || __cplusplus
#  define __P(a) a
# else
#  define __P(a) ()
#  define const
# endif
#endif

extern int   apprentice		__P((char *, int));
extern int   ascmagic		__P((unsigned char *, int));
extern void  error		__P((const char *, ...));
extern void  ckfputs		__P((const char *, FILE *));
struct stat;
extern int   fsmagic		__P((const char *, struct stat *));
extern int   is_compress	__P((const unsigned char *, int *));
extern int   is_tar		__P((unsigned char *));
extern void  magwarn		__P((const char *, ...));
extern void  mdump		__P((struct magic *));
extern void  process		__P((const char *, int));
extern void  showstr		__P((const char *));
extern int   softmagic		__P((unsigned char *, int));
extern void  tryit		__P((unsigned char *, int));
extern int   uncompress		__P((const unsigned char *, unsigned char **, int));
extern void  ckfprintf		__P((FILE *, const char *, ...));



extern int errno;		/* Some unixes don't define this..	*/

extern char *progname;		/* the program name 			*/
extern char *magicfile;		/* name of the magic file		*/
extern int lineno;		/* current line number in magic file	*/

extern struct magic *magic;	/* array of magic entries		*/
extern int nmagic;		/* number of valid magic[]s 		*/


extern int debug;		/* enable debugging?			*/
extern int zflag;		/* process compressed files?		*/
extern int lflag;		/* follow symbolic links?		*/

extern int optind;		/* From getopt(3)			*/
extern char *optarg;

#if !defined(__STDC__) || defined(sun)
extern int sys_nerr;
extern char *sys_errlist[];
#define strerror(e) \
	(((e) >= 0 && (e) < sys_nerr) ? sys_errlist[(e)] : "Unknown error")
#endif

#ifndef MAXPATHLEN
#define	MAXPATHLEN	512
#endif
