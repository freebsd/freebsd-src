/*
 * hcsecd.c
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
 * $Id: hcsecd.c,v 1.3 2003/04/27 19:45:32 max Exp $
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <bitstring.h>
#include <err.h>
#include <errno.h>
#include <ng_hci.h>
#include <ng_l2cap.h>
#include <ng_btsocket.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "hcsecd.h"

#define	HCSECD_BUFFER_SIZE	512
#define	HCSECD_IDENT		"hcsecd"
#define	HCSECD_PIDFILE		"/var/run/" HCSECD_IDENT ".pid"

static int	done = 0;

static int process_pin_code_request_event
	(int sock, struct sockaddr_hci *addr, bdaddr_p bdaddr);
static int process_link_key_request_event
	(int sock, struct sockaddr_hci *addr, bdaddr_p bdaddr);
static int send_pin_code_reply
	(int sock, struct sockaddr_hci *addr, bdaddr_p bdaddr, char const *pin);
static int send_link_key_reply
	(int sock, struct sockaddr_hci *addr, bdaddr_p bdaddr, u_int8_t *key);
static void sigint
	(int s);
static void usage
	(void);

/* Main */
int
main(int argc, char *argv[])
{
	int					 n, detach, sock, size;
	struct sigaction			 sa;
	struct sockaddr_hci			 addr;
	struct ng_btsocket_hci_raw_filter	 filter;
	char					 buffer[HCSECD_BUFFER_SIZE];
	ng_hci_event_pkt_t			*event = NULL;

	detach = 1;

	while ((n = getopt(argc, argv, "df:h")) != -1) {
		switch (n) {
		case 'd':
			detach = 0;
			break;

		case 'f':
			config_file = optarg;
			break;

		case 'h':
		default:
			usage();
			/* NOT REACHED */
		}
	}

	if (config_file == NULL)
		usage();
		/* NOT REACHED */

	if (getuid() != 0)
		errx(1, "** ERROR: You should run %s as privileged user!",
			HCSECD_IDENT);

	/* Set signal handlers */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint;
	sa.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGINT)");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGINT)");

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = read_config_file;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err(1, "Could not sigaction(SIGHUP)");

	/* Open socket and set filter */
	sock = socket(PF_BLUETOOTH, SOCK_RAW, BLUETOOTH_PROTO_HCI);
	if (sock < 0)
		err(1, "Could not create HCI socket");

	memset(&filter, 0, sizeof(filter));
	bit_set(filter.event_mask, NG_HCI_EVENT_PIN_CODE_REQ - 1);
	bit_set(filter.event_mask, NG_HCI_EVENT_LINK_KEY_REQ - 1);

	if (setsockopt(sock, SOL_HCI_RAW, SO_HCI_RAW_FILTER,
			(void * const) &filter, sizeof(filter)) < 0)
		err(1, "Could not set HCI socket filter");

	if (detach)
		if (daemon(0, 0) < 0)
			err(1, "Could not daemon()ize");

	openlog(HCSECD_IDENT, LOG_NDELAY|LOG_PERROR|LOG_PID, LOG_DAEMON);

	read_config_file(0);

	if (detach) {
		FILE	*pid = NULL;

		if ((pid = fopen(HCSECD_PIDFILE, "w")) == NULL) {
			syslog(LOG_ERR, "Could not create PID file %s. %s (%d)",
					HCSECD_PIDFILE, strerror(errno), errno);
			exit(1);
		}

		fprintf(pid, "%d", getpid());
		fclose(pid);
	}

	event = (ng_hci_event_pkt_t *) buffer;
	while (!done) {
		size = sizeof(addr);
		n = recvfrom(sock, buffer, sizeof(buffer), 0,
				(struct sockaddr *) &addr, &size);
		if (n < 0) {
			if (errno == EINTR)
				continue;

			syslog(LOG_ERR, "Could not receive from HCI socket. " \
					"%s (%d)", strerror(errno), errno);
			exit(1);
		}

		if (event->type != NG_HCI_EVENT_PKT) {
			syslog(LOG_ERR, "Received unexpected HCI packet, " \
					"type=%#x", event->type);
			continue;
		}

		switch (event->event) {
		case NG_HCI_EVENT_PIN_CODE_REQ:
			process_pin_code_request_event(sock, &addr,
							(bdaddr_p)(event + 1));
			break;

		case NG_HCI_EVENT_LINK_KEY_REQ:
			process_link_key_request_event(sock, &addr,
							(bdaddr_p)(event + 1));
			break;

		default:
			syslog(LOG_ERR, "Received unexpected HCI event, " \
					"event=%#x", event->event);
			break;
		}
	}

	if (detach)
		if (remove(HCSECD_PIDFILE) < 0)
			syslog(LOG_ERR, "Could not remove PID file %s. %s (%d)",
					HCSECD_PIDFILE, strerror(errno), errno);

	clean_config();
	closelog();
	close(sock);

	return (0);
}

/* Process PIN_Code_Request event */
static int
process_pin_code_request_event(int sock, struct sockaddr_hci *addr,
		bdaddr_p bdaddr)
{
	link_key_p	key = NULL;

	syslog(LOG_DEBUG, "Got PIN_Code_Request event from '%s', " \
			"remote bdaddr %x:%x:%x:%x:%x:%x", addr->hci_node,
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

	if ((key = get_key(bdaddr, 0)) != NULL) {
		syslog(LOG_DEBUG, "Found matching entry, " \
				"remote bdaddr %x:%x:%x:%x:%x:%x, name '%s', " \
				"PIN code %s",
				key->bdaddr.b[5], key->bdaddr.b[4],
				key->bdaddr.b[3], key->bdaddr.b[2],
				key->bdaddr.b[1], key->bdaddr.b[0],
				(key->name != NULL)? key->name : "No name",
				(key->pin != NULL)? "exists" : "doesn't exist");

		return (send_pin_code_reply(sock, addr, bdaddr, key->pin));
	}

	syslog(LOG_DEBUG, "Could not PIN code for remote bdaddr " \
			"%x:%x:%x:%x:%x:%x",
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

	return (send_pin_code_reply(sock, addr, bdaddr, NULL));
}

/* Process Link_Key_Request event */
static int
process_link_key_request_event(int sock, struct sockaddr_hci *addr,
		bdaddr_p bdaddr)
{
	link_key_p	key = NULL;

	syslog(LOG_DEBUG, "Got Link_Key_Request event from '%s', " \
			"remote bdaddr %x:%x:%x:%x:%x:%x", addr->hci_node,
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

	if ((key = get_key(bdaddr, 0)) != NULL) {
		syslog(LOG_DEBUG, "Found matching entry, " \
				"remote bdaddr %x:%x:%x:%x:%x:%x, name '%s', " \
				"link key %s",
				key->bdaddr.b[5], key->bdaddr.b[4],
				key->bdaddr.b[3], key->bdaddr.b[2],
				key->bdaddr.b[1], key->bdaddr.b[0],
				(key->name != NULL)? key->name : "No name",
				(key->key != NULL)? "exists" : "doesn't exist");

		return (send_link_key_reply(sock, addr, bdaddr, key->key));
	}

	syslog(LOG_DEBUG, "Could not find link key for remote bdaddr " \
			"%x:%x:%x:%x:%x:%x",
			bdaddr->b[5], bdaddr->b[4], bdaddr->b[3],
			bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);

	return (send_link_key_reply(sock, addr, bdaddr, NULL));
}

/* Send PIN_Code_[Negative]_Reply */
static int
send_pin_code_reply(int sock, struct sockaddr_hci *addr, 
		bdaddr_p bdaddr, char const *pin)
{
	u_int8_t		 buffer[HCSECD_BUFFER_SIZE];
	ng_hci_cmd_pkt_t	*cmd = NULL;

	memset(buffer, 0, sizeof(buffer));

	cmd = (ng_hci_cmd_pkt_t *) buffer;
	cmd->type = NG_HCI_CMD_PKT;

	if (pin != NULL) {
		ng_hci_pin_code_rep_cp	*cp = NULL;

		cmd->opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
						NG_HCI_OCF_PIN_CODE_REP));
		cmd->length = sizeof(*cp);

		cp = (ng_hci_pin_code_rep_cp *)(cmd + 1);
		memcpy(&cp->bdaddr, bdaddr, sizeof(cp->bdaddr));
		strncpy(cp->pin, pin, sizeof(cp->pin));
		cp->pin_size = strlen(cp->pin);

		syslog(LOG_DEBUG, "Sending PIN_Code_Reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
	} else {
		ng_hci_pin_code_neg_rep_cp	*cp = NULL;

		cmd->opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
						NG_HCI_OCF_PIN_CODE_NEG_REP));
		cmd->length = sizeof(*cp);

		cp = (ng_hci_pin_code_neg_rep_cp *)(cmd + 1);
		memcpy(&cp->bdaddr, bdaddr, sizeof(cp->bdaddr));

		syslog(LOG_DEBUG, "Sending PIN_Code_Negative_Reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
	}

again:
	if (sendto(sock, buffer, sizeof(*cmd) + cmd->length, 0,
			(struct sockaddr *) addr, sizeof(*addr)) < 0) {
		if (errno == EINTR)
			goto again;

		syslog(LOG_ERR, "Could not send PIN code reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x. %s (%d)",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0],
				strerror(errno), errno);
		return (-1);
	}

	return (0);
}

/* Send Link_Key_[Negative]_Reply */
static int
send_link_key_reply(int sock, struct sockaddr_hci *addr, 
		bdaddr_p bdaddr, u_int8_t *key)
{
	u_int8_t		 buffer[HCSECD_BUFFER_SIZE];
	ng_hci_cmd_pkt_t	*cmd = NULL;

	memset(buffer, 0, sizeof(buffer));

	cmd = (ng_hci_cmd_pkt_t *) buffer;
	cmd->type = NG_HCI_CMD_PKT;

	if (key != NULL) {
		ng_hci_link_key_rep_cp	*cp = NULL;

		cmd->opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
						NG_HCI_OCF_LINK_KEY_REP));
		cmd->length = sizeof(*cp);

		cp = (ng_hci_link_key_rep_cp *)(cmd + 1);
		memcpy(&cp->bdaddr, bdaddr, sizeof(cp->bdaddr));
		memcpy(&cp->key, key, sizeof(cp->key));

		syslog(LOG_DEBUG, "Sending Link_Key_Reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
	} else {
		ng_hci_link_key_neg_rep_cp	*cp = NULL;

		cmd->opcode = htole16(NG_HCI_OPCODE(NG_HCI_OGF_LINK_CONTROL,
						NG_HCI_OCF_LINK_KEY_NEG_REP));
		cmd->length = sizeof(*cp);

		cp = (ng_hci_link_key_neg_rep_cp *)(cmd + 1);
		memcpy(&cp->bdaddr, bdaddr, sizeof(cp->bdaddr));

		syslog(LOG_DEBUG, "Sending Link_Key_Negative_Reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0]);
	}

again:
	if (sendto(sock, buffer, sizeof(*cmd) + cmd->length, 0,
			(struct sockaddr *) addr, sizeof(*addr)) < 0) {
		if (errno == EINTR)
			goto again;

		syslog(LOG_ERR, "Could not send link key reply to '%s' " \
				"for remote bdaddr %x:%x:%x:%x:%x:%x. %s (%d)",
				addr->hci_node,
				bdaddr->b[5], bdaddr->b[4], bdaddr->b[3], 
				bdaddr->b[2], bdaddr->b[1], bdaddr->b[0],
				strerror(errno), errno);
		return (-1);
	}

	return (0);
}

/* Signal handler */
static void
sigint(int s)
{
	syslog(LOG_DEBUG, "Got signal %d, total number of signals %d",
			s, ++ done);
}

/* Display usage and exit */
static void
usage(void)
{
	fprintf(stderr,
"Usage: %s [-d] -f config_file [-h]\n" \
"Where:\n" \
"\t-d              do not detach from terminal\n" \
"\t-f config_file  use <config_file>\n" \
"\t-h              display this message\n", HCSECD_IDENT);

	exit(255);
}

