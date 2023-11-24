/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
 * Copyright (c) 2012 Stefan Sperling <stsp@openbsd.org>
 * Copyright (c) 2020 Henri Hennebert <hlh@restart.be>
 * Copyright (c) 2020 Gary Jennejohn <gj@freebsd.org>
 * Copyright (c) 2020 Jesper Schmitz Mouridsen <jsm@FreeBSD.org>
 * All rights reserved.
 *
 * Patch from:
 * - Lutz Bichler <Lutz.Bichler@gmail.com>
 *
 * Base on OpenBSD /sys/dev/pci/rtsx_pci.c & /dev/ic/rtsx.c
 *      on Linux   /drivers/mmc/host/rtsx_pci_sdmmc.c,
 *                 /include/linux/rtsx_pci.h &
 *                 /drivers/misc/cardreader/rtsx_pcr.c
 *      on NetBSD  /sys/dev/ic/rtsx.c
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
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

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h> /* For FreeBSD 11 */
#include <sys/types.h> /* For FreeBSD 11 */
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <machine/bus.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/sysctl.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>
#include <machine/_inttypes.h>

#include "opt_mmccam.h"

#ifdef MMCCAM
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/mmc/mmc_sim.h>
#include "mmc_sim_if.h"
#endif /* MMCCAM */

#include "rtsxreg.h"

/* The softc holds our per-instance data. */
struct rtsx_softc {
	struct mtx	rtsx_mtx;		/* device mutex */
	device_t	rtsx_dev;		/* device */
	uint16_t	rtsx_flags;		/* device flags */
	uint16_t	rtsx_device_id;		/* device ID */
	device_t	rtsx_mmc_dev;		/* device of mmc bus */
	uint32_t	rtsx_intr_enabled;	/* enabled interrupts */
	uint32_t 	rtsx_intr_status;	/* soft interrupt status */
	int		rtsx_irq_res_id;	/* bus IRQ resource id */
	struct resource *rtsx_irq_res;		/* bus IRQ resource */
	void		*rtsx_irq_cookie;	/* bus IRQ resource cookie */
	struct callout	rtsx_timeout_callout;	/* callout for timeout */
	int		rtsx_timeout_cmd;	/* interrupt timeout for setup commands */
	int		rtsx_timeout_io;	/* interrupt timeout for I/O commands */
	void		(*rtsx_intr_trans_ok)(struct rtsx_softc *sc);
						/* function to call if transfer succeed */
	void		(*rtsx_intr_trans_ko)(struct rtsx_softc *sc);
						/* function to call if transfer fail */

	struct timeout_task
			rtsx_card_insert_task;	/* card insert delayed task */
	struct task	rtsx_card_remove_task;	/* card remove task */

	int		rtsx_mem_res_id;	/* bus memory resource id */
	struct resource *rtsx_mem_res;		/* bus memory resource */
	bus_space_tag_t	   rtsx_mem_btag;	/* host register set tag */
	bus_space_handle_t rtsx_mem_bhandle;	/* host register set handle */

	bus_dma_tag_t	rtsx_cmd_dma_tag;	/* DMA tag for command transfer */
	bus_dmamap_t	rtsx_cmd_dmamap;	/* DMA map for command transfer */
	void		*rtsx_cmd_dmamem;	/* DMA mem for command transfer */
	bus_addr_t	rtsx_cmd_buffer;	/* device visible address of the DMA segment */
	int		rtsx_cmd_index;		/* index in rtsx_cmd_buffer */

	bus_dma_tag_t	rtsx_data_dma_tag;	/* DMA tag for data transfer */
	bus_dmamap_t	rtsx_data_dmamap;	/* DMA map for data transfer */
	void		*rtsx_data_dmamem;	/* DMA mem for data transfer */
	bus_addr_t	rtsx_data_buffer;	/* device visible address of the DMA segment */

#ifdef MMCCAM
	union ccb		*rtsx_ccb;	/* CAM control block */
	struct mmc_sim		rtsx_mmc_sim;	/* CAM generic sim */
	struct mmc_request	rtsx_cam_req;	/* CAM MMC request */
#endif /* MMCCAM */

	struct mmc_request *rtsx_req;		/* MMC request */
	struct mmc_host rtsx_host;		/* host parameters */
	int		rtsx_pcie_cap;		/* PCIe capability offset */
	int8_t		rtsx_bus_busy;		/* bus busy status */
	int8_t		rtsx_ios_bus_width;	/* current host.ios.bus_width */
	int32_t		rtsx_ios_clock;		/* current host.ios.clock */
	int8_t		rtsx_ios_power_mode;	/* current host.ios.power mode */
	int8_t		rtsx_ios_timing;	/* current host.ios.timing */
	int8_t		rtsx_ios_vccq;		/* current host.ios.vccq */
	uint8_t		rtsx_read_only;		/* card read only status */
	uint8_t		rtsx_inversion;		/* inversion of card detection and read only status */
	uint8_t		rtsx_force_timing;	/* force bus_timing_uhs_sdr50 */
	uint8_t		rtsx_debug_mask;	/* debugging mask */
#define 	RTSX_DEBUG_BASIC	0x01	/* debug basic flow */
#define 	RTSX_TRACE_SD_CMD	0x02	/* trace SD commands */
#define 	RTSX_DEBUG_TUNING	0x04	/* debug tuning */
#ifdef MMCCAM
	uint8_t		rtsx_cam_status;	/* CAM status - 1 if card in use */
#endif /* MMCCAM */
	uint64_t	rtsx_read_count;	/* count of read operations */
	uint64_t	rtsx_write_count;	/* count of write operations */
	bool		rtsx_discovery_mode;	/* are we in discovery mode? */
	bool		rtsx_tuning_mode;	/* are we tuning */
	bool		rtsx_double_clk;	/* double clock freqency */
	bool		rtsx_vpclk;		/* voltage at Pulse-width Modulation(PWM) clock? */
	uint8_t		rtsx_ssc_depth;		/* Spread spectrum clocking depth */
	uint8_t		rtsx_card_drive_sel;	/* value for RTSX_CARD_DRIVE_SEL */
	uint8_t		rtsx_sd30_drive_sel_3v3;/* value for RTSX_SD30_DRIVE_SEL */
};

/* rtsx_flags values */
#define	RTSX_F_DEFAULT		0x0000
#define	RTSX_F_CARD_PRESENT	0x0001
#define	RTSX_F_SDIO_SUPPORT	0x0002
#define	RTSX_F_VERSION_A	0x0004
#define	RTSX_F_VERSION_B	0x0008
#define	RTSX_F_VERSION_C	0x0010
#define	RTSX_F_VERSION_D	0x0020
#define	RTSX_F_8411B_QFN48	0x0040
#define	RTSX_F_REVERSE_SOCKET	0x0080

#define	RTSX_REALTEK		0x10ec
#define	RTSX_RTS5209		0x5209
#define	RTSX_RTS5227		0x5227
#define	RTSX_RTS5229		0x5229
#define	RTSX_RTS522A		0x522a
#define	RTSX_RTS525A		0x525a
#define	RTSX_RTS5249		0x5249
#define	RTSX_RTS5260		0x5260
#define	RTSX_RTL8402		0x5286
#define	RTSX_RTL8411		0x5289
#define	RTSX_RTL8411B		0x5287

#define	RTSX_VERSION		"2.1g"

static const struct rtsx_pciids {
	uint16_t	device_id;
	const char	*desc;
} rtsx_ids[] = {
	{ RTSX_RTS5209, RTSX_VERSION " Realtek RTS5209 PCIe SD Card Reader" },
	{ RTSX_RTS5227, RTSX_VERSION " Realtek RTS5227 PCIe SD Card Reader" },
	{ RTSX_RTS5229, RTSX_VERSION " Realtek RTS5229 PCIe SD Card Reader" },
	{ RTSX_RTS522A, RTSX_VERSION " Realtek RTS522A PCIe SD Card Reader" },
	{ RTSX_RTS525A, RTSX_VERSION " Realtek RTS525A PCIe SD Card Reader" },
	{ RTSX_RTS5249, RTSX_VERSION " Realtek RTS5249 PCIe SD Card Reader" },
	{ RTSX_RTS5260, RTSX_VERSION " Realtek RTS5260 PCIe SD Card Reader" },
	{ RTSX_RTL8402, RTSX_VERSION " Realtek RTL8402 PCIe SD Card Reader" },
	{ RTSX_RTL8411, RTSX_VERSION " Realtek RTL8411 PCIe SD Card Reader" },
	{ RTSX_RTL8411B, RTSX_VERSION " Realtek RTL8411B PCIe SD Card Reader" },
};

/* See `kenv | grep smbios.system` */
static const struct rtsx_inversion_model {
	char	*maker;
	char	*family;
	char	*product;
} rtsx_inversion_models[] = {
	{ "LENOVO",		"ThinkPad T470p",	"20J7S0PM00"},
	{ "LENOVO",		"ThinkPad X13 Gen 1",	"20UF000QRT"},
	{ NULL,			NULL,			NULL}
};

static int	rtsx_dma_alloc(struct rtsx_softc *sc);
static void	rtsx_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error);
static void	rtsx_dma_free(struct rtsx_softc *sc);
static void	rtsx_intr(void *arg);
static void	rtsx_handle_card_present(struct rtsx_softc *sc);
static void	rtsx_card_task(void *arg, int pending __unused);
static bool	rtsx_is_card_present(struct rtsx_softc *sc);
static int	rtsx_init(struct rtsx_softc *sc);
static int	rtsx_map_sd_drive(int index);
static int	rtsx_rts5227_fill_driving(struct rtsx_softc *sc);
static int	rtsx_rts5249_fill_driving(struct rtsx_softc *sc);
static int	rtsx_rts5260_fill_driving(struct rtsx_softc *sc);
static int	rtsx_read(struct rtsx_softc *, uint16_t, uint8_t *);
static int	rtsx_read_cfg(struct rtsx_softc *sc, uint8_t func, uint16_t addr, uint32_t *val);
static int	rtsx_write(struct rtsx_softc *sc, uint16_t addr, uint8_t mask, uint8_t val);
static int	rtsx_read_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t *val);
static int	rtsx_write_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t val);
static int	rtsx_bus_power_off(struct rtsx_softc *sc);
static int	rtsx_bus_power_on(struct rtsx_softc *sc);
static int	rtsx_set_bus_width(struct rtsx_softc *sc, enum mmc_bus_width width);
static int	rtsx_set_sd_timing(struct rtsx_softc *sc, enum mmc_bus_timing timing);
static int	rtsx_set_sd_clock(struct rtsx_softc *sc, uint32_t freq);
static int	rtsx_stop_sd_clock(struct rtsx_softc *sc);
static int	rtsx_switch_sd_clock(struct rtsx_softc *sc, uint8_t clk, uint8_t n, uint8_t div, uint8_t mcu);
#ifndef MMCCAM
static void	rtsx_sd_change_tx_phase(struct rtsx_softc *sc, uint8_t sample_point);
static void	rtsx_sd_change_rx_phase(struct rtsx_softc *sc, uint8_t sample_point);
static void	rtsx_sd_tuning_rx_phase(struct rtsx_softc *sc, uint32_t *phase_map);
static int	rtsx_sd_tuning_rx_cmd(struct rtsx_softc *sc, uint8_t sample_point);
static int	rtsx_sd_tuning_rx_cmd_wait(struct rtsx_softc *sc, struct mmc_command *cmd);
static void	rtsx_sd_tuning_rx_cmd_wakeup(struct rtsx_softc *sc);
static void	rtsx_sd_wait_data_idle(struct rtsx_softc *sc);
static uint8_t	rtsx_sd_search_final_rx_phase(struct rtsx_softc *sc, uint32_t phase_map);
static int	rtsx_sd_get_rx_phase_len(uint32_t phase_map, int start_bit);
#endif /* !MMCCAM */
#if 0	/* For led */
static int	rtsx_led_enable(struct rtsx_softc *sc);
static int	rtsx_led_disable(struct rtsx_softc *sc);
#endif	/* For led */
static uint8_t	rtsx_response_type(uint16_t mmc_rsp);
static void	rtsx_init_cmd(struct rtsx_softc *sc, struct mmc_command *cmd);
static void	rtsx_push_cmd(struct rtsx_softc *sc, uint8_t cmd, uint16_t reg,
			      uint8_t mask, uint8_t data);
static void	rtsx_set_cmd_data_len(struct rtsx_softc *sc, uint16_t block_cnt, uint16_t byte_cnt);
static void	rtsx_send_cmd(struct rtsx_softc *sc);
static void	rtsx_ret_resp(struct rtsx_softc *sc);
static void	rtsx_set_resp(struct rtsx_softc *sc, struct mmc_command *cmd);
static void	rtsx_stop_cmd(struct rtsx_softc *sc);
static void	rtsx_clear_error(struct rtsx_softc *sc);
static void	rtsx_req_done(struct rtsx_softc *sc);
static int	rtsx_send_req(struct rtsx_softc *sc, struct mmc_command *cmd);
static int	rtsx_xfer_short(struct rtsx_softc *sc, struct mmc_command *cmd);
static void	rtsx_ask_ppbuf_part1(struct rtsx_softc *sc);
static void	rtsx_get_ppbuf_part1(struct rtsx_softc *sc);
static void	rtsx_get_ppbuf_part2(struct rtsx_softc *sc);
static void	rtsx_put_ppbuf_part1(struct rtsx_softc *sc);
static void	rtsx_put_ppbuf_part2(struct rtsx_softc *sc);
static void	rtsx_write_ppbuf(struct rtsx_softc *sc);
static int	rtsx_xfer(struct rtsx_softc *sc, struct mmc_command *cmd);
static void	rtsx_xfer_begin(struct rtsx_softc *sc);
static void	rtsx_xfer_start(struct rtsx_softc *sc);
static void	rtsx_xfer_finish(struct rtsx_softc *sc);
static void	rtsx_timeout(void *arg);

#ifdef MMCCAM
static int	rtsx_get_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts);
static int	rtsx_set_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts);
static int	rtsx_cam_request(device_t dev, union ccb *ccb);
#endif /* MMCCAM */

static int	rtsx_read_ivar(device_t bus, device_t child, int which, uintptr_t *result);
static int	rtsx_write_ivar(device_t bus, device_t child, int which, uintptr_t value);

static int	rtsx_mmcbr_update_ios(device_t bus, device_t child __unused);
static int	rtsx_mmcbr_switch_vccq(device_t bus, device_t child __unused);
static int	rtsx_mmcbr_request(device_t bus, device_t child __unused, struct mmc_request *req);
#ifndef MMCCAM
static int	rtsx_mmcbr_tune(device_t bus, device_t child __unused, bool hs400 __unused);
static int	rtsx_mmcbr_retune(device_t bus, device_t child __unused, bool reset __unused);
static int	rtsx_mmcbr_get_ro(device_t bus, device_t child __unused);
static int	rtsx_mmcbr_acquire_host(device_t bus, device_t child __unused);
static int	rtsx_mmcbr_release_host(device_t bus, device_t child __unused);
#endif /* !MMCCAM */

static int	rtsx_probe(device_t dev);
static int	rtsx_attach(device_t dev);
static int	rtsx_detach(device_t dev);
static int	rtsx_shutdown(device_t dev);
static int	rtsx_suspend(device_t dev);
static int	rtsx_resume(device_t dev);

#define	RTSX_LOCK_INIT(_sc)	mtx_init(&(_sc)->rtsx_mtx,	\
					 device_get_nameunit(sc->rtsx_dev), "rtsx", MTX_DEF)
#define	RTSX_LOCK(_sc)		mtx_lock(&(_sc)->rtsx_mtx)
#define	RTSX_UNLOCK(_sc)	mtx_unlock(&(_sc)->rtsx_mtx)
#define	RTSX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->rtsx_mtx)

#define	RTSX_SDCLK_OFF			0
#define	RTSX_SDCLK_250KHZ	   250000
#define	RTSX_SDCLK_400KHZ	   400000
#define	RTSX_SDCLK_25MHZ	 25000000
#define	RTSX_SDCLK_50MHZ	 50000000
#define	RTSX_SDCLK_100MHZ	100000000
#define	RTSX_SDCLK_208MHZ	208000000

#define	RTSX_MIN_DIV_N		80
#define	RTSX_MAX_DIV_N		208

#define	RTSX_MAX_DATA_BLKLEN	512

#define	RTSX_DMA_ALIGN		4
#define	RTSX_HOSTCMD_MAX	256
#define	RTSX_DMA_CMD_BIFSIZE	(sizeof(uint32_t) * RTSX_HOSTCMD_MAX)
#define	RTSX_DMA_DATA_BUFSIZE	MAXPHYS

#define	ISSET(t, f) ((t) & (f))

#define	READ4(sc, reg)						\
	(bus_space_read_4((sc)->rtsx_mem_btag, (sc)->rtsx_mem_bhandle, (reg)))
#define	WRITE4(sc, reg, val)					\
	(bus_space_write_4((sc)->rtsx_mem_btag, (sc)->rtsx_mem_bhandle, (reg), (val)))

#define	RTSX_READ(sc, reg, val) 				\
	do { 							\
		int err = rtsx_read((sc), (reg), (val)); 	\
		if (err) 					\
			return (err);				\
	} while (0)

#define	RTSX_WRITE(sc, reg, val) 				\
	do { 							\
		int err = rtsx_write((sc), (reg), 0xff, (val));	\
		if (err) 					\
			return (err);				\
	} while (0)
#define	RTSX_CLR(sc, reg, bits)					\
	do { 							\
		int err = rtsx_write((sc), (reg), (bits), 0); 	\
		if (err) 					\
			return (err);				\
	} while (0)

#define	RTSX_SET(sc, reg, bits)					\
	do { 							\
		int err = rtsx_write((sc), (reg), (bits), 0xff);\
		if (err) 					\
			return (err);				\
	} while (0)

#define	RTSX_BITOP(sc, reg, mask, bits)				\
	do {							\
		int err = rtsx_write((sc), (reg), (mask), (bits));	\
		if (err)					\
			return (err);				\
	} while (0)

/*
 * We use two DMA buffers: a command buffer and a data buffer.
 *
 * The command buffer contains a command queue for the host controller,
 * which describes SD/MMC commands to run, and other parameters. The chip
 * runs the command queue when a special bit in the RTSX_HCBAR register is
 * set and signals completion with the RTSX_TRANS_OK_INT interrupt.
 * Each command is encoded as a 4 byte sequence containing command number
 * (read, write, or check a host controller register), a register address,
 * and a data bit-mask and value.
 * SD/MMC commands which do not transfer any data from/to the card only use
 * the command buffer.
 *
 * The data buffer is used for transfer longer than 512. Data transfer is
 * controlled via the RTSX_HDBAR register and completion is signalled by
 * the RTSX_TRANS_OK_INT interrupt.
 *
 * The chip is unable to perform DMA above 4GB.
 */

/*
 * Main commands in the usual seqence used:
 *
 * CMD0		Go idle state
 * CMD8		Send interface condition
 * CMD55	Application Command for next ACMD
 * ACMD41	Send Operation Conditions Register (OCR: voltage profile of the card)
 * CMD2		Send Card Identification (CID) Register
 * CMD3		Send relative address
 * CMD9		Send Card Specific Data (CSD)
 * CMD13	Send status (32 bits -  bit 25: card password protected)
 * CMD7		Select card (before Get card SCR)
 * ACMD51	Send SCR (SD CARD Configuration Register - [51:48]: Bus widths supported)
 * CMD6		SD switch function
 * ACMD13	Send SD status (512 bits)
 * ACMD42	Set/Clear card detect
 * ACMD6	Set bus width
 * CMD19	Send tuning block
 * CMD12	Stop transmission
 *
 * CMD17	Read single block (<=512)
 * CMD18	Read multiple blocks (>512)
 * CMD24	Write single block (<=512)
 * CMD25	Write multiple blocks (>512)
 *
 * CMD52	IO R/W direct
 * CMD5		Send Operation Conditions
 */

static int
rtsx_dma_alloc(struct rtsx_softc *sc)
{
	int	error = 0;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->rtsx_dev), /* inherit from parent */
	    RTSX_DMA_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RTSX_DMA_CMD_BIFSIZE, 1,	/* maxsize, nsegments */
	    RTSX_DMA_CMD_BIFSIZE,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rtsx_cmd_dma_tag);
	if (error) {
		device_printf(sc->rtsx_dev,
			      "Can't create cmd parent DMA tag\n");
		return (error);
	}
	error = bus_dmamem_alloc(sc->rtsx_cmd_dma_tag,		/* DMA tag */
	    &sc->rtsx_cmd_dmamem,				/* will hold the KVA pointer */
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,	/* flags */
	    &sc->rtsx_cmd_dmamap); 				/* DMA map */
	if (error) {
		device_printf(sc->rtsx_dev,
			      "Can't create DMA map for command transfer\n");
		goto destroy_cmd_dma_tag;

	}
	error = bus_dmamap_load(sc->rtsx_cmd_dma_tag,	/* DMA tag */
	    sc->rtsx_cmd_dmamap,	/* DMA map */
	    sc->rtsx_cmd_dmamem,	/* KVA pointer to be mapped */
	    RTSX_DMA_CMD_BIFSIZE,	/* size of buffer */
	    rtsx_dmamap_cb,		/* callback */
	    &sc->rtsx_cmd_buffer,	/* first arg of callback */
	    0);				/* flags */
	if (error || sc->rtsx_cmd_buffer == 0) {
		device_printf(sc->rtsx_dev,
			      "Can't load DMA memory for command transfer\n");
		error = (error) ? error : EFAULT;
		goto destroy_cmd_dmamem_alloc;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->rtsx_dev),	/* inherit from parent */
	    RTSX_DMA_DATA_BUFSIZE, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RTSX_DMA_DATA_BUFSIZE, 1,	/* maxsize, nsegments */
	    RTSX_DMA_DATA_BUFSIZE,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rtsx_data_dma_tag);
	if (error) {
		device_printf(sc->rtsx_dev,
			      "Can't create data parent DMA tag\n");
		goto destroy_cmd_dmamap_load;
	}
	error = bus_dmamem_alloc(sc->rtsx_data_dma_tag,		/* DMA tag */
	    &sc->rtsx_data_dmamem,				/* will hold the KVA pointer */
	    BUS_DMA_WAITOK | BUS_DMA_ZERO,			/* flags */
	    &sc->rtsx_data_dmamap); 				/* DMA map */
	if (error) {
		device_printf(sc->rtsx_dev,
			      "Can't create DMA map for data transfer\n");
		goto destroy_data_dma_tag;
	}
	error = bus_dmamap_load(sc->rtsx_data_dma_tag,	/* DMA tag */
	    sc->rtsx_data_dmamap,	/* DMA map */
	    sc->rtsx_data_dmamem,	/* KVA pointer to be mapped */
	    RTSX_DMA_DATA_BUFSIZE,	/* size of buffer */
	    rtsx_dmamap_cb,		/* callback */
	    &sc->rtsx_data_buffer,	/* first arg of callback */
	    0);				/* flags */
	if (error || sc->rtsx_data_buffer == 0) {
		device_printf(sc->rtsx_dev,
			      "Can't load DMA memory for data transfer\n");
		error = (error) ? error : EFAULT;
		goto destroy_data_dmamem_alloc;
	}
	return (error);

 destroy_data_dmamem_alloc:
	bus_dmamem_free(sc->rtsx_data_dma_tag, sc->rtsx_data_dmamem, sc->rtsx_data_dmamap);
 destroy_data_dma_tag:
	bus_dma_tag_destroy(sc->rtsx_data_dma_tag);
 destroy_cmd_dmamap_load:
	bus_dmamap_unload(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap);
 destroy_cmd_dmamem_alloc:
	bus_dmamem_free(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamem, sc->rtsx_cmd_dmamap);
 destroy_cmd_dma_tag:
	bus_dma_tag_destroy(sc->rtsx_cmd_dma_tag);

	return (error);
}

static void
rtsx_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error) {
		printf("rtsx_dmamap_cb: error %d\n", error);
		return;
	}
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
rtsx_dma_free(struct rtsx_softc *sc)
{
	if (sc->rtsx_cmd_dma_tag != NULL) {
		if (sc->rtsx_cmd_dmamap != NULL)
			bus_dmamap_unload(sc->rtsx_cmd_dma_tag,
					  sc->rtsx_cmd_dmamap);
		if (sc->rtsx_cmd_dmamem != NULL)
			bus_dmamem_free(sc->rtsx_cmd_dma_tag,
					sc->rtsx_cmd_dmamem,
					sc->rtsx_cmd_dmamap);
		sc->rtsx_cmd_dmamap = NULL;
		sc->rtsx_cmd_dmamem = NULL;
		sc->rtsx_cmd_buffer = 0;
		bus_dma_tag_destroy(sc->rtsx_cmd_dma_tag);
		sc->rtsx_cmd_dma_tag = NULL;
	}
	if (sc->rtsx_data_dma_tag != NULL) {
		if (sc->rtsx_data_dmamap != NULL)
			bus_dmamap_unload(sc->rtsx_data_dma_tag,
					  sc->rtsx_data_dmamap);
		if (sc->rtsx_data_dmamem != NULL)
			bus_dmamem_free(sc->rtsx_data_dma_tag,
					sc->rtsx_data_dmamem,
					sc->rtsx_data_dmamap);
		sc->rtsx_data_dmamap = NULL;
		sc->rtsx_data_dmamem = NULL;
		sc->rtsx_data_buffer = 0;
		bus_dma_tag_destroy(sc->rtsx_data_dma_tag);
		sc->rtsx_data_dma_tag = NULL;
	}
}

static void
rtsx_intr(void *arg)
{
	struct rtsx_softc *sc = arg;
	uint32_t	enabled;
	uint32_t	status;

	RTSX_LOCK(sc);

	enabled = sc->rtsx_intr_enabled;
	status = READ4(sc, RTSX_BIPR);	/* read Bus Interrupt Pending Register */
	sc->rtsx_intr_status = status;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "Interrupt handler - enabled: 0x%08x, status: 0x%08x\n", enabled, status);

	/* Ack interrupts. */
	WRITE4(sc, RTSX_BIPR, status);

	if (((enabled & status) == 0) || status == 0xffffffff) {
		device_printf(sc->rtsx_dev, "Spurious interrupt - enabled: 0x%08x, status: 0x%08x\n", enabled, status);
		RTSX_UNLOCK(sc);
		return;
	}

	/* Detect write protect. */
	if (status & RTSX_SD_WRITE_PROTECT)
		sc->rtsx_read_only = 1;
	else
		sc->rtsx_read_only = 0;

	/* Start task to handle SD card status change (from dwmmc.c). */
	if (status & RTSX_SD_INT) {
		device_printf(sc->rtsx_dev, "Interrupt card inserted/removed\n");
		rtsx_handle_card_present(sc);
	}

	if (sc->rtsx_req == NULL) {
		RTSX_UNLOCK(sc);
		return;
	}

	if (status & RTSX_TRANS_OK_INT) {
		sc->rtsx_req->cmd->error = MMC_ERR_NONE;
		if (sc->rtsx_intr_trans_ok != NULL)
			sc->rtsx_intr_trans_ok(sc);
	} else if (status & RTSX_TRANS_FAIL_INT) {
		uint8_t stat1;
		sc->rtsx_req->cmd->error = MMC_ERR_FAILED;
		if (rtsx_read(sc, RTSX_SD_STAT1, &stat1) == 0 &&
		    (stat1 & RTSX_SD_CRC_ERR)) {
			device_printf(sc->rtsx_dev, "CRC error\n");
			sc->rtsx_req->cmd->error = MMC_ERR_BADCRC;
		}
		if (!sc->rtsx_tuning_mode)
			device_printf(sc->rtsx_dev, "Transfer fail - status: 0x%08x\n", status);
		rtsx_stop_cmd(sc);
		if (sc->rtsx_intr_trans_ko != NULL)
			sc->rtsx_intr_trans_ko(sc);
	}

	RTSX_UNLOCK(sc);
}

/*
 * Function called from the IRQ handler (from dwmmc.c).
 */
static void
rtsx_handle_card_present(struct rtsx_softc *sc)
{
	bool	was_present;
	bool	is_present;

#ifdef MMCCAM
	was_present = sc->rtsx_cam_status;
#else  /* !MMCCAM */
	was_present = sc->rtsx_mmc_dev != NULL;
#endif /* MMCCAM */
	is_present = rtsx_is_card_present(sc);
	if (is_present)
		device_printf(sc->rtsx_dev, "Card present\n");
	else
		device_printf(sc->rtsx_dev, "Card absent\n");

	if (!was_present && is_present) {
		/*
		 * The delay is to debounce the card insert
		 * (sometimes the card detect pin stabilizes
		 * before the other pins have made good contact).
		 */
		taskqueue_enqueue_timeout(taskqueue_swi_giant,
					  &sc->rtsx_card_insert_task, -hz);
	} else if (was_present && !is_present) {
		taskqueue_enqueue(taskqueue_swi_giant, &sc->rtsx_card_remove_task);
	}
}

/*
 * This function is called at startup.
 */
static void
rtsx_card_task(void *arg, int pending __unused)
{
	struct rtsx_softc *sc = arg;

	if (rtsx_is_card_present(sc)) {
		sc->rtsx_flags |= RTSX_F_CARD_PRESENT;
		/* Card is present, attach if necessary. */
#ifdef MMCCAM
		if (sc->rtsx_cam_status == 0) {
#else  /* !MMCCAM */
		if (sc->rtsx_mmc_dev == NULL) {
#endif /* MMCCAM */
			if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
				device_printf(sc->rtsx_dev, "Card inserted\n");

			sc->rtsx_read_count = sc->rtsx_write_count = 0;
#ifdef MMCCAM
			sc->rtsx_cam_status = 1;
			mmc_cam_sim_discover(&sc->rtsx_mmc_sim);
#else  /* !MMCCAM */
			RTSX_LOCK(sc);
			sc->rtsx_mmc_dev = device_add_child(sc->rtsx_dev, "mmc", -1);
			RTSX_UNLOCK(sc);
			if (sc->rtsx_mmc_dev == NULL) {
				device_printf(sc->rtsx_dev, "Adding MMC bus failed\n");
			} else {
				device_set_ivars(sc->rtsx_mmc_dev, sc);
				device_probe_and_attach(sc->rtsx_mmc_dev);
			}
#endif /* MMCCAM */
		}
	} else {
		sc->rtsx_flags &= ~RTSX_F_CARD_PRESENT;
		/* Card isn't present, detach if necessary. */
#ifdef MMCCAM
		if (sc->rtsx_cam_status != 0) {
#else  /* !MMCCAM */
		if (sc->rtsx_mmc_dev != NULL) {
#endif /* MMCCAM */
			if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
				device_printf(sc->rtsx_dev, "Card removed\n");

			if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
				device_printf(sc->rtsx_dev, "Read count: %" PRIu64 ", write count: %" PRIu64 "\n",
					      sc->rtsx_read_count, sc->rtsx_write_count);
#ifdef MMCCAM
			sc->rtsx_cam_status = 0;
			mmc_cam_sim_discover(&sc->rtsx_mmc_sim);
#else  /* !MMCCAM */
			if (device_delete_child(sc->rtsx_dev, sc->rtsx_mmc_dev))
				device_printf(sc->rtsx_dev, "Detaching MMC bus failed\n");
			sc->rtsx_mmc_dev = NULL;
#endif /* MMCCAM */
		}
	}
}

static bool
rtsx_is_card_present(struct rtsx_softc *sc)
{
	uint32_t status;

	status = READ4(sc, RTSX_BIPR);
	if (sc->rtsx_inversion == 0)
		return (status & RTSX_SD_EXIST);
	else
		return !(status & RTSX_SD_EXIST);
}

static int
rtsx_init(struct rtsx_softc *sc)
{
	uint8_t	version;
	uint8_t	val;
	int	error;

	sc->rtsx_host.host_ocr = RTSX_SUPPORTED_VOLTAGE;
	sc->rtsx_host.f_min = RTSX_SDCLK_250KHZ;
	sc->rtsx_host.f_max = RTSX_SDCLK_208MHZ;
	sc->rtsx_host.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_HSPEED |
		MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25;

	sc->rtsx_host.caps |= MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104;
	if (sc->rtsx_device_id == RTSX_RTS5209)
		sc->rtsx_host.caps |= MMC_CAP_8_BIT_DATA;
	pci_find_cap(sc->rtsx_dev, PCIY_EXPRESS, &(sc->rtsx_pcie_cap));

	/*
	 * Check IC version.
	 */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5229:
		/* Read IC version from dummy register. */
		RTSX_READ(sc, RTSX_DUMMY_REG, &version);
		if ((version & 0x0F) == RTSX_IC_VERSION_C)
			sc->rtsx_flags |= RTSX_F_VERSION_C;
		break;
	case RTSX_RTS522A:
		/* Read IC version from dummy register. */
		RTSX_READ(sc, RTSX_DUMMY_REG, &version);
		if ((version & 0x0F) == RTSX_IC_VERSION_A)
			sc->rtsx_flags |= RTSX_F_VERSION_A;
		break;
	case RTSX_RTS525A:
		/* Read IC version from dummy register. */
		RTSX_READ(sc, RTSX_DUMMY_REG, &version);
		if ((version & 0x0F) == RTSX_IC_VERSION_A)
			sc->rtsx_flags |= RTSX_F_VERSION_A;
		break;
	case RTSX_RTL8411B:
		RTSX_READ(sc, RTSX_RTL8411B_PACKAGE, &version);
		if (version & RTSX_RTL8411B_QFN48)
			sc->rtsx_flags |= RTSX_F_8411B_QFN48;
		break;
	}

	/*
	 * Fetch vendor settings.
	 */
	/*
	 * Normally OEMs will set vendor setting to the config space
	 * of Realtek card reader in BIOS stage. This statement reads
	 * the setting and configure the internal registers according
	 * to it, to improve card reader's compatibility condition.
	 */
	sc->rtsx_card_drive_sel = RTSX_CARD_DRIVE_DEFAULT;
	switch (sc->rtsx_device_id) {
		uint32_t reg;
		uint32_t reg1;
		uint8_t  reg3;
	case RTSX_RTS5209:
		sc->rtsx_card_drive_sel = RTSX_RTS5209_CARD_DRIVE_DEFAULT;
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_DRIVER_TYPE_D;
		reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG2, 4);
		if (!(reg & 0x80)) {
			sc->rtsx_card_drive_sel = (reg >> 8) & 0x3F;
			sc->rtsx_sd30_drive_sel_3v3 = reg & 0x07;
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg: 0x%08x\n", reg);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "card_drive_sel: 0x%02x, sd30_drive_sel_3v3: 0x%02x\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3);
		break;
	case RTSX_RTS5227:
	case RTSX_RTS522A:
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_CFG_DRIVER_TYPE_B;
		reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG1, 4);
		if (!(reg & 0x1000000)) {
			sc->rtsx_card_drive_sel &= 0x3F;
			sc->rtsx_card_drive_sel |= ((reg >> 25) & 0x01) << 6;
			reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG2, 4);
			sc->rtsx_sd30_drive_sel_3v3 = (reg >> 5) & 0x03;
			if (reg & 0x4000)
				sc->rtsx_flags |= RTSX_F_REVERSE_SOCKET;
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg: 0x%08x\n", reg);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev,
				      "card_drive_sel: 0x%02x, sd30_drive_sel_3v3: 0x%02x, reverse_socket is %s\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3,
				      (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET) ? "true" : "false");
		break;
	case RTSX_RTS5229:
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_DRIVER_TYPE_D;
		reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG1, 4);
		if (!(reg & 0x1000000)) {
			sc->rtsx_card_drive_sel &= 0x3F;
			sc->rtsx_card_drive_sel |= ((reg >> 25) & 0x01) << 6;
			reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG2, 4);
			sc->rtsx_sd30_drive_sel_3v3 = rtsx_map_sd_drive((reg >> 5) & 0x03);
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg: 0x%08x\n", reg);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "card_drive_sel: 0x%02x, sd30_drive_sel_3v3: 0x%02x\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3);
		break;
	case RTSX_RTS525A:
	case RTSX_RTS5249:
	case RTSX_RTS5260:
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_CFG_DRIVER_TYPE_B;
		reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG1, 4);
		if ((reg & 0x1000000)) {
			sc->rtsx_card_drive_sel &= 0x3F;
			sc->rtsx_card_drive_sel |= ((reg >> 25) & 0x01) << 6;
			reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG2, 4);
			sc->rtsx_sd30_drive_sel_3v3 = (reg >> 5) & 0x03;
			if (reg & 0x4000)
				sc->rtsx_flags |= RTSX_F_REVERSE_SOCKET;
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg: 0x%08x\n", reg);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev,
				      "card_drive_sel = 0x%02x, sd30_drive_sel_3v3: 0x%02x, reverse_socket is %s\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3,
				      (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET) ? "true" : "false");
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
		sc->rtsx_card_drive_sel = RTSX_RTL8411_CARD_DRIVE_DEFAULT;
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_DRIVER_TYPE_D;
		reg1 = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG1, 4);
		if (reg1 & 0x1000000) {
			sc->rtsx_card_drive_sel &= 0x3F;
			sc->rtsx_card_drive_sel |= ((reg1 >> 25) & 0x01) << 6;
			reg3 = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG3, 1);
			sc->rtsx_sd30_drive_sel_3v3 = (reg3 >> 5) & 0x07;
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg1: 0x%08x\n", reg1);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev,
				      "card_drive_sel: 0x%02x, sd30_drive_sel_3v3: 0x%02x\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3);
		break;
	case RTSX_RTL8411B:
		sc->rtsx_card_drive_sel = RTSX_RTL8411_CARD_DRIVE_DEFAULT;
		sc->rtsx_sd30_drive_sel_3v3 = RTSX_DRIVER_TYPE_D;
		reg = pci_read_config(sc->rtsx_dev, RTSX_PCR_SETTING_REG1, 4);
		if (!(reg & 0x1000000)) {
			sc->rtsx_sd30_drive_sel_3v3 = rtsx_map_sd_drive(reg & 0x03);
		} else if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
			device_printf(sc->rtsx_dev, "pci_read_config() error - reg: 0x%08x\n", reg);
		}
		if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev,
				      "card_drive_sel: 0x%02x, sd30_drive_sel_3v3: 0x%02x\n",
				      sc->rtsx_card_drive_sel, sc->rtsx_sd30_drive_sel_3v3);
		break;
	}

	if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_init() rtsx_flags: 0x%04x\n", sc->rtsx_flags);

	/* Enable interrupts. */
	sc->rtsx_intr_enabled = RTSX_TRANS_OK_INT_EN | RTSX_TRANS_FAIL_INT_EN | RTSX_SD_INT_EN;
	WRITE4(sc, RTSX_BIER, sc->rtsx_intr_enabled);

	/* Power on SSC clock. */
	RTSX_CLR(sc, RTSX_FPDCTL, RTSX_SSC_POWER_DOWN);
	/* Wait SSC power stable. */
	DELAY(200);

	/* Disable ASPM */
	val = pci_read_config(sc->rtsx_dev, sc->rtsx_pcie_cap + PCIER_LINK_CTL, 1);
	pci_write_config(sc->rtsx_dev, sc->rtsx_pcie_cap + PCIER_LINK_CTL, val & 0xfc, 1);

	/*
	 * Optimize phy.
	 */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		/* Some magic numbers from Linux driver. */
		if ((error = rtsx_write_phy(sc, 0x00, 0xB966)))
			return (error);
		break;
	case RTSX_RTS5227:
		RTSX_CLR(sc, RTSX_PM_CTRL3, RTSX_D3_DELINK_MODE_EN);

		/* Optimize RX sensitivity. */
		if ((error = rtsx_write_phy(sc, 0x00, 0xBA42)))
			return (error);
		break;
	case RTSX_RTS5229:
		/* Optimize RX sensitivity. */
		if ((error = rtsx_write_phy(sc, 0x00, 0xBA42)))
			return (error);
		break;
	case RTSX_RTS522A:
		RTSX_CLR(sc, RTSX_RTS522A_PM_CTRL3, RTSX_D3_DELINK_MODE_EN);
		if (sc->rtsx_flags & RTSX_F_VERSION_A) {
			if ((error = rtsx_write_phy(sc, RTSX_PHY_RCR2, RTSX_PHY_RCR2_INIT_27S)))
				return (error);
		}
		if ((error = rtsx_write_phy(sc, RTSX_PHY_RCR1, RTSX_PHY_RCR1_INIT_27S)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_FLD0, RTSX_PHY_FLD0_INIT_27S)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_FLD3, RTSX_PHY_FLD3_INIT_27S)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_FLD4, RTSX_PHY_FLD4_INIT_27S)))
			return (error);
		break;
	case RTSX_RTS525A:
		if ((error = rtsx_write_phy(sc, RTSX__PHY_FLD0,
					    RTSX__PHY_FLD0_CLK_REQ_20C | RTSX__PHY_FLD0_RX_IDLE_EN |
					    RTSX__PHY_FLD0_BIT_ERR_RSTN | RTSX__PHY_FLD0_BER_COUNT |
					    RTSX__PHY_FLD0_BER_TIMER | RTSX__PHY_FLD0_CHECK_EN)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX__PHY_ANA03,
					    RTSX__PHY_ANA03_TIMER_MAX | RTSX__PHY_ANA03_OOBS_DEB_EN |
					    RTSX__PHY_CMU_DEBUG_EN)))
			return (error);
		if (sc->rtsx_flags & RTSX_F_VERSION_A)
			if ((error = rtsx_write_phy(sc, RTSX__PHY_REV0,
						    RTSX__PHY_REV0_FILTER_OUT | RTSX__PHY_REV0_CDR_BYPASS_PFD |
						    RTSX__PHY_REV0_CDR_RX_IDLE_BYPASS)))
				return (error);
		break;
	case RTSX_RTS5249:
		RTSX_CLR(sc, RTSX_PM_CTRL3, RTSX_D3_DELINK_MODE_EN);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_REV,
					    RTSX_PHY_REV_RESV | RTSX_PHY_REV_RXIDLE_LATCHED |
					    RTSX_PHY_REV_P1_EN | RTSX_PHY_REV_RXIDLE_EN |
					    RTSX_PHY_REV_CLKREQ_TX_EN | RTSX_PHY_REV_RX_PWST |
					    RTSX_PHY_REV_CLKREQ_DT_1_0 | RTSX_PHY_REV_STOP_CLKRD |
					    RTSX_PHY_REV_STOP_CLKWR)))
			return (error);
		DELAY(1000);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_BPCR,
					    RTSX_PHY_BPCR_IBRXSEL | RTSX_PHY_BPCR_IBTXSEL |
					    RTSX_PHY_BPCR_IB_FILTER | RTSX_PHY_BPCR_CMIRROR_EN)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_PCR,
					    RTSX_PHY_PCR_FORCE_CODE | RTSX_PHY_PCR_OOBS_CALI_50 |
					    RTSX_PHY_PCR_OOBS_VCM_08 | RTSX_PHY_PCR_OOBS_SEN_90 |
					    RTSX_PHY_PCR_RSSI_EN | RTSX_PHY_PCR_RX10K)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_RCR2,
					    RTSX_PHY_RCR2_EMPHASE_EN | RTSX_PHY_RCR2_NADJR |
					    RTSX_PHY_RCR2_CDR_SR_2 | RTSX_PHY_RCR2_FREQSEL_12 |
					    RTSX_PHY_RCR2_CDR_SC_12P | RTSX_PHY_RCR2_CALIB_LATE)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_FLD4,
					    RTSX_PHY_FLD4_FLDEN_SEL | RTSX_PHY_FLD4_REQ_REF |
					    RTSX_PHY_FLD4_RXAMP_OFF | RTSX_PHY_FLD4_REQ_ADDA |
					    RTSX_PHY_FLD4_BER_COUNT | RTSX_PHY_FLD4_BER_TIMER |
					    RTSX_PHY_FLD4_BER_CHK_EN)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_RDR,
					    RTSX_PHY_RDR_RXDSEL_1_9 | RTSX_PHY_SSC_AUTO_PWD)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_RCR1,
					    RTSX_PHY_RCR1_ADP_TIME_4 | RTSX_PHY_RCR1_VCO_COARSE)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_FLD3,
					    RTSX_PHY_FLD3_TIMER_4 | RTSX_PHY_FLD3_TIMER_6 |
					    RTSX_PHY_FLD3_RXDELINK)))
			return (error);
		if ((error = rtsx_write_phy(sc, RTSX_PHY_TUNE,
					    RTSX_PHY_TUNE_TUNEREF_1_0 | RTSX_PHY_TUNE_VBGSEL_1252 |
					    RTSX_PHY_TUNE_SDBUS_33 | RTSX_PHY_TUNE_TUNED18 |
					    RTSX_PHY_TUNE_TUNED12 | RTSX_PHY_TUNE_TUNEA12)))
			return (error);
		break;
	}

	/* Set mcu_cnt to 7 to ensure data can be sampled properly. */
	RTSX_BITOP(sc, RTSX_CLK_DIV, 0x07, 0x07);

	/* Disable sleep mode. */
	RTSX_CLR(sc, RTSX_HOST_SLEEP_STATE,
		 RTSX_HOST_ENTER_S1 | RTSX_HOST_ENTER_S3);

	/* Disable card clock. */
	RTSX_CLR(sc, RTSX_CARD_CLK_EN, RTSX_CARD_CLK_EN_ALL);

	/* Reset delink mode. */
	RTSX_CLR(sc, RTSX_CHANGE_LINK_STATE,
		 RTSX_FORCE_RST_CORE_EN | RTSX_NON_STICKY_RST_N_DBG);

	/* Card driving select. */
	RTSX_WRITE(sc, RTSX_CARD_DRIVE_SEL, sc->rtsx_card_drive_sel);

	/* Enable SSC clock. */
	RTSX_WRITE(sc, RTSX_SSC_CTL1, RTSX_SSC_8X_EN | RTSX_SSC_SEL_4M);
	RTSX_WRITE(sc, RTSX_SSC_CTL2, 0x12);

	/* Disable cd_pwr_save. */
	RTSX_BITOP(sc, RTSX_CHANGE_LINK_STATE, 0x16, RTSX_MAC_PHY_RST_N_DBG);

	/* Clear Link Ready Interrupt. */
	RTSX_BITOP(sc, RTSX_IRQSTAT0, RTSX_LINK_READY_INT, RTSX_LINK_READY_INT);

	/* Enlarge the estimation window of PERST# glitch
	 * to reduce the chance of invalid card interrupt. */
	RTSX_WRITE(sc, RTSX_PERST_GLITCH_WIDTH, 0x80);

	/* Set RC oscillator to 400K. */
	RTSX_CLR(sc, RTSX_RCCTL, RTSX_RCCTL_F_2M);

	/* Enable interrupt write-clear (default is read-clear). */
	RTSX_CLR(sc, RTSX_NFTS_TX_CTRL, RTSX_INT_READ_CLR);

	switch (sc->rtsx_device_id) {
	case RTSX_RTS525A:
	case RTSX_RTS5260:
		RTSX_BITOP(sc, RTSX_PM_CLK_FORCE_CTL, 1, 1);
		break;
	}

	/* OC power down. */
	RTSX_BITOP(sc, RTSX_FPDCTL, RTSX_SD_OC_POWER_DOWN, RTSX_SD_OC_POWER_DOWN);

	/* Enable clk_request_n to enable clock power management */
	pci_write_config(sc->rtsx_dev, sc->rtsx_pcie_cap + PCIER_LINK_CTL + 1, 1, 1);

	/* Enter L1 when host tx idle */
	pci_write_config(sc->rtsx_dev, 0x70F, 0x5B, 1);

	/*
	 * Specific extra init.
	 */
	switch (sc->rtsx_device_id) {
		uint16_t cap;
	case RTSX_RTS5209:
		/* Turn off LED. */
		RTSX_WRITE(sc, RTSX_CARD_GPIO, 0x03);
		/* Reset ASPM state to default value. */
		RTSX_CLR(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK);
		/* Force CLKREQ# PIN to drive 0 to request clock. */
		RTSX_BITOP(sc, RTSX_PETXCFG, 0x08, 0x08);
		/* Configure GPIO as output. */
		RTSX_WRITE(sc, RTSX_CARD_GPIO_DIR, 0x03);
		/* Configure driving. */
		RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, sc->rtsx_sd30_drive_sel_3v3);
		break;
	case RTSX_RTS5227:
		/* Configure GPIO as output. */
		RTSX_BITOP(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON, RTSX_GPIO_LED_ON);
		/* Reset ASPM state to default value. */
		RTSX_BITOP(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK, RTSX_FORCE_ASPM_NO_ASPM);
		/* Switch LDO3318 source from DV33 to 3V3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_BITOP(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_BITOP(sc, RTSX_OLT_LED_CTL, 0x0F, RTSX_OLT_LED_PERIOD);
		/* Configure LTR. */
		cap = pci_read_config(sc->rtsx_dev, sc->rtsx_pcie_cap + PCIER_DEVICE_CTL2, 2);
		if (cap & PCIEM_CTL2_LTR_ENABLE)
			RTSX_WRITE(sc, RTSX_LTR_CTL, 0xa3);
		/* Configure OBFF. */
		RTSX_BITOP(sc, RTSX_OBFF_CFG, RTSX_OBFF_EN_MASK, RTSX_OBFF_ENABLE);
		/* Configure driving. */
		if ((error = rtsx_rts5227_fill_driving(sc)))
			return (error);
		/* Configure force_clock_req. */
		if (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET)
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB8, 0xB8);
		else
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB8, 0x88);
		RTSX_CLR(sc, RTSX_PM_CTRL3, RTSX_D3_DELINK_MODE_EN);
		/*!!! Added for reboot after Windows. */
		RTSX_BITOP(sc, RTSX_PM_CTRL3, RTSX_PM_WAKE_EN, RTSX_PM_WAKE_EN);
		break;
	case RTSX_RTS5229:
		/* Configure GPIO as output. */
		RTSX_BITOP(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON, RTSX_GPIO_LED_ON);
		/* Reset ASPM state to default value. */
		/*  With this reset: dd if=/dev/random of=/dev/mmcsd0 encounter a timeout. */
//!!!		RTSX_BITOP(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK, RTSX_FORCE_ASPM_NO_ASPM);
		/* Force CLKREQ# PIN to drive 0 to request clock. */
		RTSX_BITOP(sc, RTSX_PETXCFG, 0x08, 0x08);
		/* Switch LDO3318 source from DV33 to card_3v3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_BITOP(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_BITOP(sc, RTSX_OLT_LED_CTL, 0x0F, RTSX_OLT_LED_PERIOD);
		/* Configure driving. */
		RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, sc->rtsx_sd30_drive_sel_3v3);
		break;
	case RTSX_RTS522A:
		/* Add specific init from RTS5227. */
		/* Configure GPIO as output. */
		RTSX_BITOP(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON, RTSX_GPIO_LED_ON);
		/* Reset ASPM state to default value. */
		RTSX_BITOP(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK, RTSX_FORCE_ASPM_NO_ASPM);
		/* Switch LDO3318 source from DV33 to 3V3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_BITOP(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_BITOP(sc, RTSX_OLT_LED_CTL, 0x0F, RTSX_OLT_LED_PERIOD);
		/* Configure LTR. */
		cap = pci_read_config(sc->rtsx_dev, sc->rtsx_pcie_cap + PCIER_DEVICE_CTL2, 2);
		if (cap & PCIEM_CTL2_LTR_ENABLE)
			RTSX_WRITE(sc, RTSX_LTR_CTL, 0xa3);
		/* Configure OBFF. */
		RTSX_BITOP(sc, RTSX_OBFF_CFG, RTSX_OBFF_EN_MASK, RTSX_OBFF_ENABLE);
		/* Configure driving. */
		if ((error = rtsx_rts5227_fill_driving(sc)))
			return (error);
		/* Configure force_clock_req. */
		if (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET)
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB8, 0xB8);
		else
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB8, 0x88);
		RTSX_CLR(sc, RTSX_RTS522A_PM_CTRL3,  0x10);

		/* specific for RTS522A. */
		RTSX_BITOP(sc, RTSX_FUNC_FORCE_CTL,
			   RTSX_FUNC_FORCE_UPME_XMT_DBG, RTSX_FUNC_FORCE_UPME_XMT_DBG);
		RTSX_BITOP(sc, RTSX_PCLK_CTL, 0x04, 0x04);
		RTSX_BITOP(sc, RTSX_PM_EVENT_DEBUG,
			   RTSX_PME_DEBUG_0, RTSX_PME_DEBUG_0);
		RTSX_WRITE(sc, RTSX_PM_CLK_FORCE_CTL, 0x11);
		break;
	case RTSX_RTS525A:
		/* Add specific init from RTS5249. */
		/* Rest L1SUB Config. */
		RTSX_CLR(sc, RTSX_L1SUB_CONFIG3, 0xff);
		/* Configure GPIO as output. */
		RTSX_BITOP(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON, RTSX_GPIO_LED_ON);
		/* Reset ASPM state to default value. */
		RTSX_BITOP(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK, RTSX_FORCE_ASPM_NO_ASPM);
		/* Switch LDO3318 source from DV33 to 3V3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_BITOP(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_BITOP(sc, RTSX_OLT_LED_CTL, 0x0F, RTSX_OLT_LED_PERIOD);
		/* Configure driving. */
		if ((error = rtsx_rts5249_fill_driving(sc)))
			return (error);
		/* Configure force_clock_req. */
		if (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET)
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0xB0);
		else
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0x80);

		/* Specifc for RTS525A. */
		RTSX_BITOP(sc, RTSX_PCLK_CTL, RTSX_PCLK_MODE_SEL, RTSX_PCLK_MODE_SEL);
		if (sc->rtsx_flags & RTSX_F_VERSION_A) {
			RTSX_WRITE(sc, RTSX_L1SUB_CONFIG2, RTSX_L1SUB_AUTO_CFG);
			RTSX_BITOP(sc, RTSX_RREF_CFG,
				   RTSX_RREF_VBGSEL_MASK, RTSX_RREF_VBGSEL_1V25);
			RTSX_BITOP(sc, RTSX_LDO_VIO_CFG,
				   RTSX_LDO_VIO_TUNE_MASK, RTSX_LDO_VIO_1V7);
			RTSX_BITOP(sc, RTSX_LDO_DV12S_CFG,
				   RTSX_LDO_D12_TUNE_MASK, RTSX_LDO_D12_TUNE_DF);
			RTSX_BITOP(sc, RTSX_LDO_AV12S_CFG,
				   RTSX_LDO_AV12S_TUNE_MASK, RTSX_LDO_AV12S_TUNE_DF);
			RTSX_BITOP(sc, RTSX_LDO_VCC_CFG0,
				   RTSX_LDO_VCC_LMTVTH_MASK, RTSX_LDO_VCC_LMTVTH_2A);
			RTSX_BITOP(sc, RTSX_OOBS_CONFIG,
				   RTSX_OOBS_AUTOK_DIS | RTSX_OOBS_VAL_MASK, 0x89);
		}
		break;
	case RTSX_RTS5249:
		/* Rest L1SUB Config. */
		RTSX_CLR(sc, RTSX_L1SUB_CONFIG3, 0xff);
		/* Configure GPIO as output. */
		RTSX_BITOP(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON, RTSX_GPIO_LED_ON);
		/* Reset ASPM state to default value. */
		RTSX_BITOP(sc, RTSX_ASPM_FORCE_CTL, RTSX_ASPM_FORCE_MASK, RTSX_FORCE_ASPM_NO_ASPM);
		/* Switch LDO3318 source from DV33 to 3V3. */
		RTSX_CLR(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33);
		RTSX_BITOP(sc, RTSX_LDO_PWR_SEL, RTSX_LDO_PWR_SEL_DV33, RTSX_LDO_PWR_SEL_3V3);
		/* Set default OLT blink period. */
		RTSX_BITOP(sc, RTSX_OLT_LED_CTL, 0x0F, RTSX_OLT_LED_PERIOD);
		/* Configure driving. */
		if ((error = rtsx_rts5249_fill_driving(sc)))
			return (error);
		/* Configure force_clock_req. */
		if (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET)
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0xB0);
		else
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0x80);
		break;
	case RTSX_RTS5260:
		/* Set mcu_cnt to 7 to ensure data can be sampled properly. */
		RTSX_BITOP(sc, RTSX_CLK_DIV, 0x07, 0x07);
		RTSX_WRITE(sc, RTSX_SSC_DIV_N_0, 0x5D);
		/* Force no MDIO */
		RTSX_WRITE(sc, RTSX_RTS5260_AUTOLOAD_CFG4, RTSX_RTS5260_MIMO_DISABLE);
		/* Modify SDVCC Tune Default Parameters! */
		RTSX_BITOP(sc, RTSX_LDO_VCC_CFG0, RTSX_RTS5260_DVCC_TUNE_MASK, RTSX_RTS5260_DVCC_33);

		RTSX_BITOP(sc, RTSX_PCLK_CTL, RTSX_PCLK_MODE_SEL, RTSX_PCLK_MODE_SEL);

		RTSX_BITOP(sc, RTSX_L1SUB_CONFIG1, RTSX_AUX_CLK_ACTIVE_SEL_MASK, RTSX_MAC_CKSW_DONE);
		/* Rest L1SUB Config */
		RTSX_CLR(sc, RTSX_L1SUB_CONFIG3, 0xFF);
		RTSX_BITOP(sc, RTSX_PM_CLK_FORCE_CTL, RTSX_CLK_PM_EN, RTSX_CLK_PM_EN);
		RTSX_WRITE(sc, RTSX_PWD_SUSPEND_EN, 0xFF);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_PWR_GATE_EN, RTSX_PWR_GATE_EN);
		RTSX_BITOP(sc, RTSX_REG_VREF, RTSX_PWD_SUSPND_EN, RTSX_PWD_SUSPND_EN);
		RTSX_BITOP(sc, RTSX_RBCTL, RTSX_U_AUTO_DMA_EN_MASK, RTSX_U_AUTO_DMA_DISABLE);
		if (sc->rtsx_flags & RTSX_F_REVERSE_SOCKET)
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0xB0);
		else
			RTSX_BITOP(sc, RTSX_PETXCFG, 0xB0, 0x80);
		RTSX_BITOP(sc, RTSX_OBFF_CFG, RTSX_OBFF_EN_MASK, RTSX_OBFF_DISABLE);

		RTSX_CLR(sc, RTSX_RTS5260_DVCC_CTRL, RTSX_RTS5260_DVCC_OCP_EN | RTSX_RTS5260_DVCC_OCP_CL_EN);

		/* CLKREQ# PIN will be forced to drive low. */
		RTSX_BITOP(sc, RTSX_PETXCFG, RTSX_FORCE_CLKREQ_DELINK_MASK, RTSX_FORCE_CLKREQ_LOW);

		RTSX_CLR(sc, RTSX_RTS522A_PM_CTRL3,  0x10);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
		RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, sc->rtsx_sd30_drive_sel_3v3);
		RTSX_BITOP(sc, RTSX_CARD_PAD_CTL, RTSX_CD_DISABLE_MASK | RTSX_CD_AUTO_DISABLE,
			   RTSX_CD_ENABLE);
		break;
	case RTSX_RTL8411B:
		if (sc->rtsx_flags & RTSX_F_8411B_QFN48)
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf5);
		RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, sc->rtsx_sd30_drive_sel_3v3);
		/* Enable SD interrupt. */
		RTSX_BITOP(sc, RTSX_CARD_PAD_CTL, RTSX_CD_DISABLE_MASK | RTSX_CD_AUTO_DISABLE,
			   RTSX_CD_ENABLE);
		/* Clear hw_pfm_en to disable hardware PFM mode. */
		RTSX_BITOP(sc, RTSX_FUNC_FORCE_CTL, 0x06, 0x00);
		break;
	}

	/*!!! Added for reboot after Windows. */
	rtsx_bus_power_off(sc);
	rtsx_set_sd_timing(sc, bus_timing_normal);
	rtsx_set_sd_clock(sc, 0);
	/*!!! Added for reboot after Windows. */

	return (0);
}

static int
rtsx_map_sd_drive(int index)
{
	uint8_t	sd_drive[4] =
		{
		 0x01,	/* Type D */
		 0x02,	/* Type C */
		 0x05,	/* Type A */
		 0x03	/* Type B */
		};
	return (sd_drive[index]);
}

/* For voltage 3v3. */
static int
rtsx_rts5227_fill_driving(struct rtsx_softc *sc)
{
	u_char	driving_3v3[4][3] = {
				     {0x13, 0x13, 0x13},
				     {0x96, 0x96, 0x96},
				     {0x7F, 0x7F, 0x7F},
				     {0x96, 0x96, 0x96},
	};
	RTSX_WRITE(sc, RTSX_SD30_CLK_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][0]);
	RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][1]);
	RTSX_WRITE(sc, RTSX_SD30_DAT_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][2]);

	return (0);
}

/* For voltage 3v3. */
static int
rtsx_rts5249_fill_driving(struct rtsx_softc *sc)
{
	u_char	driving_3v3[4][3] = {
				     {0x11, 0x11, 0x18},
				     {0x55, 0x55, 0x5C},
				     {0xFF, 0xFF, 0xFF},
				     {0x96, 0x96, 0x96},
	};
	RTSX_WRITE(sc, RTSX_SD30_CLK_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][0]);
	RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][1]);
	RTSX_WRITE(sc, RTSX_SD30_DAT_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][2]);

	return (0);
}

static int
rtsx_rts5260_fill_driving(struct rtsx_softc *sc)
{
	u_char	driving_3v3[4][3] = {
				     {0x11, 0x11, 0x11},
				     {0x22, 0x22, 0x22},
				     {0x55, 0x55, 0x55},
				     {0x33, 0x33, 0x33},
	};
	RTSX_WRITE(sc, RTSX_SD30_CLK_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][0]);
	RTSX_WRITE(sc, RTSX_SD30_CMD_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][1]);
	RTSX_WRITE(sc, RTSX_SD30_DAT_DRIVE_SEL, driving_3v3[sc->rtsx_sd30_drive_sel_3v3][2]);

	return (0);
}

static int
rtsx_read(struct rtsx_softc *sc, uint16_t addr, uint8_t *val)
{
	int	 tries = 1024;
	uint32_t arg;
	uint32_t reg;

	arg = RTSX_HAIMR_BUSY | (uint32_t)((addr & 0x3FFF) << 16);
	WRITE4(sc, RTSX_HAIMR, arg);

	while (tries--) {
		reg = READ4(sc, RTSX_HAIMR);
		if (!(reg & RTSX_HAIMR_BUSY))
			break;
	}
	*val = (reg & 0xff);

	if (tries > 0) {
		return (0);
	} else {
		device_printf(sc->rtsx_dev, "rtsx_read(0x%x) timeout\n", arg);
		return (ETIMEDOUT);
	}
}

static int
rtsx_read_cfg(struct rtsx_softc *sc, uint8_t func, uint16_t addr, uint32_t *val)
{
	int	tries = 1024;
	uint8_t	data0, data1, data2, data3, rwctl;

	RTSX_WRITE(sc, RTSX_CFGADDR0, addr);
	RTSX_WRITE(sc, RTSX_CFGADDR1, addr >> 8);
	RTSX_WRITE(sc, RTSX_CFGRWCTL, RTSX_CFG_BUSY | (func & 0x03 << 4));

	while (tries--) {
		RTSX_READ(sc, RTSX_CFGRWCTL, &rwctl);
		if (!(rwctl & RTSX_CFG_BUSY))
			break;
	}

	if (tries == 0)
		return (ETIMEDOUT);

	RTSX_READ(sc, RTSX_CFGDATA0, &data0);
	RTSX_READ(sc, RTSX_CFGDATA1, &data1);
	RTSX_READ(sc, RTSX_CFGDATA2, &data2);
	RTSX_READ(sc, RTSX_CFGDATA3, &data3);

	*val = (data3 << 24) | (data2 << 16) | (data1 << 8) | data0;

	return (0);
}

static int
rtsx_write(struct rtsx_softc *sc, uint16_t addr, uint8_t mask, uint8_t val)
{
	int 	 tries = 1024;
	uint32_t arg;
	uint32_t reg;

	arg = RTSX_HAIMR_BUSY | RTSX_HAIMR_WRITE |
		(uint32_t)(((addr & 0x3FFF) << 16) |
			   (mask << 8) | val);
	WRITE4(sc, RTSX_HAIMR, arg);

	while (tries--) {
		reg = READ4(sc, RTSX_HAIMR);
		if (!(reg & RTSX_HAIMR_BUSY)) {
			if (val != (reg & 0xff)) {
				device_printf(sc->rtsx_dev, "rtsx_write(0x%x) error reg=0x%x\n", arg, reg);
				return (EIO);
			}
			return (0);
		}
	}
	device_printf(sc->rtsx_dev, "rtsx_write(0x%x) timeout\n", arg);

	return (ETIMEDOUT);
}

static int
rtsx_read_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t *val)
{
	int	tries = 100000;
	uint8_t	data0, data1, rwctl;

	RTSX_WRITE(sc, RTSX_PHY_ADDR, addr);
	RTSX_WRITE(sc, RTSX_PHY_RWCTL, RTSX_PHY_BUSY | RTSX_PHY_READ);

	while (tries--) {
		RTSX_READ(sc, RTSX_PHY_RWCTL, &rwctl);
		if (!(rwctl & RTSX_PHY_BUSY))
			break;
	}
	if (tries == 0)
		return (ETIMEDOUT);

	RTSX_READ(sc, RTSX_PHY_DATA0, &data0);
	RTSX_READ(sc, RTSX_PHY_DATA1, &data1);
	*val = data1 << 8 | data0;

	return (0);
}

static int
rtsx_write_phy(struct rtsx_softc *sc, uint8_t addr, uint16_t val)
{
	int	tries = 100000;
	uint8_t	rwctl;

	RTSX_WRITE(sc, RTSX_PHY_DATA0, val);
	RTSX_WRITE(sc, RTSX_PHY_DATA1, val >> 8);
	RTSX_WRITE(sc, RTSX_PHY_ADDR, addr);
	RTSX_WRITE(sc, RTSX_PHY_RWCTL, RTSX_PHY_BUSY | RTSX_PHY_WRITE);

	while (tries--) {
		RTSX_READ(sc, RTSX_PHY_RWCTL, &rwctl);
		if (!(rwctl & RTSX_PHY_BUSY))
			break;
	}

	return ((tries == 0) ? ETIMEDOUT : 0);
}

/*
 * Notice that the meaning of RTSX_PWR_GATE_CTRL changes between RTS5209 and
 * RTS5229. In RTS5209 it is a mask of disabled power gates, while in RTS5229
 * it is a mask of *enabled* gates.
 */
static int
rtsx_bus_power_off(struct rtsx_softc *sc)
{
	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_bus_power_off()\n");

	/* Disable SD clock. */
	RTSX_CLR(sc, RTSX_CARD_CLK_EN, RTSX_SD_CLK_EN);

	/* Disable SD output. */
	RTSX_CLR(sc, RTSX_CARD_OE, RTSX_SD_OUTPUT_EN);

	/* Turn off power. */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK | RTSX_PMOS_STRG_MASK,
			   RTSX_SD_PWR_OFF | RTSX_PMOS_STRG_400mA);
		RTSX_SET(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_OFF);
		break;
	case RTSX_RTS5227:
	case RTSX_RTS5229:
	case RTSX_RTS522A:
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK | RTSX_PMOS_STRG_MASK,
			   RTSX_SD_PWR_OFF | RTSX_PMOS_STRG_400mA);
		RTSX_CLR(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK);
		break;
	case RTSX_RTS5260:
		rtsx_stop_cmd(sc);
		/* Switch vccq to 330 */
		RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_DV331812_VDD1, RTSX_DV331812_VDD1);
		RTSX_BITOP(sc, RTSX_LDO_DV18_CFG, RTSX_DV331812_MASK, RTSX_DV331812_33);
		RTSX_CLR(sc, RTSX_SD_PAD_CTL, RTSX_SD_IO_USING_1V8);
		rtsx_rts5260_fill_driving(sc);

		RTSX_BITOP(sc, RTSX_LDO_VCC_CFG1, RTSX_LDO_POW_SDVDD1_MASK, RTSX_LDO_POW_SDVDD1_OFF);
		RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_DV331812_POWERON, RTSX_DV331812_POWEROFF);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
	case RTSX_RTL8411B:
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
			   RTSX_BPP_POWER_OFF);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
			   RTSX_BPP_LDO_SUSPEND);
		break;
	default:
		RTSX_CLR(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK);
		RTSX_SET(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_OFF);
		RTSX_CLR(sc, RTSX_CARD_PWR_CTL, RTSX_PMOS_STRG_800mA);
		break;
	}

	/* Disable pull control. */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_DISABLE3);
		break;
	case RTSX_RTS5227:
	case RTSX_RTS522A:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_DISABLE3);
		break;
	case RTSX_RTS5229:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_DISABLE12);
		if (sc->rtsx_flags & RTSX_F_VERSION_C)
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_DISABLE3_TYPE_C);
		else
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_DISABLE3);
		break;
	case RTSX_RTS525A:
	case RTSX_RTS5249:
	case RTSX_RTS5260:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x66);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_DISABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_DISABLE3);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x55);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x65);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x55);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0x95);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x05);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x04);
		break;
	case RTSX_RTL8411B:
		if (sc->rtsx_flags & RTSX_F_8411B_QFN48) {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf5);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		} else {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x65);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xd5);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x59);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		}
		break;
	}

	return (0);
}

static int
rtsx_bus_power_on(struct rtsx_softc *sc)
{
	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_bus_power_on()\n");

	/* Select SD card. */
	RTSX_BITOP(sc, RTSX_CARD_SELECT, 0x07, RTSX_SD_MOD_SEL);
	RTSX_BITOP(sc, RTSX_CARD_SHARE_MODE, RTSX_CARD_SHARE_MASK, RTSX_CARD_SHARE_48_SD);

	/* Enable SD clock. */
	RTSX_BITOP(sc, RTSX_CARD_CLK_EN, RTSX_SD_CLK_EN,  RTSX_SD_CLK_EN);

	/* Enable pull control. */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, RTSX_PULL_CTL_ENABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_ENABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_ENABLE3);
		break;
	case RTSX_RTS5227:
	case RTSX_RTS522A:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_ENABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_ENABLE3);
		break;
	case RTSX_RTS5229:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_ENABLE12);
		if (sc->rtsx_flags & RTSX_F_VERSION_C)
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_ENABLE3_TYPE_C);
		else
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_ENABLE3);
		break;
	case RTSX_RTS525A:
	case RTSX_RTS5249:
	case RTSX_RTS5260:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0x66);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, RTSX_PULL_CTL_ENABLE12);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, RTSX_PULL_CTL_ENABLE3);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0xaa);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0xaa);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xa9);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x09);
		RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x04);
		break;
	case RTSX_RTL8411B:
		if (sc->rtsx_flags & RTSX_F_8411B_QFN48) {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xf9);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x19);
		} else {
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL1, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL2, 0xaa);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL3, 0xd9);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL4, 0x59);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL5, 0x55);
			RTSX_WRITE(sc, RTSX_CARD_PULL_CTL6, 0x15);
		}
		break;
	}

	/*
	 * To avoid a current peak, enable card power in two phases
	 * with a delay in between.
	 */
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		/* Partial power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PARTIAL_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK, RTSX_LDO3318_VCC2);

		DELAY(200);

		/* Full power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK, RTSX_LDO3318_ON);
		break;
	case RTSX_RTS5227:
	case RTSX_RTS522A:
		/* Partial power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PARTIAL_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK, RTSX_LDO3318_VCC1);

		DELAY(20000);

		/* Full power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK,
			   RTSX_LDO3318_VCC1 | RTSX_LDO3318_VCC2);
		RTSX_BITOP(sc, RTSX_CARD_OE, RTSX_SD_OUTPUT_EN, RTSX_SD_OUTPUT_EN);
		RTSX_BITOP(sc, RTSX_CARD_OE, RTSX_MS_OUTPUT_EN, RTSX_MS_OUTPUT_EN);
		break;
	case RTSX_RTS5229:
		/* Partial power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PARTIAL_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK, RTSX_LDO3318_VCC1);

		DELAY(200);

		/* Full power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK,
			   RTSX_LDO3318_VCC1 | RTSX_LDO3318_VCC2);
		break;
	case RTSX_RTS525A:
		RTSX_BITOP(sc, RTSX_LDO_VCC_CFG1, RTSX_LDO_VCC_TUNE_MASK, RTSX_LDO_VCC_3V3);
	case RTSX_RTS5249:
		/* Partial power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PARTIAL_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK, RTSX_LDO3318_VCC1);

		DELAY(5000);

		/* Full power. */
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_SD_PWR_MASK, RTSX_SD_PWR_ON);
		RTSX_BITOP(sc, RTSX_PWR_GATE_CTRL, RTSX_LDO3318_PWR_MASK,
			   RTSX_LDO3318_VCC1 | RTSX_LDO3318_VCC2);
		break;
	case RTSX_RTS5260:
		RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_DV331812_VDD1, RTSX_DV331812_VDD1);
		RTSX_BITOP(sc, RTSX_LDO_VCC_CFG0, RTSX_RTS5260_DVCC_TUNE_MASK, RTSX_RTS5260_DVCC_33);
		RTSX_BITOP(sc, RTSX_LDO_VCC_CFG1, RTSX_LDO_POW_SDVDD1_MASK, RTSX_LDO_POW_SDVDD1_ON);
		RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_DV331812_POWERON, RTSX_DV331812_POWERON);

		DELAY(20000);

		RTSX_BITOP(sc, RTSX_SD_CFG1, RTSX_SD_MODE_MASK | RTSX_SD_ASYNC_FIFO_NOT_RST,
			   RTSX_SD30_MODE | RTSX_SD_ASYNC_FIFO_NOT_RST);
		RTSX_BITOP(sc, RTSX_CLK_CTL, RTSX_CHANGE_CLK, RTSX_CLK_LOW_FREQ);
		RTSX_WRITE(sc, RTSX_CARD_CLK_SOURCE,
			   RTSX_CRC_VAR_CLK0 | RTSX_SD30_FIX_CLK | RTSX_SAMPLE_VAR_CLK1);
		RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

		/* Initialize SD_CFG1 register */
		RTSX_WRITE(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_128 | RTSX_SD20_MODE);
		RTSX_WRITE(sc, RTSX_SD_SAMPLE_POINT_CTL, RTSX_SD20_RX_POS_EDGE);
		RTSX_CLR(sc, RTSX_SD_PUSH_POINT_CTL, 0xff);
		RTSX_BITOP(sc, RTSX_CARD_STOP, RTSX_SD_STOP | RTSX_SD_CLR_ERR,
			   RTSX_SD_STOP | RTSX_SD_CLR_ERR);
		/* Reset SD_CFG3 register */
		RTSX_CLR(sc, RTSX_SD_CFG3, RTSX_SD30_CLK_END_EN);
		RTSX_CLR(sc, RTSX_REG_SD_STOP_SDCLK_CFG,
			 RTSX_SD30_CLK_STOP_CFG_EN | RTSX_SD30_CLK_STOP_CFG0 | RTSX_SD30_CLK_STOP_CFG1);
		RTSX_CLR(sc, RTSX_REG_PRE_RW_MODE, RTSX_EN_INFINITE_MODE);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
	case RTSX_RTL8411B:
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
			   RTSX_BPP_POWER_5_PERCENT_ON);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
			   RTSX_BPP_LDO_SUSPEND);
		DELAY(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
			   RTSX_BPP_POWER_10_PERCENT_ON);
		DELAY(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
			   RTSX_BPP_POWER_15_PERCENT_ON);
		DELAY(150);
		RTSX_BITOP(sc, RTSX_CARD_PWR_CTL, RTSX_BPP_POWER_MASK,
			   RTSX_BPP_POWER_ON);
		RTSX_BITOP(sc, RTSX_LDO_CTL, RTSX_BPP_LDO_POWB,
			   RTSX_BPP_LDO_ON);
		break;
	}

	/* Enable SD card output. */
	RTSX_WRITE(sc, RTSX_CARD_OE, RTSX_SD_OUTPUT_EN);

	DELAY(200);

	return (0);
}

/*
 * Set but width.
 */
static int
rtsx_set_bus_width(struct rtsx_softc *sc, enum mmc_bus_width width)
{
	uint32_t bus_width;

	switch (width) {
	case bus_width_1:
		bus_width = RTSX_BUS_WIDTH_1;
		break;
	case bus_width_4:
		bus_width = RTSX_BUS_WIDTH_4;
		break;
	case bus_width_8:
		bus_width = RTSX_BUS_WIDTH_8;
		break;
	default:
		return (MMC_ERR_INVALID);
	}
	RTSX_BITOP(sc, RTSX_SD_CFG1, RTSX_BUS_WIDTH_MASK, bus_width);

	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
		char *busw[] = {
				"1 bit",
				"4 bits",
				"8 bits"
		};
		device_printf(sc->rtsx_dev, "Setting bus width to %s\n", busw[bus_width]);
	}
	return (0);
}

static int
rtsx_set_sd_timing(struct rtsx_softc *sc, enum mmc_bus_timing timing)
{
	if (timing == bus_timing_hs && sc->rtsx_force_timing) {
		timing = bus_timing_uhs_sdr50;
		sc->rtsx_ios_timing = timing;
	}

	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_set_sd_timing(%u)\n", timing);

	switch (timing) {
	case bus_timing_uhs_sdr50:
	case bus_timing_uhs_sdr104:
		sc->rtsx_double_clk = false;
		sc->rtsx_vpclk = true;
		RTSX_BITOP(sc, RTSX_SD_CFG1, 0x0c | RTSX_SD_ASYNC_FIFO_NOT_RST,
			   RTSX_SD30_MODE | RTSX_SD_ASYNC_FIFO_NOT_RST);
		RTSX_BITOP(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ, RTSX_CLK_LOW_FREQ);
		RTSX_WRITE(sc, RTSX_CARD_CLK_SOURCE,
			   RTSX_CRC_VAR_CLK0 | RTSX_SD30_FIX_CLK | RTSX_SAMPLE_VAR_CLK1);
		RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);
		break;
	case bus_timing_hs:
		RTSX_BITOP(sc, RTSX_SD_CFG1, RTSX_SD_MODE_MASK, RTSX_SD20_MODE);
		RTSX_BITOP(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ, RTSX_CLK_LOW_FREQ);
		RTSX_WRITE(sc, RTSX_CARD_CLK_SOURCE,
			   RTSX_CRC_FIX_CLK | RTSX_SD30_VAR_CLK0 | RTSX_SAMPLE_VAR_CLK1);
		RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

		RTSX_BITOP(sc, RTSX_SD_PUSH_POINT_CTL,
			   RTSX_SD20_TX_SEL_MASK, RTSX_SD20_TX_14_AHEAD);
		RTSX_BITOP(sc, RTSX_SD_SAMPLE_POINT_CTL,
			   RTSX_SD20_RX_SEL_MASK, RTSX_SD20_RX_14_DELAY);
		break;
	default:
		RTSX_BITOP(sc, RTSX_SD_CFG1, RTSX_SD_MODE_MASK, RTSX_SD20_MODE);
		RTSX_BITOP(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ, RTSX_CLK_LOW_FREQ);
		RTSX_WRITE(sc, RTSX_CARD_CLK_SOURCE,
			   RTSX_CRC_FIX_CLK | RTSX_SD30_VAR_CLK0 | RTSX_SAMPLE_VAR_CLK1);
		RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

		RTSX_WRITE(sc, RTSX_SD_PUSH_POINT_CTL, RTSX_SD20_TX_NEG_EDGE);
		RTSX_BITOP(sc, RTSX_SD_SAMPLE_POINT_CTL,
			   RTSX_SD20_RX_SEL_MASK, RTSX_SD20_RX_POS_EDGE);
		break;
	}

	return (0);
}

/*
 * Set or change SDCLK frequency or disable the SD clock.
 * Return zero on success.
 */
static int
rtsx_set_sd_clock(struct rtsx_softc *sc, uint32_t freq)
{
	uint8_t	clk;
	uint8_t	clk_divider, n, div, mcu;
	int	error = 0;

	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_set_sd_clock(%u)\n", freq);

	if (freq == RTSX_SDCLK_OFF) {
		error = rtsx_stop_sd_clock(sc);
		return error;
	}

	sc->rtsx_ssc_depth = RTSX_SSC_DEPTH_500K;
	sc->rtsx_discovery_mode = (freq <= 1000000) ? true : false;

	if (sc->rtsx_discovery_mode) {
		/* We use 250k(around) here, in discovery stage. */
		clk_divider = RTSX_CLK_DIVIDE_128;
		freq = 30000000;
	} else {
		clk_divider = RTSX_CLK_DIVIDE_0;
	}
	RTSX_BITOP(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, clk_divider);

	freq /= 1000000;
	if (sc->rtsx_discovery_mode || !sc->rtsx_double_clk)
		clk = freq;
	else
		clk = freq * 2;

	switch (sc->rtsx_device_id) {
	case RTSX_RTL8402:
	case RTSX_RTL8411:
	case RTSX_RTL8411B:
		n = clk * 4 / 5 - 2;
		break;
	default:
		n = clk - 2;
		break;
	}
	if ((clk <= 2) || (n > RTSX_MAX_DIV_N))
		return (MMC_ERR_INVALID);

	mcu = 125 / clk + 3;
	if (mcu > 15)
		mcu = 15;

	/* Make sure that the SSC clock div_n is not less than RTSX_MIN_DIV_N. */
	div = RTSX_CLK_DIV_1;
	while ((n < RTSX_MIN_DIV_N) && (div < RTSX_CLK_DIV_8)) {
		switch (sc->rtsx_device_id) {
		case RTSX_RTL8402:
		case RTSX_RTL8411:
		case RTSX_RTL8411B:
			n = (((n + 2) * 5 / 4) * 2) * 4 / 5 - 2;
			break;
		default:
			n = (n + 2) * 2 - 2;
			break;
		}
		div++;
	}

	if (sc->rtsx_double_clk && sc->rtsx_ssc_depth > 1)
		sc->rtsx_ssc_depth -= 1;

	if (div > RTSX_CLK_DIV_1) {
		if (sc->rtsx_ssc_depth > (div - 1))
			sc->rtsx_ssc_depth -= (div - 1);
		else
			sc->rtsx_ssc_depth = RTSX_SSC_DEPTH_4M;
	}

	/* Enable SD clock. */
	error = rtsx_switch_sd_clock(sc, clk, n, div, mcu);

	return (error);
}

static int
rtsx_stop_sd_clock(struct rtsx_softc *sc)
{
	RTSX_CLR(sc, RTSX_CARD_CLK_EN, RTSX_CARD_CLK_EN_ALL);
	RTSX_SET(sc, RTSX_SD_BUS_STAT, RTSX_SD_CLK_FORCE_STOP);

	return (0);
}

static int
rtsx_switch_sd_clock(struct rtsx_softc *sc, uint8_t clk, uint8_t n, uint8_t div, uint8_t mcu)
{
	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC) {
		device_printf(sc->rtsx_dev, "rtsx_switch_sd_clock() - discovery-mode is %s, ssc_depth: %d\n",
			      (sc->rtsx_discovery_mode) ? "true" : "false", sc->rtsx_ssc_depth);
		device_printf(sc->rtsx_dev, "rtsx_switch_sd_clock() - clk: %d, n: %d, div: %d, mcu: %d\n",
			      clk, n, div, mcu);
	}

	RTSX_BITOP(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ, RTSX_CLK_LOW_FREQ);
	RTSX_WRITE(sc, RTSX_CLK_DIV, (div << 4) | mcu);
	RTSX_CLR(sc, RTSX_SSC_CTL1, RTSX_RSTB);
	RTSX_BITOP(sc, RTSX_SSC_CTL2, RTSX_SSC_DEPTH_MASK, sc->rtsx_ssc_depth);
	RTSX_WRITE(sc, RTSX_SSC_DIV_N_0, n);
	RTSX_BITOP(sc, RTSX_SSC_CTL1, RTSX_RSTB, RTSX_RSTB);
	if (sc->rtsx_vpclk) {
		RTSX_CLR(sc, RTSX_SD_VPCLK0_CTL, RTSX_PHASE_NOT_RESET);
		RTSX_BITOP(sc, RTSX_SD_VPCLK0_CTL, RTSX_PHASE_NOT_RESET, RTSX_PHASE_NOT_RESET);
	}

	/* Wait SSC clock stable. */
	DELAY(200);

	RTSX_CLR(sc, RTSX_CLK_CTL, RTSX_CLK_LOW_FREQ);

	return (0);
}

#ifndef MMCCAM
static void
rtsx_sd_change_tx_phase(struct rtsx_softc *sc, uint8_t sample_point)
{
	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_sd_change_tx_phase() - sample_point: %d\n", sample_point);

	rtsx_write(sc, RTSX_CLK_CTL, RTSX_CHANGE_CLK, RTSX_CHANGE_CLK);
	rtsx_write(sc, RTSX_SD_VPCLK0_CTL, RTSX_PHASE_SELECT_MASK, sample_point);
	rtsx_write(sc, RTSX_SD_VPCLK0_CTL, RTSX_PHASE_NOT_RESET, 0);
	rtsx_write(sc, RTSX_SD_VPCLK0_CTL, RTSX_PHASE_NOT_RESET, RTSX_PHASE_NOT_RESET);
	rtsx_write(sc, RTSX_CLK_CTL, RTSX_CHANGE_CLK, 0);
	rtsx_write(sc, RTSX_SD_CFG1, RTSX_SD_ASYNC_FIFO_NOT_RST, 0);
}

static void
rtsx_sd_change_rx_phase(struct rtsx_softc *sc, uint8_t sample_point)
{
	if (sc->rtsx_debug_mask & RTSX_DEBUG_TUNING)
		device_printf(sc->rtsx_dev, "rtsx_sd_change_rx_phase() - sample_point: %d\n", sample_point);

	rtsx_write(sc, RTSX_CLK_CTL, RTSX_CHANGE_CLK, RTSX_CHANGE_CLK);
	rtsx_write(sc, RTSX_SD_VPCLK1_CTL, RTSX_PHASE_SELECT_MASK, sample_point);
	rtsx_write(sc, RTSX_SD_VPCLK1_CTL, RTSX_PHASE_NOT_RESET, 0);
	rtsx_write(sc, RTSX_SD_VPCLK1_CTL, RTSX_PHASE_NOT_RESET, RTSX_PHASE_NOT_RESET);
	rtsx_write(sc, RTSX_CLK_CTL, RTSX_CHANGE_CLK, 0);
	rtsx_write(sc, RTSX_SD_CFG1, RTSX_SD_ASYNC_FIFO_NOT_RST, 0);
}

static void
rtsx_sd_tuning_rx_phase(struct rtsx_softc *sc, uint32_t *phase_map)
{
	uint32_t raw_phase_map = 0;
	int	 i;
	int	 error;

	for (i = 0; i < RTSX_RX_PHASE_MAX; i++) {
		error = rtsx_sd_tuning_rx_cmd(sc, (uint8_t)i);
		if (error == 0)
			raw_phase_map |= 1 << i;
	}
	if (phase_map != NULL)
		*phase_map = raw_phase_map;
}

static int
rtsx_sd_tuning_rx_cmd(struct rtsx_softc *sc, uint8_t sample_point)
{
	struct mmc_request req = {};
	struct mmc_command cmd = {};
	int	error = 0;

	cmd.opcode = MMC_SEND_TUNING_BLOCK;
	cmd.arg = 0;
	req.cmd = &cmd;

	RTSX_LOCK(sc);

	sc->rtsx_req = &req;

	rtsx_sd_change_rx_phase(sc, sample_point);

	rtsx_write(sc, RTSX_SD_CFG3, RTSX_SD_RSP_80CLK_TIMEOUT_EN,
		   RTSX_SD_RSP_80CLK_TIMEOUT_EN);

	rtsx_init_cmd(sc, &cmd);
	rtsx_set_cmd_data_len(sc, 1, 0x40);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2, 0xff,
		      RTSX_SD_CALCULATE_CRC7 | RTSX_SD_CHECK_CRC16 |
		      RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_CHECK_CRC7 | RTSX_SD_RSP_LEN_6);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
		      0xff, RTSX_TM_AUTO_TUNING | RTSX_SD_TRANSFER_START);
	rtsx_push_cmd(sc, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
		      RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

	/* Set interrupt post processing */
	sc->rtsx_intr_trans_ok = rtsx_sd_tuning_rx_cmd_wakeup;
	sc->rtsx_intr_trans_ko = rtsx_sd_tuning_rx_cmd_wakeup;

	/* Run the command queue. */
	rtsx_send_cmd(sc);

	error = rtsx_sd_tuning_rx_cmd_wait(sc, &cmd);

	if (error) {
		if (sc->rtsx_debug_mask & RTSX_DEBUG_TUNING)
			device_printf(sc->rtsx_dev, "rtsx_sd_tuning_rx_cmd() - error: %d\n", error);
		rtsx_sd_wait_data_idle(sc);
		rtsx_clear_error(sc);
	}
	rtsx_write(sc, RTSX_SD_CFG3, RTSX_SD_RSP_80CLK_TIMEOUT_EN, 0);

	sc->rtsx_req = NULL;

	RTSX_UNLOCK(sc);

	return (error);
}

static int
rtsx_sd_tuning_rx_cmd_wait(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	int	status;
	int	mask = RTSX_TRANS_OK_INT | RTSX_TRANS_FAIL_INT;

	status = sc->rtsx_intr_status & mask;
	while (status == 0) {
		if (msleep(&sc->rtsx_intr_status, &sc->rtsx_mtx, 0, "rtsxintr", sc->rtsx_timeout_cmd) == EWOULDBLOCK) {
			cmd->error = MMC_ERR_TIMEOUT;
			return (MMC_ERR_TIMEOUT);
		}
		status = sc->rtsx_intr_status & mask;
	}
	return (cmd->error);
}

static void
rtsx_sd_tuning_rx_cmd_wakeup(struct rtsx_softc *sc)
{
	wakeup(&sc->rtsx_intr_status);
}

static void
rtsx_sd_wait_data_idle(struct rtsx_softc *sc)
{
	int	i;
	uint8_t	val;

	for (i = 0; i < 100; i++) {
		rtsx_read(sc, RTSX_SD_DATA_STATE, &val);
		if (val & RTSX_SD_DATA_IDLE)
			return;
		DELAY(100);
	}
}

static uint8_t
rtsx_sd_search_final_rx_phase(struct rtsx_softc *sc, uint32_t phase_map)
{
	int	start = 0, len = 0;
	int	start_final = 0, len_final = 0;
	uint8_t	final_phase = 0xff;

	while (start < RTSX_RX_PHASE_MAX) {
		len = rtsx_sd_get_rx_phase_len(phase_map, start);
		if (len_final < len) {
			start_final = start;
			len_final = len;
		}
		start += len ? len : 1;
	}

	final_phase = (start_final + len_final / 2) % RTSX_RX_PHASE_MAX;

	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev,
			      "rtsx_sd_search_final_rx_phase() - phase_map: %x, start_final: %d, len_final: %d, final_phase: %d\n",
			      phase_map, start_final, len_final, final_phase);

	return final_phase;
}

static int
rtsx_sd_get_rx_phase_len(uint32_t phase_map, int start_bit)
{
	int	i;

	for (i = 0; i < RTSX_RX_PHASE_MAX; i++) {
		if ((phase_map & (1 << (start_bit + i) % RTSX_RX_PHASE_MAX)) == 0)
			return i;
	}
	return RTSX_RX_PHASE_MAX;
}
#endif /* !MMCCAM */

#if 0	/* For led */
static int
rtsx_led_enable(struct rtsx_softc *sc)
{
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		RTSX_CLR(sc, RTSX_CARD_GPIO, RTSX_CARD_GPIO_LED_OFF);
		RTSX_WRITE(sc, RTSX_CARD_AUTO_BLINK,
			   RTSX_LED_BLINK_EN | RTSX_LED_BLINK_SPEED);
		break;
	case RTSX_RTL8411B:
		RTSX_CLR(sc, RTSX_GPIO_CTL, 0x01);
		RTSX_WRITE(sc, RTSX_CARD_AUTO_BLINK,
			   RTSX_LED_BLINK_EN | RTSX_LED_BLINK_SPEED);
		break;
	default:
		RTSX_SET(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON);
		RTSX_SET(sc, RTSX_OLT_LED_CTL, RTSX_OLT_LED_AUTOBLINK);
		break;
	}

	return (0);
}

static int
rtsx_led_disable(struct rtsx_softc *sc)
{
	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
		RTSX_CLR(sc, RTSX_CARD_AUTO_BLINK, RTSX_LED_BLINK_EN);
		RTSX_WRITE(sc, RTSX_CARD_GPIO, RTSX_CARD_GPIO_LED_OFF);
		break;
	case RTSX_RTL8411B:
		RTSX_CLR(sc, RTSX_CARD_AUTO_BLINK, RTSX_LED_BLINK_EN);
		RTSX_SET(sc, RTSX_GPIO_CTL, 0x01);
		break;
	default:
		RTSX_CLR(sc, RTSX_OLT_LED_CTL, RTSX_OLT_LED_AUTOBLINK);
		RTSX_CLR(sc, RTSX_GPIO_CTL, RTSX_GPIO_LED_ON);
		break;
	}

	return (0);
}
#endif	/* For led */

static uint8_t
rtsx_response_type(uint16_t mmc_rsp)
{
	int	i;
	struct rsp_type {
		uint16_t mmc_rsp;
		uint8_t  rtsx_rsp;
	} rsp_types[] = {
		{ MMC_RSP_NONE,	RTSX_SD_RSP_TYPE_R0 },
		{ MMC_RSP_R1,	RTSX_SD_RSP_TYPE_R1 },
		{ MMC_RSP_R1B,	RTSX_SD_RSP_TYPE_R1B },
		{ MMC_RSP_R2,	RTSX_SD_RSP_TYPE_R2 },
		{ MMC_RSP_R3,	RTSX_SD_RSP_TYPE_R3 },
		{ MMC_RSP_R4,	RTSX_SD_RSP_TYPE_R4 },
		{ MMC_RSP_R5,	RTSX_SD_RSP_TYPE_R5 },
		{ MMC_RSP_R6,	RTSX_SD_RSP_TYPE_R6 },
		{ MMC_RSP_R7,	RTSX_SD_RSP_TYPE_R7 }
	};

	for (i = 0; i < nitems(rsp_types); i++) {
		if (mmc_rsp == rsp_types[i].mmc_rsp)
			return (rsp_types[i].rtsx_rsp);
	}

	return (0);
}

/*
 * Init command buffer with SD command index and argument.
 */
static void
rtsx_init_cmd(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	sc->rtsx_cmd_index = 0;
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CMD0,
		      0xff, RTSX_SD_CMD_START  | cmd->opcode);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CMD1,
		     0xff, cmd->arg >> 24);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CMD2,
		      0xff, cmd->arg >> 16);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CMD3,
		     0xff, cmd->arg >> 8);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CMD4,
		     0xff, cmd->arg);
}

/*
 * Append a properly encoded host command to the host command buffer.
 */
static void
rtsx_push_cmd(struct rtsx_softc *sc, uint8_t cmd, uint16_t reg,
	      uint8_t mask, uint8_t data)
{
	KASSERT(sc->rtsx_cmd_index < RTSX_HOSTCMD_MAX,
		("rtsx: Too many host commands (%d)\n", sc->rtsx_cmd_index));

	uint32_t *cmd_buffer = (uint32_t *)(sc->rtsx_cmd_dmamem);
	cmd_buffer[sc->rtsx_cmd_index++] =
		htole32((uint32_t)(cmd & 0x3) << 30) |
		((uint32_t)(reg & 0x3fff) << 16) |
		((uint32_t)(mask) << 8) |
		((uint32_t)data);
}

/*
 * Queue commands to configure data transfer size.
 */
static void
rtsx_set_cmd_data_len(struct rtsx_softc *sc, uint16_t block_cnt, uint16_t byte_cnt)
{
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_L,
		      0xff, block_cnt & 0xff);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_BLOCK_CNT_H,
		      0xff, block_cnt >> 8);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_L,
		      0xff, byte_cnt & 0xff);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_BYTE_CNT_H,
		      0xff, byte_cnt >> 8);
}

/*
 * Run the command queue.
 */
static void
rtsx_send_cmd(struct rtsx_softc *sc)
{
	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_send_cmd()\n");

	sc->rtsx_intr_status = 0;

	/* Sync command DMA buffer. */
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_PREWRITE);

	/* Tell the chip where the command buffer is and run the commands. */
	WRITE4(sc, RTSX_HCBAR, (uint32_t)sc->rtsx_cmd_buffer);
	WRITE4(sc, RTSX_HCBCTLR,
	       ((sc->rtsx_cmd_index * 4) & 0x00ffffff) | RTSX_START_CMD | RTSX_HW_AUTO_RSP);
}

/*
 * Stop previous command.
 */
static void
rtsx_stop_cmd(struct rtsx_softc *sc)
{
	/* Stop command transfer. */
	WRITE4(sc, RTSX_HCBCTLR, RTSX_STOP_CMD);

	/* Stop DMA transfer. */
	WRITE4(sc, RTSX_HDBCTLR, RTSX_STOP_DMA);

	switch (sc->rtsx_device_id) {
	case RTSX_RTS5260:
		rtsx_write(sc, RTSX_RTS5260_DMA_RST_CTL_0,
			   RTSX_RTS5260_DMA_RST | RTSX_RTS5260_ADMA3_RST,
			   RTSX_RTS5260_DMA_RST | RTSX_RTS5260_ADMA3_RST);
		rtsx_write(sc, RTSX_RBCTL, RTSX_RB_FLUSH, RTSX_RB_FLUSH);
		break;
	default:
		rtsx_write(sc, RTSX_DMACTL, RTSX_DMA_RST, RTSX_DMA_RST);

		rtsx_write(sc, RTSX_RBCTL, RTSX_RB_FLUSH, RTSX_RB_FLUSH);
		break;
	}
}

/*
 * Clear error.
 */
static void
rtsx_clear_error(struct rtsx_softc *sc)
{
	/* Clear error. */
	rtsx_write(sc, RTSX_CARD_STOP, RTSX_SD_STOP | RTSX_SD_CLR_ERR,
		   RTSX_SD_STOP | RTSX_SD_CLR_ERR);
}

/*
 * Signal end of request to mmc/mmcsd.
 */
static void
rtsx_req_done(struct rtsx_softc *sc)
{
#ifdef MMCCAM
	union ccb *ccb;
#endif /* MMCCAM */
	struct mmc_request *req;

	req = sc->rtsx_req;
	if (req->cmd->error == MMC_ERR_NONE) {
		if (req->cmd->opcode == MMC_READ_SINGLE_BLOCK ||
		    req->cmd->opcode == MMC_READ_MULTIPLE_BLOCK)
			sc->rtsx_read_count++;
		else if (req->cmd->opcode == MMC_WRITE_BLOCK ||
			 req->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)
			sc->rtsx_write_count++;
	} else {
		rtsx_clear_error(sc);
	}
	callout_stop(&sc->rtsx_timeout_callout);
	sc->rtsx_req = NULL;
#ifdef MMCCAM
	ccb = sc->rtsx_ccb;
	sc->rtsx_ccb = NULL;
	ccb->ccb_h.status = (req->cmd->error == 0 ? CAM_REQ_CMP : CAM_REQ_CMP_ERR);
	xpt_done(ccb);
#else  /* !MMCCAM */
	req->done(req);
#endif /* MMCCAM */
}

/*
 * Send request.
 */
static int
rtsx_send_req(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	uint8_t	 rsp_type;
	uint16_t reg;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_send_req() - CMD%d\n", cmd->opcode);

	/* Convert response type. */
	rsp_type = rtsx_response_type(cmd->flags & MMC_RSP_MASK);
	if (rsp_type == 0) {
		device_printf(sc->rtsx_dev, "Unknown rsp_type: 0x%lx\n", (cmd->flags & MMC_RSP_MASK));
		cmd->error = MMC_ERR_INVALID;
		return (MMC_ERR_INVALID);
	}

	rtsx_init_cmd(sc, cmd);

	/* Queue command to set response type. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2, 0xff, rsp_type);

	/* Use the ping-pong buffer (cmd buffer) for commands which do not transfer data. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_CARD_DATA_SOURCE,
		      0x01, RTSX_PINGPONG_BUFFER);

	/* Queue commands to perform SD transfer. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
		      0xff, RTSX_TM_CMD_RSP | RTSX_SD_TRANSFER_START);
	rtsx_push_cmd(sc, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
		      RTSX_SD_TRANSFER_END|RTSX_SD_STAT_IDLE,
		      RTSX_SD_TRANSFER_END|RTSX_SD_STAT_IDLE);

	/* If needed queue commands to read back card status response. */
	if (rsp_type == RTSX_SD_RSP_TYPE_R2) {
		/* Read data from ping-pong buffer. */
		for (reg = RTSX_PPBUF_BASE2; reg < RTSX_PPBUF_BASE2 + 16; reg++)
			rtsx_push_cmd(sc, RTSX_READ_REG_CMD, reg, 0, 0);
	} else if (rsp_type != RTSX_SD_RSP_TYPE_R0) {
		/* Read data from SD_CMDx registers. */
		for (reg = RTSX_SD_CMD0; reg <= RTSX_SD_CMD4; reg++)
			rtsx_push_cmd(sc, RTSX_READ_REG_CMD, reg, 0, 0);
	}
	rtsx_push_cmd(sc, RTSX_READ_REG_CMD, RTSX_SD_STAT1, 0, 0);

	/* Set transfer OK function. */
	if (sc->rtsx_intr_trans_ok == NULL)
		sc->rtsx_intr_trans_ok = rtsx_ret_resp;

	/* Run the command queue. */
	rtsx_send_cmd(sc);

	return (0);
}

/*
 * Return response of previous command (case cmd->data == NULL) and complete resquest.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_ret_resp(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;

	cmd = sc->rtsx_req->cmd;
	rtsx_set_resp(sc, cmd);
	rtsx_req_done(sc);
}

/*
 * Set response of previous command.
 */
static void
rtsx_set_resp(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	uint8_t	 rsp_type;

	rsp_type = rtsx_response_type(cmd->flags & MMC_RSP_MASK);

	/* Sync command DMA buffer. */
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTWRITE);

	/* Copy card response into mmc response buffer. */
	if (ISSET(cmd->flags, MMC_RSP_PRESENT)) {
		uint32_t *cmd_buffer = (uint32_t *)(sc->rtsx_cmd_dmamem);

		if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD) {
			device_printf(sc->rtsx_dev, "cmd_buffer: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				      cmd_buffer[0], cmd_buffer[1], cmd_buffer[2], cmd_buffer[3], cmd_buffer[4]);
		}

		if (rsp_type == RTSX_SD_RSP_TYPE_R2) {
			/* First byte is CHECK_REG_CMD return value, skip it. */
			unsigned char *ptr = (unsigned char *)cmd_buffer + 1;
			int i;

			/*
			 * The controller offloads the last byte {CRC-7, end bit 1}
			 * of response type R2. Assign dummy CRC, 0, and end bit to this
			 * byte (ptr[16], goes into the LSB of resp[3] later).
			 */
			ptr[16] = 0x01;
			/* The second byte is the status of response, skip it. */
			for (i = 0; i < 4; i++)
				cmd->resp[i] = be32dec(ptr + 1 + i * 4);
		} else {
			/*
			 * First byte is CHECK_REG_CMD return value, second
			 * one is the command op code -- we skip those.
			 */
			cmd->resp[0] =
				((be32toh(cmd_buffer[0]) & 0x0000ffff) << 16) |
				((be32toh(cmd_buffer[1]) & 0xffff0000) >> 16);
		}

		if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
			device_printf(sc->rtsx_dev, "cmd->resp: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				      cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);
	}
}

/*
 * Use the ping-pong buffer (cmd buffer) for transfer <= 512 bytes.
 */
static int
rtsx_xfer_short(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	int	read;

	if (cmd->data == NULL || cmd->data->len == 0) {
		cmd->error = MMC_ERR_INVALID;
		return (MMC_ERR_INVALID);
	}
	cmd->data->xfer_len = (cmd->data->len > RTSX_MAX_DATA_BLKLEN) ?
		RTSX_MAX_DATA_BLKLEN : cmd->data->len;

	read = ISSET(cmd->data->flags, MMC_DATA_READ);

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_xfer_short() - %s xfer: %ld bytes with block size %ld\n",
			      read ? "Read" : "Write",
			      (unsigned long)cmd->data->len, (unsigned long)cmd->data->xfer_len);

	if (cmd->data->len > 512) {
		device_printf(sc->rtsx_dev, "rtsx_xfer_short() - length too large: %ld > 512\n",
			      (unsigned long)cmd->data->len);
		cmd->error = MMC_ERR_INVALID;
		return (MMC_ERR_INVALID);
	}

	if (read) {
		if (sc->rtsx_discovery_mode)
			rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, RTSX_CLK_DIVIDE_0);

		rtsx_init_cmd(sc, cmd);

		/* Queue commands to configure data transfer size. */
		rtsx_set_cmd_data_len(sc, cmd->data->len / cmd->data->xfer_len, cmd->data->xfer_len);

		/* From Linux: rtsx_pci_sdmmc.c sd_read_data(). */
		rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2, 0xff,
			      RTSX_SD_CALCULATE_CRC7 | RTSX_SD_CHECK_CRC16 |
			      RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_CHECK_CRC7 | RTSX_SD_RSP_LEN_6);

		/* Use the ping-pong buffer (cmd buffer). */
		rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_CARD_DATA_SOURCE,
			      0x01, RTSX_PINGPONG_BUFFER);

		/* Queue commands to perform SD transfer. */
		rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
			      0xff, RTSX_TM_NORMAL_READ | RTSX_SD_TRANSFER_START);
		rtsx_push_cmd(sc, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
			      RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

		/* Set transfer OK function. */
		sc->rtsx_intr_trans_ok = rtsx_ask_ppbuf_part1;

		/* Run the command queue. */
		rtsx_send_cmd(sc);
	} else {
		/* Set transfer OK function. */
		sc->rtsx_intr_trans_ok = rtsx_put_ppbuf_part1;

		/* Run the command queue. */
		rtsx_send_req(sc, cmd);
	}

	return (0);
}

/*
 * Use the ping-pong buffer (cmd buffer) for the transfer - first part <= 256 bytes.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_ask_ppbuf_part1(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	uint16_t reg = RTSX_PPBUF_BASE2;
	int	 len;
	int	 i;

	cmd = sc->rtsx_req->cmd;
	len = (cmd->data->len > RTSX_HOSTCMD_MAX) ? RTSX_HOSTCMD_MAX : cmd->data->len;

	sc->rtsx_cmd_index = 0;
	for (i = 0; i < len; i++) {
		rtsx_push_cmd(sc, RTSX_READ_REG_CMD, reg++, 0, 0);
	}

	/* Set transfer OK function. */
	sc->rtsx_intr_trans_ok = rtsx_get_ppbuf_part1;

	/* Run the command queue. */
	rtsx_send_cmd(sc);
}

/*
 * Get the data from the ping-pong buffer (cmd buffer) - first part <= 256 bytes.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_get_ppbuf_part1(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	uint8_t	 *ptr;
	int	 len;

	cmd = sc->rtsx_req->cmd;
	ptr = cmd->data->data;
	len = (cmd->data->len > RTSX_HOSTCMD_MAX) ? RTSX_HOSTCMD_MAX : cmd->data->len;

	/* Sync command DMA buffer. */
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTWRITE);

	memcpy(ptr, sc->rtsx_cmd_dmamem, len);

	len = (cmd->data->len > RTSX_HOSTCMD_MAX) ? cmd->data->len - RTSX_HOSTCMD_MAX : 0;

	/* Use the ping-pong buffer (cmd buffer) for the transfer - second part > 256 bytes. */
	if (len > 0) {
		uint16_t reg = RTSX_PPBUF_BASE2 + RTSX_HOSTCMD_MAX;
		int	 i;

		sc->rtsx_cmd_index = 0;
		for (i = 0; i < len; i++) {
			rtsx_push_cmd(sc, RTSX_READ_REG_CMD, reg++, 0, 0);
		}

		/* Set transfer OK function. */
		sc->rtsx_intr_trans_ok = rtsx_get_ppbuf_part2;

		/* Run the command queue. */
		rtsx_send_cmd(sc);
	} else {
		if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD && cmd->opcode == ACMD_SEND_SCR) {
			uint8_t *ptr = cmd->data->data;
			device_printf(sc->rtsx_dev, "SCR: 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				      ptr[0], ptr[1], ptr[2], ptr[3],
				      ptr[4], ptr[5], ptr[6], ptr[7]);
		}

		if (sc->rtsx_discovery_mode)
			rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, RTSX_CLK_DIVIDE_128);

		rtsx_req_done(sc);
	}
}

/*
 * Get the data from the ping-pong buffer (cmd buffer) - second part > 256 bytes.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_get_ppbuf_part2(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	uint8_t	*ptr;
	int	len;

	cmd = sc->rtsx_req->cmd;
	ptr = cmd->data->data;
	ptr += RTSX_HOSTCMD_MAX;
	len = cmd->data->len - RTSX_HOSTCMD_MAX;

	/* Sync command DMA buffer. */
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->rtsx_cmd_dma_tag, sc->rtsx_cmd_dmamap, BUS_DMASYNC_POSTWRITE);

	memcpy(ptr, sc->rtsx_cmd_dmamem, len);

	if (sc->rtsx_discovery_mode)
		rtsx_write(sc, RTSX_SD_CFG1, RTSX_CLK_DIVIDE_MASK, RTSX_CLK_DIVIDE_128);

	rtsx_req_done(sc);
}

/*
 * Use the ping-pong buffer (cmd buffer) for transfer - first part <= 256 bytes.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_put_ppbuf_part1(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	uint16_t reg = RTSX_PPBUF_BASE2;
	uint8_t	 *ptr;
	int	 len;
	int	 i;

	cmd = sc->rtsx_req->cmd;
	ptr = cmd->data->data;
	len = (cmd->data->len > RTSX_HOSTCMD_MAX) ? RTSX_HOSTCMD_MAX : cmd->data->len;

	rtsx_set_resp(sc, cmd);

	sc->rtsx_cmd_index = 0;
	for (i = 0; i < len; i++) {
		rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, reg++, 0xff, *ptr);
		ptr++;
	}

	/* Set transfer OK function. */
	if (cmd->data->len > RTSX_HOSTCMD_MAX)
		sc->rtsx_intr_trans_ok = rtsx_put_ppbuf_part2;
	else
		sc->rtsx_intr_trans_ok = rtsx_write_ppbuf;

	/* Run the command queue. */
	rtsx_send_cmd(sc);
}

/*
 * Use the ping-pong buffer (cmd buffer) for transfer - second part > 256 bytes.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_put_ppbuf_part2(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	uint16_t reg = RTSX_PPBUF_BASE2 + RTSX_HOSTCMD_MAX;
	uint8_t	 *ptr;
	int	 len;
	int	 i;

	cmd = sc->rtsx_req->cmd;
	ptr = cmd->data->data;
	ptr += RTSX_HOSTCMD_MAX;
	len = cmd->data->len - RTSX_HOSTCMD_MAX;

	sc->rtsx_cmd_index = 0;
	for (i = 0; i < len; i++) {
		rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, reg++, 0xff, *ptr);
		ptr++;
	}

	/* Set transfer OK function. */
	sc->rtsx_intr_trans_ok = rtsx_write_ppbuf;

	/* Run the command queue. */
	rtsx_send_cmd(sc);
}

/*
 * Write the data previously given via the ping-pong buffer on the card.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_write_ppbuf(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;

	cmd = sc->rtsx_req->cmd;

	sc->rtsx_cmd_index = 0;

	/* Queue commands to configure data transfer size. */
	rtsx_set_cmd_data_len(sc, cmd->data->len / cmd->data->xfer_len, cmd->data->xfer_len);

	/* From Linux: rtsx_pci_sdmmc.c sd_write_data(). */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2, 0xff,
		      RTSX_SD_CALCULATE_CRC7 | RTSX_SD_CHECK_CRC16 |
		      RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_CHECK_CRC7 | RTSX_SD_RSP_LEN_0);

	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER, 0xff,
		      RTSX_TM_AUTO_WRITE3 | RTSX_SD_TRANSFER_START);
	rtsx_push_cmd(sc, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
		      RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

	/* Set transfer OK function. */
	sc->rtsx_intr_trans_ok = rtsx_req_done;

	/* Run the command queue. */
	rtsx_send_cmd(sc);
}

/*
 * Use the data buffer for transfer > 512 bytes.
 */
static int
rtsx_xfer(struct rtsx_softc *sc, struct mmc_command *cmd)
{
	int	read = ISSET(cmd->data->flags, MMC_DATA_READ);

	cmd->data->xfer_len = (cmd->data->len > RTSX_MAX_DATA_BLKLEN) ?
		RTSX_MAX_DATA_BLKLEN : cmd->data->len;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_xfer() - %s xfer: %ld bytes with block size %ld\n",
			      read ? "Read" : "Write",
			      (unsigned long)cmd->data->len, (unsigned long)cmd->data->xfer_len);

	if (cmd->data->len > RTSX_DMA_DATA_BUFSIZE) {
		device_printf(sc->rtsx_dev, "rtsx_xfer() length too large: %ld > %d\n",
			      (unsigned long)cmd->data->len, RTSX_DMA_DATA_BUFSIZE);
		cmd->error = MMC_ERR_INVALID;
		return (MMC_ERR_INVALID);
	}

	if (!read) {
		/* Set transfer OK function. */
		sc->rtsx_intr_trans_ok = rtsx_xfer_begin;

		/* Run the command queue. */
		rtsx_send_req(sc, cmd);
	} else {
		rtsx_xfer_start(sc);
	}

	return (0);
}

/*
 * Get request response and start dma data transfer (write command).
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_xfer_begin(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;

	cmd = sc->rtsx_req->cmd;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_xfer_begin() - CMD%d\n", cmd->opcode);

	rtsx_set_resp(sc, cmd);
	rtsx_xfer_start(sc);
}

/*
 * Start dma data transfer.
 */
static void
rtsx_xfer_start(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	int	read;
	uint8_t	cfg2;
	int	dma_dir;
	int	tmode;

	cmd = sc->rtsx_req->cmd;
	read = ISSET(cmd->data->flags, MMC_DATA_READ);

	/* Configure DMA transfer mode parameters. */
	if (cmd->opcode == MMC_READ_MULTIPLE_BLOCK)
		cfg2 = RTSX_SD_CHECK_CRC16 | RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_RSP_LEN_6;
	else
		cfg2 = RTSX_SD_CHECK_CRC16 | RTSX_SD_NO_WAIT_BUSY_END | RTSX_SD_RSP_LEN_0;
	if (read) {
		dma_dir = RTSX_DMA_DIR_FROM_CARD;
		/*
		 * Use transfer mode AUTO_READ1, which assume we not
		 * already send the read command and don't need to send
		 * CMD 12 manually after read.
		 */
		tmode = RTSX_TM_AUTO_READ1;
		cfg2 |= RTSX_SD_CALCULATE_CRC7 | RTSX_SD_CHECK_CRC7;

		rtsx_init_cmd(sc, cmd);
	} else {
		dma_dir = RTSX_DMA_DIR_TO_CARD;
		/*
		 * Use transfer mode AUTO_WRITE3, wich assumes we've already
		 * sent the write command and gotten the response, and will
		 * send CMD 12 manually after writing.
		 */
		tmode = RTSX_TM_AUTO_WRITE3;
		cfg2 |= RTSX_SD_NO_CALCULATE_CRC7 | RTSX_SD_NO_CHECK_CRC7;

		sc->rtsx_cmd_index = 0;
	}

	/* Queue commands to configure data transfer size. */
	rtsx_set_cmd_data_len(sc, cmd->data->len / cmd->data->xfer_len, cmd->data->xfer_len);

	/* Configure DMA controller. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_IRQSTAT0,
		     RTSX_DMA_DONE_INT, RTSX_DMA_DONE_INT);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_DMATC3,
		     0xff, cmd->data->len >> 24);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_DMATC2,
		     0xff, cmd->data->len >> 16);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_DMATC1,
		     0xff, cmd->data->len >> 8);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_DMATC0,
		     0xff, cmd->data->len);
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_DMACTL,
		     RTSX_DMA_EN | RTSX_DMA_DIR | RTSX_DMA_PACK_SIZE_MASK,
		     RTSX_DMA_EN | dma_dir | RTSX_DMA_512);

	/* Use the DMA ring buffer for commands which transfer data. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_CARD_DATA_SOURCE,
		      0x01, RTSX_RING_BUFFER);

	/* Queue command to set response type. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_CFG2, 0xff, cfg2);

	/* Queue commands to perform SD transfer. */
	rtsx_push_cmd(sc, RTSX_WRITE_REG_CMD, RTSX_SD_TRANSFER,
		      0xff, tmode | RTSX_SD_TRANSFER_START);
	rtsx_push_cmd(sc, RTSX_CHECK_REG_CMD, RTSX_SD_TRANSFER,
		      RTSX_SD_TRANSFER_END, RTSX_SD_TRANSFER_END);

	/* Run the command queue. */
	rtsx_send_cmd(sc);

	if (!read)
		memcpy(sc->rtsx_data_dmamem, cmd->data->data, cmd->data->len);

	/* Sync data DMA buffer. */
	bus_dmamap_sync(sc->rtsx_data_dma_tag, sc->rtsx_data_dmamap, BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->rtsx_data_dma_tag, sc->rtsx_data_dmamap, BUS_DMASYNC_PREWRITE);

	/* Set transfer OK function. */
	sc->rtsx_intr_trans_ok = rtsx_xfer_finish;

	/* Tell the chip where the data buffer is and run the transfer. */
	WRITE4(sc, RTSX_HDBAR, sc->rtsx_data_buffer);
	WRITE4(sc, RTSX_HDBCTLR, RTSX_TRIG_DMA | (read ? RTSX_DMA_READ : 0) |
	       (cmd->data->len & 0x00ffffff));
}

/*
 * Finish dma data transfer.
 * This Function is called by the interrupt handler via sc->rtsx_intr_trans_ok.
 */
static void
rtsx_xfer_finish(struct rtsx_softc *sc)
{
	struct mmc_command *cmd;
	int	read;

	cmd = sc->rtsx_req->cmd;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_xfer_finish() - CMD%d\n", cmd->opcode);

	read = ISSET(cmd->data->flags, MMC_DATA_READ);

	/* Sync data DMA buffer. */
	bus_dmamap_sync(sc->rtsx_data_dma_tag, sc->rtsx_data_dmamap, BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->rtsx_data_dma_tag, sc->rtsx_data_dmamap, BUS_DMASYNC_POSTWRITE);

	if (read) {
		memcpy(cmd->data->data, sc->rtsx_data_dmamem, cmd->data->len);
		rtsx_req_done(sc);
	} else {
		/* Send CMD12 after AUTO_WRITE3 (see mmcsd_rw() in mmcsd.c) */
		/* and complete request. */
		sc->rtsx_intr_trans_ok = NULL;
		rtsx_send_req(sc, sc->rtsx_req->stop);
	}
}

/*
 * Manage request timeout.
 */
static void
rtsx_timeout(void *arg)
{
	struct rtsx_softc *sc;

	sc = (struct rtsx_softc *)arg;
	if (sc->rtsx_req != NULL) {
		device_printf(sc->rtsx_dev, "Controller timeout for CMD%u\n",
			      sc->rtsx_req->cmd->opcode);
		sc->rtsx_req->cmd->error = MMC_ERR_TIMEOUT;
		rtsx_stop_cmd(sc);
		rtsx_req_done(sc);
	} else {
		device_printf(sc->rtsx_dev, "Controller timeout!\n");
	}
}

#ifdef MMCCAM
static int
rtsx_get_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(dev);

	cts->host_ocr = sc->rtsx_host.host_ocr;
	cts->host_f_min = sc->rtsx_host.f_min;
	cts->host_f_max = sc->rtsx_host.f_max;
	cts->host_caps = sc->rtsx_host.caps;
	cts->host_max_data = RTSX_DMA_DATA_BUFSIZE / MMC_SECTOR_SIZE;
	memcpy(&cts->ios, &sc->rtsx_host.ios, sizeof(struct mmc_ios));

	return (0);
}

/*
 *  Apply settings and return status accordingly.
*/
static int
rtsx_set_tran_settings(device_t dev, struct ccb_trans_settings_mmc *cts)
{
	struct rtsx_softc *sc;
	struct mmc_ios *ios;
	struct mmc_ios *new_ios;

	sc = device_get_softc(dev);

	ios = &sc->rtsx_host.ios;
	new_ios = &cts->ios;

	/* Update only requested fields */
	if (cts->ios_valid & MMC_CLK) {
		ios->clock = new_ios->clock;
		sc->rtsx_ios_clock = -1;	/* To be updated by rtsx_mmcbr_update_ios(). */
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - clock: %u\n", ios->clock);
	}
	if (cts->ios_valid & MMC_VDD) {
		ios->vdd = new_ios->vdd;
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - vdd: %d\n", ios->vdd);
	}
	if (cts->ios_valid & MMC_CS) {
		ios->chip_select = new_ios->chip_select;
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - chip_select: %d\n", ios->chip_select);
	}
	if (cts->ios_valid & MMC_BW) {
		ios->bus_width = new_ios->bus_width;
		sc->rtsx_ios_bus_width = -1;	/* To be updated by rtsx_mmcbr_update_ios(). */
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - bus width: %d\n", ios->bus_width);
	}
	if (cts->ios_valid & MMC_PM) {
		ios->power_mode = new_ios->power_mode;
		sc->rtsx_ios_power_mode = -1;	/* To be updated by rtsx_mmcbr_update_ios(). */
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - power mode: %d\n", ios->power_mode);
	}
	if (cts->ios_valid & MMC_BT) {
		ios->timing = new_ios->timing;
		sc->rtsx_ios_timing = -1;	/* To be updated by rtsx_mmcbr_update_ios(). */
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - timing: %d\n", ios->timing);
	}
	if (cts->ios_valid & MMC_BM) {
		ios->bus_mode = new_ios->bus_mode;
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - bus mode: %d\n", ios->bus_mode);
	}
#if  __FreeBSD_version >= 1300000
	if (cts->ios_valid & MMC_VCCQ) {
		ios->vccq = new_ios->vccq;
		sc->rtsx_ios_vccq = -1;		/* To be updated by rtsx_mmcbr_update_ios(). */
		if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
			device_printf(sc->rtsx_dev, "rtsx_set_tran_settings() - vccq: %d\n", ios->vccq);
	}
#endif /* __FreeBSD_version >= 1300000 */
	if (rtsx_mmcbr_update_ios(sc->rtsx_dev, NULL) == 0)
		return (CAM_REQ_CMP);
	else
		return (CAM_REQ_CMP_ERR);
}

/*
 * Build a request and run it.
 */
static int
rtsx_cam_request(device_t dev, union ccb *ccb)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(dev);

	RTSX_LOCK(sc);
	if (sc->rtsx_ccb != NULL) {
		RTSX_UNLOCK(sc);
		return (CAM_BUSY);
	}
	sc->rtsx_ccb = ccb;
	sc->rtsx_cam_req.cmd = &ccb->mmcio.cmd;
	sc->rtsx_cam_req.stop = &ccb->mmcio.stop;
	RTSX_UNLOCK(sc);

	rtsx_mmcbr_request(sc->rtsx_dev, NULL, &sc->rtsx_cam_req);
	return (0);
}
#endif /* MMCCAM */

static int
rtsx_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);
	switch (which) {
	case MMCBR_IVAR_BUS_MODE:		/* ivar  0 - 1 = opendrain, 2 = pushpull */
		*result = sc->rtsx_host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:		/* ivar  1 - 0 = 1b   2 = 4b, 3 = 8b */
		*result = sc->rtsx_host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:		/* ivar  2 - O = dontcare, 1 = cs_high, 2 = cs_low */
		*result = sc->rtsx_host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:			/* ivar  3 - clock in Hz */
		*result = sc->rtsx_host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:			/* ivar  4 */
		*result = sc->rtsx_host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:			/* ivar  5 */
		*result = sc->rtsx_host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR: 		/* ivar  6 - host operation conditions register */
		*result = sc->rtsx_host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:			/* ivar  7 - 0 = mode_mmc, 1 = mode_sd */
		*result = sc->rtsx_host.mode;
		break;
	case MMCBR_IVAR_OCR:			/* ivar  8 - operation conditions register */
		*result = sc->rtsx_host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:		/* ivar  9 - 0 = off, 1 = up, 2 = on */
		*result = sc->rtsx_host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:			/* ivar 11 - voltage power pin */
		*result = sc->rtsx_host.ios.vdd;
		break;
	case MMCBR_IVAR_VCCQ:			/* ivar 12 - signaling: 0 = 1.20V, 1 = 1.80V, 2 = 3.30V */
		*result = sc->rtsx_host.ios.vccq;
		break;
	case MMCBR_IVAR_CAPS:			/* ivar 13 */
		*result = sc->rtsx_host.caps;
		break;
	case MMCBR_IVAR_TIMING:			/* ivar 14 - 0 = normal, 1 = timing_hs, ... */
		*result = sc->rtsx_host.ios.timing;
		break;
	case MMCBR_IVAR_MAX_DATA:		/* ivar 15 */
		*result = RTSX_DMA_DATA_BUFSIZE / MMC_SECTOR_SIZE;
		break;
	case MMCBR_IVAR_RETUNE_REQ:		/* ivar 10 */
	case MMCBR_IVAR_MAX_BUSY_TIMEOUT:	/* ivar 16 */
	default:
		return (EINVAL);
	}

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(bus, "Read ivar #%d, value %#x / #%d\n",
			      which, *(int *)result, *(int *)result);

	return (0);
}

static int
rtsx_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);
	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(bus, "Write ivar #%d, value %#x / #%d\n",
			      which, (int)value, (int)value);

	switch (which) {
	case MMCBR_IVAR_BUS_MODE:		/* ivar  0 - 1 = opendrain, 2 = pushpull */
		sc->rtsx_host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:		/* ivar  1 - 0 = 1b   2 = 4b, 3 = 8b */
		sc->rtsx_host.ios.bus_width = value;
		sc->rtsx_ios_bus_width = -1;	/* To be updated on next rtsx_mmcbr_update_ios(). */
		break;
	case MMCBR_IVAR_CHIP_SELECT:		/* ivar  2 - O = dontcare, 1 = cs_high, 2 = cs_low */
		sc->rtsx_host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:			/* ivar  3 - clock in Hz */
		sc->rtsx_host.ios.clock = value;
		sc->rtsx_ios_clock = -1;	/* To be updated on next rtsx_mmcbr_update_ios(). */
		break;
	case MMCBR_IVAR_MODE:			/* ivar  7 - 0 = mode_mmc, 1 = mode_sd */
		sc->rtsx_host.mode = value;
		break;
	case MMCBR_IVAR_OCR:			/* ivar  8 - operation conditions register */
		sc->rtsx_host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:		/* ivar  9 - 0 = off, 1 = up, 2 = on */
		sc->rtsx_host.ios.power_mode = value;
		sc->rtsx_ios_power_mode = -1;	/* To be updated on next rtsx_mmcbr_update_ios(). */
		break;
	case MMCBR_IVAR_VDD:			/* ivar 11 - voltage power pin */
		sc->rtsx_host.ios.vdd = value;
		break;
	case MMCBR_IVAR_VCCQ:			/* ivar 12 - signaling: 0 = 1.20V, 1 = 1.80V, 2 = 3.30V */
		sc->rtsx_host.ios.vccq = value;
		sc->rtsx_ios_vccq = value;	/* rtsx_mmcbr_switch_vccq() will be called by mmc.c (MMCCAM undef). */
		break;
	case MMCBR_IVAR_TIMING:			/* ivar 14 - 0 = normal, 1 = timing_hs, ... */
		sc->rtsx_host.ios.timing = value;
		sc->rtsx_ios_timing = -1;	/* To be updated on next rtsx_mmcbr_update_ios(). */
		break;
	/* These are read-only. */
	case MMCBR_IVAR_F_MIN:			/* ivar  4 */
	case MMCBR_IVAR_F_MAX:			/* ivar  5 */
	case MMCBR_IVAR_HOST_OCR: 		/* ivar  6 - host operation conditions register */
	case MMCBR_IVAR_RETUNE_REQ:		/* ivar 10 */
	case MMCBR_IVAR_CAPS:			/* ivar 13 */
	case MMCBR_IVAR_MAX_DATA:		/* ivar 15 */
	case MMCBR_IVAR_MAX_BUSY_TIMEOUT:	/* ivar 16 */
	default:
		return (EINVAL);
	}

	return (0);
}

static int
rtsx_mmcbr_update_ios(device_t bus, device_t child__unused)
{
	struct rtsx_softc *sc;
	struct mmc_ios	  *ios;
	int	error;

	sc = device_get_softc(bus);
	ios = &sc->rtsx_host.ios;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(bus, "rtsx_mmcbr_update_ios()\n");

	/* if MMCBR_IVAR_BUS_WIDTH updated. */
	if (sc->rtsx_ios_bus_width < 0) {
		sc->rtsx_ios_bus_width = ios->bus_width;
		if ((error = rtsx_set_bus_width(sc, ios->bus_width)))
			return (error);
	}

	/* if MMCBR_IVAR_POWER_MODE updated. */
	if (sc->rtsx_ios_power_mode < 0) {
		sc->rtsx_ios_power_mode = ios->power_mode;
		switch (ios->power_mode) {
		case power_off:
			if ((error = rtsx_bus_power_off(sc)))
				return (error);
			break;
		case power_up:
			if ((error = rtsx_bus_power_on(sc)))
				return (error);
			break;
		case power_on:
			if ((error = rtsx_bus_power_on(sc)))
				return (error);
			break;
		}
	}

	sc->rtsx_double_clk = true;
	sc->rtsx_vpclk = false;

	/* if MMCBR_IVAR_TIMING updated. */
	if (sc->rtsx_ios_timing < 0) {
		sc->rtsx_ios_timing = ios->timing;
		if ((error = rtsx_set_sd_timing(sc, ios->timing)))
			return (error);
	}

	/* if MMCBR_IVAR_CLOCK updated, must be after rtsx_set_sd_timing() */
	if (sc->rtsx_ios_clock < 0) {
		sc->rtsx_ios_clock = ios->clock;
		if ((error = rtsx_set_sd_clock(sc, ios->clock)))
			return (error);
	}

	/* if MMCCAM and vccq updated */
	if (sc->rtsx_ios_vccq < 0) {
		sc->rtsx_ios_vccq = ios->vccq;
		if ((error = rtsx_mmcbr_switch_vccq(sc->rtsx_dev, NULL)))
			return (error);
	}

	return (0);
}

/*
 * Set output stage logic power voltage.
 */
static int
rtsx_mmcbr_switch_vccq(device_t bus, device_t child __unused)
{
	struct rtsx_softc *sc;
	int	vccq = 0;
	int	error;

	sc = device_get_softc(bus);

	switch (sc->rtsx_host.ios.vccq) {
	case vccq_120:
		vccq = 120;
		break;
	case vccq_180:
		vccq = 180;
		break;
	case vccq_330:
		vccq = 330;
		break;
	};
	/* It seems it is always vccq_330. */
	if (vccq == 330) {
		switch (sc->rtsx_device_id) {
			uint16_t val;
		case RTSX_RTS5227:
			if ((error = rtsx_write_phy(sc, 0x08, 0x4FE4)))
				return (error);
			if ((error = rtsx_rts5227_fill_driving(sc)))
				return (error);
			break;
		case RTSX_RTS5209:
		case RTSX_RTS5229:
			RTSX_BITOP(sc, RTSX_SD30_CMD_DRIVE_SEL, RTSX_SD30_DRIVE_SEL_MASK, sc->rtsx_sd30_drive_sel_3v3);
			if ((error = rtsx_write_phy(sc, 0x08, 0x4FE4)))
				return (error);
			break;
		case RTSX_RTS522A:
			if ((error = rtsx_write_phy(sc, 0x08, 0x57E4)))
				return (error);
			if ((error = rtsx_rts5227_fill_driving(sc)))
				return (error);
			break;
		case RTSX_RTS525A:
			RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_LDO_D3318_MASK, RTSX_LDO_D3318_33V);
			RTSX_BITOP(sc, RTSX_SD_PAD_CTL, RTSX_SD_IO_USING_1V8, 0);
			if ((error = rtsx_rts5249_fill_driving(sc)))
				return (error);
			break;
		case RTSX_RTS5249:
			if ((error = rtsx_read_phy(sc, RTSX_PHY_TUNE, &val)))
				return (error);
			if ((error = rtsx_write_phy(sc, RTSX_PHY_TUNE,
						    (val & RTSX_PHY_TUNE_VOLTAGE_MASK) | RTSX_PHY_TUNE_VOLTAGE_3V3)))
				return (error);
			if ((error = rtsx_rts5249_fill_driving(sc)))
				return (error);
			break;
		case RTSX_RTS5260:
			RTSX_BITOP(sc, RTSX_LDO_CONFIG2, RTSX_DV331812_VDD1, RTSX_DV331812_VDD1);
			RTSX_BITOP(sc, RTSX_LDO_DV18_CFG, RTSX_DV331812_MASK, RTSX_DV331812_33);
			RTSX_CLR(sc, RTSX_SD_PAD_CTL, RTSX_SD_IO_USING_1V8);
			if ((error = rtsx_rts5260_fill_driving(sc)))
				return (error);
			break;
		case RTSX_RTL8402:
			RTSX_BITOP(sc, RTSX_SD30_CMD_DRIVE_SEL, RTSX_SD30_DRIVE_SEL_MASK, sc->rtsx_sd30_drive_sel_3v3);
			RTSX_BITOP(sc, RTSX_LDO_CTL,
				   (RTSX_BPP_ASIC_MASK << RTSX_BPP_SHIFT_8402) | RTSX_BPP_PAD_MASK,
				   (RTSX_BPP_ASIC_3V3 << RTSX_BPP_SHIFT_8402) | RTSX_BPP_PAD_3V3);
			break;
		case RTSX_RTL8411:
		case RTSX_RTL8411B:
			RTSX_BITOP(sc, RTSX_SD30_CMD_DRIVE_SEL, RTSX_SD30_DRIVE_SEL_MASK, sc->rtsx_sd30_drive_sel_3v3);
			RTSX_BITOP(sc, RTSX_LDO_CTL,
				   (RTSX_BPP_ASIC_MASK << RTSX_BPP_SHIFT_8411) | RTSX_BPP_PAD_MASK,
				   (RTSX_BPP_ASIC_3V3 << RTSX_BPP_SHIFT_8411) | RTSX_BPP_PAD_3V3);
			break;
		}
		DELAY(300);
	}

	if (sc->rtsx_debug_mask & (RTSX_DEBUG_BASIC | RTSX_TRACE_SD_CMD))
		device_printf(sc->rtsx_dev, "rtsx_mmcbr_switch_vccq(%d)\n", vccq);

	return (0);
}

#ifndef MMCCAM
/*
 * Tune card if bus_timing_uhs_sdr50.
 */
static int
rtsx_mmcbr_tune(device_t bus, device_t child __unused, bool hs400)
{
	struct rtsx_softc *sc;
	uint32_t raw_phase_map[RTSX_RX_TUNING_CNT] = {0};
	uint32_t phase_map;
	uint8_t	 final_phase;
	int	 i;

	sc = device_get_softc(bus);

	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_mmcbr_tune() - hs400 is %s\n",
			      (hs400) ? "true" : "false");

	if (sc->rtsx_ios_timing != bus_timing_uhs_sdr50)
		return (0);

	sc->rtsx_tuning_mode = true;

	switch (sc->rtsx_device_id) {
	case RTSX_RTS5209:
	case RTSX_RTS5227:
		rtsx_sd_change_tx_phase(sc, 27);
		break;
	case RTSX_RTS522A:
		rtsx_sd_change_tx_phase(sc, 20);
		break;
	case RTSX_RTS5229:
		rtsx_sd_change_tx_phase(sc, 27);
		break;
	case RTSX_RTS525A:
	case RTSX_RTS5249:
		rtsx_sd_change_tx_phase(sc, 29);
		break;
	case RTSX_RTL8402:
	case RTSX_RTL8411:
	case RTSX_RTL8411B:
		rtsx_sd_change_tx_phase(sc, 7);
		break;
	}

	/* trying rx tuning for bus_timing_uhs_sdr50. */
	for (i = 0; i < RTSX_RX_TUNING_CNT; i++) {
		rtsx_sd_tuning_rx_phase(sc, &(raw_phase_map[i]));
		if (raw_phase_map[i] == 0)
			break;
	}

	phase_map = 0xffffffff;
	for (i = 0; i < RTSX_RX_TUNING_CNT; i++) {
		if (sc->rtsx_debug_mask & (RTSX_DEBUG_BASIC | RTSX_DEBUG_TUNING))
			device_printf(sc->rtsx_dev, "rtsx_mmcbr_tune() - RX raw_phase_map[%d]: 0x%08x\n",
				      i, raw_phase_map[i]);
		phase_map &= raw_phase_map[i];
	}
	if (sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(sc->rtsx_dev, "rtsx_mmcbr_tune() - RX phase_map: 0x%08x\n", phase_map);

	if (phase_map) {
		final_phase = rtsx_sd_search_final_rx_phase(sc, phase_map);
		if (final_phase != 0xff) {
			rtsx_sd_change_rx_phase(sc, final_phase);
		}
	}

	sc->rtsx_tuning_mode = false;

	return (0);
}

static int
rtsx_mmcbr_retune(device_t bus, device_t child __unused, bool reset __unused)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_mmcbr_retune()\n");

	return (0);
}
#endif /* !MMCCAM */

static int
rtsx_mmcbr_request(device_t bus, device_t child __unused, struct mmc_request *req)
{
	struct rtsx_softc  *sc;
	struct mmc_command *cmd;
	int	timeout;
	int	error;

	sc = device_get_softc(bus);

	RTSX_LOCK(sc);
	if (sc->rtsx_req != NULL) {
		RTSX_UNLOCK(sc);
		return (EBUSY);
	}
	sc->rtsx_req = req;
	cmd = req->cmd;
	cmd->error = error = MMC_ERR_NONE;
	sc->rtsx_intr_status = 0;
	sc->rtsx_intr_trans_ok = NULL;
	sc->rtsx_intr_trans_ko = rtsx_req_done;

	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(sc->rtsx_dev, "rtsx_mmcbr_request(CMD%u arg %#x, flags %#x, dlen %u, dflags %#x)\n",
			      cmd->opcode, cmd->arg, cmd->flags,
			      cmd->data != NULL ? (unsigned int)cmd->data->len : 0,
			      cmd->data != NULL ? cmd->data->flags : 0);

	/* Check if card present. */
	if (!ISSET(sc->rtsx_flags, RTSX_F_CARD_PRESENT)) {
		cmd->error = error = MMC_ERR_FAILED;
		goto end;
	}

	/* Refuse SDIO probe if the chip doesn't support SDIO. */
	if (cmd->opcode == IO_SEND_OP_COND &&
	    !ISSET(sc->rtsx_flags, RTSX_F_SDIO_SUPPORT)) {
		cmd->error = error = MMC_ERR_INVALID;
		goto end;
	}

	/* Return MMC_ERR_TIMEOUT for SD_IO_RW_DIRECT and IO_SEND_OP_COND. */
	if (cmd->opcode == SD_IO_RW_DIRECT || cmd->opcode == IO_SEND_OP_COND) {
		cmd->error = error = MMC_ERR_TIMEOUT;
		goto end;
	}

	/* Select SD card. */
	RTSX_BITOP(sc, RTSX_CARD_SELECT, 0x07, RTSX_SD_MOD_SEL);
	RTSX_BITOP(sc, RTSX_CARD_SHARE_MODE, RTSX_CARD_SHARE_MASK, RTSX_CARD_SHARE_48_SD);

	if (cmd->data == NULL) {
		DELAY(200);
		timeout = sc->rtsx_timeout_cmd;
		error = rtsx_send_req(sc, cmd);
	} else if (cmd->data->len <= 512) {
		timeout = sc->rtsx_timeout_io;
		error = rtsx_xfer_short(sc, cmd);
	} else {
		timeout = sc->rtsx_timeout_io;
		error = rtsx_xfer(sc, cmd);
	}
 end:
	if (error == MMC_ERR_NONE) {
		callout_reset(&sc->rtsx_timeout_callout, timeout * hz, rtsx_timeout, sc);
	} else {
		rtsx_req_done(sc);
	}
	RTSX_UNLOCK(sc);

	return (error);
}

#ifndef MMCCAM
static int
rtsx_mmcbr_get_ro(device_t bus, device_t child __unused)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);

	if (sc->rtsx_inversion == 0)
		return (sc->rtsx_read_only);
	else
		return !(sc->rtsx_read_only);
}

static int
rtsx_mmcbr_acquire_host(device_t bus, device_t child __unused)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);
	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(bus, "rtsx_mmcbr_acquire_host()\n");

	RTSX_LOCK(sc);
	while (sc->rtsx_bus_busy)
		msleep(&sc->rtsx_bus_busy, &sc->rtsx_mtx, 0, "rtsxah", 0);
	sc->rtsx_bus_busy++;
	RTSX_UNLOCK(sc);

	return (0);
}

static int
rtsx_mmcbr_release_host(device_t bus, device_t child __unused)
{
	struct rtsx_softc *sc;

	sc = device_get_softc(bus);
	if (sc->rtsx_debug_mask & RTSX_TRACE_SD_CMD)
		device_printf(bus, "rtsx_mmcbr_release_host()\n");

	RTSX_LOCK(sc);
	sc->rtsx_bus_busy--;
	wakeup(&sc->rtsx_bus_busy);
	RTSX_UNLOCK(sc);

	return (0);
}
#endif /* !MMCCAM */

/*
 *
 * PCI Support Functions
 *
 */

/*
 * Compare the device ID (chip) of this device against the IDs that this driver
 * supports. If there is a match, set the description and return success.
 */
static int
rtsx_probe(device_t dev)
{
	uint16_t vendor_id;
	uint16_t device_id;
	int	 i;

	vendor_id = pci_get_vendor(dev);
	device_id = pci_get_device(dev);

	if (vendor_id != RTSX_REALTEK)
		return (ENXIO);
	for (i = 0; i < nitems(rtsx_ids); i++) {
		if (rtsx_ids[i].device_id == device_id) {
			device_set_desc(dev, rtsx_ids[i].desc);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

/*
 * Attach function is only called if the probe is successful.
 */
static int
rtsx_attach(device_t dev)
{
	struct rtsx_softc 	*sc = device_get_softc(dev);
	uint16_t 		vendor_id;
	uint16_t 		device_id;
	struct sysctl_ctx_list	*ctx;
	struct sysctl_oid_list	*tree;
	int			msi_count = 1;
	uint32_t		sdio_cfg;
	int			error;
	char			*maker;
	char			*family;
	char			*product;
	int			i;

	vendor_id = pci_get_vendor(dev);
	device_id = pci_get_device(dev);
	if (bootverbose)
		device_printf(dev, "Attach - Vendor ID: 0x%x - Device ID: 0x%x\n",
			      vendor_id, device_id);

	sc->rtsx_dev = dev;
	sc->rtsx_device_id = device_id;
	sc->rtsx_req = NULL;
	sc->rtsx_timeout_cmd = 1;
	sc->rtsx_timeout_io = 10;
	sc->rtsx_read_only = 0;
	sc->rtsx_inversion = 0;
	sc->rtsx_force_timing = 0;
	sc->rtsx_debug_mask = 0;
	sc->rtsx_read_count = 0;
	sc->rtsx_write_count = 0;

	maker = kern_getenv("smbios.system.maker");
	family = kern_getenv("smbios.system.family");
	product = kern_getenv("smbios.system.product");
	for (i = 0; rtsx_inversion_models[i].maker != NULL; i++) {
		if (strcmp(rtsx_inversion_models[i].maker, maker) == 0 &&
		    strcmp(rtsx_inversion_models[i].family, family) == 0 &&
		    strcmp(rtsx_inversion_models[i].product, product) == 0) {
			device_printf(dev, "Inversion activated for %s/%s/%s, see BUG in rtsx(4)\n", maker, family, product);
			device_printf(dev, "If a card is detected without an SD card present,"
				      " add dev.rtsx.0.inversion=0 in loader.conf(5)\n");
			sc->rtsx_inversion = 1;
		}
	}

	RTSX_LOCK_INIT(sc);

	ctx = device_get_sysctl_ctx(dev);
	tree = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "timeout_io", CTLFLAG_RW,
		       &sc->rtsx_timeout_io, 0, "Request timeout for I/O commands in seconds");
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "timeout_cmd", CTLFLAG_RW,
		       &sc->rtsx_timeout_cmd, 0, "Request timeout for setup commands in seconds");
	SYSCTL_ADD_U8(ctx, tree, OID_AUTO, "read_only", CTLFLAG_RD,
		      &sc->rtsx_read_only, 0, "Card is write protected");
	SYSCTL_ADD_U8(ctx, tree, OID_AUTO, "inversion", CTLFLAG_RWTUN,
		      &sc->rtsx_inversion, 0, "Inversion of card detection and read only status");
	SYSCTL_ADD_U8(ctx, tree, OID_AUTO, "force_timing", CTLFLAG_RW,
		      &sc->rtsx_force_timing, 0, "Force bus_timing_uhs_sdr50");
	SYSCTL_ADD_U8(ctx, tree, OID_AUTO, "debug_mask", CTLFLAG_RWTUN,
		      &sc->rtsx_debug_mask, 0, "debugging mask, see rtsx(4)");
	SYSCTL_ADD_U64(ctx, tree, OID_AUTO, "read_count", CTLFLAG_RD | CTLFLAG_STATS,
		       &sc->rtsx_read_count, 0, "Count of read operations");
	SYSCTL_ADD_U64(ctx, tree, OID_AUTO, "write_count", CTLFLAG_RD | CTLFLAG_STATS,
		       &sc->rtsx_write_count, 0, "Count of write operations");

	if (bootverbose || sc->rtsx_debug_mask & RTSX_DEBUG_BASIC)
		device_printf(dev, "We are running with inversion: %d\n", sc->rtsx_inversion);

	/* Allocate IRQ. */
	sc->rtsx_irq_res_id = 0;
	if (pci_alloc_msi(dev, &msi_count) == 0)
		sc->rtsx_irq_res_id = 1;
	sc->rtsx_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->rtsx_irq_res_id,
						  RF_ACTIVE | (sc->rtsx_irq_res_id != 0 ? 0 : RF_SHAREABLE));
	if (sc->rtsx_irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ resources for %d\n", sc->rtsx_irq_res_id);
		pci_release_msi(dev);
		return (ENXIO);
	}

	callout_init_mtx(&sc->rtsx_timeout_callout, &sc->rtsx_mtx, 0);

	/* Allocate memory resource. */
	if (sc->rtsx_device_id == RTSX_RTS525A)
		sc->rtsx_mem_res_id = PCIR_BAR(1);
	else
		sc->rtsx_mem_res_id = PCIR_BAR(0);
	sc->rtsx_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rtsx_mem_res_id, RF_ACTIVE);
	if (sc->rtsx_mem_res == NULL) {
		device_printf(dev, "Can't allocate memory resource for %d\n", sc->rtsx_mem_res_id);
		goto destroy_rtsx_irq_res;
	}

	if (bootverbose)
		device_printf(dev, "rtsx_irq_res_id: %d, rtsx_mem_res_id: %d\n",
			      sc->rtsx_irq_res_id, sc->rtsx_mem_res_id);

	sc->rtsx_mem_btag = rman_get_bustag(sc->rtsx_mem_res);
	sc->rtsx_mem_bhandle = rman_get_bushandle(sc->rtsx_mem_res);

	TIMEOUT_TASK_INIT(taskqueue_swi_giant, &sc->rtsx_card_insert_task, 0,
			  rtsx_card_task, sc);
	TASK_INIT(&sc->rtsx_card_remove_task, 0, rtsx_card_task, sc);

	/* Allocate two DMA buffers: a command buffer and a data buffer. */
	error = rtsx_dma_alloc(sc);
	if (error)
		goto destroy_rtsx_irq_res;

	/* Activate the interrupt. */
	error = bus_setup_intr(dev, sc->rtsx_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
			       NULL, rtsx_intr, sc, &sc->rtsx_irq_cookie);
	if (error) {
		device_printf(dev, "Can't set up irq [0x%x]!\n", error);
		goto destroy_rtsx_mem_res;
	}
	pci_enable_busmaster(dev);

	if (rtsx_read_cfg(sc, 0, RTSX_SDIOCFG_REG, &sdio_cfg) == 0) {
		if ((sdio_cfg & RTSX_SDIOCFG_SDIO_ONLY) ||
		    (sdio_cfg & RTSX_SDIOCFG_HAVE_SDIO))
			sc->rtsx_flags |= RTSX_F_SDIO_SUPPORT;
	}

#ifdef MMCCAM
	sc->rtsx_ccb = NULL;
	sc->rtsx_cam_status = 0;

	SYSCTL_ADD_U8(ctx, tree, OID_AUTO, "cam_status", CTLFLAG_RD,
		      &sc->rtsx_cam_status, 0, "driver cam card present");

	if (mmc_cam_sim_alloc(dev, "rtsx_mmc", &sc->rtsx_mmc_sim) != 0) {
		device_printf(dev, "Can't allocate CAM SIM\n");
		goto destroy_rtsx_irq;
	}
#endif /* MMCCAM */

	/* Initialize device. */
	error = rtsx_init(sc);
	if (error) {
		device_printf(dev, "Error %d during rtsx_init()\n", error);
		goto destroy_rtsx_irq;
	}

	/*
	 * Schedule a card detection as we won't get an interrupt
	 * if the card is inserted when we attach. We wait a quarter
	 * of a second to allow for a "spontaneous" interrupt which may
	 * change the card presence state. This delay avoid a panic
	 * on some configuration (e.g. Lenovo T540p).
	 */
	DELAY(250000);
	if (rtsx_is_card_present(sc))
		device_printf(sc->rtsx_dev, "A card is detected\n");
	else
		device_printf(sc->rtsx_dev, "No card is detected\n");
	rtsx_card_task(sc, 0);

	if (bootverbose)
		device_printf(dev, "Device attached\n");

	return (0);

 destroy_rtsx_irq:
	bus_teardown_intr(dev, sc->rtsx_irq_res, sc->rtsx_irq_cookie);
 destroy_rtsx_mem_res:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->rtsx_mem_res_id,
			     sc->rtsx_mem_res);
	rtsx_dma_free(sc);
 destroy_rtsx_irq_res:
	callout_drain(&sc->rtsx_timeout_callout);
	bus_release_resource(dev, SYS_RES_IRQ, sc->rtsx_irq_res_id,
			     sc->rtsx_irq_res);
	pci_release_msi(dev);
	RTSX_LOCK_DESTROY(sc);

	return (ENXIO);
}

static int
rtsx_detach(device_t dev)
{
	struct rtsx_softc *sc = device_get_softc(dev);
	int	error;

	if (bootverbose)
		device_printf(dev, "Detach - Vendor ID: 0x%x - Device ID: 0x%x\n",
			      pci_get_vendor(dev), sc->rtsx_device_id);

	/* Disable interrupts. */
	sc->rtsx_intr_enabled = 0;
	WRITE4(sc, RTSX_BIER, sc->rtsx_intr_enabled);

	/* Stop device. */
	error = device_delete_children(sc->rtsx_dev);
	sc->rtsx_mmc_dev = NULL;
	if (error)
		return (error);

	taskqueue_drain_timeout(taskqueue_swi_giant, &sc->rtsx_card_insert_task);
	taskqueue_drain(taskqueue_swi_giant, &sc->rtsx_card_remove_task);

	/* Teardown the state in our softc created in our attach routine. */
	rtsx_dma_free(sc);
	if (sc->rtsx_mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rtsx_mem_res_id,
				     sc->rtsx_mem_res);
	if (sc->rtsx_irq_cookie != NULL)
		bus_teardown_intr(dev, sc->rtsx_irq_res, sc->rtsx_irq_cookie);
	if (sc->rtsx_irq_res != NULL) {
		callout_drain(&sc->rtsx_timeout_callout);
		bus_release_resource(dev, SYS_RES_IRQ, sc->rtsx_irq_res_id,
				     sc->rtsx_irq_res);
		pci_release_msi(dev);
	}
	RTSX_LOCK_DESTROY(sc);
#ifdef MMCCAM
	mmc_cam_sim_free(&sc->rtsx_mmc_sim);
#endif /* MMCCAM */

	return (0);
}

static int
rtsx_shutdown(device_t dev)
{
	if (bootverbose)
		device_printf(dev, "Shutdown\n");

	return (0);
}

/*
 * Device suspend routine.
 */
static int
rtsx_suspend(device_t dev)
{
	struct rtsx_softc *sc = device_get_softc(dev);

	device_printf(dev, "Suspend\n");

#ifdef MMCCAM
	if (sc->rtsx_ccb != NULL) {
		device_printf(dev, "Request in progress: CMD%u, rtsr_intr_status: 0x%08x\n",
			      sc->rtsx_ccb->mmcio.cmd.opcode, sc->rtsx_intr_status);
	}
#else  /* !MMCCAM */
	if (sc->rtsx_req != NULL) {
		device_printf(dev, "Request in progress: CMD%u, rtsr_intr_status: 0x%08x\n",
			      sc->rtsx_req->cmd->opcode, sc->rtsx_intr_status);
	}
#endif /* MMCCAM */

	bus_generic_suspend(dev);

	return (0);
}

/*
 * Device resume routine.
 */
static int
rtsx_resume(device_t dev)
{
	device_printf(dev, "Resume\n");

	rtsx_init(device_get_softc(dev));

	bus_generic_resume(dev);

	return (0);
}

static device_method_t rtsx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rtsx_probe),
	DEVMETHOD(device_attach,	rtsx_attach),
	DEVMETHOD(device_detach,	rtsx_detach),
	DEVMETHOD(device_shutdown,	rtsx_shutdown),
	DEVMETHOD(device_suspend,	rtsx_suspend),
	DEVMETHOD(device_resume,	rtsx_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	rtsx_read_ivar),
	DEVMETHOD(bus_write_ivar,	rtsx_write_ivar),

#ifndef MMCCAM
	/* MMC bridge interface */
	DEVMETHOD(mmcbr_update_ios,	rtsx_mmcbr_update_ios),
	DEVMETHOD(mmcbr_switch_vccq,	rtsx_mmcbr_switch_vccq),
	DEVMETHOD(mmcbr_tune,		rtsx_mmcbr_tune),
	DEVMETHOD(mmcbr_retune,		rtsx_mmcbr_retune),
	DEVMETHOD(mmcbr_request,	rtsx_mmcbr_request),
	DEVMETHOD(mmcbr_get_ro,		rtsx_mmcbr_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	rtsx_mmcbr_acquire_host),
	DEVMETHOD(mmcbr_release_host,	rtsx_mmcbr_release_host),
#endif /* !MMCCAM */

#ifdef MMCCAM
	/* MMCCAM interface */
	DEVMETHOD(mmc_sim_get_tran_settings,	rtsx_get_tran_settings),
	DEVMETHOD(mmc_sim_set_tran_settings,	rtsx_set_tran_settings),
	DEVMETHOD(mmc_sim_cam_request,		rtsx_cam_request),
#endif /* MMCCAM */

	DEVMETHOD_END
};

DEFINE_CLASS_0(rtsx, rtsx_driver, rtsx_methods, sizeof(struct rtsx_softc));
DRIVER_MODULE(rtsx, pci, rtsx_driver, NULL, NULL);

/* For Plug and Play */
MODULE_PNP_INFO("U16:device;D:#;T:vendor=0x10ec", pci, rtsx,
		rtsx_ids, nitems(rtsx_ids));

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(rtsx);
#endif /* !MMCCAM */
