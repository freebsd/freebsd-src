/*	$NetBSD: extern.h,v 1.5 2001/01/12 19:31:25 jdolecek Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

void	 append  __P((const u_char **, int, int, FILE *,
	    void (*)(const RECHEADER *, FILE *), struct field *));
void	 concat __P((FILE *, FILE *));
length_t enterkey __P((RECHEADER *, DBT *, size_t, struct field *));
void	 fixit __P((int *, char **));
void	 fldreset __P((struct field *));
FILE	*ftmp __P((void));
void	 fmerge __P((int, int, struct filelist *, int,
		get_func_t, FILE *, put_func_t, struct field *));
void	 fsort __P((int, int, int, struct filelist *, int, FILE *,
		struct field *));
int	 geteasy __P((int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *));
int	 getnext __P((int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *));
int	 makekey __P((int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *));
int	 makeline __P((int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *));
void	 merge __P((int, int, get_func_t, FILE *, put_func_t, struct field *));
void	 num_init __P((void));
void	 onepass __P((const u_char **, int, long, long *, u_char *, FILE *));
int	 optval __P((int, int));
void	 order __P((struct filelist *, get_func_t, struct field *));
void	 putline __P((const RECHEADER *, FILE *));
void	 putrec __P((const RECHEADER *, FILE *));
void	 rd_append __P((int, int, int, FILE *, u_char *, u_char *));
int	 setfield __P((const char *, struct field *, int));
void	 settables __P((int));
