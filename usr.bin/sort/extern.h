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

void	 append(const u_char **, int, int, FILE *,
	    void (*)(const RECHEADER *, FILE *), struct field *);
void	 concat(FILE *, FILE *);
length_t enterkey(RECHEADER *, DBT *, size_t, struct field *);
void	 fixit(int *, char **);
void	 fldreset(struct field *);
FILE	*ftmp(void);
void	 fmerge(int, int, struct filelist *, int,
		get_func_t, FILE *, put_func_t, struct field *);
void	 fsort(int, int, int, struct filelist *, int, FILE *,
		struct field *);
int	 geteasy(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 getnext(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 makekey(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
int	 makeline(int, int, struct filelist *,
	    int, RECHEADER *, u_char *, struct field *);
void	 merge(int, int, get_func_t, FILE *, put_func_t, struct field *);
void	 num_init(void);
void	 onepass(const u_char **, int, long, long *, u_char *, FILE *);
int	 optval(int, int);
void	 order(struct filelist *, get_func_t, struct field *);
void	 putline(const RECHEADER *, FILE *);
void	 putrec(const RECHEADER *, FILE *);
void	 rd_append(int, int, int, FILE *, u_char *, u_char *);
int	 setfield(const char *, struct field *, int);
void	 settables(int);
