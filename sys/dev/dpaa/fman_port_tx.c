/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * FMan TX port driver.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <machine/bus.h>

#include "fman.h"
#include "fman_parser.h"
#include "fman_port.h"
#include "fman_port_var.h"
#include "fman_if.h"
#include "fman_port_if.h"

#define	TX_10G_PORT_BASE	0x30

/* BMI Tx registers. */
#define	FMBM_TCFG		0x000
#define	FMBM_TST		0x004
#define	FMBM_TDA		0x008
#define	FMBM_TFP		0x00c
#define	  BMI_FIFO_PIPELINE_DEPTH_SHIFT	12
#define	FMBM_TFED		0x010
#define	FMBM_TICP		0x014
#define	  TICP_ICEOF_M		  0x001f0000
#define	  TICP_ICEOF_S		  16
#define	  TICP_ICIOF_M		  0x00000f00
#define	  TICP_ICIOF_S		  8
#define	  TICP_ICSZ_S		  0x0000001f
#define	FMBM_TFDNE		0x018
#define	FMBM_TFCA		0x01c
#define	  TFCA_MR_DEF		  0
#define	  TFCA_ATTR_ORDER	  0x80000000
#define	FMBM_TCFQID		0x020
#define	FMBM_TEFQID		0x024
#define	FMBM_TFENE		0x028
#define	FMBM_TFNE		0x070
#define	  TFNE_EBD		  0x80000000

/* QMI dequeue-config (FMQM_PNDC) field layout: TX-only consumer. */
#define	QMI_DEQ_CFG_PRI			0x80000000
#define	QMI_DEQ_CFG_TYPE1		0x10000000
#define	QMI_DEQ_CFG_TYPE2		0x20000000
#define	QMI_DEQ_CFG_TYPE3		0x30000000
#define	QMI_DEQ_CFG_PREFETCH_PARTIAL	0x01000000
#define	QMI_DEQ_CFG_PREFETCH_FULL	0x03000000
#define	QMI_DEQ_CFG_SP_MASK		0xf
#define	QMI_DEQ_CFG_SP_SHIFT		20

struct fman_port_tx_softc {
	struct fman_port_softc sc_base;

	int sc_deq_byte_count;
	int sc_deq_high_priority;
	int sc_tx_deq_pipeline_depth;
	int sc_qman_channel_id;
};

static struct ofw_compat_data tx_compats[] = {
	{ "fsl,fman-v2-port-tx", 0 },
	{ "fsl,fman-v3-port-tx", PORT_V3 },
	{ NULL, 0 }
};

static int
fman_port_tx_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, tx_compats)->ocd_str == NULL)
		return (ENXIO);

	device_set_desc(dev, "FMan TX port");
	return (BUS_PROBE_DEFAULT);
}

static int
fman_port_tx_attach(device_t dev)
{
	struct fman_port_tx_softc *sc;
	uintptr_t compat_data;
	int port_speed;
	int err;

	sc = device_get_softc(dev);
	compat_data = ofw_bus_search_compatible(dev, tx_compats)->ocd_data;

	port_speed = 1000;
	if ((compat_data & PORT_V3) == PORT_V3) {
		if (OF_hasprop(ofw_bus_get_node(dev), "fsl,fman-10g-port"))
			port_speed = 10000;
	} else {
		int cell = 0;
		OF_getencprop(ofw_bus_get_node(dev), "cell-index", &cell,
		    sizeof(cell));
		if (cell > TX_10G_PORT_BASE)
			port_speed = 10000;
	}

	err = fman_port_attach_common(dev, &sc->sc_base, port_speed);
	if (err != 0)
		return (err);

	if (port_speed == 10000) {
		sc->sc_deq_high_priority = true;
		sc->sc_deq_byte_count = 0x1400;
	} else {
		sc->sc_deq_high_priority = false;
		sc->sc_deq_byte_count = 0x0400;
	}

	sc->sc_qman_channel_id = FMAN_GET_QMAN_CHANNEL_ID(
	    device_get_parent(dev), sc->sc_base.sc_port_id);

	return (0);
}

static int
fman_port_tx_detach(device_t dev)
{
	struct fman_port_tx_softc *sc = device_get_softc(dev);

	return (fman_port_detach_common(dev, &sc->sc_base));
}

static int
fman_port_tx_config(device_t dev, struct fman_port_params *params)
{
	struct fman_port_tx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;

	fman_port_config_common(base, params);

	if (base->sc_revision_major >= 6 && base->sc_port_speed == 1000)
		/* Errata A005127 workaround */
		bus_write_4(base->sc_mem, FMBM_TFP, 0x00001013);

	base->sc_tasks.extra = 0;
	switch (base->sc_port_speed) {
	case 10000:
		base->sc_tasks.num = (base->sc_revision_major < 6) ? 16 : 14;
		break;
	case 1000:
		base->sc_tasks.num = (base->sc_revision_major >= 6) ? 4 : 3;
		break;
	default:
		base->sc_tasks.num = 0;
		break;
	}

	if (base->sc_revision_major >= 6) {
		base->sc_open_dmas.extra = 0;
		base->sc_open_dmas.num =
		    (base->sc_port_speed == 10000) ? 12 : 3;
	} else if (base->sc_port_speed == 10000) {
		base->sc_open_dmas.num = 8;
		base->sc_open_dmas.extra = 8;
	} else {
		base->sc_open_dmas.num = 1;
		base->sc_open_dmas.extra = 1;
	}

	if (base->sc_revision_major >= 6)
		base->sc_fifo_bufs.num =
		    (base->sc_port_speed == 10000) ? 64 : 50;
	else
		base->sc_fifo_bufs.num =
		    (base->sc_port_speed == 10000) ? 48 : 44;
	base->sc_fifo_bufs.extra = 0;
	base->sc_fifo_bufs.num *= FMAN_BMI_FIFO_UNITS;

	/* TODO: buf_margins?  See fman_sp_build_buffer_struct */
	return (0);
}

static int
fman_port_tx_init_bmi(struct fman_port_tx_softc *sc)
{
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;
	int depth;

	bus_write_4(base->sc_mem, FMBM_TCFG, 0);
	bus_write_4(base->sc_mem, FMBM_TDA, 0);
	bus_write_4(base->sc_mem, FMBM_TFED, 0);

	if (base->sc_port_speed == 10000)
		depth = 4;
	else if (base->sc_revision_major >= 6)
		depth = 2;
	else
		depth = 1;
	sc->sc_tx_deq_pipeline_depth = depth;
	reg = ((depth - 1) << BMI_FIFO_PIPELINE_DEPTH_SHIFT) | 0x13;
	bus_write_4(base->sc_mem, FMBM_TFP, reg);

	/* Default color: green */
	bus_write_4(base->sc_mem, FMBM_TFCA, TFCA_MR_DEF | TFCA_ATTR_ORDER);

	bus_write_4(base->sc_mem, FMBM_TFDNE, NIA_ENG_QMI_DEQ);
	bus_write_4(base->sc_mem, FMBM_TFENE,
	    NIA_ENG_QMI_ENQ | NIA_ORDER_RESTORE);

	/* Insert internal context ahead of the frame */
	reg = howmany(FMAN_PARSE_RESULT_OFF, 0x10) << TICP_ICIOF_S;
	reg |= howmany(sizeof(struct fman_internal_context), 0x10);
	bus_write_4(base->sc_mem, FMBM_TICP, reg);

	if (base->sc_revision_major >= 6)
		bus_write_4(base->sc_mem, FMBM_TFNE,
		    (base->sc_default_fqid == 0 ? TFNE_EBD : 0) |
		    NIA_BMI_AC_FETCH_ALLFRAME);
	bus_write_4(base->sc_mem, FMBM_TCFQID, base->sc_default_fqid);
	bus_write_4(base->sc_mem, FMBM_TEFQID, base->sc_err_fqid);

	return (0);
}

static int
fman_port_tx_init_qmi(struct fman_port_tx_softc *sc)
{
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;

	bus_write_4(base->sc_mem, FMQM_PNDN, NIA_ENG_BMI | NIA_BMI_AC_TX);
	bus_write_4(base->sc_mem, FMQM_PNEN,
	    NIA_ENG_BMI | NIA_BMI_AC_TX_RELEASE);

	reg = 0;
	if (sc->sc_deq_high_priority)
		reg |= QMI_DEQ_CFG_PRI;
	reg |= QMI_DEQ_CFG_TYPE1;
	reg |= QMI_DEQ_CFG_PREFETCH_FULL;
	reg |= (sc->sc_qman_channel_id & QMI_DEQ_CFG_SP_MASK) <<
	    QMI_DEQ_CFG_SP_SHIFT;
	reg |= sc->sc_deq_byte_count;
	bus_write_4(base->sc_mem, FMQM_PNDC, reg);

	return (0);
}

static int
fman_port_tx_init(device_t dev)
{
	struct fman_port_tx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	struct fman_port_init_params params;
	int err;

	params.port_id = base->sc_port_id;
	params.is_rx_port = false;
	params.num_tasks = base->sc_tasks.num;
	params.extra_tasks = base->sc_tasks.extra;
	params.open_dmas = base->sc_open_dmas.num;
	params.extra_dmas = base->sc_open_dmas.extra;
	params.fifo_size = base->sc_fifo_bufs.num;
	params.extra_fifo_size = base->sc_fifo_bufs.extra;
	params.max_frame_length = base->sc_max_frame_length;
	params.deq_pipeline_size = sc->sc_tx_deq_pipeline_depth;

	/* TODO: verify_size_of_fifo() from Linux driver */
	err = FMAN_SET_PORT_PARAMS(device_get_parent(dev), &params);
	if (err != 0)
		return (err);

	err = fman_port_tx_init_bmi(sc);
	if (err != 0)
		return (err);

	return (fman_port_tx_init_qmi(sc));
}

static int
fman_port_tx_disable(device_t dev)
{
	struct fman_port_tx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;
	int count;

	reg = bus_read_4(base->sc_mem, FMQM_PNC);
	bus_write_4(base->sc_mem, FMQM_PNC, reg & ~PNC_EN);
	for (count = 0; count < 100; count++) {
		DELAY(10);
		reg = bus_read_4(base->sc_mem, FMQM_PNS);
		if (!(reg & PNS_DEQ_FD_BSY))
			break;
	}
	if (count == 100)
		device_printf(base->sc_dev, "Timeout stopping QMI\n");

	reg = bus_read_4(base->sc_mem, FMBM_TCFG);
	bus_write_4(base->sc_mem, FMBM_TCFG, reg & ~BMI_PORT_CFG_EN);
	for (count = 0; count < 100; count++) {
		DELAY(10);
		reg = bus_read_4(base->sc_mem, FMBM_TST);
		if (!(reg & PNS_DEQ_FD_BSY))
			break;
	}
	if (count == 100)
		device_printf(base->sc_dev, "Timeout stopping BMI\n");

	return (0);
}

static int
fman_port_tx_enable(device_t dev)
{
	struct fman_port_tx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;

	reg = bus_read_4(base->sc_mem, FMQM_PNC);
	bus_write_4(base->sc_mem, FMQM_PNC, reg | PNC_EN | PNC_STEN);

	reg = bus_read_4(base->sc_mem, FMBM_TCFG);
	bus_write_4(base->sc_mem, FMBM_TCFG, reg | BMI_PORT_CFG_EN);

	return (0);
}

static device_method_t fman_port_tx_methods[] = {
	DEVMETHOD(device_probe,		fman_port_tx_probe),
	DEVMETHOD(device_attach,	fman_port_tx_attach),
	DEVMETHOD(device_detach,	fman_port_tx_detach),

	DEVMETHOD(fman_port_config,	fman_port_tx_config),
	DEVMETHOD(fman_port_init,	fman_port_tx_init),
	DEVMETHOD(fman_port_enable,	fman_port_tx_enable),
	DEVMETHOD(fman_port_disable,	fman_port_tx_disable),

	DEVMETHOD_END
};

DEFINE_CLASS_0(fman_port_tx, fman_port_tx_driver, fman_port_tx_methods,
    sizeof(struct fman_port_tx_softc));
EARLY_DRIVER_MODULE(fman_port_tx, fman, fman_port_tx_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
