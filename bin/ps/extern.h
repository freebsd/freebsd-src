/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/bin/ps/extern.h,v 1.9 1999/08/27 23:14:50 peter Exp $
 */

struct kinfo;
struct nlist;
struct var;
struct varent;

extern fixpt_t ccpu;
extern int eval, fscale, mempages, nlistread, rawcpu, cflag;
extern int sumrusage, termwidth, totwidth;
extern VAR var[];
extern VARENT *vhead;

__BEGIN_DECLS
void	 command __P((KINFO *, VARENT *));
void	 cputime __P((KINFO *, VARENT *));
int	 donlist __P((void));
void	 evar __P((KINFO *, VARENT *));
char	*fmt_argv __P((char **, char *, int));
double	 getpcpu __P((KINFO *));
double	 getpmem __P((KINFO *));
void	 logname __P((KINFO *, VARENT *));
void	 longtname __P((KINFO *, VARENT *));
void	 lstarted __P((KINFO *, VARENT *));
void	 maxrss __P((KINFO *, VARENT *));
void	 nlisterr __P((struct nlist *));
void	 p_rssize __P((KINFO *, VARENT *));
void	 pagein __P((KINFO *, VARENT *));
void	 parsefmt __P((char *));
void	 pcpu __P((KINFO *, VARENT *));
void	 pmem __P((KINFO *, VARENT *));
void	 pri __P((KINFO *, VARENT *));
void	 rtprior __P((KINFO *, VARENT *));
void	 printheader __P((void));
void	 pvar __P((KINFO *, VARENT *));
void	 rssize __P((KINFO *, VARENT *));
void	 runame __P((KINFO *, VARENT *));
int	 s_runame __P((KINFO *));
void	 rvar __P((KINFO *, VARENT *));
void	 showkey __P((void));
void	 started __P((KINFO *, VARENT *));
void	 state __P((KINFO *, VARENT *));
void	 tdev __P((KINFO *, VARENT *));
void	 tname __P((KINFO *, VARENT *));
void	 tsize __P((KINFO *, VARENT *));
void	 ucomm __P((KINFO *, VARENT *));
void	 uname __P((KINFO *, VARENT *));
int	 s_uname __P((KINFO *));
void	 uvar __P((KINFO *, VARENT *));
void	 vsize __P((KINFO *, VARENT *));
void	 wchan __P((KINFO *, VARENT *));
__END_DECLS
