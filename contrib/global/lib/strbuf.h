/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	strbuf.h					5-Jul-98
 *
 */

#ifndef _STRBUF_H
#define _STRBUF_H

#include <string.h>
#define INITIALSIZE 80
#define EXPANDSIZE 80

typedef struct {
	char	*sbuf;
	char	*endp;
	char	*curp;
	int	sbufsize;
} STRBUF;

#define strputc(sb,c)	do {\
	if (sb->curp + 1 > sb->endp)\
		expandbuf(sb, 0);\
	*sb->curp++ = c;\
} while (0)
#define strunputc(sb,c)	do {\
	if (sb->curp > sb->sbuf && *(sb->curp - 1) == c)\
		sb->curp--;\
} while (0)
#define strnputs(sb, s, len) do {\
	unsigned int _length = len;\
	if (sb->curp + _length > sb->endp)\
		expandbuf(sb, _length);\
	strncpy(sb->curp, s, _length);\
	sb->curp += _length;\
} while (0)
#define strputs(sb, s) strnputs(sb, s, strlen(s))

#ifndef __P
#if defined(__STDC__)
#define __P(protos)     protos
#else
#define __P(protos)     ()
#endif
#endif

void	expandbuf __P((STRBUF *, int));
STRBUF	*stropen __P((void));
void	strstart __P((STRBUF *));
int	strbuflen __P((STRBUF *));
char	*strvalue __P((STRBUF *));
void	strclose __P((STRBUF *));

#endif /* ! _STRBUF_H */
