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
 *      This product includes software developed by Shigio Yamaguchi.
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
 *	gctags.h					17-Oct-98
 */
#include <sys/param.h>
#include "token.h"
/*
 * target type.
 */
#define DEF	1
#define REF	2
#define SYM	3

#define NOTFUNCTION	".notfunction"

extern int	bflag;
extern int	dflag;
extern int	eflag;
extern int	nflag;
extern int	rflag;
extern int	sflag;
extern int	wflag;
extern int      yaccfile;

struct words {
        const char *name;
        int val;
};

#define PUT(tag, lno, line) {						\
	DBG_PRINT(level, line);						\
	if (!nflag) {							\
		if (strlen(tag) >= 16 && lno >= 1000)			\
			printf("%-16s %4d %-16s %s\n",			\
					tag, lno, curfile, line);	\
		else							\
			printf("%-16s%4d %-16s %s\n",			\
					tag, lno, curfile, line);	\
	}								\
}

#define IS_RESERVED(a)  ((a) > 255)

#ifndef __P
#ifdef __STDC__
#define __P(protos)	protos
#else
#define __P(protos)	()
#endif
#endif

#ifdef DEBUG
void    dbg_print __P((int, const char *));
#define DBG_PRINT(level, a) dbg_print(level, a)
#else
#define DBG_PRINT(level, a)
#endif

int	isnotfunction __P((char *));
int	cmp __P((const void *, const void *));
void	C __P((int));
void	assembler __P((void));
void	java __P((void));
