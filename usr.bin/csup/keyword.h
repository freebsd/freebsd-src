/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _KEYWORD_H_
#define _KEYWORD_H_

/* CVS expansion modes. */
#define	EXPAND_DEFAULT		0
#define	EXPAND_KEYVALUE		1
#define	EXPAND_KEYVALUELOCKER	2
#define	EXPAND_KEY		3
#define	EXPAND_OLD		4
#define	EXPAND_BINARY		5
#define	EXPAND_VALUE		6

struct diffinfo;
struct keyword;

struct keyword	*keyword_new(void);
int		 keyword_decode_expand(const char *);
const char	*keyword_encode_expand(int);
int		 keyword_alias(struct keyword *, const char *, const char *);
int		 keyword_enable(struct keyword *, const char *);
int		 keyword_disable(struct keyword *, const char *);
void		 keyword_prepare(struct keyword *);
int		 keyword_expand(struct keyword *, struct diffinfo *, char *,
		     size_t, char **, size_t *);
void		 keyword_free(struct keyword *);

#endif /* !_KEYWORD_H_ */
