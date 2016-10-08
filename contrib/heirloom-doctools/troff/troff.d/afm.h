/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)afm.h	1.35 (gritter) 10/5/06
 */

#ifndef	TROFF_AFM_H
#define	TROFF_AFM_H

enum spec {
	SPEC_NONE	= 00000,
	SPEC_MATH	= 00001,
	SPEC_GREEK	= 00002,
	SPEC_PUNCT	= 00004,
	SPEC_LARGE	= 00010,
	SPEC_S1		= 01000,
	SPEC_S		= 02000
};

#define	NOCODE		((unsigned short)-1)

#define	NKERNPAIRS	45
struct kernpairs {
	struct kernpairs	*next;
	int	sorted;
	int	cnt;
	unsigned short	ch2[NKERNPAIRS];
	short	k[NKERNPAIRS];
};

struct namecache {
	unsigned short	afpos;
	unsigned short	fival[2];
	unsigned short	gid;
};

struct charpair {
	unsigned short	ch1;
	unsigned short	ch2;
};

struct feature {
	char	*name;
	struct charpair	*pairs;
	int	npairs;
};

extern struct afmtab {
	struct Font	Font;
	char	*encpath;
	char	*path;
	char	*file;
	char	*base;
	char	*fontname;
	char	*supply;
	int	*fontab;
	short	**bbtab;
	char	*kerntab;
	unsigned short	*codetab;
	unsigned short	*fitab;
	char	**nametab;
	int	*unitab;
	int	nunitab;
	void	*unimap;
	int	*encmap;
	struct namecache	*namecache;
	int	nameprime;
	struct kernpairs	*kernpairs;
	struct charpair	*gid2tr;
	int	nspace;
	struct feature	**features;
	int	rq;
	int	lineno;
	int	nchars;
	int	fichars;
	int	capheight;
	int	xheight;
	int	isFixedPitch;
	int	ascender;
	int	descender;
	enum spec	spec;
	enum {
		TYPE_AFM,
		TYPE_OTF,
		TYPE_TTF
	}	type;
} **afmtab;
extern int nafm;

extern	unsigned short	**fitab;
extern	int		**fontab;
extern	char		**kerntab;
extern	unsigned short	**codetab;
extern	struct Font	**fontbase;

extern	int		NCHARS;

extern unsigned short	unitsPerEm;

extern	int	afmget(struct afmtab *, char *, size_t);
extern	int	otfget(struct afmtab *, char *, size_t);
extern	struct namecache	*afmnamelook(struct afmtab *, const char *);
extern	int	afmgetkern(struct afmtab *, int, int);
extern	void	makefont(int, char *, char *, char *, char *, int);
extern	int	unitconv(int);
extern	void	afmalloc(struct afmtab *, int);
extern	void	afmremap(struct afmtab *);
extern	int	afmmapname(const char *, enum spec);
extern	void	afmaddchar(struct afmtab *, int, int, int, int, int[],
			char *, enum spec, int);
extern void	afmaddkernpair(struct afmtab *, int, int, int);
extern	int	nextprime(int n);
extern	unsigned	pjw(const char *);
extern	char	*afmencodepath(const char *);
extern	char	*afmdecodepath(const char *);
#ifdef	DPOST
#include <stdio.h>
extern	int	otfcff(const char *, char *, size_t, size_t *, size_t *);
extern	int	otft42(char *, char *, char *, size_t, FILE *);
extern	int	fprintenc(FILE *, const char *);
#endif

extern struct dev	dev;

#define	_unitconv(i)	(unitsPerEm * 72 == dev.res ? (i) : unitconv(i))

#endif	/* !TROFF_AFM_H */
