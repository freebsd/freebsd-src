/*-
 * Copyright (c) 1993
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
 *	@(#)extern.h	8.1 (Berkeley) 6/11/93
 */

void	 addword __P((char *));
void	 badword __P((void));
char	*batchword __P((FILE *));
void	 checkdict __P((void));
int	 checkword __P((char *, int, int *));
void	 cleanup __P((void));
void	 delay __P((int));
long	 dictseek __P((FILE *, long, int));
void	 findword __P((void));
void	 flushin __P((FILE *));
char	*getline __P((char *));
void	 getword __P((char *));
int	 help __P((void));
int	 inputch __P((void));
int	 loaddict __P((FILE *));
int	 loadindex __P((char *));
void	 newgame __P((char *));
char	*nextword __P((FILE *));
FILE	*opendict __P((char *));
void	 playgame __P((void));
void	 prompt __P((char *));
void	 prtable __P((char *[],
	    int, int, int, void (*)(char *[], int), int (*)(char *[], int)));
void	 putstr __P((char *));
void	 redraw __P((void));
void	 results __P((void));
int	 setup __P((int, long));
void	 showboard __P((char *));
void	 showstr __P((char *, int));
void	 showword __P((int));
void	 starttime __P((void));
void	 startwords __P((void));
void	 stoptime __P((void));
int	 timerch __P((void));
void	 usage __P((void));
int	 validword __P((char *));
