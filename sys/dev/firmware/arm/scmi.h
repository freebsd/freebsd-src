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

#ifndef	_ARM64_SCMI_SCMI_H_
#define	_ARM64_SCMI_SCMI_H_

#include <sys/sysctl.h>

#include "scmi_if.h"

#define SCMI_DEF_MAX_MSG		32
#define SCMI_DEF_MAX_MSG_PAYLD_SIZE	128

#define SCMI_MAX_MSG_PAYLD_SIZE(sc)	((sc)->trs_desc.max_payld_sz + sizeof(uint32_t))
#define SCMI_MAX_MSG_REPLY_SIZE(sc)	(SCMI_MAX_MSG_PAYLD_SIZE((sc)) + sizeof(uint32_t))
#define SCMI_MAX_MSG_SIZE(sc)		(SCMI_MAX_MSG_REPLY_SIZE(sc) + sizeof(uint32_t))
#define SCMI_MAX_MSG(sc)		((sc)->trs_desc.max_msg)
#define SCMI_MAX_MSG_TIMEOUT_MS(sc)	((sc)->trs_desc.reply_timo_ms)

enum scmi_chan {
	SCMI_CHAN_A2P,
	SCMI_CHAN_P2A,
	SCMI_CHAN_MAX
};

struct scmi_transport_desc {
	bool no_completion_irq;
	unsigned int max_msg;
	unsigned int max_payld_sz;
	unsigned int reply_timo_ms;
};

struct scmi_transport;

struct scmi_softc {
	struct simplebus_softc		simplebus_sc;
	device_t			dev;
	struct mtx			mtx;
	struct scmi_transport_desc	trs_desc;
	struct scmi_transport		*trs;
	struct sysctl_oid		*sysctl_root;
};

struct scmi_msg {
	bool		polling;
	int		poll_done;
	uint32_t	tx_len;
	uint32_t	rx_len;
#define SCMI_MSG_HDR_SIZE	(sizeof(uint32_t))
	uint32_t	hdr;
	uint8_t		payld[];
};
#define hdr_to_msg(h)	__containerof((h), struct scmi_msg, hdr)

void *scmi_buf_get(device_t dev, uint8_t protocol_id, uint8_t message_id,
		   int tx_payd_sz, int rx_payld_sz);
void scmi_buf_put(device_t dev, void *buf);
struct scmi_msg *scmi_msg_get(device_t dev, int tx_payld_sz, int rx_payld_sz);
void scmi_msg_put(device_t dev, struct scmi_msg *msg);
int scmi_request(device_t dev, void *in, void **);
int scmi_request_tx(device_t dev, void *in);
int scmi_msg_async_enqueue(struct scmi_msg *msg);
void scmi_rx_irq_callback(device_t dev, void *chan, uint32_t hdr, uint32_t rx_len);

DECLARE_CLASS(scmi_driver);

int scmi_attach(device_t dev);

#endif /* !_ARM64_SCMI_SCMI_H_ */
