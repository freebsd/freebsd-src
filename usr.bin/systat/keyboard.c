/*-
 * Copyright (c) 1980, 1992, 1993
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
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifdef lint
static const char sccsid[] = "@(#)keyboard.c	8.1 (Berkeley) 6/6/93";
#endif

#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>

#include "systat.h"
#include "extern.h"

int
keyboard(void)
{
        char ch, line[80];
	int oldmask;

        for (;;) {
                col = 0;
                move(CMDLINE, 0);
                do {
                        refresh();
                        ch = getch();
                        if (ch == ERR) {
                                if (errno == EINTR)
                                        continue;
                                exit(1);
                        }
                        if (ch >= 'A' && ch <= 'Z')
                                ch += 'a' - 'A';
                        if (col == 0) {
#define	mask(s)	(1 << ((s) - 1))
                                if (ch == CTRL('l')) {
					oldmask = sigblock(mask(SIGALRM));
					wrefresh(curscr);
					sigsetmask(oldmask);
                                        continue;
                                }
				if (ch == CTRL('g')) {
					oldmask = sigblock(mask(SIGALRM));
					status();
					sigsetmask(oldmask);
					continue;
				}
                                if (ch != ':')
                                        continue;
                                move(CMDLINE, 0);
                                clrtoeol();
                        }
                        if (ch == erasechar() && col > 0) {
                                if (col == 1 && line[0] == ':')
                                        continue;
                                col--;
                                goto doerase;
                        }
                        if (ch == CTRL('w') && col > 0) {
                                while (--col >= 0 && isspace(line[col]))
                                        ;
                                col++;
                                while (--col >= 0 && !isspace(line[col]))
                                        if (col == 0 && line[0] == ':')
                                                break;
                                col++;
                                goto doerase;
                        }
                        if (ch == killchar() && col > 0) {
                                col = 0;
                                if (line[0] == ':')
                                        col++;
                doerase:
                                move(CMDLINE, col);
                                clrtoeol();
                                continue;
                        }
                        if (isprint(ch) || ch == ' ') {
                                line[col] = ch;
                                mvaddch(CMDLINE, col, ch);
                                col++;
                        }
                } while (col == 0 || (ch != '\r' && ch != '\n'));
                line[col] = '\0';
		oldmask = sigblock(mask(SIGALRM));
                command(line + 1);
		sigsetmask(oldmask);
        }
	/*NOTREACHED*/
}
