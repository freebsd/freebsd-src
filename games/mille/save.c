/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)save.c	5.6 (Berkeley) 6/1/90";
#endif /* not lint */

#include	"mille.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<string.h>
#ifndef	unctrl
#include	"unctrl.h"
#endif

# ifdef	attron
#	include	<term.h>
#	define	_tty	cur_term->Nttyb
# endif	attron

/*
 * @(#)save.c	1.2 (Berkeley) 3/28/83
 */

typedef	struct stat	STAT;

char	*ctime();

int	read(), write();

/*
 *	This routine saves the current game for use at a later date
 */

save() {

	extern int	errno;
	reg char	*sp;
	reg int		outf;
	reg time_t	*tp;
	char		buf[80];
	time_t		tme;
	STAT		junk;

	tp = &tme;
	if (Fromfile && getyn(SAMEFILEPROMPT))
		strcpy(buf, Fromfile);
	else {
over:
		prompt(FILEPROMPT);
		leaveok(Board, FALSE);
		refresh();
		sp = buf;
		while ((*sp = readch()) != '\n') {
			if (*sp == killchar())
				goto over;
			else if (*sp == erasechar()) {
				if (--sp < buf)
					sp = buf;
				else {
					addch('\b');
					/*
					 * if the previous char was a control
					 * char, cover up two characters.
					 */
					if (*sp < ' ')
						addch('\b');
					clrtoeol();
				}
			}
			else {
				addstr(unctrl(*sp));
				++sp;
			}
			refresh();
		}
		*sp = '\0';
		leaveok(Board, TRUE);
	}

	/*
	 * check for existing files, and confirm overwrite if needed
	 */

	if (sp == buf || (!Fromfile && stat(buf, &junk) > -1
	    && getyn(OVERWRITEFILEPROMPT) == FALSE))
		return FALSE;

	if ((outf = creat(buf, 0644)) < 0) {
		error(strerror(errno));
		return FALSE;
	}
	mvwaddstr(Score, ERR_Y, ERR_X, buf);
	wrefresh(Score);
	time(tp);			/* get current time		*/
	strcpy(buf, ctime(tp));
	for (sp = buf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
	varpush(outf, write);
	close(outf);
	wprintw(Score, " [%s]", buf);
	wclrtoeol(Score);
	wrefresh(Score);
	return TRUE;
}

/*
 *	This does the actual restoring.  It returns TRUE if the
 * backup was made on exiting, in which case certain things must
 * be cleaned up before the game starts.
 */
rest_f(file)
reg char	*file; {

	reg char	*sp;
	reg int		inf;
	char		buf[80];
	STAT		sbuf;

	if ((inf = open(file, 0)) < 0) {
		perror(file);
		exit(1);
	}
	if (fstat(inf, &sbuf) < 0) {		/* get file stats	*/
		perror(file);
		exit(1);
	}
	varpush(inf, read);
	close(inf);
	strcpy(buf, ctime(&sbuf.st_mtime));
	for (sp = buf; *sp != '\n'; sp++)
		continue;
	*sp = '\0';
	/*
	 * initialize some necessary values
	 */
	(void)sprintf(Initstr, "%s [%s]\n", file, buf);
	Fromfile = file;
	return !On_exit;
}

