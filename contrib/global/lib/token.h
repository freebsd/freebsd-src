/*
 * Copyright (c) 1998 Shigio Yamaguchi. All rights reserved.
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
 *	token.h						14-Aug-98
 */

#ifndef _TOKEN_H_
#define _TOKEN_H_

#include <sys/param.h>
#include "mgets.h"
#define MAXTOKEN	512
#define SYMBOL		0

extern char     *sp, *cp, *lp;
extern int      lineno;
extern int	crflag;
extern int	cmode;
extern int	ymode;
extern char	token[MAXTOKEN];
extern char	curfile[MAXPATHLEN];

#define nextchar() \
	(cp == NULL ? \
		((sp = cp = mgets(ip, NULL, 0)) == NULL ? \
			EOF : \
			(lineno++, *cp == 0 ? \
				lp = cp, cp = NULL, '\n' : \
				*cp++)) : \
		(*cp == 0 ? (lp = cp, cp = NULL, '\n') : *cp++))
#define atfirst (sp && sp == (cp ? cp - 1 : lp))

#ifndef __P
#if defined(__STDC__)
#define __P(protos)     protos
#else
#define __P(protos)     ()
#endif
#endif

int	opentoken __P((char *));
void	closetoken __P((void));
int	nexttoken __P((const char *, int (*)(char *)));
void	pushbacktoken __P((void));
int	peekc __P((int));
int     atfirst_exceptspace __P((void));

#endif /* ! _TOKEN_H_ */
