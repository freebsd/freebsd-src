/*
 * l2control.c
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: l2control.c,v 1.5 2002/09/04 21:30:40 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <bitstring.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "l2control.h"

/* Prototypes */
static int                    do_l2cap_command    (bdaddr_p, int, char **);
static struct l2cap_command * find_l2cap_command  (char const *, 
                                                   struct l2cap_command *);
static void                   print_l2cap_command (struct l2cap_command *);
static void                   usage               (void);

/* Main */
int
main(int argc, char *argv[])
{
	int		n;
	bdaddr_t	bdaddr;

	memset(&bdaddr, 0, sizeof(bdaddr));

	/* Process command line arguments */
	while ((n = getopt(argc, argv, "a:")) != -1) {
		switch (n) {
		case 'a': {
			int	a0, a1, a2, a3, a4, a5;

			if (sscanf(optarg, "%x:%x:%x:%x:%x:%x",
					&a5, &a4, &a3, &a2, &a1, &a0) != 6) {
				usage();
				break;
			}

			bdaddr.b[0] = (a0 & 0xff);
			bdaddr.b[1] = (a1 & 0xff);
			bdaddr.b[2] = (a2 & 0xff);
			bdaddr.b[3] = (a3 & 0xff);
			bdaddr.b[4] = (a4 & 0xff);
			bdaddr.b[5] = (a5 & 0xff);
			} break;

		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (*argv == NULL)
		usage();

	return (do_l2cap_command(&bdaddr, argc, argv));
} /* main */

/* Execute commands */
static int
do_l2cap_command(bdaddr_p bdaddr, int argc, char **argv)
{
	char			*cmd = argv[0];
	struct l2cap_command	*c = NULL;
	struct sockaddr_l2cap	 sa;
	int			 s, e, help;

	help = 0;
	if (strcasecmp(cmd, "help") == 0) {
		argc --;
		argv ++;

		if (argc <= 0) {
			fprintf(stdout, "Supported commands:\n");
			print_l2cap_command(l2cap_commands);
			fprintf(stdout, "\nFor more information use " \
				"'help command'\n");

			return (OK);
		}

		help = 1;
		cmd = argv[0];
	}

	c = find_l2cap_command(cmd, l2cap_commands);
	if (c == NULL) {
		fprintf(stdout, "Unknown command: \"%s\"\n", cmd);
		return (ERROR);
	}

	if (!help) {
		if (memcmp(bdaddr, NG_HCI_BDADDR_ANY, sizeof(*bdaddr)) == 0)
			usage();

		memset(&sa, 0, sizeof(sa));
		sa.l2cap_len = sizeof(sa);
		sa.l2cap_family = AF_BLUETOOTH;
		memcpy(&sa.l2cap_bdaddr, bdaddr, sizeof(sa.l2cap_bdaddr));

		s = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_L2CAP);
		if (s < 0)
			err(1, "Could not create socket");
	
		if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
			err(2,
"Could not bind socket, bdaddr=%x:%x:%x:%x:%x:%x",
				sa.l2cap_bdaddr.b[5], sa.l2cap_bdaddr.b[4],
				sa.l2cap_bdaddr.b[3], sa.l2cap_bdaddr.b[2],
				sa.l2cap_bdaddr.b[1], sa.l2cap_bdaddr.b[0]);

		if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0)
			err(2,
"Could not connect socket, bdaddr=%x:%x:%x:%x:%x:%x",
				sa.l2cap_bdaddr.b[5], sa.l2cap_bdaddr.b[4],
				sa.l2cap_bdaddr.b[3], sa.l2cap_bdaddr.b[2],
				sa.l2cap_bdaddr.b[1], sa.l2cap_bdaddr.b[0]);

		e = 0x0ffff;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &e, sizeof(e)) < 0)
			err(3, "Coult not setsockopt(RCVBUF, %d)", e);

		e = (c->handler)(s, -- argc, ++ argv);

		close(s);
	} else
		e = USAGE;

	switch (e) {
	case OK:
	case FAILED:
		break;

	case ERROR:
		fprintf(stdout, "Could not execute command \"%s\". %s\n",
			cmd, strerror(errno));
		break;

	case USAGE:
		fprintf(stdout, "Usage: %s\n%s\n", c->command, c->description);
		break;

	default: assert(0); break;
	}

	return (e);
} /* do_l2cap_command */

/* Try to find command in specified category */
static struct l2cap_command *
find_l2cap_command(char const *command, struct l2cap_command *category)
{
	struct l2cap_command	*c = NULL;

	for (c = category; c->command != NULL; c++) {
		char 	*c_end = strchr(c->command, ' ');

		if (c_end != NULL) {
			int	len = c_end - c->command;

			if (strncasecmp(command, c->command, len) == 0)
				return (c);
		} else if (strcasecmp(command, c->command) == 0)
				return (c);
	}

	return (NULL);
} /* find_l2cap_command */

/* Try to find command in specified category */
static void
print_l2cap_command(struct l2cap_command *category)
{
	struct l2cap_command	*c = NULL;

	for (c = category; c->command != NULL; c++)
		fprintf(stdout, "\t%s\n", c->command);
} /* print_l2cap_command */

/* Usage */
static void
usage(void)
{
	fprintf(stdout, "Usage: l2control -a BD_ADDR cmd [p1] [..]]\n");
	exit(255);
} /* usage */

