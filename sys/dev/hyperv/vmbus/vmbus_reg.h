/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMBUS_REG_H_
#define _VMBUS_REG_H_

#include <sys/param.h>

/*
 * Hyper-V SynIC message format.
 */

#define VMBUS_MSG_DSIZE_MAX		240
#define VMBUS_MSG_SIZE			256

struct vmbus_message {
	uint32_t	msg_type;	/* HYPERV_MSGTYPE_ */
	uint8_t		msg_dsize;	/* data size */
	uint8_t		msg_flags;	/* VMBUS_MSGFLAG_ */
	uint16_t	msg_rsvd;
	uint64_t	msg_id;
	uint8_t		msg_data[VMBUS_MSG_DSIZE_MAX];
} __packed;
CTASSERT(sizeof(struct vmbus_message) == VMBUS_MSG_SIZE);

#define VMBUS_MSGFLAG_PENDING		0x01

/*
 * Hyper-V SynIC event flags
 */

#ifdef __LP64__
#define VMBUS_EVTFLAGS_MAX	32
#define VMBUS_EVTFLAG_SHIFT	6
#else
#define VMBUS_EVTFLAGS_MAX	64
#define VMBUS_EVTFLAG_SHIFT	5
#endif
#define VMBUS_EVTFLAG_LEN	(1 << VMBUS_EVTFLAG_SHIFT)
#define VMBUS_EVTFLAG_MASK	(VMBUS_EVTFLAG_LEN - 1)
#define VMBUS_EVTFLAGS_SIZE	256

struct vmbus_evtflags {
	u_long		evt_flags[VMBUS_EVTFLAGS_MAX];
} __packed;
CTASSERT(sizeof(struct vmbus_evtflags) == VMBUS_EVTFLAGS_SIZE);

/*
 * Channel
 */

#define VMBUS_CHAN_MAX_COMPAT	256
#define VMBUS_CHAN_MAX		(VMBUS_EVTFLAG_LEN * VMBUS_EVTFLAGS_MAX)

/*
 * Channel messages
 * - Embedded in vmbus_message.msg_data, e.g. response.
 * - Embedded in hypercall_postmsg_in.hc_data, e.g. request.
 */

#define VMBUS_CHANMSG_TYPE_INIT_CONTACT		14	/* REQ */
#define VMBUS_CHANMSG_TYPE_VERSION_RESP		15	/* RESP */

struct vmbus_chanmsg_hdr {
	uint32_t	chm_type;	/* VMBUS_CHANMSG_TYPE_ */
	uint32_t	chm_rsvd;
} __packed;

/* VMBUS_CHANMSG_TYPE_INIT_CONTACT */
struct vmbus_chanmsg_init_contact {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint32_t	chm_ver;
	uint32_t	chm_rsvd;
	uint64_t	chm_evtflags;
	uint64_t	chm_mnf1;
	uint64_t	chm_mnf2;
} __packed;

/* VMBUS_CHANMSG_TYPE_VERSION_RESP */
struct vmbus_chanmsg_version_resp {
	struct vmbus_chanmsg_hdr chm_hdr;
	uint8_t		chm_supp;
} __packed;

#endif	/* !_VMBUS_REG_H_ */
