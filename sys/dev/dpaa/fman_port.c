/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <machine/bus.h>
#include "fman.h"
#include "fman_parser.h"
#include "fman_port.h"
#include "fman_if.h"
#include "fman_port_if.h"

struct fman_port_rsrc {
	uint32_t	num;
	uint32_t	extra;
};

#define	MAX_BM_POOLS	64
struct fman_port_softc {
	device_t sc_dev;
	struct resource	*sc_mem;
	int sc_port_id;
	int sc_port_speed;
	int sc_port_type;

	int sc_revision_major;
	int sc_revision_minor;

	int sc_max_frame_length;
	int sc_bm_max_pools;
	int sc_max_port_fifo_size;
	int sc_qman_channel_id;

	int sc_deq_byte_count;
	int sc_deq_high_priority;
	int sc_tx_deq_pipeline_depth;

	int sc_default_fqid;
	int sc_err_fqid;
	int pcd_base_fqid;
	int pcd_fqs_count;

	int sc_max_ext_portals;
	int sc_max_sub_portals;

	struct fman_port_rsrc sc_open_dmas;
	struct fman_port_rsrc sc_tasks;
	struct fman_port_rsrc sc_fifo_bufs;

	struct fman_port_buffer_pool sc_bpools[FMAN_PORT_MAX_POOLS];
};

#define	TX_10G_PORT_BASE	0x30
#define	RX_10G_PORT_BASE	0x10

#define	FMAN_PORT_TYPE_TX	0
#define	FMAN_PORT_TYPE_RX	1

#define	PORT_RX		0x01
#define	PORT_TX		0x02
#define	PORT_V3		0x04

#define	FMBM_RCFG		0x000
#define	  BMI_PORT_CFG_EN	  0x80000000
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

#define	FMBM_TCFG	0x000
#define	FMBM_TST		0x004
#define	FMBM_TDA		0x008
#define	FMBM_TFP		0x00c
#define	  BMI_FIFO_PIPELINE_DEPTH_SHIFT	12
#define	FMBM_TFED	0x010
#define	FMBM_TICP	0x014
#define	  TICP_ICEOF_M		  0x001f0000
#define	  TICP_ICEOF_S		  16
#define	  TICP_ICIOF_M		  0x00000f00
#define	  TICP_ICIOF_S		  8
#define	  TICP_ICSZ_S		  0x0000001f
#define	FMBM_TFDNE	0x018
#define	FMBM_TFCA	0x01c
#define	  TFCA_MR_DEF	  0
#define	  TFCA_ATTR_ORDER	  0x80000000
#define	FMBM_TCFQID	0x020
#define	FMBM_TEFQID	0x024
#define	FMBM_TFENE	0x028
#define	FMBM_TFNE	0x070
#define	  TFNE_EBD		  0x80000000

#define	FMQM_PNC			0x400
#define	  PNC_EN			  0x80000000
#define	  PNC_STEN			  0x80000000
#define	FMQM_PNS			0x404
#define	  PNS_DEQ_FD_BSY		  0x20000000
#define	FMQM_PNEN			0x41c
#define	FMQM_PNDN			0x42c
#define	FMQM_PNDC			0x430
#define	  QMI_DEQ_CFG_PRI		  0x80000000
#define	  QMI_DEQ_CFG_TYPE1		  0x10000000
#define	  QMI_DEQ_CFG_TYPE2		  0x20000000
#define	  QMI_DEQ_CFG_TYPE3		  0x30000000
#define	  QMI_DEQ_CFG_PREFETCH_PARTIAL	  0x01000000
#define	  QMI_DEQ_CFG_PREFETCH_FULL	  0x03000000
#define	  QMI_DEQ_CFG_SP_MASK		  0xf
#define	  QMI_DEQ_CFG_SP_SHIFT		  20

#define	HWP_PCAC		0xbf8
#define	  HWP_PCAC_PSTOP	  0x00000001
#define	  HWP_HXS_PCAC_PSTAT	  0x00000100
#define	HWP_HXS_SSA(x)		(0x800 + x * 2 * sizeof(uint32_t))
#define	HWP_HXS_LCV(x)		(0x800 + (x * 2 + 1) * sizeof(uint32_t))
#define	HWP_HXS_TCP		  0xA
#define	HWP_HXS_UDP		  0xB
#define	HXS_SH_PAD_REM		  0x80000000
#define	HWP_HXS_COUNT		16

#define	PORT_MAX_FRAME_LENGTH	9600

#define	NIA_ORDER_RESTORE	0x00800000
#define	NIA_ENG_BMI		0x00500000
#define	NIA_ENG_QMI_DEQ		0x00580000
#define	NIA_ENG_QMI_ENQ		0x00540000
#define	NIA_ENG_HWP		0x00440000
#define	NIA_ENG_HWK		0x00480000
#define	NIA_BMI_AC_TX_RELEASE	0x000002c0
#define	NIA_BMI_AC_TX		0x00000274
#define	NIA_BMI_AC_RELEASE	0x000000c0
#define	NIA_BMI_AC_ENQ_FRAME	0x00000002
#define	NIA_BMI_AC_FETCH_ALLFRAME	0x0000020c

#define	BMI_RX_ERR		(FM_FD_ERR_DMA | FM_FD_ERR_FPE | \
				 FM_FD_ERR_FSE | FM_FD_ERR_DIS | \
				 FM_FD_ERR_EOF | FM_FD_ERR_NSS | \
				 FM_FD_ERR_KSO | FM_FD_ERR_IPP | \
				 FM_FD_ERR_PTE | FM_FD_ERR_PHE | \
				 FM_FD_ERR_BLE)

/* Default configurations */
#define	DEFAULT_RX_CUT_END_BYTES	4

static struct ofw_compat_data compats[] = {
	{ "fsl,fman-v2-port-rx", PORT_RX },
	{ "fsl,fman-v2-port-tx", PORT_TX },
	{ "fsl,fman-v3-port-rx", PORT_V3 | PORT_RX },
	{ "fsl,fman-v3-port-tx", PORT_V3 | PORT_TX },
	{ NULL, 0 }
};

static int
fman_port_probe(device_t dev)
{
	if (ofw_bus_search_compatible(dev, compats)->ocd_str == NULL)
		return (ENXIO);

	device_set_desc(dev, "FMan port");

	return (BUS_PROBE_DEFAULT);
}

static int
fman_port_attach(device_t dev)
{
	struct fman_port_softc *sc;
	phandle_t node;
	pcell_t cell;
	uintptr_t compat_data =
	    ofw_bus_search_compatible(dev, compats)->ocd_data;
	int port_speed = 1000;
	int port_type;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "cell-index", &cell, sizeof(cell)) < 0) {
		device_printf(dev, "No cell-index property");
		return (ENXIO);
	}

	sc->sc_port_id = cell;

	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0,
	    RF_ACTIVE | RF_SHAREABLE);

	if (sc->sc_mem == NULL) {
		device_printf(dev, "failed to allocate MMIO");
		return (ENXIO);
	}

	FMAN_GET_REVISION(device_get_parent(dev),
	    &sc->sc_revision_major, &sc->sc_revision_minor);

	if ((compat_data & PORT_TX) == PORT_TX)
		port_type = FMAN_PORT_TYPE_TX;
	else
		port_type = FMAN_PORT_TYPE_RX;

	if ((compat_data & PORT_V3) == PORT_V3) {
		if (OF_hasprop(node, "fsl,fman-10g-port"))
			port_speed = 10000;
	} else {
		if ((compat_data & PORT_TX) &&
		    sc->sc_port_id > TX_10G_PORT_BASE)
			port_speed = 10000;
		else if ((compat_data & PORT_RX) &&
		    sc->sc_port_id > RX_10G_PORT_BASE)
			port_speed = 10000;
	}

	if (sc->sc_port_speed == 10000) {
		sc->sc_deq_high_priority = true;
		sc->sc_deq_byte_count = 0x1400;
	} else {
		sc->sc_deq_high_priority = false;
		sc->sc_deq_byte_count = 0x0400;
	}

	sc->sc_port_type = port_type;
	sc->sc_port_speed = port_speed;

	sc->sc_bm_max_pools = MAX_BM_POOLS;
	sc->sc_max_frame_length = PORT_MAX_FRAME_LENGTH;

	if (port_type == FMAN_PORT_TYPE_TX)
		sc->sc_qman_channel_id =
		    FMAN_GET_QMAN_CHANNEL_ID(device_get_parent(dev),
		    sc->sc_port_id);

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

	return (0);
}

static int
fman_port_detach(device_t dev)
{
	struct fman_port_softc *sc = device_get_softc(dev);

	if (sc->sc_mem != NULL)
		bus_release_resource(dev, sc->sc_mem);

	return (0);
}

static int
fman_port_config(device_t dev, struct fman_port_params *params)
{
	struct fman_port_softc *sc = device_get_softc(dev);

	sc->sc_default_fqid = params->dflt_fqid;
	sc->sc_err_fqid = params->err_fqid;

	sc->sc_max_port_fifo_size =
	    FMAN_GET_BMI_MAX_FIFO_SIZE(device_get_parent(dev));
	switch (sc->sc_revision_major) {
	case 2:
	case 3:
		sc->sc_max_ext_portals = 4;
		sc->sc_max_sub_portals = 12;
		break;
	case 6:
		sc->sc_max_ext_portals = 8;
		sc->sc_max_sub_portals = 16;
		break;
	}
	if (sc->sc_revision_major >= 6 &&
	    sc->sc_port_type == FMAN_PORT_TYPE_TX && sc->sc_port_speed == 1000)
		/* Errata A005127 workaround */
		bus_write_4(sc->sc_mem, FMBM_TFP, 0x00001013);

	sc->sc_tasks.extra = 0;

	switch (sc->sc_port_speed) {
	case 10000:
		if (sc->sc_revision_major < 6) {
			sc->sc_tasks.num = 16;
			if (sc->sc_port_type == FMAN_PORT_TYPE_RX)
				sc->sc_tasks.extra = 8;
		} else
			sc->sc_tasks.num = 14;
		break;
	case 1000:
		if (sc->sc_revision_major >= 6)
			sc->sc_tasks.num = 4;
		else {
			sc->sc_tasks.num = 3;
			if (sc->sc_port_type == FMAN_PORT_TYPE_RX)
				sc->sc_tasks.extra = 2;
		}
		break;
	default:
		sc->sc_tasks.num = 0;
		break;
	}

	/* Open DMAs */
	if (sc->sc_revision_major >= 6) {
		sc->sc_open_dmas.extra = 0;
		if (sc->sc_port_speed == 10000) {
			if (sc->sc_port_type == FMAN_PORT_TYPE_TX)
				sc->sc_open_dmas.num = 12;
			else
				sc->sc_open_dmas.num = 8;
		} else {
			if (sc->sc_port_type == FMAN_PORT_TYPE_TX)
				sc->sc_open_dmas.num = 3;
			else
				sc->sc_open_dmas.num = 2;
		}
	} else {
		if (sc->sc_port_speed == 10000) {
			sc->sc_open_dmas.num = 8;
			sc->sc_open_dmas.num = 8;
		} else {
			sc->sc_open_dmas.num = 1;
			sc->sc_open_dmas.extra = 1;
		}
	}

	/* FIFO bufs */
	if (sc->sc_revision_major >= 6) {
		if (sc->sc_port_type == FMAN_PORT_TYPE_TX)
			if (sc->sc_port_speed == 10000)
				sc->sc_fifo_bufs.num = 64;
			else
				sc->sc_fifo_bufs.num = 50;
		else
			if (sc->sc_port_speed == 10000)
				sc->sc_fifo_bufs.num = 96;
			else
				sc->sc_fifo_bufs.num = 50;
	} else {
		if (sc->sc_port_type == FMAN_PORT_TYPE_TX)
			if (sc->sc_port_speed == 10000)
				sc->sc_fifo_bufs.num = 48;
			else
				sc->sc_fifo_bufs.num = 44;
		else
			if (sc->sc_port_speed == 10000)
				sc->sc_fifo_bufs.num = 48;
			else
				sc->sc_fifo_bufs.num = 45;
	}

	sc->sc_fifo_bufs.extra = 0;
	sc->sc_fifo_bufs.num *= FMAN_BMI_FIFO_UNITS;

	if (sc->sc_port_type == FMAN_PORT_TYPE_RX)
		for (int i = 0; i < params->rx_params.num_pools; i++)
			sc->sc_bpools[i] = params->rx_params.bpools[i];

	/* TODO: buf_margins?  See fman_sp_build_buffer_struct */

	return (0);
}

static int
fman_port_init_bmi_rx(struct fman_port_softc *sc)
{
	uint32_t reg;

	/* TODO: Sort the buffer pool list.  */
	/* TODO: Backup pools */
	/* TODO: Depletion mode */
	for (int i = 0; i < FMAN_PORT_MAX_POOLS; i++) {
		/* Initialize the external pool info */
		if (sc->sc_bpools[i].size != 0) {
			bus_write_4(sc->sc_mem, FMBM_REBMPI(i),
			    REBMPI_VAL | REBMPI_ACE |
			    (sc->sc_bpools[i].bpid << REBMPI_BPID_S) |
			    sc->sc_bpools[i].size);
		} else
			/* Mark invalid if zero */
			bus_write_4(sc->sc_mem, FMBM_REBMPI(i), 0);
	}

	bus_write_4(sc->sc_mem, FMBM_RDA, RDA_WOPT);

	bus_write_4(sc->sc_mem, FMBM_RFCA,
	    RFCA_OR | RFCA_SYNC_REQ | RFCA_MR_DEF);

	bus_write_4(sc->sc_mem, FMBM_RFPNE,
	    NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME);
	bus_write_4(sc->sc_mem, FMBM_RFENE,
	    NIA_ENG_QMI_ENQ | NIA_ORDER_RESTORE);

	bus_write_4(sc->sc_mem, FMBM_RFQID, sc->sc_default_fqid);
	bus_write_4(sc->sc_mem, FMBM_REFQID, sc->sc_err_fqid);

	if (sc->sc_revision_major < 6)
		bus_write_4(sc->sc_mem, FMBM_RETH, RETH_ETHE);

	/* Errata A006320 makes CFED field bad */
	if (sc->sc_revision_major == 6 && (sc->sc_revision_minor == 0))
		/* These are under errata A006320 */;
	else
		bus_write_4(sc->sc_mem, FMBM_RFED,
		    DEFAULT_RX_CUT_END_BYTES << BMI_RX_FRAME_END_CUT_SHIFT);

	/* Insert internal context ahead of the frame */
	reg = sizeof(struct fman_internal_context) << REBM_BSM_S;
	bus_write_4(sc->sc_mem, FMBM_REBM, reg);
	reg = howmany(FMAN_PARSE_RESULT_OFF, 0x10) << RICP_ICIOF_S;
	reg |= howmany(sizeof(struct fman_internal_context), 0x10);
	bus_write_4(sc->sc_mem, FMBM_RICP, reg);

	bus_write_4(sc->sc_mem, FMBM_RFNE, NIA_ENG_HWP);
	bus_write_4(sc->sc_mem, FMBM_RFSDM, FM_FD_ERR_DIS);
	bus_write_4(sc->sc_mem, FMBM_RFSEM, BMI_RX_ERR & ~FM_FD_ERR_DIS);

	return (0);
}

static int
fman_port_init_bmi_tx(struct fman_port_softc *sc)
{
	uint32_t reg;
	int depth;

	bus_write_4(sc->sc_mem, FMBM_TCFG, 0);
	bus_write_4(sc->sc_mem, FMBM_TDA, 0);
	bus_write_4(sc->sc_mem, FMBM_TFED, 0);
	if (sc->sc_port_speed == 10000)
		depth = 4;
	else if (sc->sc_revision_major >= 6)
		depth = 2;
	else
		depth = 1;
	sc->sc_tx_deq_pipeline_depth = depth;
	reg = ((depth - 1) << BMI_FIFO_PIPELINE_DEPTH_SHIFT) | 0x13;
	bus_write_4(sc->sc_mem, FMBM_TFP, reg);

	/* Default color: green */
	bus_write_4(sc->sc_mem, FMBM_TFCA,
	    TFCA_MR_DEF | TFCA_ATTR_ORDER);

	bus_write_4(sc->sc_mem, FMBM_TFDNE, NIA_ENG_QMI_DEQ);
	bus_write_4(sc->sc_mem, FMBM_TFENE,
	    NIA_ENG_QMI_ENQ | NIA_ORDER_RESTORE);

	/* Insert internal context ahead of the frame */
	reg = howmany(FMAN_PARSE_RESULT_OFF, 0x10) << TICP_ICIOF_S;
	reg |= howmany(sizeof(struct fman_internal_context), 0x10);
	bus_write_4(sc->sc_mem, FMBM_TICP, reg);

	if (sc->sc_revision_major >= 6)
		bus_write_4(sc->sc_mem, FMBM_TFNE,
		    (sc->sc_default_fqid == 0 ? TFNE_EBD : 0) |
		    NIA_BMI_AC_FETCH_ALLFRAME);
	bus_write_4(sc->sc_mem, FMBM_TCFQID, sc->sc_default_fqid);
	bus_write_4(sc->sc_mem, FMBM_TEFQID, sc->sc_err_fqid);

	return (0);
}

static int
fman_port_init_hwp(struct fman_port_softc *sc)
{
	int i;

	/* Stop the parser so we can initialize it for our uses */
	bus_write_4(sc->sc_mem, HWP_PCAC, HWP_PCAC_PSTOP);

	for (i = 0; i < 100 &&
	    (bus_read_4(sc->sc_mem, HWP_PCAC) & HWP_HXS_PCAC_PSTAT) != 0; i++) {
		DELAY(10);
	}
	if (i == 100) {
		device_printf(sc->sc_dev, "Timeout stopping HW parser\n");
		return (ENXIO);
	}

	/* set the parser examination config */
	for (i = 0; i < HWP_HXS_COUNT; i++) {
		bus_write_4(sc->sc_mem, HWP_HXS_SSA(i), 0);
		bus_write_4(sc->sc_mem, HWP_HXS_LCV(i), 0xffffffff);
	}
	bus_write_4(sc->sc_mem, HWP_HXS_SSA(HWP_HXS_TCP), HXS_SH_PAD_REM);
	bus_write_4(sc->sc_mem, HWP_HXS_SSA(HWP_HXS_UDP), HXS_SH_PAD_REM);

	/* Re-enable the parser */
	bus_write_4(sc->sc_mem, HWP_PCAC, 0);

	return (0);
}

static int
fman_port_init_qmi(struct fman_port_softc *sc)
{
	uint32_t reg;

	if (sc->sc_port_type == FMAN_PORT_TYPE_RX) {
		bus_write_4(sc->sc_mem, FMQM_PNEN,
		    NIA_ENG_BMI | NIA_BMI_AC_RELEASE);
		return (0);
	}

	/* TX port */
	bus_write_4(sc->sc_mem, FMQM_PNDN,
	    NIA_ENG_BMI | NIA_BMI_AC_TX);
	/* TX port */
	bus_write_4(sc->sc_mem, FMQM_PNEN,
	    NIA_ENG_BMI | NIA_BMI_AC_TX_RELEASE);

	reg = 0;

	if (sc->sc_deq_high_priority)
		reg |= QMI_DEQ_CFG_PRI;

	reg |= QMI_DEQ_CFG_TYPE1;
	reg |= QMI_DEQ_CFG_PREFETCH_FULL;
	reg |= (sc->sc_qman_channel_id & QMI_DEQ_CFG_SP_MASK) << QMI_DEQ_CFG_SP_SHIFT;
	reg |= sc->sc_deq_byte_count;
	bus_write_4(sc->sc_mem, FMQM_PNDC, reg);

	return (0);
}

static int
fman_port_init(device_t dev)
{
	struct fman_port_init_params params;
	struct fman_port_softc *sc = device_get_softc(dev);
	int err;

	if (sc->sc_port_type == FMAN_PORT_TYPE_RX) {
		/* Set up RX buffers and fifo */
	}
	params.port_id = sc->sc_port_id;
	params.is_rx_port = (sc->sc_port_type == FMAN_PORT_TYPE_RX);
	params.num_tasks = sc->sc_tasks.num;
	params.extra_tasks = sc->sc_tasks.extra;
	params.open_dmas = sc->sc_open_dmas.num;
	params.extra_dmas = sc->sc_open_dmas.extra;
	params.fifo_size = sc->sc_fifo_bufs.num;
	params.extra_fifo_size = sc->sc_fifo_bufs.extra;
	params.max_frame_length = sc->sc_max_frame_length;
	params.deq_pipeline_size = sc->sc_tx_deq_pipeline_depth;

	/* TODO: verify_size_of_fifo() from Linux driver */
	err = FMAN_SET_PORT_PARAMS(device_get_parent(dev), &params);

	if (err != 0)
		return (err);

	if (sc->sc_port_type == FMAN_PORT_TYPE_TX)
		err = fman_port_init_bmi_tx(sc);
	else {
		err = fman_port_init_bmi_rx(sc);
		if (err == 0)
			fman_port_init_hwp(sc);
	}

	if (err != 0)
		return (err);

	err = fman_port_init_qmi(sc);

	/* TODO: keygen here */

	return (err);
}

static int
fman_port_disable(device_t dev)
{
	struct fman_port_softc *sc;
	uint32_t	reg;
	int		count;

	sc = device_get_softc(dev);

	switch (sc->sc_port_type) {
	case FMAN_PORT_TYPE_TX:
		reg = bus_read_4(sc->sc_mem, FMQM_PNC);
		bus_write_4(sc->sc_mem, FMQM_PNC, reg & ~PNC_EN);
		for (count = 0; count < 100; count++) {
			DELAY(10);
			reg = bus_read_4(sc->sc_mem, FMQM_PNS);
			if (!(reg & PNS_DEQ_FD_BSY))
				break;
		}
		if (count == 100)
			device_printf(sc->sc_dev, "Timeout stopping QMI\n");
		reg = bus_read_4(sc->sc_mem, FMBM_TCFG);
		bus_write_4(sc->sc_mem, FMBM_TCFG, reg & ~BMI_PORT_CFG_EN);
		for (count = 0; count < 100; count++) {
			DELAY(10);
			reg = bus_read_4(sc->sc_mem, FMBM_TST);
			if (!(reg & PNS_DEQ_FD_BSY))
				break;
		}
		if (count == 100)
			device_printf(sc->sc_dev, "Timeout stopping BMI");
		break;
	case FMAN_PORT_TYPE_RX:
		reg = bus_read_4(sc->sc_mem, FMBM_RCFG);
		bus_write_4(sc->sc_mem, FMBM_RCFG, reg & ~BMI_PORT_CFG_EN);
		for (count = 0; count < 100; count++) {
			DELAY(10);
			reg = bus_read_4(sc->sc_mem, FMBM_RST);
			if (!(reg & PNS_DEQ_FD_BSY))
				break;
		}
		if (count == 100)
			device_printf(sc->sc_dev, "Timeout stopping BMI");
		break;
	}

	return (0);
}

static int
fman_port_enable(device_t dev)
{
	struct fman_port_softc *sc;
	uint32_t	reg;

	sc = device_get_softc(dev);
	switch (sc->sc_port_type) {
	case FMAN_PORT_TYPE_TX:
		reg = bus_read_4(sc->sc_mem, FMQM_PNC);
		bus_write_4(sc->sc_mem, FMQM_PNC, reg | PNC_EN | PNC_STEN);
		reg = bus_read_4(sc->sc_mem, FMBM_TCFG);
		bus_write_4(sc->sc_mem, FMBM_TCFG, reg | BMI_PORT_CFG_EN);
		break;
	case FMAN_PORT_TYPE_RX:
		reg = bus_read_4(sc->sc_mem, FMBM_RCFG);
		bus_write_4(sc->sc_mem, FMQM_PNC, reg | PNC_EN | PNC_STEN);
		bus_write_4(sc->sc_mem, FMBM_RCFG, reg | BMI_PORT_CFG_EN);
		bus_write_4(sc->sc_mem, FMBM_RSTC, RSTC_EN);
		break;
	}

	return (0);
}

static device_method_t fman_port_methods[] = {
	DEVMETHOD(device_probe,		fman_port_probe),
	DEVMETHOD(device_attach,	fman_port_attach),
	DEVMETHOD(device_detach,	fman_port_detach),

	DEVMETHOD(fman_port_config,	fman_port_config),
	DEVMETHOD(fman_port_init,	fman_port_init),
	DEVMETHOD(fman_port_enable,	fman_port_enable),
	DEVMETHOD(fman_port_disable,	fman_port_disable),

	DEVMETHOD_END
};

DEFINE_CLASS_0(fman_port, fman_port_driver, fman_port_methods,
    sizeof(struct fman_port_softc));
EARLY_DRIVER_MODULE(fman_port, fman, fman_port_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
