#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Threaded Mach IPC test scenario
# https://people.freebsd.org/~pho/stress/log/kip014.txt

ps -p1 | grep -q launchd || exit 0

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > machipc.c
cc -o machipc -Wall -Wextra -O2 -g machipc.c -lmach -lpthread || exit 1
rm machipc.c
cd $odir

(cd ../testcases/swap; ./swap -t 20m -i 20 -h -v) > /dev/null 2>&1 &
sleep 5
/tmp/machipc
pkill swap
wait
rm -f /tmp/machipc
exit 0
EOF
#include <sys/types.h>

#include <mach/mach.h>

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
	unsigned int msgt_name : 8,
		     msgt_size : 8,
		     msgt_number : 12,
		     msgt_inline : 1,
		     msgt_longform : 1,
		     msgt_deallocate : 1,
		     msgt_unused : 1;
} mach_msg_type_t;

struct integer_message {
	mach_msg_header_t head;
	mach_msg_type_t type;

	int inline_integer;
};

struct message_recv
{
	mach_msg_header_t head;
	mach_msg_type_t type;
	int inline_integer;
	mach_msg_trailer_t trailer;
};

mach_port_t bootstrap_port;

#define MACH_MSG_TYPE_INTEGER_32 2
#define N 100000000

static void
error(int exitcode, int macherr, const char *funcname)
{
	printf("%s failed with %x\n", funcname, macherr);
	exit(exitcode);
}

void *
client(void *arg)
{
	mach_port_t port = *(mach_port_t *) arg;
	struct message_recv message = {};
	int err, i;

	message.head.msgh_local_port = port;
	message.head.msgh_size = sizeof(message);

	for (i = 0; i < N; i++) {
		/* Receive a message */
		err = mach_msg(&message.head,	/* The header */
		    MACH_RCV_MSG,		/* Flags */
		    0,				/* Send size */
		    sizeof(message),		/* Max receive size */
		    port,			/* Receive port */
		    MACH_MSG_TIMEOUT_NONE,	/* No timeout */
		    MACH_PORT_NULL);		/* No notification */
		if (err)
			error(1, err, "server mach_msg MACH_RCV_MSG");
		if (message.inline_integer != i)
			errx(1, "FAIL message.inline_integer = %d, i = %d",
			    message.inline_integer, i);
	}

	return(0);
}

int
main(void)
{
	pthread_t ptd;
	mach_port_t port;
	struct integer_message message;
	int err, i;

	/* Allocate a port */
	err = mach_port_allocate(mach_task_self(),
	    MACH_PORT_RIGHT_RECEIVE, &port);
	if (err)
		error(1, err, "mach_port_allocate");

	err = mach_port_insert_right(mach_task_self(),
	    port, port, MACH_MSG_TYPE_MAKE_SEND);

	if (err)
		error(10, err, "mach_port_insert_right");

	if ((err = pthread_create(&ptd, NULL, client, &port)) != 0) {
		errc(1, err, "pthread_create failed");
	}

	/* Fill the header fields : */
	message.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
	message.head.msgh_size = sizeof( struct integer_message );
	message.head.msgh_local_port = MACH_PORT_NULL;
	message.head.msgh_remote_port = port;
	message.head.msgh_id = 0;			/* Message id */
	message.head.msgh_size = sizeof(message);	/* Message size */

	/* Set the message type */
	message.type.msgt_name = MACH_MSG_TYPE_INTEGER_32;
	message.type.msgt_size = sizeof(message);
	message.type.msgt_number = 1;
	message.type.msgt_inline = TRUE;
	message.type.msgt_longform = FALSE;
	message.type.msgt_deallocate = FALSE;

	for (i = 0; i < N; i++) {

		message.inline_integer = i;

		/* Send the message */
		err = mach_msg(&message.head,	/* The header */
		    MACH_SEND_MSG,		/* Flags */
		    sizeof(message),		/* Send size */
		    0,				/* Max receive Size */
		    port,			/* Send port */
		    MACH_MSG_TIMEOUT_NONE,	/* No timeout */
		    MACH_PORT_NULL);		/* No notification */
		if (err)
			error(3, err, "client mach_msg");
	}
	pthread_join(ptd, NULL);

	return (0);
}
