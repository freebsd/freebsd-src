/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef DPAA_FMAN_PORT_VAR_H
#define	DPAA_FMAN_PORT_VAR_H

#include "fman_port.h"

struct fman_port_rsrc {
	uint32_t	num;
	uint32_t	extra;
};

/* Base softc.  Shared by all FMan port drivers. */
#define	MAX_BM_POOLS	64
struct fman_port_softc {
	device_t sc_dev;
	struct resource *sc_mem;
	int sc_port_id;
	int sc_port_type;
	int sc_port_speed;

	int sc_revision_major;
	int sc_revision_minor;

	int sc_max_frame_length;
	int sc_bm_max_pools;
	int sc_max_port_fifo_size;

	int sc_default_fqid;
	int sc_err_fqid;

	int sc_max_ext_portals;
	int sc_max_sub_portals;

	struct fman_port_rsrc sc_open_dmas;
	struct fman_port_rsrc sc_tasks;
	struct fman_port_rsrc sc_fifo_bufs;
};

#define	PORT_V3		0x01

/* QMI per-port registers. */
#define	FMQM_PNC			0x400
#define	  PNC_EN			  0x80000000
#define	  PNC_STEN			  0x80000000
#define	FMQM_PNS			0x404
#define	  PNS_DEQ_FD_BSY		  0x20000000
#define	FMQM_PNEN			0x41c
#define	FMQM_PNDN			0x42c
#define	FMQM_PNDC			0x430

#define	FMBM_CFG			0x000
#define	  BMI_PORT_CFG_EN		  0x80000000

/*
 * Next Invoked Action (NIA) engine + action codes.  Written into the
 * per-engine "next" registers to chain BMI -> Parser -> KeyGen ->
 * BMI-enqueue -> QMI-enqueue.
 */
#define	NIA_ORDER_RESTORE		0x00800000
#define	NIA_ENG_BMI			0x00500000
#define	NIA_ENG_QMI_DEQ			0x00580000
#define	NIA_ENG_QMI_ENQ			0x00540000
#define	NIA_ENG_HWP			0x00440000
#define	NIA_ENG_HWK			0x00480000
#define	NIA_BMI_AC_TX_RELEASE		0x000002c0
#define	NIA_BMI_AC_TX			0x00000274
#define	NIA_BMI_AC_RELEASE		0x000000c0
#define	NIA_BMI_AC_ENQ_FRAME		0x00000002
#define	NIA_BMI_AC_FETCH_ALLFRAME	0x0000020c

#define	PORT_MAX_FRAME_LENGTH		9600

int fman_port_attach_common(device_t dev, struct fman_port_softc *sc,
    int port_speed);
int fman_port_detach_common(device_t dev, struct fman_port_softc *sc);

void fman_port_config_common(struct fman_port_softc *sc,
    struct fman_port_params *params);

#endif /* DPAA_FMAN_PORT_VAR_H */
