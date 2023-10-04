/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
 */

#ifndef	_ARM64_SCMI_SCMI_H_
#define	_ARM64_SCMI_SCMI_H_

#include "scmi_if.h"

#define	SCMI_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	SCMI_UNLOCK(sc)		mtx_unlock(&(sc)->mtx)
#define	SCMI_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->mtx, MA_OWNED)

#define dprintf(fmt, ...)

struct scmi_softc {
	struct simplebus_softc	simplebus_sc;
	device_t		dev;
	device_t		tx_shmem;
	struct mtx		mtx;
};

/* Shared Memory Transfer. */
struct scmi_smt_header {
	uint32_t reserved;
	uint32_t channel_status;
#define	SCMI_SHMEM_CHAN_STAT_CHANNEL_ERROR	(1 << 1)
#define	SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE	(1 << 0)
	uint32_t reserved1[2];
	uint32_t flags;
#define	SCMI_SHMEM_FLAG_INTR_ENABLED		(1 << 0)
	uint32_t length;
	uint32_t msg_header;
	uint8_t msg_payload[0];
};

#define	SMT_HEADER_SIZE			sizeof(struct scmi_smt_header)

#define	SMT_HEADER_TOKEN_S		18
#define	SMT_HEADER_TOKEN_M		(0x3fff << SMT_HEADER_TOKEN_S)
#define	SMT_HEADER_PROTOCOL_ID_S	10
#define	SMT_HEADER_PROTOCOL_ID_M	(0xff << SMT_HEADER_PROTOCOL_ID_S)
#define	SMT_HEADER_MESSAGE_TYPE_S	8
#define	SMT_HEADER_MESSAGE_TYPE_M	(0x3 << SMT_HEADER_MESSAGE_TYPE_S)
#define	SMT_HEADER_MESSAGE_ID_S		0
#define	SMT_HEADER_MESSAGE_ID_M		(0xff << SMT_HEADER_MESSAGE_ID_S)

struct scmi_req {
	int protocol_id;
	int message_id;
	const void *in_buf;
	uint32_t in_size;
	void *out_buf;
	uint32_t out_size;
};

DECLARE_CLASS(scmi_driver);

int scmi_attach(device_t dev);
int scmi_request(device_t dev, struct scmi_req *req);

void scmi_shmem_read(device_t dev, bus_size_t offset, void *buf,
    bus_size_t len);
void scmi_shmem_write(device_t dev, bus_size_t offset, const void *buf,
    bus_size_t len);

#endif /* !_ARM64_SCMI_SCMI_H_ */
