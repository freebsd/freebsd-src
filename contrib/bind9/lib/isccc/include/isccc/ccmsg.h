/*
 * Portions Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: ccmsg.h,v 1.4.18.2 2005-04-29 00:17:13 marka Exp $ */

#ifndef ISCCC_CCMSG_H
#define ISCCC_CCMSG_H 1

/*! \file */

#include <isc/buffer.h>
#include <isc/lang.h>
#include <isc/socket.h>

/*% ISCCC Message Structure */
typedef struct isccc_ccmsg {
	/* private (don't touch!) */
	unsigned int		magic;
	isc_uint32_t		size;
	isc_buffer_t		buffer;
	unsigned int		maxsize;
	isc_mem_t	       *mctx;
	isc_socket_t	       *sock;
	isc_task_t	       *task;
	isc_taskaction_t	action;
	void		       *arg;
	isc_event_t		event;
	/* public (read-only) */
	isc_result_t		result;
	isc_sockaddr_t		address;
} isccc_ccmsg_t;

ISC_LANG_BEGINDECLS

void
isccc_ccmsg_init(isc_mem_t *mctx, isc_socket_t *sock, isccc_ccmsg_t *ccmsg);
/*%
 * Associate a cc message state with a given memory context and
 * TCP socket.
 *
 * Requires:
 *
 *\li	"mctx" and "sock" be non-NULL and valid types.
 *
 *\li	"sock" be a read/write TCP socket.
 *
 *\li	"ccmsg" be non-NULL and an uninitialized or invalidated structure.
 *
 * Ensures:
 *
 *\li	"ccmsg" is a valid structure.
 */

void
isccc_ccmsg_setmaxsize(isccc_ccmsg_t *ccmsg, unsigned int maxsize);
/*%
 * Set the maximum packet size to "maxsize"
 *
 * Requires:
 *
 *\li	"ccmsg" be valid.
 *
 *\li	512 <= "maxsize" <= 4294967296
 */

isc_result_t
isccc_ccmsg_readmessage(isccc_ccmsg_t *ccmsg,
		       isc_task_t *task, isc_taskaction_t action, void *arg);
/*%
 * Schedule an event to be delivered when a command channel message is
 * readable, or when an error occurs on the socket.
 *
 * Requires:
 *
 *\li	"ccmsg" be valid.
 *
 *\li	"task", "taskaction", and "arg" be valid.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		-- no error
 *\li	Anything that the isc_socket_recv() call can return.  XXXMLG
 *
 * Notes:
 *
 *\li	The event delivered is a fully generic event.  It will contain no
 *	actual data.  The sender will be a pointer to the isccc_ccmsg_t.
 *	The result code inside that structure should be checked to see
 *	what the final result was.
 */

void
isccc_ccmsg_cancelread(isccc_ccmsg_t *ccmsg);
/*%
 * Cancel a readmessage() call.  The event will still be posted with a
 * CANCELED result code.
 *
 * Requires:
 *
 *\li	"ccmsg" be valid.
 */

void
isccc_ccmsg_invalidate(isccc_ccmsg_t *ccmsg);
/*%
 * Clean up all allocated state, and invalidate the structure.
 *
 * Requires:
 *
 *\li	"ccmsg" be valid.
 *
 * Ensures:
 *
 *\li	"ccmsg" is invalidated and disassociated with all memory contexts,
 *	sockets, etc.
 */

ISC_LANG_ENDDECLS

#endif /* ISCCC_CCMSG_H */
