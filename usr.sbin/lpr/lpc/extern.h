/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
 *      @(#)extern.h	8.1 (Berkeley) 6/6/93
 *
 * $FreeBSD$
 */


#include <sys/types.h>
#include <sys/cdefs.h>


__BEGIN_DECLS
void	 clean_q(struct printer *_pp);
void	 disable(struct printer *_pp);
void	 doabort(struct printer *_pp);
void	 down(int _argc, char *_argv[]);
void	 enable(struct printer *_pp);
void	 generic(void (*_specificrtn)(struct printer *_pp),
	    void (*_initcmd)(int _argc, char *_argv[]),
	    int _argc, char *_argv[]);
void	 help(int _argc, char *_argv[]);
void	 init_clean(int _argc, char *_argv[]);
void	 init_tclean(int _argc, char *_argv[]);
void	 quit(int _argc, char *_argv[]);
void	 restart(struct printer *_pp);
void	 startcmd(struct printer *_pp);
void	 status(struct printer *_pp);
void	 stop(struct printer *_pp);
void	 topq(int _argc, char *_argv[]);
void	 up(struct printer *_pp);
__END_DECLS

extern int NCMDS;
extern struct cmd cmdtab[];
extern uid_t	uid, euid;
