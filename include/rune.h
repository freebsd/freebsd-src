/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 *	@(#)rune.h	8.1 (Berkeley) 6/27/93
 */

#ifndef	_RUNE_H_
#define	_RUNE_H_

#include <runetype.h>
#include <stdio.h>

#define	_PATH_LOCALE	"/usr/share/locale"

#define _INVALID_RUNE   _CurrentRuneLocale->invalid_rune

#define __sgetrune      _CurrentRuneLocale->sgetrune
#define __sputrune      _CurrentRuneLocale->sputrune

#define sgetrune(s, n, r)       (*__sgetrune)((s), (n), (r))
#define sputrune(c, s, n, r)    (*__sputrune)((c), (s), (n), (r))

__BEGIN_DECLS
char	*mbrune __P((const char *, rune_t));
char	*mbrrune __P((const char *, rune_t));
char	*mbmb __P((const char *, char *));
long	 fgetrune __P((FILE *));
int	 fputrune __P((rune_t, FILE *));
int	 fungetrune __P((rune_t, FILE *));
int	 setrunelocale __P((char *));
void	 setinvalidrune __P((rune_t));
__END_DECLS

#endif	/*! _RUNE_H_ */
