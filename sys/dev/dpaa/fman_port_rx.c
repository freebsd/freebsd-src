/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * FMan RX port driver.
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

#define	RX_10G_PORT_BASE	0x10

/* BMI Rx registers. */
#define	FMBM_RCFG		0x000
#define	FMBM_RST		0x004
#define	FMBM_RDA		0x008
#define	  RDA_WOPT		  0x00100000
#define	FMBM_RFP		0x00c
#define	FMBM_RFED		0x010
#define	  BMI_RX_FRAME_END_CUT_SHIFT	16
#define	FMBM_RICP		0x014	/* Counts are units of 16 bytes */
#define	  RICP_ICEOF_M		  0x001f0000
#define	  RICP_ICEOF_S		  16
#define	  RICP_ICIOF_M		  0x00000f00
#define	  RICP_ICIOF_S		  8
#define	  RICP_ICSZ_S		  0x0000001f
#define	FMBM_RIM		0x018
#define	FMBM_REBM		0x01c
#define	  REBM_BSM_M		  0x01ff0000
#define	  REBM_BSM_S		  16
#define	  REBM_BEM_M		  0x000001ff
#define	FMBM_RFNE		0x020
#define	FMBM_RFCA		0x024
#define	  RFCA_OR		  0x80000000
#define	  RFCA_COLOR		  0x0c000000
#define	  RFCA_SYNC		  0x03000000
#define	  RFCA_SYNC_REQ		  0x02000000
#define	  RFCA_MR		  0x003f0000
#define	  RFCA_MR_DEF		  0x003c0000
#define	FMBM_RFPNE		0x028
#define	FMBM_RETH		0x038
#define	  RETH_ETHE		  0x80000000 /* Excessive Threshold Enable */
#define	FMBM_RFQID		0x060
#define	FMBM_REFQID		0x064
#define	FMBM_RFSDM		0x068
#define	FMBM_RFSEM		0x06c
#define	FMBM_RFENE		0x070
#define	FMBM_REBMPI(i)		(0x100 + (4 * (i)))
#define	  REBMPI_VAL		  0x80000000
#define	  REBMPI_ACE		  0x40000000
#define	  REBMPI_BPID_S		  16
#define	FMBM_RSTC		0x0200
#define	  RSTC_EN		  0x80000000

/* Hardware Parser (HWP). */
#define	HWP_PCAC		0xbf8
#define	  HWP_PCAC_PSTOP	  0x00000001
#define	  HWP_HXS_PCAC_PSTAT	  0x00000100
#define	HWP_HXS_SSA(x)		(0x800 + x * 2 * sizeof(uint32_t))
#define	HWP_HXS_LCV(x)		(0x800 + (x * 2 + 1) * sizeof(uint32_t))
#define	HWP_HXS_TCP		  0xA
#define	HWP_HXS_UDP		  0xB
#define	HXS_SH_PAD_REM		  0x80000000
#define	HWP_HXS_COUNT		16

#define	BMI_RX_ERR		(FM_FD_ERR_DMA | FM_FD_ERR_FPE |	\
				 FM_FD_ERR_FSE | FM_FD_ERR_DIS |	\
				 FM_FD_ERR_EOF | FM_FD_ERR_NSS |	\
				 FM_FD_ERR_KSO | FM_FD_ERR_IPP |	\
				 FM_FD_ERR_PTE | FM_FD_ERR_PHE |	\
				 FM_FD_ERR_BLE)

#define	DEFAULT_RX_CUT_END_BYTES	4

struct fman_port_rx_softc {
	struct fman_port_softc sc_base;
	struct fman_port_buffer_pool sc_bpools[FMAN_PORT_MAX_POOLS];
};

static struct ofw_compat_data rx_compats[] = {
	{ "fsl,fman-v2-port-rx", 0 },
	{ "fsl,fman-v3-port-rx", PORT_V3 },
	{ NULL, 0 }
};

static int
fman_port_rx_probe(device_t dev)
{

	if (ofw_bus_search_compatible(dev, rx_compats)->ocd_str == NULL)
		return (ENXIO);

	device_set_desc(dev, "FMan RX port");
	return (BUS_PROBE_DEFAULT);
}

static int
fman_port_rx_attach(device_t dev)
{
	struct fman_port_rx_softc *sc;
	uintptr_t compat_data;
	int port_speed;

	sc = device_get_softc(dev);
	compat_data = ofw_bus_search_compatible(dev, rx_compats)->ocd_data;

	port_speed = 1000;
	if ((compat_data & PORT_V3) == PORT_V3) {
		if (OF_hasprop(ofw_bus_get_node(dev), "fsl,fman-10g-port"))
			port_speed = 10000;
	} else {
		int cell = 0;
		OF_getencprop(ofw_bus_get_node(dev), "cell-index", &cell,
		    sizeof(cell));
		if (cell > RX_10G_PORT_BASE)
			port_speed = 10000;
	}

	return (fman_port_attach_common(dev, &sc->sc_base, port_speed));
}

static int
fman_port_rx_detach(device_t dev)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);

	return (fman_port_detach_common(dev, &sc->sc_base));
}

static int
fman_port_rx_config(device_t dev, struct fman_port_params *params)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;

	fman_port_config_common(base, params);

	base->sc_tasks.extra = 0;
	switch (base->sc_port_speed) {
	case 10000:
		if (base->sc_revision_major < 6) {
			base->sc_tasks.num = 16;
			base->sc_tasks.extra = 8;
		} else
			base->sc_tasks.num = 14;
		break;
	case 1000:
		if (base->sc_revision_major >= 6)
			base->sc_tasks.num = 4;
		else {
			base->sc_tasks.num = 3;
			base->sc_tasks.extra = 2;
		}
		break;
	default:
		base->sc_tasks.num = 0;
		break;
	}

	if (base->sc_revision_major >= 6) {
		base->sc_open_dmas.extra = 0;
		base->sc_open_dmas.num =
		    (base->sc_port_speed == 10000) ? 8 : 2;
	} else if (base->sc_port_speed == 10000) {
		base->sc_open_dmas.num = 8;
		base->sc_open_dmas.extra = 8;
	} else {
		base->sc_open_dmas.num = 1;
		base->sc_open_dmas.extra = 1;
	}

	if (base->sc_revision_major >= 6)
		base->sc_fifo_bufs.num =
		    (base->sc_port_speed == 10000) ? 96 : 50;
	else
		base->sc_fifo_bufs.num =
		    (base->sc_port_speed == 10000) ? 48 : 45;
	base->sc_fifo_bufs.extra = 0;
	base->sc_fifo_bufs.num *= FMAN_BMI_FIFO_UNITS;

	for (int i = 0; i < params->rx_params.num_pools; i++)
		sc->sc_bpools[i] = params->rx_params.bpools[i];

	/* TODO: buf_margins?  See fman_sp_build_buffer_struct */
	return (0);
}

static int
fman_port_rx_init_bmi(struct fman_port_rx_softc *sc)
{
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;

	/* TODO: Sort the buffer pool list.  */
	/* TODO: Backup pools */
	/* TODO: Depletion mode */
	for (int i = 0; i < FMAN_PORT_MAX_POOLS; i++) {
		if (sc->sc_bpools[i].size != 0) {
			bus_write_4(base->sc_mem, FMBM_REBMPI(i),
			    REBMPI_VAL | REBMPI_ACE |
			    (sc->sc_bpools[i].bpid << REBMPI_BPID_S) |
			    sc->sc_bpools[i].size);
		} else
			bus_write_4(base->sc_mem, FMBM_REBMPI(i), 0);
	}

	bus_write_4(base->sc_mem, FMBM_RDA, RDA_WOPT);

	bus_write_4(base->sc_mem, FMBM_RFCA,
	    RFCA_OR | RFCA_SYNC_REQ | RFCA_MR_DEF);

	/*
	 * Route the Parser output through KeyGen (HWK), not straight to
	 * BMI enqueue.  KG then hands off to BMI-enqueue via its own
	 * default NIA (FMKG_GCR low bits, programmed by fman_kg_init as
	 * NIA_ENG_BMI|NIA_BMI_AC_ENQ_FRAME).  Ports with no bound KG
	 * scheme fall through KG's default path to the port's dflt_fqid,
	 * matching the old direct-to-BMI behaviour; ports with a scheme
	 * bound (via fman_kg_alloc_hash_scheme, e.g. from the dpaa_eth
	 * RSS setup) get real hash distribution.
	 */
	bus_write_4(base->sc_mem, FMBM_RFPNE, NIA_ENG_HWK);
	bus_write_4(base->sc_mem, FMBM_RFENE,
	    NIA_ENG_QMI_ENQ | NIA_ORDER_RESTORE);

	bus_write_4(base->sc_mem, FMBM_RFQID, base->sc_default_fqid);
	bus_write_4(base->sc_mem, FMBM_REFQID, base->sc_err_fqid);

	if (base->sc_revision_major < 6)
		bus_write_4(base->sc_mem, FMBM_RETH, RETH_ETHE);

	/* Errata A006320 makes CFED field bad */
	if (base->sc_revision_major == 6 && base->sc_revision_minor == 0)
		/* These are under errata A006320 */;
	else
		bus_write_4(base->sc_mem, FMBM_RFED,
		    DEFAULT_RX_CUT_END_BYTES << BMI_RX_FRAME_END_CUT_SHIFT);

	/* Insert internal context ahead of the frame */
	reg = sizeof(struct fman_internal_context) << REBM_BSM_S;
	bus_write_4(base->sc_mem, FMBM_REBM, reg);
	reg = howmany(FMAN_PARSE_RESULT_OFF, 0x10) << RICP_ICIOF_S;
	reg |= howmany(sizeof(struct fman_internal_context), 0x10);
	bus_write_4(base->sc_mem, FMBM_RICP, reg);

	bus_write_4(base->sc_mem, FMBM_RFNE, NIA_ENG_HWP);
	bus_write_4(base->sc_mem, FMBM_RFSDM, FM_FD_ERR_DIS);
	bus_write_4(base->sc_mem, FMBM_RFSEM, BMI_RX_ERR & ~FM_FD_ERR_DIS);

	return (0);
}

static int
fman_port_rx_init_hwp(struct fman_port_rx_softc *sc)
{
	struct fman_port_softc *base = &sc->sc_base;
	int i;

	bus_write_4(base->sc_mem, HWP_PCAC, HWP_PCAC_PSTOP);
	for (i = 0; i < 100 &&
	    (bus_read_4(base->sc_mem, HWP_PCAC) & HWP_HXS_PCAC_PSTAT) != 0;
	    i++) {
		DELAY(10);
	}
	if (i == 100) {
		device_printf(base->sc_dev, "Timeout stopping HW parser\n");
		return (ENXIO);
	}

	for (i = 0; i < HWP_HXS_COUNT; i++) {
		bus_write_4(base->sc_mem, HWP_HXS_SSA(i), 0);
		bus_write_4(base->sc_mem, HWP_HXS_LCV(i), 0xffffffff);
	}
	bus_write_4(base->sc_mem, HWP_HXS_SSA(HWP_HXS_TCP), HXS_SH_PAD_REM);
	bus_write_4(base->sc_mem, HWP_HXS_SSA(HWP_HXS_UDP), HXS_SH_PAD_REM);

	bus_write_4(base->sc_mem, HWP_PCAC, 0);
	return (0);
}

static int
fman_port_rx_init(device_t dev)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	struct fman_port_init_params params;
	int err;

	params.port_id = base->sc_port_id;
	params.is_rx_port = true;
	params.num_tasks = base->sc_tasks.num;
	params.extra_tasks = base->sc_tasks.extra;
	params.open_dmas = base->sc_open_dmas.num;
	params.extra_dmas = base->sc_open_dmas.extra;
	params.fifo_size = base->sc_fifo_bufs.num;
	params.extra_fifo_size = base->sc_fifo_bufs.extra;
	params.max_frame_length = base->sc_max_frame_length;
	params.deq_pipeline_size = 0;

	/* TODO: verify_size_of_fifo() from Linux driver */
	err = FMAN_SET_PORT_PARAMS(device_get_parent(dev), &params);
	if (err != 0)
		return (err);

	err = fman_port_rx_init_bmi(sc);
	if (err == 0)
		err = fman_port_rx_init_hwp(sc);
	if (err != 0)
		return (err);

	bus_write_4(base->sc_mem, FMQM_PNEN,
	    NIA_ENG_BMI | NIA_BMI_AC_RELEASE);

	/* TODO: keygen here */
	return (0);
}

static int
fman_port_rx_disable(device_t dev)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;
	int count;

	reg = bus_read_4(base->sc_mem, FMBM_RCFG);
	bus_write_4(base->sc_mem, FMBM_RCFG, reg & ~BMI_PORT_CFG_EN);
	for (count = 0; count < 100; count++) {
		DELAY(10);
		reg = bus_read_4(base->sc_mem, FMBM_RST);
		if (!(reg & PNS_DEQ_FD_BSY))
			break;
	}
	if (count == 100)
		device_printf(base->sc_dev, "Timeout stopping BMI\n");

	return (0);
}

static int
fman_port_rx_enable(device_t dev)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);
	struct fman_port_softc *base = &sc->sc_base;
	uint32_t reg;

	reg = bus_read_4(base->sc_mem, FMQM_PNC);
	bus_write_4(base->sc_mem, FMQM_PNC, reg | PNC_EN | PNC_STEN);

	reg = bus_read_4(base->sc_mem, FMBM_RCFG);
	bus_write_4(base->sc_mem, FMBM_RCFG, reg | BMI_PORT_CFG_EN);

	bus_write_4(base->sc_mem, FMBM_RSTC, RSTC_EN);

	return (0);
}

void
fman_port_rx_use_kg(device_t dev, bool enable)
{
	struct fman_port_rx_softc *sc = device_get_softc(dev);

	bus_write_4(sc->sc_base.sc_mem, FMBM_RFPNE,
	    enable ? NIA_ENG_HWK :
	    (NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME));
}

static device_method_t fman_port_rx_methods[] = {
	DEVMETHOD(device_probe,		fman_port_rx_probe),
	DEVMETHOD(device_attach,	fman_port_rx_attach),
	DEVMETHOD(device_detach,	fman_port_rx_detach),

	DEVMETHOD(fman_port_config,	fman_port_rx_config),
	DEVMETHOD(fman_port_init,	fman_port_rx_init),
	DEVMETHOD(fman_port_enable,	fman_port_rx_enable),
	DEVMETHOD(fman_port_disable,	fman_port_rx_disable),

	DEVMETHOD_END
};

DEFINE_CLASS_0(fman_port_rx, fman_port_rx_driver, fman_port_rx_methods,
    sizeof(struct fman_port_rx_softc));
EARLY_DRIVER_MODULE(fman_port_rx, fman, fman_port_rx_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
