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
 *	@(#)sex_screen.h	8.12 (Berkeley) 11/29/93
 */

#define	SEX_NORAW(t)							\
	    tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &(t))

#define	SEX_RAW(t, rawt) {						\
	if (tcgetattr(STDIN_FILENO, &(t)))				\
		return (1);						\
	(rawt) = (t);							\
	(rawt).c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INLCR|IGNCR|ICRNL);	\
	(rawt).c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);		\
	(rawt).c_cc[VMIN] = 1;						\
	(rawt).c_cc[VTIME] = 0;						\
	if (tcsetattr(STDIN_FILENO, TCSADRAIN | TCSASOFT, &(rawt)))	\
		return (1);						\
}

void	sex_bell __P((SCR *));
void	sex_busy __P((SCR *, char const *));
enum confirm
	sex_confirm __P((SCR *, EXF *, MARK *, MARK *));
enum input
	sex_get __P((SCR *, EXF *, TEXTH *, int, u_int));
enum input
	sex_get_notty __P((SCR *, EXF *, TEXTH *, int, u_int));
enum input
	sex_key_read __P((SCR *, int *, struct timeval *));
int	sex_refresh __P((SCR *, EXF *));
int	sex_screen_copy __P((SCR *, SCR *));
int	sex_screen_edit __P((SCR *, EXF *));
int	sex_screen_end __P((SCR *));
int	sex_split __P((SCR *, char *[]));
int	sex_suspend __P((SCR *));
