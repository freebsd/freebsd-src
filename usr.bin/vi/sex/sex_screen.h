/*-
 * Copyright (c) 1993, 1994
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
 *	@(#)sex_screen.h	8.20 (Berkeley) 8/8/94
 */

#define	SEX_NORAW(t)							\
	    tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &(t))

#define	SEX_RAW(t) {							\
	struct termios __rawt;						\
	if (tcgetattr(STDIN_FILENO, &(t)))				\
		return (1);						\
	__rawt = (t);							\
	__rawt.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INLCR|IGNCR|ICRNL);	\
	__rawt.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);		\
	__rawt.c_oflag |= OPOST | ONLCR;				\
	__rawt.c_cc[VMIN] = 1;						\
	__rawt.c_cc[VTIME] = 0;						\
	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &__rawt))	\
		return (1);						\
}

typedef struct _sex_private {
/* INITIALIZED AT SCREEN CREATE. */
	int	 __sex_unused;		/* Make sure it's not empty. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
#ifndef SYSV_CURSES
	char	*SE, *SO;		/* Inverse video termcap strings. */
#endif
} SEX_PRIVATE;

#define	SXP(sp)		((SEX_PRIVATE *)((sp)->sex_private))

void	sex_bell __P((SCR *));
void	sex_busy __P((SCR *, char const *));
enum confirm
	sex_confirm __P((SCR *, EXF *, MARK *, MARK *));
enum input
	sex_get __P((SCR *, EXF *, TEXTH *, ARG_CHAR_T, u_int));
enum input
	sex_key_read __P((SCR *, int *, struct timeval *));
int	sex_optchange __P((SCR *, int));
int	sex_refresh __P((SCR *, EXF *));
int	sex_screen_copy __P((SCR *, SCR *));
int	sex_screen_edit __P((SCR *, EXF *));
int	sex_screen_end __P((SCR *));
int	sex_suspend __P((SCR *));
int	sex_window __P((SCR *, int));
