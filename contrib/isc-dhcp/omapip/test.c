/* test.c

   Test code for omapip... */

/*
 * Copyright (c) 1999-2000 Internet Software Consortium.
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
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <isc-dhcp/result.h>
#include <sys/time.h>
#include <omapip/omapip.h>

int main (int argc, char **argv)
{
	omapi_object_t *listener = (omapi_object_t*)0;
	omapi_object_t *connection = (omapi_object_t*)0;
	isc_result_t status;

	omapi_init ();

	if (argc > 1 && !strcmp (argv [1], "listen")) {
		if (argc < 3) {
			fprintf (stderr, "Usage: test listen port\n");
			exit (1);
		}
		status = omapi_generic_new (&listener, MDL);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "omapi_generic_new: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
		status = omapi_protocol_listen (listener,
						(unsigned)atoi (argv [2]), 1);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "omapi_listen: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
		omapi_dispatch (0);
	} else if (argc > 1 && !strcmp (argv [1], "connect")) {
		if (argc < 4) {
			fprintf (stderr, "Usage: test listen address port\n");
			exit (1);
		}
		status = omapi_generic_new (&connection, MDL);
		if (status != ISC_R_SUCCESS) {
			fprintf (stderr, "omapi_generic_new: %s\n",
				 isc_result_totext (status));
			exit (1);
		}
		status = omapi_protocol_connect (connection,
						 argv [2],
						 (unsigned)atoi (argv [3]), 0);
		fprintf (stderr, "connect: %s\n", isc_result_totext (status));
		if (status != ISC_R_SUCCESS)
			exit (1);
		status = omapi_wait_for_completion (connection, 0);
		fprintf (stderr, "completion: %s\n",
			 isc_result_totext (status));
		if (status != ISC_R_SUCCESS)
			exit (1);
		/* ... */
	} else {
		fprintf (stderr, "Usage: test [listen | connect] ...\n");
		exit (1);
	}

	return 0;
}
