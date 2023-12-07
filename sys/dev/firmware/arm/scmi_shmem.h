/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2023 Arm Ltd
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

#ifndef	_ARM64_SCMI_SCMI_SHMEM_H_
#define	_ARM64_SCMI_SCMI_SHMEM_H_

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

#define	SMT_SIZE_HEADER			sizeof(struct scmi_smt_header)

#define	SMT_OFFSET_CHAN_STATUS		\
	__offsetof(struct scmi_smt_header, channel_status)
#define	SMT_SIZE_CHAN_STATUS		sizeof(uint32_t)

#define	SMT_OFFSET_LENGTH		\
	__offsetof(struct scmi_smt_header, length)
#define	SMT_SIZE_LENGTH			sizeof(uint32_t)

#define	SMT_OFFSET_MSG_HEADER		\
    __offsetof(struct scmi_smt_header, msg_header)
#define	SMT_SIZE_MSG_HEADER		sizeof(uint32_t)

struct scmi_req;

device_t	scmi_shmem_get(device_t sdev, phandle_t node, int index);
int		scmi_shmem_prepare_msg(device_t dev, struct scmi_req *req,
    bool polling);
bool scmi_shmem_poll_msg(device_t, uint32_t msg_header);
int scmi_shmem_read_msg_header(device_t dev, uint32_t *msg_header);
int scmi_shmem_read_msg_payload(device_t dev, uint8_t *buf, uint32_t buf_len);
void scmi_shmem_tx_complete(device_t);
void scmi_shmem_clear_channel(device_t dev);

#endif /* !_ARM64_SCMI_SCMI_SHMEM_H_ */
