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
 *	from: @(#)extern.h	8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/bin/ls/extern.h,v 1.7.2.3 2000/07/12 06:19:14 assar Exp $
 */

int	 acccmp __P((const FTSENT *, const FTSENT *));
int	 revacccmp __P((const FTSENT *, const FTSENT *));
int	 modcmp __P((const FTSENT *, const FTSENT *));
int	 revmodcmp __P((const FTSENT *, const FTSENT *));
int	 namecmp __P((const FTSENT *, const FTSENT *));
int	 revnamecmp __P((const FTSENT *, const FTSENT *));
int	 statcmp __P((const FTSENT *, const FTSENT *));
int	 revstatcmp __P((const FTSENT *, const FTSENT *));

void	 printcol __P((DISPLAY *));
void	 printlong __P((DISPLAY *));
void	 printscol __P((DISPLAY *));
void	 usage __P((void));
int	 len_octal __P((const char *, int));
int	 prn_octal __P((const char *));
int	 prn_printable __P((const char *));
#ifdef COLORLS
void	 parsecolors __P((char *cs));
void     colorquit __P((int));

extern  char    *ansi_fgcol;
extern  char    *ansi_bgcol;
extern  char    *ansi_coloff;
#endif
