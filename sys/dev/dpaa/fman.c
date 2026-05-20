/*
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/fdt/simplebus.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include "opt_platform.h"

#include <powerpc/mpc85xx/mpc85xx.h>

#include "fman.h"

#define	FMAN_BMI_OFFSET		0x80000
#define	FMAN_QMI_OFFSET		0x80400
#define	FMAN_KG_OFFSET		0xc1000
#define	FMAN_DMA_OFFSET		0xc2000
#define	FMAN_FPM_OFFSET		0xc3000
#define	FMAN_IMEM_OFFSET	0xc4000
#define	FMAN_HWP_OFFSET		0xc7000
#define	FMAN_CGP_OFFSET		0xdb000

#define	FM_IP_REV_1		(FMAN_FPM_OFFSET + 0x0c4)
#define	  IP_REV_1_MAJ_M	  0x0000ff00
#define	  IP_REV_1_MAJ_S	  8
#define	  IP_REV_1_MIN_M	  0x000000ff
#define	FM_RSTC			(FMAN_FPM_OFFSET + 0x0cc)
#define	  FM_RSTC_FM_RESET	  0x80000000

#define	FMBM_INIT	(FMAN_BMI_OFFSET + 0x000)
#define	  INIT_STR	  0x80000000
#define	FMBM_CFG1	(FMAN_BMI_OFFSET + 0x0004)
#define	  FBPS_M	  0x07ff0000
#define	  FBPS_S	  16
#define	  FBPO_M	  0x000007ff
#define	FMBM_CFG2	(FMAN_BMI_OFFSET + 0x0008)
#define	  TNTSKS_M	  0x007f0000
#define	  TNTSKS_S	  16
#define	FMBM_IEVR	(FMAN_BMI_OFFSET + 0x0020)
#define	  IEVR_SPEC	  0x80000000
#define	  IEVR_LEC	  0x40000000
#define	  IEVR_STEC	  0x20000000
#define	  IEVR_DEC	  0x10000000
#define	FMBM_IER	(FMAN_BMI_OFFSET + 0x0024)
#define	  IER_SPECE	  0x80000000
#define	  IER_LECE	  0x40000000
#define	  IER_STECE	  0x20000000
#define	  IER_DECE	  0x10000000
#define	FMBM_PP(n)	(FMAN_BMI_OFFSET + 0x104 + ((n - 1) * 4))
#define	  PP_MXT_M	  0x3f000000
#define	  PP_MXT_S	  24
#define	  PP_EXT_M	  0x000f0000
#define	  PP_EXT_S	  16
#define	  PP_MXD_M	  0x00000f00
#define	  PP_MXD_S	  8
#define	  PP_EXD_M	  0x0000000f
#define	FMBM_PFS(n)	(FMAN_BMI_OFFSET + 0x204 + ((n - 1) * 4))
#define	  PFS_EXBS_M	  0x03ff0000
#define	  PFS_EXBS_S	  16
#define	  PFS_IFSZ_M	  0x000003ff
#define	FMQM_GC		(FMAN_QMI_OFFSET + 0x000)
#define	  GC_STEN	  0x10000000
#define	  GC_ENQ_THR_S	  8
#define	  GC_ENQ_THR_M	  0x00003f00
#define	  GC_DEQ_THR_M	  0x0000003f
#define	FMQM_EIE	(FMAN_QMI_OFFSET + 0x008)
#define	  EIE_DEE	  0x80000000
#define	  EIE_DFUPE	  0x40000000
#define	FMQM_EIEN	(FMAN_QMI_OFFSET + 0x00c)
#define	  EIEN_DEE	  0x80000000
#define	  EIEN_DFUPE	  0x40000000
#define FMQM_IE
#define	IRAM_ADDR	(FMAN_IMEM_OFFSET + 0x000)
#define	  IADD_AIE	  0x80000000
#define	IRAM_DATA	(FMAN_IMEM_OFFSET + 0x004)
#define	IRAM_READY	(FMAN_IMEM_OFFSET + 0x0c)
#define	  IREADY_READY	  0x80000000

#define	FMPR_RPIMAC	(FMAN_HWP_OFFSET + 0x844)
#define	  HWP_RPIMAC_PEN	  0x00000001

#define	FMDM_SR		(FMAN_DMA_OFFSET + 0x000)
#define	  SR_CMDQNE	  0x10000000
#define	  SR_BER	  0x08000000
#define	  SR_RDB_ECC	  0x04000000
#define	  SR_WRB_SECC	  0x02000000
#define	FMDM_MR		(FMAN_DMA_OFFSET + 0x004)
#define	  MR_CEN_M	  0x0000e000
#define	  MR_CEN_S	  13
#define	FMDM_SETR	(FMAN_DMA_OFFSET + 0x010)
#define	FMDM_EBCR	(FMAN_DMA_OFFSET + 0x2c)
#define	FMDM_PLRn(n)	(FMAN_DMA_OFFSET + 0x060 + (4 * (n / 2)))
#define	PLRN_LIODN_M(n)	  (0xfff << PLRN_LIODN_S(n))
#define	PLRN_LIODN_S(n)	  ((n & 1) ? 0 : 16)

#define	FMFP_TSC1	(FMAN_FPM_OFFSET + 0x060)
#define	  TSC1_TEN	  0x80000000
#define	FMFP_TSC2	(FMAN_FPM_OFFSET + 0x064)
#define	  TSC2_TSIV_INT_S	  16
#define	FM_RCR		(FMAN_FPM_OFFSET + 0x070)
#define	  RCR_FEE	  0x80000000
#define	  RCR_IEE	  0x40000000
#define	  RCR_MET	  0x20000000
#define	  RCR_IET	  0x10000000
#define	  RCR_SFE	  0x08000000
#define	FMFP_EE		(FMAN_FPM_OFFSET + 0x0dc)
#define	  EE_DECC	  0x80000000
#define	  EE_STL	  0x40000000
#define	  EE_SECC	  0x20000000
#define	  EE_RFM	  0x00010000
#define	  EE_DECC_EN	  0x00008000
#define	  EE_STL_EN	  0x00004000
#define	  EE_SECC_EN	  0x00002000
#define	  EE_EHM	  0x00000008
#define	  EE_CER	  0x00000002
#define	  EE_DER	  0x00000001
#define	FMFP_CEV0	(FMAN_FPM_OFFSET + 0x0e0)
#define	FMFP_CEV1	(FMAN_FPM_OFFSET + 0x0e4)
#define	FMFP_CEV2	(FMAN_FPM_OFFSET + 0x0e8)
#define	FMFP_CEV3	(FMAN_FPM_OFFSET + 0x0ec)

/* DMA constants */
#define	DMA_CAM_UNITS	8
#define	DMA_CAM_SIZE	64
#define	DMA_CAM_ALIGN	64


/* Timestamp counter */
#define	FM_TIMESTAMP_1US_BIT	8

static MALLOC_DEFINE(M_FMAN, "fman", "fman devices information");

static void fman_intr(void *arg);

/**
 * @group FMan private defines.
 * @{
 */

/**
 * @group FMan private methods/members.
 * @{
 */

int
fman_activate_resource(device_t bus, device_t child, struct resource *res)
{
	struct fman_softc *sc;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int i, rv;

	sc = device_get_softc(bus);
	if (rman_get_type(res) != SYS_RES_IRQ) {
		for (i = 0; i < sc->sc_base.nranges; i++) {
			if (rman_is_region_manager(res, &sc->rman) != 0) {
				bt = rman_get_bustag(sc->mem_res);
				rv = bus_space_subregion(bt,
				    rman_get_bushandle(sc->mem_res),
				    rman_get_start(res) -
				    rman_get_start(sc->mem_res),
				    rman_get_size(res), &bh);
				if (rv != 0)
					return (rv);
				rman_set_bustag(res, bt);
				rman_set_bushandle(res, bh);
				return (rman_activate_resource(res));
			}
		}
		return (EINVAL);
	}
	return (bus_generic_activate_resource(bus, child, res));
}

int
fman_release_resource(device_t bus, device_t child, struct resource *res)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;
	int passthrough, rv;

	passthrough = (device_get_parent(child) != bus);
	rl = BUS_GET_RESOURCE_LIST(bus, child);
	if (rman_get_type(res) != SYS_RES_IRQ) {
		if ((rman_get_flags(res) & RF_ACTIVE) != 0) {
			rv = bus_deactivate_resource(child, res);
			if (rv != 0)
				return (rv);
		}
		rv = rman_release_resource(res);
		if (rv != 0)
			return (rv);
		if (!passthrough) {
			rle = resource_list_find(rl, rman_get_type(res),
			    rman_get_rid(res));
			KASSERT(rle != NULL,
			    ("%s: resource entry not found!", __func__));
			KASSERT(rle->res != NULL,
			   ("%s: resource entry is not busy", __func__));
			rle->res = NULL;
		}
		return (0);
	}
	return (resource_list_release(rl, bus, child, res));
}

struct resource *
fman_alloc_resource(device_t bus, device_t child, int type, int rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct fman_softc *sc;
	struct resource_list *rl;
	struct resource_list_entry *rle = NULL;
	struct resource *res;
	int i, isdefault, passthrough;

	isdefault = RMAN_IS_DEFAULT_RANGE(start, end);
	passthrough = (device_get_parent(child) != bus);
	sc = device_get_softc(bus);
	rl = BUS_GET_RESOURCE_LIST(bus, child);
	switch (type) {
	case SYS_RES_MEMORY:
		KASSERT(!(isdefault && passthrough),
		    ("%s: passthrough of default allocation", __func__));
		if (!passthrough) {
			rle = resource_list_find(rl, type, rid);
			if (rle == NULL)
				return (NULL);
			KASSERT(rle->res == NULL,
			    ("%s: resource entry is busy", __func__));
			if (isdefault) {
				start = rle->start;
				count = ulmax(count, rle->count);
				end = ulmax(rle->end, start + count - 1);
			}
		}

		res = NULL;
		/* Map fman ranges to nexus ranges. */
		for (i = 0; i < sc->sc_base.nranges; i++) {
			if (start >= sc->sc_base.ranges[i].bus && end <
			    sc->sc_base.ranges[i].bus + sc->sc_base.ranges[i].size) {
				start += rman_get_start(sc->mem_res);
				end += rman_get_start(sc->mem_res);
				res = rman_reserve_resource(&sc->rman, start,
				    end, count, flags & ~RF_ACTIVE, child);
				if (res == NULL)
					return (NULL);
				rman_set_rid(res, rid);
				rman_set_type(res, type);
				if ((flags & RF_ACTIVE) != 0 && bus_activate_resource(
				    child, type, rid, res) != 0) {
					rman_release_resource(res);
					return (NULL);
				}
				break;
			}
		}
		if (!passthrough)
			rle->res = res;
		return (res);
	case SYS_RES_IRQ:
		return (resource_list_alloc(rl, bus, child, type, rid, start,
		    end, count, flags));
	}
	return (NULL);
}


static int
fman_get_revision_major(struct fman_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, FM_IP_REV_1);

	return ((reg & IP_REV_1_MAJ_M) >> IP_REV_1_MAJ_S);
}

static int
fman_get_revision_minor(struct fman_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, FM_IP_REV_1);

	return ((reg & IP_REV_1_MIN_M));
}

static void
fman_fill_soc_params(struct fman_softc *sc)
{

	switch (sc->sc_revision_major) {
	case 2:
		sc->bmi_max_fifo_size = 160 * 1024;
		sc->iram_size = 64 * 1024;
		sc->dma_thresh_max_commq = 31;
		sc->dma_thresh_max_buf = 127;
		sc->qmi_max_tnums = 64;
		sc->qmi_def_tnums_thresh = 48;
		sc->bmi_max_tasks = 128;
		sc->max_open_dmas = 32;
		sc->dma_cam_num_entries = 32;
		sc->port_cgs = 256;
		sc->rx_ports = 5;
		sc->total_fifo_size = 100 * 1024;
		break;
	case 3:
		sc->bmi_max_fifo_size = 160 * 1024;
		sc->iram_size = 64 * 1024;
		sc->dma_thresh_max_commq = 31;
		sc->dma_thresh_max_buf = 127;
		sc->qmi_max_tnums = 64;
		sc->qmi_def_tnums_thresh = 48;
		sc->bmi_max_tasks = 128;
		sc->max_open_dmas = 32;
		sc->dma_cam_num_entries = 32;
		sc->port_cgs = 256;
		sc->rx_ports = 6;
		sc->total_fifo_size = 136 * 1024;
		break;
	case 6:
		sc->dma_thresh_max_commq = 31;
		sc->dma_thresh_max_buf = 127;
		sc->qmi_max_tnums = 64;
		sc->qmi_def_tnums_thresh = 48;
		sc->dma_cam_num_entries = 64;
		sc->port_cgs = 256;
		switch (sc->sc_revision_minor) {
		case 1:
		case 4:
			sc->bmi_max_fifo_size = 192 * 1024;
			sc->bmi_max_tasks = 64;
			sc->max_open_dmas = 32;
			sc->rx_ports = 5;
			sc->total_fifo_size = 156 * 1024;
			if (sc->sc_revision_minor == 1)
				sc->iram_size = 32 * 1024;
			else
				sc->iram_size = 64 * 1024;
			break;
		case 0:
		case 2:
		case 3:
			sc->bmi_max_fifo_size = 384 * 1024;
			sc->bmi_max_tasks = 128;
			sc->max_open_dmas = 84;
			sc->rx_ports = 8;
			sc->iram_size = 64 * 1024;
			sc->total_fifo_size = 295 * 1024;
			break;
		default:
			device_printf(sc->sc_base.dev,
			    "Unsupported FManv3 revision: %d\n",
			    sc->sc_revision_minor);
			break;
		}
		break;
	default:
		device_printf(sc->sc_base.dev,
		    "Unsupported FMan version: %d\n", sc->sc_revision_major);
		break;
	}
}

static int
fman_reset(struct fman_softc *sc)
{
	unsigned int count;

	if (sc->sc_revision_major < 6) {
		bus_write_4(sc->mem_res, FM_RSTC, FM_RSTC_FM_RESET);
		count = 100;
		do {
			DELAY(1);
		} while ((bus_read_4(sc->mem_res, FM_RSTC) & FM_RSTC_FM_RESET) &&
		    --count);
		if (count == 0)
			return (EBUSY);
		return (0);
	} else {
#ifdef __powerpc__
		phandle_t node;
		u_long base, size;
		uint32_t devdisr2;
#define	GUTS_DEVDISR2	0x0074
#define	DEVDISR2_FMAN1	0xfcc00000
#define	DEVDISR2_FMAN2	0x000fcc00

		node = ofw_bus_get_node(device_get_parent(sc->sc_base.dev));
		node = fdt_find_compatible(node, "fsl,qoriq-device-config-2.0",
		    false);

		if (node == 0) {
			device_printf(sc->sc_base.dev,
			    "missing device-config node in FDT.  Cannot reset FMAN");
			return (0);
		}
		fdt_regsize(node, &base, &size);

		devdisr2 = ccsr_read4(ccsrbar_va + base + GUTS_DEVDISR2);
		if (sc->fm_id == 0)
			ccsr_write4(ccsrbar_va + base + GUTS_DEVDISR2,
			    devdisr2 & ~DEVDISR2_FMAN1);
		else
			ccsr_write4(ccsrbar_va + base + GUTS_DEVDISR2,
			    devdisr2 & ~DEVDISR2_FMAN2);
#endif
		bus_write_4(sc->mem_res, FM_RSTC, FM_RSTC_FM_RESET);
		count = 100;
		do {
			DELAY(1);
		} while ((bus_read_4(sc->mem_res, FM_RSTC) & FM_RSTC_FM_RESET) &&
		    --count);
#ifdef __powerpc__
		ccsr_write4(ccsrbar_va + base + GUTS_DEVDISR2, devdisr2);
#endif
		if (count == 0)
			return (EBUSY);
		return (0);
	}
}

static int
fman_clear_iram(struct fman_softc *sc)
{
#ifdef notyet
	int i;

	/*
	 * TODO: Allow clearing the IRAM and loading new firmware.  Currently
	 * this is not supported, so assume that there's already firmware
	 * loaded, and don't clear it just yet.
	 */
	bus_write_4(sc->mem_res, IRAM_ADDR, IADD_AIE);
	for (i = 0; i < 100 && bus_read_4(sc->mem_res, IRAM_ADDR) != IADD_AIE; i++)
		DELAY(1);

	if (i == 100)
		return (EBUSY);

	for (i = 0; i < sc->iram_size / 4; i++)
		bus_write_4(sc->mem_res, IRAM_DATA, 0xffffffff);

	bus_write_4(sc->mem_res, IRAM_ADDR, sc->iram_size - 4);
	for (i = 0; i < 100 &&
	    bus_read_4(sc->mem_res, IRAM_DATA) != 0xffffffff; i++)
		DELAY(1);

	if (i == 100)
		return (EBUSY);
#endif

	return (0);
}

static int
fman_dma_init(struct fman_softc *sc)
{
	vmem_addr_t addr;
	uint32_t reg;
	int err;

	reg = bus_read_4(sc->mem_res, FMDM_SR);
	bus_write_4(sc->mem_res, FMDM_SR, reg | SR_BER);
	reg = bus_read_4(sc->mem_res, FMDM_MR) & ~MR_CEN_M;
	reg |= ((sc->dma_cam_num_entries / DMA_CAM_UNITS) - 1) << MR_CEN_S;
	bus_write_4(sc->mem_res, FMDM_MR, reg);

	err = vmem_xalloc(sc->muram_vmem,
	    sc->dma_cam_num_entries * DMA_CAM_SIZE, DMA_CAM_ALIGN, 0, 0,
	    VMEM_ADDR_MIN, VMEM_ADDR_MAX, M_BESTFIT | M_WAITOK, &addr);
	if (err != 0)
		device_printf(sc->sc_base.dev,
		    "failed to allocate DMA buffer\n");
	reg = addr;
	bus_write_4(sc->mem_res, FMDM_EBCR, reg);
	return (0);
}

static int
fman_bmi_init(struct fman_softc *sc)
{
	uint32_t reg;

	reg = sc->bmi_fifo_base / FMAN_BMI_FIFO_ALIGN;
	reg |= (sc->total_fifo_size / FMAN_BMI_FIFO_UNITS - 1) << FBPS_S;
	bus_write_4(sc->mem_res, FMBM_CFG1, reg);

	reg = ((sc->bmi_max_tasks - 1) << TNTSKS_S) & TNTSKS_M;
	//bus_write_4(sc->mem_res, FMBM_CFG2, reg);

	bus_write_4(sc->mem_res, FMBM_IEVR,
	    IEVR_SPEC | IEVR_LEC | IEVR_STEC | IEVR_DEC);

	bus_write_4(sc->mem_res, FMBM_IER,
	    IER_SPECE | IER_LECE | IER_STECE | IER_DECE);

	return (0);
}

static int
fman_qmi_init(struct fman_softc *sc)
{
	bus_write_4(sc->mem_res, FMQM_EIE, EIE_DEE | EIE_DFUPE);
	bus_write_4(sc->mem_res, FMQM_EIEN, EIEN_DEE | EIEN_DFUPE);
	return (0);
}

static void
fman_hwp_init(struct fman_softc *sc)
{
	/* Start up the parser */
	bus_write_4(sc->mem_res, FMPR_RPIMAC, HWP_RPIMAC_PEN);
}

static int
fman_enable(struct fman_softc *sc)
{
	bus_write_4(sc->mem_res, FMBM_INIT, INIT_STR);
	bus_write_4(sc->mem_res, FMQM_GC, 0xc0000000 |
	    GC_STEN | (sc->qmi_def_tnums_thresh << GC_ENQ_THR_S) |
	    sc->qmi_def_tnums_thresh);

	return (0);
}

/*
 * Enable timestamp counting.  Matching Freescale's reference code, generate the
 * timestamp incrementer to be roughly 256MHz, such that bit 23 would update
 * every microsecond.
 */
static int
fman_enable_timestamp(struct fman_softc *sc)
{
	uint64_t frac;
	uint32_t clock = fman_get_clock(sc) / 1000000;
	uint32_t intgr, tmp;
	uint32_t ts_freq = 1 << FM_TIMESTAMP_1US_BIT;

	intgr = ts_freq / clock;

	frac = ((uint64_t)ts_freq << 16) - ((uint64_t)intgr << 16) * clock;
	frac = (frac % clock ? 1 : 0) + (frac / clock);

	tmp = (intgr << TSC2_TSIV_INT_S) | (uint32_t)frac;

	bus_write_4(sc->mem_res, FMFP_TSC2, tmp);
	bus_write_4(sc->mem_res, FMFP_TSC1, TSC1_TEN);

	return (0);
}

static int
fman_keygen_init(struct fman_softc *sc)
{
	/* TODO: keygen */
	return (0);
}

static int
fman_fpm_init(struct fman_softc *sc)
{
	/* Clear all events, and enable interrupts. */
	bus_write_4(sc->mem_res, FMFP_EE,
	    EE_DECC | EE_STL | EE_SECC | EE_EHM |
	    EE_DECC_EN | EE_STL_EN | EE_SECC_EN);

	bus_write_4(sc->mem_res, FMFP_CEV0, 0xffffffff);
	bus_write_4(sc->mem_res, FMFP_CEV1, 0xffffffff);
	bus_write_4(sc->mem_res, FMFP_CEV2, 0xffffffff);
	bus_write_4(sc->mem_res, FMFP_CEV3, 0xffffffff);

	bus_write_4(sc->mem_res, FM_RCR, RCR_FEE | RCR_IEE);

	return (0);
}

static int
fman_init(struct fman_softc *sc)
{
	vmem_addr_t base_addr;
	sc->sc_revision_major = fman_get_revision_major(sc);
	sc->sc_revision_minor = fman_get_revision_minor(sc);

	if (bootverbose)
		device_printf(sc->sc_base.dev, "Hardware version: %d.%d.\n",
		    sc->sc_revision_major, sc->sc_revision_minor);

	fman_fill_soc_params(sc);
	bus_set_region_4(sc->mem_res, FMAN_CGP_OFFSET, 0, sc->port_cgs / 4);

	if (fman_reset(sc) != 0)
		goto err;

	if (fman_clear_iram(sc) != 0)
		goto err;

	if (fman_dma_init(sc) != 0)
		goto err;

	fman_fpm_init(sc);

	vmem_alloc(sc->muram_vmem, sc->total_fifo_size, M_BESTFIT | M_WAITOK,
	    &base_addr);
	sc->bmi_fifo_base = base_addr;

	fman_bmi_init(sc);
	fman_qmi_init(sc);
	fman_hwp_init(sc);
	if (fman_keygen_init(sc) != 0)
		goto err;

	if (fman_enable(sc) != 0)
		goto err;

	fman_enable_timestamp(sc);

	return (0);
err:
	return (ENXIO);
}

void
fman_get_revision(device_t dev, int *major, int *minor)
{
	struct fman_softc *sc = device_get_softc(dev);

	if (major)
		*major = sc->sc_revision_major;
	if (minor)
		*minor = sc->sc_revision_minor;
}

/** @} */

static int
fman_init_muram(struct fman_softc *sc)
{
	u_long base, size;
	phandle_t node;

	node = ofw_bus_get_node(sc->sc_base.dev);
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		char compat[255];

		if (OF_getprop(node, "compatible", compat, sizeof(compat)) < 0)
			continue;
		if (strcmp(compat, "fsl,fman-muram") == 0)
			break;
	}
	if (node == 0) {
		device_printf(sc->sc_base.dev, "no muram node\n");
		return (ENXIO);
	}
	if (fdt_regsize(node, &base, &size) != 0) {
		device_printf(sc->sc_base.dev, "failed to get muram reg\n");
		return (ENXIO);
	}
	sc->muram_vmem = vmem_create("MURAM",
	    base, size, 1, 0, M_WAITOK);

	return (0);
}

int
fman_attach(device_t dev)
{
	struct fman_softc *sc;
	pcell_t qchan_range[2];
	pcell_t cell;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->sc_base.dev = dev;

	cell = 0;
	node = ofw_bus_get_node(dev);
	OF_getencprop(node, "cell-index", &cell, sizeof(cell));
	sc->fm_id = cell;

	if (OF_getencprop(node, "fsl,qman-channel-range", qchan_range,
	    sizeof(qchan_range)) <= 0) {
		device_printf(dev, "Missing QMan channel range property!\n");
		return (ENXIO);
	}
	sc->qman_chan_base = qchan_range[0];
	sc->qman_chan_count = qchan_range[1];
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->mem_res) {
		device_printf(dev, "could not allocate memory.\n");
		return (ENXIO);
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (!sc->irq_res) {
		device_printf(dev, "could not allocate interrupt.\n");
		goto err;
	}

	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, fman_intr, sc, &sc->irq_cookie) != 0) {
		device_printf(dev, "error setting up interrupt handler.\n");
		goto err;
	}

	sc->err_irq_rid = 1;
	sc->err_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->err_irq_rid, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->err_irq_res) {
		device_printf(dev, "could not allocate error interrupt.\n");
		goto err;
	}

	/* Initialize the simplebus part of things */
	sc->rman.rm_type = RMAN_ARRAY;
	sc->rman.rm_descr = "FMan range";
	rman_init_from_resource(&sc->rman, sc->mem_res);
	simplebus_attach_impl(sc->sc_base.dev);

	if (fman_init_muram(sc) != 0)
		goto err;

	/* TODO: Interrupts... */

	if (fman_init(sc) != 0)
		goto err;

	bus_attach_children(dev);
	return (0);

err:
	fman_detach(dev);
	return (ENXIO);
}

int
fman_detach(device_t dev)
{
	struct fman_softc *sc;
	int rv;

	rv = simplebus_detach(dev);

	if (rv != 0)
		return (rv);

	sc = device_get_softc(dev);

	if (sc->mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->err_irq_rid,
		    sc->err_irq_res);
	}

	if (sc->muram_vmem != NULL)
		vmem_destroy(sc->muram_vmem);

	return (0);
}

int
fman_suspend(device_t dev)
{

	return (0);
}

int
fman_resume_dev(device_t dev)
{

	return (0);
}

int
fman_shutdown(device_t dev)
{

	return (0);
}

static void
fman_intr(void *arg)
{
	/* TODO: All FMAN interrupts */
}

int
fman_qman_channel_id(device_t dev, int port)
{
	struct fman_softc *sc;
	int i;

	sc = device_get_softc(dev);
	if (sc->sc_revision_major >= 6) {
		static const int qman_port_id[] = {
		    0x30, 0x31, 0x28, 0x29, 0x2a, 0x2b,
		    0x2c, 0x2d, 0x02, 0x03, 0x04, 0x05, 0x07, 0x07
		};
		for (i = 0; i < sc->qman_chan_count; i++) {
			if (qman_port_id[i] == port)
				return (sc->qman_chan_base + i);
		}
	} else {
		static const int qman_port_id[] = {
		    0x31, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x01,
		    0x02, 0x03, 0x04, 0x05, 0x07, 0x07
		};
		for (i = 0; i < sc->qman_chan_count; i++) {
			if (qman_port_id[i] == port)
				return (sc->qman_chan_base + i);
		}
	}

	return (0);
}

size_t
fman_get_bmi_max_fifo_size(device_t dev)
{
	struct fman_softc *sc = device_get_softc(dev);

	return (sc->bmi_max_fifo_size);
}

int
fman_reset_mac(device_t dev, int mac_id)
{
	struct fman_softc *sc = device_get_softc(dev);
	int timeout = 100;
	uint32_t mask;

	if (mac_id < 0 || mac_id > 9)
		return (EINVAL);

	/* MAC bits start at bit 1 for MAC0, and go down */
	mask = (1 << (30 - mac_id));
	bus_write_4(sc->mem_res, FM_RSTC, mask);
	while ((bus_read_4(sc->mem_res, FM_RSTC) & mask) && --timeout)
		DELAY(10);

	if (timeout == 0)
		return (EIO);

	return (0);
}

static int
fman_set_port_tasks(struct fman_softc *sc, int port_id,
    uint8_t tasks, uint8_t extra)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, FMBM_PP(port_id));

	reg &= ~(PP_MXT_M | PP_EXT_M);
	reg |= ((uint32_t)(tasks - 1) << PP_MXT_S) |
	    ((uint32_t)extra << PP_EXT_S);
	bus_write_4(sc->mem_res, FMBM_PP(port_id), reg);

	return (0);
}

static int
fman_set_port_fifo_size(struct fman_softc *sc, int port_id,
    uint32_t fifo_size, uint32_t extra)
{
	uint32_t reg;

	reg = (fifo_size / FMAN_BMI_FIFO_UNITS - 1) |
	    ((extra / FMAN_BMI_FIFO_UNITS) << PFS_EXBS_S);

	/* TODO: Make sure fifo size doesn't overrun */
	/* See Linux driver, fman set_size_of_fifo */

	bus_write_4(sc->mem_res, FMBM_PFS(port_id), reg);
	return (0);
}

static int
fman_set_port_dmas(struct fman_softc *sc, int port_id,
    int open_dmas, int extra_dmas)
{
	/* TODO: set port DMAs */
	return (0);
}

static void
fman_set_port_liodn(struct fman_softc *sc, int port_id, uint32_t liodn)
{
	uint32_t reg;

	reg = bus_read_4(sc->mem_res, FMDM_PLRn(port_id));
	reg &= ~PLRN_LIODN_M(port_id);
	reg |= liodn << PLRN_LIODN_S(port_id);
	bus_write_4(sc->mem_res, FMDM_PLRn(port_id), reg);
}

int
fman_set_port_params(device_t dev, struct fman_port_init_params *params)
{
	struct fman_softc *sc = device_get_softc(dev);
	int error;

	error = fman_set_port_tasks(sc, params->port_id,
	    params->num_tasks, params->extra_tasks);

	if (error != 0)
		return (error);

	if (!params->is_rx_port) {
	}
	error = fman_set_port_fifo_size(sc, params->port_id, params->fifo_size,
	    params->extra_fifo_size);

	if (error != 0)
		return (error);

	error = fman_set_port_dmas(sc, params->port_id,
	    params->open_dmas, params->extra_dmas);

	if (error != 0)
		return (error);

	fman_set_port_liodn(sc, params->port_id, params->liodn);

	return (0);
}

/** @} */
