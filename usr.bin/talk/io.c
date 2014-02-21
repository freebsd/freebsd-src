/*
 * Copyright (c) 1983, 1993
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

#ifndef lint
static const char sccsid[] = "@(#)io.c	8.1 (Berkeley) 6/6/93";
#endif

/*
 * This file contains the I/O handling and the exchange of
 * edit characters. This connection itself is established in
 * ctl.c
 */

#include <sys/filio.h>

#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "talk.h"
#include "talk_ctl.h"

#define A_LONG_TIME 10000000

volatile sig_atomic_t gotwinch = 0;

/*
 * The routine to do the actual talking
 */
void
talk(void)
{
	struct hostent *hp, *hp2;
	int nb;
	fd_set read_set, read_template;
	char buf[BUFSIZ], **addr, *his_machine_name;
	struct timeval wait;

	his_machine_name = NULL;
	hp = gethostbyaddr((const char *)&his_machine_addr.s_addr,
	    sizeof(his_machine_addr.s_addr), AF_INET);
	if (hp != NULL) {
		hp2 = gethostbyname(hp->h_name);
		if (hp2 != NULL && hp2->h_addrtype == AF_INET &&
		    hp2->h_length == sizeof(his_machine_addr))
			for (addr = hp2->h_addr_list; *addr != NULL; addr++)
				if (memcmp(*addr, &his_machine_addr,
				    sizeof(his_machine_addr)) == 0) {
					his_machine_name = strdup(hp->h_name);
					break;
				}
	}
	if (his_machine_name == NULL)
		his_machine_name = strdup(inet_ntoa(his_machine_addr));
	snprintf(buf, sizeof(buf), "Connection established with %s@%s.",
	    msg.r_name, his_machine_name);
	free(his_machine_name);
	message(buf);
	write(STDOUT_FILENO, "\007\007\007", 3);
	
	current_line = 0;

	/*
	 * Wait on both the other process (sockt_mask) and
	 * standard input ( STDIN_MASK )
	 */
	FD_ZERO(&read_template);
	FD_SET(sockt, &read_template);
	FD_SET(fileno(stdin), &read_template);
	for (;;) {
		read_set = read_template;
		wait.tv_sec = A_LONG_TIME;
		wait.tv_usec = 0;
		nb = select(32, &read_set, 0, 0, &wait);
		if (gotwinch) {
			resize_display();
			gotwinch = 0;
		}
		if (nb <= 0) {
			if (errno == EINTR) {
				read_set = read_template;
				continue;
			}
			/* panic, we don't know what happened */
			p_error("Unexpected error from select");
			quit();
		}
		if (FD_ISSET(sockt, &read_set)) {
			/* There is data on sockt */
			nb = read(sockt, buf, sizeof buf);
			if (nb <= 0) {
				message("Connection closed. Exiting");
				quit();
			}
			display(&his_win, buf, nb);
		}
		if (FD_ISSET(fileno(stdin), &read_set)) {
			/*
			 * We can't make the tty non_blocking, because
			 * curses's output routines would screw up
			 */
			int i;
			ioctl(0, FIONREAD, (void *) &nb);
			if (nb > (ssize_t)(sizeof buf))
				nb = sizeof buf;
			nb = read(STDIN_FILENO, buf, nb);
			display(&my_win, buf, nb);
			/* might lose data here because sockt is non-blocking */
			for (i = 0; i < nb; ++i)
				if (buf[i] == '\r')
					buf[i] = '\n';
			write(sockt, buf, nb);
		}
	}
}

/*
 * p_error prints the system error message on the standard location
 * on the screen and then exits. (i.e. a curses version of perror)
 */
void
p_error(const char *string)
{
	wmove(my_win.x_win, current_line, 0);
	wprintw(my_win.x_win, "[%s : %s (%d)]\n",
	    string, strerror(errno), errno);
	wrefresh(my_win.x_win);
	move(LINES-1, 0);
	refresh();
	quit();
}

/*
 * Display string in the standard location
 */
void
message(const char *string)
{
	wmove(my_win.x_win, current_line, 0);
	wprintw(my_win.x_win, "[%s]\n", string);
	if (current_line < my_win.x_nlines - 1)
		current_line++;
	wrefresh(my_win.x_win);
}
