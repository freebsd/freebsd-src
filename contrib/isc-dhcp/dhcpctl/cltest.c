/* cltest.c

   Example program that uses the dhcpctl library. */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2000-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 *
 * This software was contributed to Internet Systems Consortium
 * by Brian Murrell.
 */

#ifndef lint
static char ocopyright[] =
"$Id: cltest.c,v 1.11.2.3 2004/06/10 17:59:23 dhankins Exp $ Copyright (c) 2004 Internet Systems Consortium.  All rights reserved.\n";
#endif /* not lint */

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <isc-dhcp/result.h>
#include "dhcpctl.h"

int main (int, char **);

enum modes { up, down, undefined };

static void usage (char *s) {
	fprintf (stderr,
		 "Usage: %s [-n <username>] [-p <password>] [-a <algorithm>]"
		 "(-u | -d) <if>\n", s);
	exit (1);
}

int main (argc, argv)
	int argc;
	char **argv;
{
	isc_result_t status, waitstatus;
	dhcpctl_handle authenticator;
	dhcpctl_handle connection;
	dhcpctl_handle host_handle, group_handle, interface_handle;
	dhcpctl_data_string cid;
	dhcpctl_data_string result, groupname, identifier;
	int i;
	int mode = undefined;
	const char *interface = 0;
	const char *action;
	
	for (i = 1; i < argc; i++) {
		if (!strcmp (argv[i], "-u")) {
			mode = up;
		} else if (!strcmp (argv [i], "-d")) {
			mode = down;
		} else if (argv[i][0] == '-') {
			usage(argv[0]);
		} else {
			interface = argv[i];
		}
	}

	if (!interface)
		usage(argv[0]);
	if (mode == undefined)
		usage(argv[0]);

	status = dhcpctl_initialize ();
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_initialize: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	authenticator = dhcpctl_null_handle;
	connection = dhcpctl_null_handle;

	status = dhcpctl_connect (&connection, "127.0.0.1", 7911,
				  authenticator);
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_connect: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	interface_handle = dhcpctl_null_handle;
	status = dhcpctl_new_object (&interface_handle,
				     connection, "interface");
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_new_object: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	status = dhcpctl_set_string_value (interface_handle,
					   interface, "name");
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_set_value: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	if (mode == up) {
		/* "up" the interface */
		printf ("upping interface %s\n", interface);
		action = "create";
		status = dhcpctl_open_object (interface_handle, connection,
					      DHCPCTL_CREATE | DHCPCTL_EXCL);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "dhcpctl_open_object: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
	} else {
		/* down the interface */
		printf ("downing interface %s\n", interface);
		action = "remove";
		status = dhcpctl_open_object (interface_handle, connection, 0);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "dhcpctl_open_object: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
		status = dhcpctl_wait_for_completion (interface_handle,
						      &waitstatus);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "dhcpctl_wait_for_completion: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
		if (waitstatus != ISC_R_SUCCESS) {
			fprintf (stderr, "dhcpctl_wait_for_completion: %s\n",
				 isc_result_totext (waitstatus));
			exit (1);
		}
		status = dhcpctl_object_remove (connection, interface_handle);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "dhcpctl_open_object: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
	}

	status = dhcpctl_wait_for_completion (interface_handle, &waitstatus);
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_wait_for_completion: %s\n",
			 isc_result_totext (status));
		exit (1);
	}
	if (waitstatus != ISC_R_SUCCESS) {
		fprintf (stderr, "interface object %s: %s\n", action,
			 isc_result_totext (waitstatus));
		exit (1);
	}

	memset (&result, 0, sizeof result);
	status = dhcpctl_get_value (&result, interface_handle, "state");
	if (status != ISC_R_SUCCESS) {
		fprintf (stderr, "dhcpctl_get_value: %s\n",
			 isc_result_totext (status));
		exit (1);
	}

	exit (0);
}
