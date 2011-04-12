/*
 * Copyright (c) 2005-2011 Sandvine Incorporated
 * Copyright (c) 2000 Darrell Anderson <anderson@cs.duke.edu>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETINET_NETDUMP_H_
#define	_NETINET_NETDUMP_H_

#include <sys/types.h>

#define	NETDUMP_PORT		20023	/* Server udp port number for data. */
#define	NETDUMP_ACKPORT		20024	/* Client udp port number for acks. */

#define	NETDUMP_HERALD		1	/* Broadcast before starting a dump. */
#define	NETDUMP_FINISHED	2	/* Send after finishing a dump. */
#define	NETDUMP_VMCORE		3	/* Contains dump datas. */
#define	NETDUMP_KDH		4	/* Contains kernel dump header. */

#define	NETDUMP_DATASIZE	8192	/* Packets payload. */

struct netdump_msg_hdr {
	uint32_t	mh_type; /* NETDUMP_HERALD, _FINISHED, _VMCORE, _KDH. */
	uint32_t	mh_seqno;	/* Match acks with msgs. */
	uint64_t	mh_offset;	/* vmcore offset (bytes). */
	uint32_t	mh_len;		/* Attached data (bytes). */
	uint32_t	mh__pad; /* Pad space matching 32- and 64-bits archs. */
};

struct netdump_ack {
	uint32_t	na_seqno;	/* Match acks with msgs. */
};

struct netdump_msg {
	struct netdump_msg_hdr nm_hdr;
	uint8_t		nm_data[NETDUMP_DATASIZE];
};

#ifdef _KERNEL

typedef void ndumplock_handler_t(struct ifnet *);

struct netdump_methods {
	poll_handler_t		*ne_poll_locked;
	poll_handler_t		*ne_poll_unlocked;
	ndumplock_handler_t	*ne_disable_intr;
	ndumplock_handler_t	*ne_enable_intr;
};

int	 netdumpsys(void);

#endif

#endif /* !_NETINET_NETDUMP_H_ */
