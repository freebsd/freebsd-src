/*
 * Copyright (c) 2005 Apple Computer, Inc.
 * All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_BSD_LICENSE_HEADER_END@
 *
 * $P4: //depot/projects/trustedbsd/openbsm/bin/audit/audit.c#7 $
 */
/*
 * Program to trigger the audit daemon with a message that is either:
 *    - Open a new audit log file
 *    - Read the audit control file and take action on it
 *    - Close the audit log file and exit
 *
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <bsm/libbsm.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: audit -n | -s | -t \n");
	exit(-1);
}

/*
 * Main routine to process command line options.
 */
int
main(int argc, char **argv)
{
	int ch;
	unsigned int trigger = 0;

	if (argc != 2)
		usage();

	while ((ch = getopt(argc, argv, "nst")) != -1) {
		switch(ch) {

		case 'n':
			trigger = AUDIT_TRIGGER_ROTATE_USER;
			break;

		case 's':
			trigger = AUDIT_TRIGGER_READ_FILE;
			break;

		case 't':
			trigger = AUDIT_TRIGGER_CLOSE_AND_DIE;
			break;

		case '?':
		default:
			usage();
			break;
		}
	}
	if (auditon(A_SENDTRIGGER, &trigger, sizeof(trigger)) < 0) {
		perror("Error sending trigger");
		exit(-1);
	} else {
		printf("Trigger sent.\n");
		exit (0);
	}
}
