/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)extern.h	8.2 (Berkeley) 1/7/94
 * $FreeBSD$
 */

struct entry	*addentry __P((char *, ino_t, int));
long		 addfile __P((char *, ino_t, int));
int		 addwhiteout __P((char *));
void		 badentry __P((struct entry *, char *));
void	 	 canon __P((char *, char *, int));
void		 checkrestore __P((void));
void		 closemt __P((void));
void		 createfiles __P((void));
void		 createleaves __P((char *));
void		 createlinks __P((void));
long		 deletefile __P((char *, ino_t, int));
void		 deleteino __P((ino_t));
void		 delwhiteout __P((struct entry *));
ino_t		 dirlookup __P((const char *));
void 	 	 done __P((int)) __dead2;
void		 dumpsymtable __P((char *, long));
void	 	 extractdirs __P((int));
int		 extractfile __P((char *));
void		 findunreflinks __P((void));
char		*flagvalues __P((struct entry *));
void		 freeentry __P((struct entry *));
void		 freename __P((char *));
int	 	 genliteraldir __P((char *, ino_t));
char		*gentempname __P((struct entry *));
void		 getfile __P((void (*)(char *, long), void (*)(char *, long)));
void		 getvol __P((long));
void		 initsymtable __P((char *));
int	 	 inodetype __P((ino_t));
int		 linkit __P((char *, char *, int));
struct entry	*lookupino __P((ino_t));
struct entry	*lookupname __P((char *));
long		 listfile __P((char *, ino_t, int));
ino_t		 lowerbnd __P((ino_t));
void		 mktempname __P((struct entry *));
void		 moveentry __P((struct entry *, char *));
void		 msg __P((const char *, ...));
char		*myname __P((struct entry *));
void		 newnode __P((struct entry *));
void		 newtapebuf __P((long));
long		 nodeupdates __P((char *, ino_t, int));
void	 	 onintr __P((int));
void		 panic __P((const char *, ...));
void		 pathcheck __P((char *));
struct direct	*pathsearch __P((const char *));
void		 printdumpinfo __P((void));
void		 removeleaf __P((struct entry *));
void		 removenode __P((struct entry *));
void		 removeoldleaves __P((void));
void		 removeoldnodes __P((void));
void		 renameit __P((char *, char *));
int		 reply __P((char *));
RST_DIR		*rst_opendir __P((const char *));
struct direct	*rst_readdir __P((RST_DIR *));
void		 rst_closedir __P((RST_DIR *dirp));
void	 	 runcmdshell __P((void));
char		*savename __P((char *));
void	 	 setdirmodes __P((int));
void		 setinput __P((char *));
void		 setup __P((void));
void	 	 skipdirs __P((void));
void		 skipfile __P((void));
void		 skipmaps __P((void));
void		 swabst __P((u_char *, u_char *));
void	 	 treescan __P((char *, ino_t, long (*)(char *, ino_t, int)));
ino_t		 upperbnd __P((ino_t));
long		 verifyfile __P((char *, ino_t, int));
void		 xtrnull __P((char *, long));

/* From ../dump/dumprmt.c */
void		rmtclose __P((void));
int		rmthost __P((char *));
int		rmtioctl __P((int, int));
int		rmtopen __P((char *, int));
int		rmtread __P((char *, int));
int		rmtseek __P((int, int));
