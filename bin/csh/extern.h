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
 *	@(#)extern.h	8.1 (Berkeley) 5/31/93
 */

#include <sys/cdefs.h>

/*
 * csh.c
 */
int	gethdir __P((Char *));
void	dosource __P((Char **, struct command *));
void	exitstat __P((void));
void	goodbye __P((void));
void	importpath __P((Char *));
void	initdesc __P((void));
void	pintr __P((int));
void	pintr1 __P((bool));
void	printprompt __P((void));
void	process __P((bool));
void	rechist __P((void));
void	untty __P((void));
int	vis_fputc __P((int, FILE *));

#ifdef PROF
void done __P((int));
#else
void xexit __P((int));
#endif

/*
 * dir.c
 */
void	 dinit __P((Char *));
void	 dodirs __P((Char **, struct command *));
Char	*dcanon __P((Char *, Char *));
void	 dtildepr __P((Char *, Char *));
void	 dtilde __P((void));
void	 dochngd __P((Char **, struct command *));
Char	*dnormalize __P((Char *));
void	 dopushd __P((Char **, struct command *));
void	 dopopd __P((Char **, struct command *));
struct directory;
void	 dfree __P((struct directory *));

/*
 * dol.c
 */
void	 Dfix __P((struct command *));
Char	*Dfix1 __P((Char *));
void	 heredoc __P((Char *));

/*
 * err.c
 */
void	seterror __P((int, ...));
void	stderror __P((int, ...));

/*
 * exec.c
 */
void	doexec __P((Char **, struct command *));
void	dohash __P((Char **, struct command *));
void	dounhash __P((Char **, struct command *));
void	dowhich __P((Char **, struct command *));
void	execash __P((Char **, struct command *));
void	hashstat __P((Char **, struct command *));
void	xechoit __P((Char **));

/*
 * exp.c
 */
int	expr __P((Char ***));
int	exp0 __P((Char ***, bool));

/*
 * file.c
 */
#ifdef FILEC
int	tenex __P((Char *, int));
#endif

/*
 * func.c
 */
void	Setenv __P((Char *, Char *));
void	doalias __P((Char **, struct command *));
void	dobreak __P((Char **, struct command *));
void	docontin __P((Char **, struct command *));
void	doecho __P((Char **, struct command *));
void	doelse __P((Char **, struct command *));
void	doend __P((Char **, struct command *));
void	doeval __P((Char **, struct command *));
void	doexit __P((Char **, struct command *));
void	doforeach __P((Char **, struct command *));
void	doglob __P((Char **, struct command *));
void	dogoto __P((Char **, struct command *));
void	doif __P((Char **, struct command *));
void	dolimit __P((Char **, struct command *));
void	dologin __P((Char **, struct command *));
void	dologout __P((Char **, struct command *));
void	donohup __P((Char **, struct command *));
void	doonintr __P((Char **, struct command *));
void	doprintf __P((Char **, struct command *));
void	dorepeat __P((Char **, struct command *));
void	dosetenv __P((Char **, struct command *));
void	dosuspend __P((Char **, struct command *));
void	doswbrk __P((Char **, struct command *));
void	doswitch __P((Char **, struct command *));
void	doumask __P((Char **, struct command *));
void	dounlimit __P((Char **, struct command *));
void	dounsetenv __P((Char **, struct command *));
void	dowhile __P((Char **, struct command *));
void	dozip __P((Char **, struct command *));
void	func __P((struct command *, struct biltins *));
struct	biltins *
	isbfunc __P((struct command *));
void	prvars __P((void));
void	gotolab __P((Char *));
int	srchx __P((Char *));
void	unalias __P((Char **, struct command *));
void	wfree __P((void));

/*
 * glob.c
 */
Char	**dobackp __P((Char *, bool));
void	  Gcat __P((Char *, Char *));
Char	 *globone __P((Char *, int));
int	  Gmatch __P((Char *, Char *));
void	  ginit __P((void));
Char	**globall __P((Char **));
void	  rscan __P((Char **, void (*)()));
void	  tglob __P((Char **));
void	  trim __P((Char **));
#ifdef FILEC
int	  sortscmp __P((const ptr_t, const ptr_t));
#endif /* FILEC */

/*
 * hist.c
 */
void	dohist __P((Char **, struct command *));
struct Hist *
	enthist __P((int, struct wordent *, bool));
void	savehist __P((struct wordent *));

/*
 * lex.c
 */
void	 addla __P((Char *));
void	 bseek __P((struct Ain *));
void	 btell __P((struct Ain *));
void	 btoeof __P((void));
void	 copylex __P((struct wordent *, struct wordent *));
Char	*domod __P((Char *, int));
void	 freelex __P((struct wordent *));
int	 lex __P((struct wordent *));
void	 prlex __P((FILE *, struct wordent *));
int	 readc __P((bool));
void	 settell __P((void));
void	 unreadc __P((int));

/*
 * misc.c
 */
int	  any __P((char *, int));
Char	**blkcat __P((Char **, Char **));
Char	**blkcpy __P((Char **, Char **));
Char	**blkend __P((Char **));
void	  blkfree __P((Char **));
int	  blklen __P((Char **));
void	  blkpr __P((FILE *, Char **));
Char	**blkspl __P((Char **, Char **));
void	  closem __P((void));
Char	**copyblk __P((Char **));
int	  dcopy __P((int, int));
int	  dmove __P((int, int));
void	  donefds __P((void));
Char	  lastchr __P((Char *));
void	  lshift __P((Char **, int));
int	  number __P((Char *));
int	  prefix __P((Char *, Char *));
Char	**saveblk __P((Char **));
void	  setzero __P((char *, int));
Char	 *strip __P((Char *));
char	 *strsave __P((char *));
char	 *strspl __P((char *, char *));
void	  udvar __P((Char *));

#ifndef	SHORT_STRINGS
# ifdef NOTUSED
char	 *strstr __P((const char *, const char *));
# endif /* NOTUSED */
char	 *strend __P((char *));
#endif

/*
 * parse.c
 */
void	alias __P((struct wordent *));
void	freesyn __P((struct command *));
struct command *
	syntax __P((struct wordent *, struct wordent *, int));


/*
 * proc.c
 */
void	dobg __P((Char **, struct command *));
void	dobg1 __P((Char **, struct command *));
void	dofg __P((Char **, struct command *));
void	dofg1 __P((Char **, struct command *));
void	dojobs __P((Char **, struct command *));
void	dokill __P((Char **, struct command *));
void	donotify __P((Char **, struct command *));
void	dostop __P((Char **, struct command *));
void	dowait __P((Char **, struct command *));
void	palloc __P((int, struct command *));
void	panystop __P((bool));
void	pchild __P((int));
void	pendjob __P((void));
struct process *
	pfind __P((Char *));
int	pfork __P((struct command *, int));
void	pgetty __P((int, int));
void	pjwait __P((struct process *));
void	pnote __P((void));
void	prestjob __P((void));
void	psavejob __P((void));
void	pstart __P((struct process *, int));
void	pwait __P((void));

/*
 * sem.c
 */
void	execute __P((struct command *, int, int *, int *));
void	mypipe __P((int *));

/*
 * set.c
 */
struct	varent
	*adrof1 __P((Char *, struct varent *));
void	 doset __P((Char **, struct command *));
void	 dolet __P((Char **, struct command *));
Char	*putn __P((int));
int	 getn __P((Char *));
Char	*value1 __P((Char *, struct varent *));
void	 set __P((Char *, Char *));
void	 set1 __P((Char *, Char **, struct varent *));
void	 setq __P((Char *, Char **, struct varent *));
void	 unset __P((Char **, struct command *));
void	 unset1 __P((Char *[], struct varent *));
void	 unsetv __P((Char *));
void	 setNS __P((Char *));
void	 shift __P((Char **, struct command *));
void	 plist __P((struct varent *));

/*
 * time.c
 */
void	donice __P((Char **, struct command *));
void	dotime __P((Char **, struct command *));
void	prusage __P((struct rusage *, struct rusage *,
	    struct timeval *, struct timeval *));
void	ruadd __P((struct rusage *, struct rusage *));
void	settimes __P((void));
void	tvadd __P((struct timeval *, struct timeval *));
void	tvsub __P((struct timeval *, struct timeval *, struct timeval *));
void	pcsecs __P((long));
void	psecs __P((long));

/*
 * alloc.c
 */
void	Free __P((ptr_t));
ptr_t	Malloc __P((size_t));
ptr_t	Realloc __P((ptr_t, size_t));
ptr_t	Calloc __P((size_t, size_t));
void	showall __P((Char **, struct command *));

/*
 * str.c:
 */
#ifdef SHORT_STRINGS
Char	 *s_strchr __P((Char *, int));
Char	 *s_strrchr __P((Char *, int));
Char	 *s_strcat __P((Char *, Char *));
#ifdef NOTUSED
Char	 *s_strncat __P((Char *, Char *, size_t));
#endif
Char	 *s_strcpy __P((Char *, Char *));
Char	 *s_strncpy __P((Char *, Char *, size_t));
Char	 *s_strspl __P((Char *, Char *));
size_t	  s_strlen __P((Char *));
int	  s_strcmp __P((Char *, Char *));
int	  s_strncmp __P((Char *, Char *, size_t));
Char	 *s_strsave __P((Char *));
Char	 *s_strend __P((Char *));
Char	 *s_strstr __P((Char *, Char *));
Char	 *str2short __P((char *));
Char	**blk2short __P((char **));
char	 *short2str __P((Char *));
char	**short2blk __P((Char **));
#endif
char	 *short2qstr __P((Char *));
char	 *vis_str    __P((Char *));
