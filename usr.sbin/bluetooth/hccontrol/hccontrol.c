/*
 * hccontrol.c
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
 * $Id: hccontrol.c,v 1.11 2002/09/12 18:19:43 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <bitstring.h>
#include <err.h>
#include <errno.h>
#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "hccontrol.h"

/* Prototypes */
static int                  do_hci_command    (char const *, int, char **);
static struct hci_command * find_hci_command  (char const *, struct hci_command *);
static void                 print_hci_command (struct hci_command *);
static void usage                             (void);

/* Globals */
int	 verbose = 0; 
int	 timeout;

/* Main */
int
main(int argc, char *argv[])
{
	char	*node = NULL;
	int	 n;

	/* Process command line arguments */
	while ((n = getopt(argc, argv, "n:v")) != -1) {
		switch (n) {
		case 'n':
			node = optarg;
			break;

		case 'v':
			verbose = 1;
			break;

		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (*argv == NULL)
		usage();

	n = do_hci_command(node, argc, argv);

	return (n);
} /* main */

/* Create socket and bind it */
static int
socket_open(char const *node)
{
	struct sockaddr_hci			addr;
	struct ng_btsocket_hci_raw_filter	filter;
	int					s, mib[4];
	size_t					size;

	if (node == NULL)
		usage();

	s = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_HCI);
	if (s < 0)
		err(1, "Could not create socket");

	memset(&addr, 0, sizeof(addr));
	addr.hci_len = sizeof(addr);
	addr.hci_family = AF_BLUETOOTH;
	strncpy(addr.hci_node, node, sizeof(addr.hci_node));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		err(2, "Could not bind socket, node=%s", node);

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		err(3, "Could not connect socket, node=%s", node);

	memset(&filter, 0, sizeof(filter));
	bit_set(filter.event_mask, NG_HCI_EVENT_COMMAND_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_COMMAND_STATUS - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_INQUIRY_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_INQUIRY_RESULT - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_CON_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_DISCON_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_REMOTE_NAME_REQ_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_READ_REMOTE_FEATURES_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_READ_REMOTE_VER_INFO_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_RETURN_LINK_KEYS - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_READ_CLOCK_OFFSET_COMPL - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_CON_PKT_TYPE_CHANGED - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_ROLE_CHANGE - 1);

	if (setsockopt(s, SOL_HCI_RAW, SO_HCI_RAW_FILTER, 
			(void * const) &filter, sizeof(filter)) < 0)
		err(4, "Could not setsockopt()");

	size = (sizeof(mib)/sizeof(mib[0]));
	if (sysctlnametomib("net.bluetooth.hci.command_timeout",mib,&size) < 0)
		err(5, "Could not sysctlnametomib()");

	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]),
			(void *) &timeout, &size, NULL, 0) < 0)
		err(6, "Could not sysctl()");

	timeout ++;

	return (s);
} /* socket_open */

/* Execute commands */
static int
do_hci_command(char const *node, int argc, char **argv)
{
	char			*cmd = argv[0];
	struct hci_command	*c = NULL;
	int			 s, e, help;
	
	help = 0;
	if (strcasecmp(cmd, "help") == 0) {
		argc --;
		argv ++;

		if (argc <= 0) {
			fprintf(stdout, "Supported commands:\n");
			print_hci_command(link_control_commands);
			print_hci_command(link_policy_commands);
			print_hci_command(host_controller_baseband_commands);
			print_hci_command(info_commands);
			print_hci_command(status_commands);
			print_hci_command(node_commands);
			fprintf(stdout, "\nFor more information use " \
				"'help command'\n");

			return (OK);
		}

		help = 1;
		cmd = argv[0];
	}

	c = find_hci_command(cmd, link_control_commands);
	if (c != NULL)
		goto execute;

	c = find_hci_command(cmd, link_policy_commands);
	if (c != NULL)
		goto execute;

	c = find_hci_command(cmd, host_controller_baseband_commands);
	if (c != NULL)
		goto execute;

	c = find_hci_command(cmd, info_commands);
	if (c != NULL)
		goto execute;

	c = find_hci_command(cmd, status_commands);
	if (c != NULL)
		goto execute;

	c = find_hci_command(cmd, node_commands);
	if (c == NULL) {
		fprintf(stdout, "Unknown command: \"%s\"\n", cmd);
		return (ERROR);
	}
execute:
	if (!help) {
		s = socket_open(node);
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
} /* do_hci_command */

/* Try to find command in specified category */
static struct hci_command *
find_hci_command(char const *command, struct hci_command *category)
{
	struct hci_command	*c = NULL;

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
} /* find_hci_command */

/* Try to find command in specified category */
static void
print_hci_command(struct hci_command *category)
{
	struct hci_command	*c = NULL;

	for (c = category; c->command != NULL; c++)
		fprintf(stdout, "\t%s\n", c->command);
} /* print_hci_command */

/* Usage */
static void
usage(void)
{
	fprintf(stdout, "Usage: hccontrol -n HCI_node_name cmd [p1] [..]]\n");
	exit(255);
} /* usage */

