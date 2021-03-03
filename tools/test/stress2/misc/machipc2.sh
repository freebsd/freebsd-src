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

# Non threaded Mach IPC test scenario
# https://people.freebsd.org/~pho/stress/log/kip018.txt

ps -p1 | grep -q launchd || exit 0

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > machipc2.c
# Test fails without the -lpthread. Need to investigate why.
cc -o machipc2 -Wall -Wextra -O2 -g machipc2.c -lmach -lpthread || exit 1
rm machipc2.c
cd $odir

(cd ../testcases/swap; ./swap -t 5m -i 20 -h -v) &
sleep 5
/tmp/machipc2
pkill swap
rm -f /tmp/machipc2
exit 0
EOF
/*
   Inspired by: Michael Weber: http://www.foldr.org/~michaelw/log/2009/03/13/
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <mach/mach.h>
#include <servers/bootstrap.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MACH_MSG_TYPE_INTEGER_32 2
#define N 200000000

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

static task_t child_task = MACH_PORT_NULL;

mach_port_t bootstrap_port;

#define CHECK_MACH_ERROR(err, s) \
	do { \
		if (err != KERN_SUCCESS) { \
			fprintf(stderr, "%s: %s", s, mach_error_string(err)); \
			exit(1); \
		} \
	} while (0)

static int
setup_recv_port (mach_port_t *recv_port)
{
	kern_return_t kerr;
	mach_port_t port = MACH_PORT_NULL;
	kerr = mach_port_allocate (mach_task_self (),
	    MACH_PORT_RIGHT_RECEIVE, &port);
	CHECK_MACH_ERROR (kerr, "mach_port_allocate failed:");

	kerr = mach_port_insert_right (mach_task_self (),
	    port, port, MACH_MSG_TYPE_MAKE_SEND);
	CHECK_MACH_ERROR (kerr, "mach_port_insert_right failed:");

	*recv_port = port;

	return (0);
}

static int
send_port (mach_port_t remote_port, mach_port_t port)
{
	kern_return_t kerr;

	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
		mach_msg_port_descriptor_t task_port;
	} msg;

	msg.header.msgh_remote_port = remote_port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0) |
	    MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_size = sizeof msg;

	msg.body.msgh_descriptor_count = 1;
	msg.task_port.name = port;
	msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
	msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;

	kerr = mach_msg_send (&msg.header);
	CHECK_MACH_ERROR(kerr, "mach_msg_send failed:");

	return (0);
}

static int
recv_port(mach_port_t recv_port, mach_port_t *port)
{
	kern_return_t kerr;
	struct {
		mach_msg_header_t header;
		mach_msg_body_t body;
		mach_msg_port_descriptor_t task_port;
		mach_msg_trailer_t trailer;
	} msg;

	kerr = mach_msg(&msg.header, MACH_RCV_MSG,
	    0, sizeof msg, recv_port,
	    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	CHECK_MACH_ERROR(kerr, "mach_msg failed:");

	*port = msg.task_port.name;
	return (0);
}

void
writeint(mach_port_t port)
{
	struct integer_message message;
	int kerr, i;

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
		kerr = mach_msg(&message.head,	/* The header */
		    MACH_SEND_MSG,		/* Flags */
		    sizeof(message),		/* Send size */
		    0,				/* Max receive Size */
		    port,			/* Send port */
		    MACH_MSG_TIMEOUT_NONE,	/* No timeout */
		    MACH_PORT_NULL);		/* No notification */
		if (kerr)
			errx(1, "client mach_msg: %s", mach_error_string(kerr));
	}
}

void
readint(mach_port_t port)
{
	struct message_recv rmessage = {};
	int kerr, i;

	rmessage.head.msgh_local_port = port;
	rmessage.head.msgh_size = sizeof(rmessage);

	for (i = 0; i < N; i++) {
		/* Receive a message */
		kerr = mach_msg(&rmessage.head,	/* The header */
		    MACH_RCV_MSG,		/* Flags */
		    0,				/* Send size */
		    sizeof(rmessage),		/* Max receive size */
		    port,			/* Receive port */
		    MACH_MSG_TIMEOUT_NONE,	/* No timeout */
		    MACH_PORT_NULL);		/* No notification */
		if (kerr)
			errx(1, "client mach_msg MACH_RCV_MSG: %s", mach_error_string(kerr));
		if (rmessage.inline_integer != i)
			errx(1, "FAIL message.inline_integer = %d, i = %d",
			    rmessage.inline_integer, i);
	}
}

void
sampling_fork(void)
{
	pid_t pid;
	kern_return_t kerr;
	mach_port_t parent_recv_port = MACH_PORT_NULL;
	mach_port_t child_recv_port = MACH_PORT_NULL;

	if (setup_recv_port(&parent_recv_port) != 0)
		return;
	kerr = task_set_bootstrap_port(mach_task_self(), parent_recv_port);
	CHECK_MACH_ERROR(kerr, "task_set_bootstrap_port failed:");

	if ((pid = fork()) == -1)
		err(1, "fork");

	if (pid == 0) {
		kerr = task_get_bootstrap_port(mach_task_self(), &parent_recv_port);
		CHECK_MACH_ERROR(kerr, "task_get_bootstrap_port failed:");
		if (setup_recv_port(&child_recv_port) != 0)
			return;
		if (send_port(parent_recv_port, mach_task_self()) != 0)
			return;
		if (send_port(parent_recv_port, child_recv_port) != 0)
			return;
		if (recv_port(child_recv_port, &bootstrap_port) != 0)
			return;
		kerr = task_set_bootstrap_port(mach_task_self(), bootstrap_port);
		CHECK_MACH_ERROR(kerr, "task_set_bootstrap_port failed:");

		readint(child_recv_port);

		_exit(0);
	}

	/* parent */
	kerr = task_set_bootstrap_port(mach_task_self(), bootstrap_port);
	CHECK_MACH_ERROR(kerr, "task_set_bootstrap_port failed:");
	if (recv_port(parent_recv_port, &child_task) != 0)
		return;
	if (recv_port(parent_recv_port, &child_recv_port) != 0)
		return;
	if (send_port(child_recv_port, bootstrap_port) != 0)
		return;
	kerr = mach_port_deallocate(mach_task_self(), parent_recv_port);
	CHECK_MACH_ERROR(kerr, "mach_port_deallocate failed:");

	writeint(child_recv_port);
}

int
main(void)
{
	sampling_fork();
	wait(NULL);

	return (0);
}
