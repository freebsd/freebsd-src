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
#if 0
static char sccsid[] = "@(#)get_names.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/talk/get_names.c,v 1.6 1999/08/28 01:06:12 peter Exp $";
#endif /* not lint */

#include <err.h>
#include <pwd.h>
#include <string.h>
#include <sys/param.h>
#include "talk.h"

extern	CTL_MSG msg;

static void
usage(void)
{
	fprintf(stderr, "usage: talk person [ttyname]\n");
	exit(1);
}

/*
 * Determine the local and remote user, tty, and machines
 */
void
get_names(argc, argv)
	int argc;
	char *argv[];
{
	char hostname[MAXHOSTNAMELEN];
	char *his_name, *my_name;
	char *my_machine_name, *his_machine_name;
	char *my_tty, *his_tty;
	register char *cp;

	if (argc < 2 )
		usage();
	if (!isatty(0))
		errx(1, "standard input must be a tty, not a pipe or a file");
	if ((my_name = getlogin()) == NULL) {
		struct passwd *pw;

		if ((pw = getpwuid(getuid())) == NULL)
			errx(1, "you don't exist. Go away");
		my_name = pw->pw_name;
	}
	gethostname(hostname, sizeof (hostname));
	my_machine_name = hostname;
	/* check for, and strip out, the machine name of the target */
	for (cp = argv[1]; *cp && !index("@:!", *cp); cp++)
		;
	if (*cp == '\0') {
		/* this is a local to local talk */
		his_name = argv[1];
		his_machine_name = my_machine_name;
	} else {
		if (*cp++ == '@') {
			/* user@host */
			his_name = argv[1];
			his_machine_name = cp;
		} else {
			/* host!user or host:user */
			his_name = cp;
			his_machine_name = argv[1];
		}
		*--cp = '\0';
	}
	if (argc > 2)
		his_tty = argv[2];	/* tty name is arg 2 */
	else
		his_tty = "";
	get_addrs(my_machine_name, his_machine_name);
	/*
	 * Initialize the message template.
	 */
	msg.vers = TALK_VERSION;
	msg.addr.sa_family = htons(AF_INET);
	msg.ctl_addr.sa_family = htons(AF_INET);
	msg.id_num = htonl(0);
	strncpy(msg.l_name, my_name, NAME_SIZE);
	msg.l_name[NAME_SIZE - 1] = '\0';
	strncpy(msg.r_name, his_name, NAME_SIZE);
	msg.r_name[NAME_SIZE - 1] = '\0';
	strncpy(msg.r_tty, his_tty, TTY_SIZE);
	msg.r_tty[TTY_SIZE - 1] = '\0';
}
