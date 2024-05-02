/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_H__
#define	__NVMF_H__

#include <sys/ioccom.h>
#ifndef _KERNEL
#include <stdbool.h>
#endif

/*
 * Default settings in Fabrics controllers.  These match values used by the
 * Linux target.
 */
#define	NVMF_MAX_IO_ENTRIES	(1024)
#define	NVMF_CC_EN_TIMEOUT	(15)	/* In 500ms units */

/* Allows for a 16k data buffer + SQE */
#define	NVMF_IOCCSZ		(sizeof(struct nvme_command) + 16 * 1024)
#define	NVMF_IORCSZ		(sizeof(struct nvme_completion))

#define	NVMF_NN			(1024)

struct nvmf_handoff_qpair_params {
	bool	admin;
	bool	sq_flow_control;
	u_int	qsize;
	uint16_t sqhd;
	uint16_t sqtail;	/* host only */
	union {
		struct {
			int	fd;
			uint8_t	rxpda;
			uint8_t txpda;
			bool	header_digests;
			bool	data_digests;
			uint32_t maxr2t;
			uint32_t maxh2cdata;
			uint32_t max_icd;
		} tcp;
	};
};

struct nvmf_handoff_host {
	u_int	trtype;
	u_int	num_io_queues;
	u_int	kato;
	struct nvmf_handoff_qpair_params admin;
	struct nvmf_handoff_qpair_params *io;
	const struct nvme_controller_data *cdata;
};

struct nvmf_reconnect_params {
	uint16_t cntlid;
	char	subnqn[256];
};

struct nvmf_handoff_controller_qpair {
	u_int	trtype;
	struct nvmf_handoff_qpair_params params;
	const struct nvmf_fabric_connect_cmd *cmd;
	const struct nvmf_fabric_connect_data *data;
};

/* Operations on /dev/nvmf */
#define	NVMF_HANDOFF_HOST	_IOW('n', 200, struct nvmf_handoff_host)
#define	NVMF_DISCONNECT_HOST	_IOW('n', 201, const char *)
#define	NVMF_DISCONNECT_ALL	_IO('n', 202)

/* Operations on /dev/nvmeX */
#define	NVMF_RECONNECT_PARAMS	_IOR('n', 203, struct nvmf_reconnect_params)
#define	NVMF_RECONNECT_HOST	_IOW('n', 204, struct nvmf_handoff_host)

#endif /* !__NVMF_H__ */
