/* cltest.c

   Example program that uses the dhcpctl library. */

/* Copyright (C) 2000-2002 Internet Software Consortium
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software was contributed to the Internet Software Consortium
 * by Brian Murrell.
 */

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
