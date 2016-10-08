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
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "nii.c	1.7	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)nii.c	1.48 (gritter) 3/5/07
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#include "tdef.h"
#ifdef NROFF
#include "tw.h"
#endif
#include "pt.h"
#include "ext.h"

struct 	s *frame, *stk;
int	ejl;
struct	s *nxf;

pid_t	pipeflg = -1;
int	hflg;	/* used in nroff only */
int	eqflg;	/* used in nroff only */

#ifndef NROFF
int	xpts;
int	ppts;
int	pfont;
int	mpts;
int	mfont;
int	cs;
int	ccs;
int	bd;
char	*fchartab;
#else
long	lvmot;
#endif

int	stdi;
int	nofeed;
int	quiet;
int	stop;
char	ibuf[IBUFSZ];
char	xbuf[IBUFSZ];
char	*ibufp;
char	*xbufp;
char	*eibuf;
char	*xeibuf;
tchar	*pbbuf;		/* pushback buffer for arguments, \n, etc. */
int	pbsize;		/* number of members allocated for pbbuf */
int	pbp;		/* next free slot in pbbuf */
int	lastpbp;	/* pbp in previous stack frame */
int	nx;
int	mflg;
tchar	ch = 0;
int	ttyod;
int	iflg;
int	rargc;
char	**argp;
int	*trtab;
int	*trintab;
int	*trnttab;
int	lgf;
int	copyf;
filep	ip;
int	nlflg;
int	donef;
int	nflush;
int	nfo;
int	ifile;
int	padc;
int	raw;
int	ifl[NSO];
int	ifi;
int	flss;
int	nonumb;
int	trap;
int	tflg;
int	ejf;
int	gflag;
int	dilev;
filep	offset;
int	em;
int	ds;
filep	woff;
int	app;
int	ndone;
int	lead;
int	ralss;
filep	nextb;
tchar	nrbits;
int	nform;
int	macerr;
filep	apptr;
int	diflg;
filep	roff;
int	wbfi;
int	vflag;
int	noscale;
int	po1;
int	ev;
int	tty;
int	sfont	= FT;	/* appears to be "standard" font; used by .ul */
int	sv;
int	esc;
int	widthp;
int	xflag = 1;
int	xfont;
int	setwdf;
int	over;
int	nhyp;
tchar	**hyp;
tchar	*olinep;
int	dotT;
int	no_out;
struct	widcache *widcache;
struct	d *d;
struct	d *dip;
int	mb_cur_max;
struct env	initenv;
int	lastkern;
int	lasttrack;
int	defaultpl;
int	NCHARS;
int	NDI;
int	spreadwarn;
int	spreadlimit;
int	lastrq;
long	realpage;
int	tryglf;
char	*gchtab;
int	tailflg;
int	minflg;
int	minspc;
int	blmac;
int	lsmac;
int	glss;
int	lsn;
int	rawwidth;
int	*olt;
int	nolt;
int	clonef;
size_t	olinesz;
tchar	**chartab;
struct charout	*charout;
int	charoutsz;
int	charf;
int	fmtchar;
int	Tflg;
int	dl;
int	padj;
int	defcf;
int	donep;
int	gemu;
int	argdelim;
int	chomp;
int	chompend;
