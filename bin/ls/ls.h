/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ls.h	5.11 (Berkeley) 7/22/90
 *
 *	$Header: /a/cvs/386BSD/src/bin/ls/ls.h,v 1.2 1993/06/29 02:59:32 nate Exp $
 */

typedef struct _lsstruct {
	char *name;			/* file name */
	int len;			/* file name length */
	struct stat lstat;		/* lstat(2) for file */
} LS;

/*
 * overload -- we probably have to save blocks and/or maxlen with the lstat
 * array, so tabdir() stuffs it into unused fields in the first stat structure.
 * If there's ever a type larger than u_long, fix this.  Any calls to qsort
 * must save and restore the values.
 */
#define	st_btotal	st_flags
#define	st_maxlen	st_gen

extern int errno;

extern int f_accesstime;	/* use time of last access */
extern int f_group;		/* show group ownership of a file */
extern int f_inode;		/* print inode */
extern int f_kblocks;		/* print size in kilobytes */
extern int f_longform;		/* long listing format */
extern int f_sectime;		/* print the real time for all files */
extern int f_singlecol;		/* use single column output */
extern int f_size;		/* list size in short listing */
extern int f_statustime;	/* use time of last mode change */
extern int f_total;		/* if precede with "total" line */
extern int f_type;		/* add type character for non-regular files */
