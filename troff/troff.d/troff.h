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
 * Sccsid @(#)troff.h	1.27 (gritter) 8/19/08
 */

extern struct tracktab {
	int	s1;
	int	n1;
	int	s2;
	int	n2;
} *tracktab;

extern struct box {
	int	val[4];
	int	flag;
} mediasize, bleedat, trimat, cropat;

extern struct ref {
	struct ref	*next;
	char	*name;
	int	cnt;
} *anchors, *links, *ulinks;

extern	struct dev	dev;
extern	int		Nfont;
extern	int		*cstab;
extern	int		*ccstab;
extern	int		**fallbacktab;
extern	float		*zoomtab;

extern	int		nchtab;
extern	char		*chname;
extern	int		c_endash;

extern	int		kern;
extern	int		lettrack;
extern	float		horscale;

extern	void		growfonts(int);
extern	void		setlig(int, int);
extern	int		loadafm(int, int, char *, char *, int, enum spec);
extern	int		getkw(tchar, tchar);
extern	void		ptpapersize(void);
extern	void		ptcut(void);
