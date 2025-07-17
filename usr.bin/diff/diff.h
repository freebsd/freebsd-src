/*	$OpenBSD: diff.h,v 1.34 2020/11/01 18:16:08 jcs Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/types.h>

#include <stdbool.h>
#include <regex.h>

/*
 * Output format options
 */
#define	D_NORMAL	0	/* Normal output */
#define	D_EDIT		-1	/* Editor script out */
#define	D_REVERSE	1	/* Reverse editor script */
#define	D_CONTEXT	2	/* Diff with context */
#define	D_UNIFIED	3	/* Unified context diff */
#define	D_IFDEF		4	/* Diff with merged #ifdef's */
#define	D_NREVERSE	5	/* Reverse ed script with numbered
				   lines and no trailing . */
#define	D_BRIEF		6	/* Say if the files differ */
#define	D_GFORMAT	7	/* Diff with defined changed group format */
#define D_SIDEBYSIDE    8	/* Side by side */

#define	D_UNSET		-2

/*
 * Algorithms
 */

#define D_DIFFNONE     0
#define D_DIFFSTONE    1       /* Stone or 'old diff' algorithm */
#define D_DIFFMYERS    2       /* Myers diff algorithm */
#define D_DIFFPATIENCE 3       /* Patience diff algorithm */

/*
 * Output flags
 */
#define	D_HEADER	0x001	/* Print a header/footer between files */
#define	D_EMPTY1	0x002	/* Treat first file as empty (/dev/null) */
#define	D_EMPTY2	0x004	/* Treat second file as empty (/dev/null) */

/*
 * Command line flags
 */
#define D_FORCEASCII		0x008	/* Treat file as ascii regardless of content */
#define D_FOLDBLANKS		0x010	/* Treat all white space as equal */
#define D_MINIMAL		0x020	/* Make diff as small as possible */
#define D_IGNORECASE		0x040	/* Case-insensitive matching */
#define D_PROTOTYPE		0x080	/* Display C function prototype */
#define D_EXPANDTABS		0x100	/* Expand tabs to spaces */
#define D_IGNOREBLANKS		0x200	/* Ignore white space changes */
#define D_STRIPCR		0x400	/* Strip trailing cr */
#define D_SKIPBLANKLINES	0x800	/* Skip blank lines */
#define D_MATCHLAST		0x1000	/* Display last line matching provided regex */

/* Features supported by new algorithms */
#define D_NEWALGO_FLAGS                (D_FORCEASCII | D_PROTOTYPE | D_IGNOREBLANKS)

/*
 * Status values for print_status() and diffreg() return values
 */
#define	D_SAME		0	/* Files are the same */
#define	D_DIFFER	1	/* Files are different */
#define	D_BINARY	2	/* Binary files are different */
#define	D_MISMATCH1	3	/* path1 was a dir, path2 a file */
#define	D_MISMATCH2	4	/* path1 was a file, path2 a dir */
#define	D_SKIPPED1	5	/* path1 was a special file */
#define	D_SKIPPED2	6	/* path2 was a special file */
#define	D_ERROR		7	/* A file access error occurred */

/*
 * Color options
 */
#define COLORFLAG_NEVER		0
#define COLORFLAG_AUTO		1
#define COLORFLAG_ALWAYS	2

struct excludes {
	char *pattern;
	struct excludes *next;
};

extern bool	 lflag, Nflag, Pflag, rflag, sflag, Tflag, cflag;
extern bool	 ignore_file_case, suppress_common, color, noderef, algorithm_set;
extern int	 diff_format, diff_context, diff_algorithm, status;
extern bool	 diff_algorithm_set;
extern int	 tabsize, width;
extern char	*start, *ifdefname, *diffargs, *label[2];
extern char	*ignore_pats, *most_recent_pat;
extern char	*group_format;
extern const char	*add_code, *del_code;
extern struct stat stb1, stb2;
extern struct excludes *excludes_list;
extern regex_t	 ignore_re, most_recent_re;

int	 diffreg(char *, char *, int, int);
int	 diffreg_new(char *, char *, int, int);
bool	 can_libdiff(int);
void	 diffdir(char *, char *, int);
void	 print_status(int, char *, char *, const char *);
