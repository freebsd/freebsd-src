/*-
 * Copyright (c) 2011-2012 Semihalf
 * All rights reserved.
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

/*
 * Driver for Freescale integrated eSDHC controller.
 * Limitations:
 * 	- No support for multi-block transfers.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/vmparam.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcvar.h>
#include <dev/mmc/mmcbrvar.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "opt_platform.h"

#include "mmcbr_if.h"

#include "fsl_sdhc.h"

#ifdef DEBUG
#define	DPRINTF(fmt, arg...)	printf("DEBUG %s(): " fmt, __FUNCTION__, ##arg)
#else
#define	DPRINTF(fmt, arg...)
#endif


/*****************************************************************************
 * Register the driver
 *****************************************************************************/
/* Forward declarations */
static int	fsl_sdhc_probe(device_t);
static int	fsl_sdhc_attach(device_t);
static int	fsl_sdhc_detach(device_t);

static int	fsl_sdhc_read_ivar(device_t, device_t, int, uintptr_t *);
static int	fsl_sdhc_write_ivar(device_t, device_t, int, uintptr_t);

static int	fsl_sdhc_update_ios(device_t, device_t);
static int	fsl_sdhc_request(device_t, device_t, struct mmc_request *);
static int	fsl_sdhc_get_ro(device_t, device_t);
static int	fsl_sdhc_acquire_host(device_t, device_t);
static int	fsl_sdhc_release_host(device_t, device_t);

static device_method_t fsl_sdhc_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, fsl_sdhc_probe),
	DEVMETHOD(device_attach, fsl_sdhc_attach),
	DEVMETHOD(device_detach, fsl_sdhc_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar, fsl_sdhc_read_ivar),
	DEVMETHOD(bus_write_ivar, fsl_sdhc_write_ivar),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,   ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,    ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,     ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,     ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,     ofw_bus_gen_get_type),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios, fsl_sdhc_update_ios),
	DEVMETHOD(mmcbr_request, fsl_sdhc_request),
	DEVMETHOD(mmcbr_get_ro, fsl_sdhc_get_ro),
	DEVMETHOD(mmcbr_acquire_host, fsl_sdhc_acquire_host),
	DEVMETHOD(mmcbr_release_host, fsl_sdhc_release_host),

	{0, 0},
};

/* kobj_class definition */
static driver_t fsl_sdhc_driver = {
	"sdhci_fsl",
	fsl_sdhc_methods,
	sizeof(struct fsl_sdhc_softc)
};

static devclass_t fsl_sdhc_devclass;

DRIVER_MODULE(sdhci_fsl, simplebus, fsl_sdhc_driver, fsl_sdhc_devclass, 0, 0);
DRIVER_MODULE(mmc, sdhci_fsl, mmc_driver, mmc_devclass, NULL, NULL);
MODULE_DEPEND(sdhci_fsl, mmc, 1, 1, 1);

/*****************************************************************************
 * Private methods
 *****************************************************************************/
static inline int
read4(struct fsl_sdhc_softc *sc, unsigned int offset)
{

	return bus_space_read_4(sc->bst, sc->bsh, offset);
}

static inline void
write4(struct fsl_sdhc_softc *sc, unsigned int offset, int value)
{

	bus_space_write_4(sc->bst, sc->bsh, offset, value);
}

static inline void
set_bit(struct fsl_sdhc_softc *sc, uint32_t offset, uint32_t mask)
{
	uint32_t x = read4(sc, offset);

	write4(sc, offset, x | mask);
}

static inline void
clear_bit(struct fsl_sdhc_softc *sc, uint32_t offset, uint32_t mask)
{
	uint32_t x = read4(sc, offset);

	write4(sc, offset, x & ~mask);
}

static int
wait_for_bit_clear(struct fsl_sdhc_softc *sc, enum sdhc_reg_off reg,
    uint32_t bit)
{
	uint32_t timeout = 10;
	uint32_t stat;

	stat = read4(sc, reg);
	while (stat & bit) {
		if (timeout == 0) {
			return (-1);
		}
		--timeout;
		DELAY(1000);
		stat = read4(sc, reg);
	}

	return (0);
}

static int
wait_for_free_line(struct fsl_sdhc_softc *sc, enum sdhc_line line)
{
	uint32_t timeout = 100;
	uint32_t stat;

	stat = read4(sc, SDHC_PRSSTAT);
	while (stat & line) {
		if (timeout == 0) {
			return (-1);
		}
		--timeout;
		DELAY(1000);
		stat = read4(sc, SDHC_PRSSTAT);
	}

	return (0);
}

static uint32_t
get_platform_clock(struct fsl_sdhc_softc *sc)
{
	device_t self, parent;
	phandle_t node;
	uint32_t clock;

	self = sc->self;
	node = ofw_bus_get_node(self);

	/* Get sdhci node properties */
	if((OF_getprop(node, "clock-frequency", (void *)&clock,
	    sizeof(clock)) <= 0) || (clock == 0)) {

		/*
		 * Trying to get clock from parent device (soc) if correct
		 * clock cannot be acquired from sdhci node.
		 */
		parent = device_get_parent(self);
		node = ofw_bus_get_node(parent);

		/* Get soc properties */
		if ((OF_getprop(node, "bus-frequency", (void *)&clock,
		    sizeof(clock)) <= 0) || (clock == 0)) {
			device_printf(self,"Cannot acquire correct sdhci "
			    "frequency from DTS.\n");

			return (0);
		}
	}

	DPRINTF("Acquired clock: %d from DTS\n", clock);

	return (clock);
}

/**
 * Set clock driving card.
 * @param sc
 * @param clock Desired clock frequency in Hz
 */
static void
set_clock(struct fsl_sdhc_softc *sc, uint32_t clock)
{
	uint32_t base_clock;
	uint32_t divisor, prescaler = 1;
	uint32_t round = 0;

	if (clock == sc->slot.clock)
		return;

	if (clock == 0) {
		clear_bit(sc, SDHC_SYSCTL, MASK_CLOCK_CONTROL | SYSCTL_PEREN |
		    SYSCTL_HCKEN | SYSCTL_IPGEN);
		return;
	}

	base_clock = sc->platform_clock;
	round = base_clock & 0x2;
	base_clock >>= 2;
	base_clock += round;
	round = 0;

	/* SD specification 1.1 doesn't allow frequences above 50 MHz */
	if (clock > FSL_SDHC_MAX_CLOCK)
		clock = FSL_SDHC_MAX_CLOCK;

	/*
	 * divisor = ceil(base_clock / clock)
	 * TODO: Reconsider symmetric rounding here instead of ceiling.
	 */
	divisor = howmany(base_clock, clock);

	while (divisor > 16) {
		round = divisor & 0x1;
		divisor >>= 1;

		prescaler <<= 1;
	}
	divisor += round - 1;

	/* Turn off the clock. */
	clear_bit(sc, SDHC_SYSCTL, MASK_CLOCK_CONTROL);

	/* Write clock settings. */
	set_bit(sc, SDHC_SYSCTL, (prescaler << SHIFT_SDCLKFS) |
	    (divisor << SHIFT_DVS));

	/*
	 * Turn on clocks.
	 * TODO: This actually disables clock automatic gating off feature of
	 * the controller which eventually should be enabled but as for now
	 * it prevents controller from generating card insertion/removal
	 * interrupts correctly.
	 */
	set_bit(sc, SDHC_SYSCTL, SYSCTL_PEREN | SYSCTL_HCKEN | SYSCTL_IPGEN);

	sc->slot.clock = clock;

	DPRINTF("given clock = %d, computed clock = %d\n", clock,
	    (base_clock / prescaler) / (divisor + 1));
}

static inline void
send_80_clock_ticks(struct fsl_sdhc_softc *sc)
{
	int err;

	err = wait_for_free_line(sc, SDHC_CMD_LINE | SDHC_DAT_LINE);
	if (err != 0) {
		device_printf(sc->self, "Can't acquire data/cmd lines\n");
		return;
	}

	set_bit(sc, SDHC_SYSCTL, SYSCTL_INITA);
	err = wait_for_bit_clear(sc, SDHC_SYSCTL, SYSCTL_INITA);
	if (err != 0) {
		device_printf(sc->self, "Can't send 80 clocks to the card.\n");
	}
}

static void
set_bus_width(struct fsl_sdhc_softc *sc, enum mmc_bus_width width)
{

	DPRINTF("setting bus width to %d\n", width);
	switch (width) {
	case bus_width_1:
		set_bit(sc, SDHC_PROCTL, DTW_1);
		break;
	case bus_width_4:
		set_bit(sc, SDHC_PROCTL, DTW_4);
		break;
	case bus_width_8:
		set_bit(sc, SDHC_PROCTL, DTW_8);
		break;
	default:
		device_printf(sc->self, "Unsupported bus width\n");
	}
}

static void
reset_controller_all(struct fsl_sdhc_softc *sc)
{
	uint32_t count = 5;

	set_bit(sc, SDHC_SYSCTL, SYSCTL_RSTA);
	while (read4(sc, SDHC_SYSCTL) & SYSCTL_RSTA) {
		DELAY(FSL_SDHC_RESET_DELAY);
		--count;
		if (count == 0) {
			device_printf(sc->self,
			    "Can't reset the controller\n");
			return;
		}
	}
}

static void
reset_controller_dat_cmd(struct fsl_sdhc_softc *sc)
{
	int err;

	set_bit(sc, SDHC_SYSCTL, SYSCTL_RSTD | SYSCTL_RSTC);
	err = wait_for_bit_clear(sc, SDHC_SYSCTL, SYSCTL_RSTD | SYSCTL_RSTC);
	if (err != 0) {
		device_printf(sc->self, "Can't reset data & command part!\n");
		return;
	}
}

static void
init_controller(struct fsl_sdhc_softc *sc)
{

	/* Enable interrupts. */
#ifdef FSL_SDHC_NO_DMA
	write4(sc, SDHC_IRQSTATEN, MASK_IRQ_ALL & ~IRQ_DINT & ~IRQ_DMAE);
	write4(sc, SDHC_IRQSIGEN, MASK_IRQ_ALL & ~IRQ_DINT & ~IRQ_DMAE);
#else
	write4(sc, SDHC_IRQSTATEN, MASK_IRQ_ALL & ~IRQ_BRR & ~IRQ_BWR);
	write4(sc, SDHC_IRQSIGEN, MASK_IRQ_ALL & ~IRQ_BRR & ~IRQ_BWR);

	/* Write DMA address */
	write4(sc, SDHC_DSADDR, sc->dma_phys);

	/* Enable snooping and fix for AHB2MAG bypass. */
	write4(sc, SDHC_DCR, DCR_SNOOP | DCR_AHB2MAG_BYPASS);
#endif
	/* Set data timeout. */
	set_bit(sc, SDHC_SYSCTL, 0xe << SHIFT_DTOCV);

	/* Set water-mark levels (FIFO buffer size). */
	write4(sc, SDHC_WML, (FSL_SDHC_FIFO_BUF_WORDS << 16) |
	    FSL_SDHC_FIFO_BUF_WORDS);
}

static void
init_mmc_host_struct(struct fsl_sdhc_softc *sc)
{
	struct mmc_host *host = &sc->mmc_host;

	/* Clear host structure. */
	bzero(host, sizeof(struct mmc_host));

	/* Calculate minimum and maximum operating frequencies. */
	host->f_min = sc->platform_clock / FSL_SDHC_MAX_DIV;
	host->f_max = FSL_SDHC_MAX_CLOCK;

	/* Set operation conditions (voltage). */
	host->host_ocr = MMC_OCR_320_330 | MMC_OCR_330_340;

	/* Set additional host controller capabilities. */
	host->caps = MMC_CAP_4_BIT_DATA;

	/* Set mode. */
	host->mode = mode_sd;
}

static void
card_detect_task(void *arg, int pending)
{
	struct fsl_sdhc_softc *sc = (struct fsl_sdhc_softc *)arg;
	int err;
	int insert;

	insert = read4(sc, SDHC_PRSSTAT) & PRSSTAT_CINS;

	mtx_lock(&sc->mtx);

	if (insert) {
		if (sc->child != NULL) {
			mtx_unlock(&sc->mtx);
			return;
		}

		sc->child = device_add_child(sc->self, "mmc", -1);
		if (sc->child == NULL) {
			device_printf(sc->self, "Couldn't add MMC bus!\n");
			mtx_unlock(&sc->mtx);
			return;
		}

		/* Initialize MMC bus host structure. */
		init_mmc_host_struct(sc);
		device_set_ivars(sc->child, &sc->mmc_host);

	} else {
		if (sc->child == NULL) {
			mtx_unlock(&sc->mtx);
			return;
		}
	}

	mtx_unlock(&sc->mtx);

	if (insert) {
		if ((err = device_probe_and_attach(sc->child)) != 0) {
			device_printf(sc->self, "MMC bus failed on probe "
			    "and attach! error %d\n", err);
			device_delete_child(sc->self, sc->child);
			sc->child = NULL;
		}
	} else {
		if (device_delete_child(sc->self, sc->child) != 0)
			device_printf(sc->self, "Could not delete MMC bus!\n");
		sc->child = NULL;
	}
}

static void
card_detect_delay(void *arg)
{
	struct fsl_sdhc_softc *sc = arg;

	taskqueue_enqueue(taskqueue_swi_giant, &sc->card_detect_task);
}

static void
finalize_request(struct fsl_sdhc_softc *sc)
{

	DPRINTF("finishing request %p\n", sc->request);

	sc->request->done(sc->request);
	sc->request = NULL;
}

/**
 * Read response from card.
 * @todo Implement Auto-CMD responses being held in R3 for multi-block xfers.
 * @param sc
 */
static void
get_response(struct fsl_sdhc_softc *sc)
{
	struct mmc_command *cmd = sc->request->cmd;
	int i;
	uint32_t val;
	uint8_t ext = 0;

	if (cmd->flags & MMC_RSP_136) {
		/* CRC is stripped, need to shift one byte left. */
		for (i = 0; i < 4; i++) {
			val = read4(sc, SDHC_CMDRSP0 + i * 4);
			cmd->resp[3 - i] = (val << 8) + ext;
			ext = val >> 24;
		}
	} else {
		cmd->resp[0] = read4(sc, SDHC_CMDRSP0);
	}
}

#ifdef FSL_SDHC_NO_DMA
/**
 * Read all content of a fifo buffer.
 * @warning Assumes data buffer is 32-bit aligned.
 * @param sc
 */
static void
read_block_pio(struct fsl_sdhc_softc *sc)
{
	struct mmc_data *data = sc->request->cmd->data;
	size_t left = min(FSL_SDHC_FIFO_BUF_SIZE, data->len);
	uint8_t *buf = data->data;
	uint32_t word;

	buf += sc->data_offset;
	bus_space_read_multi_4(sc->bst, sc->bsh, SDHC_DATPORT, (uint32_t *)buf,
	    left >> 2);

	sc->data_offset += left;

	/* Handle 32-bit unaligned size case. */
	left &= 0x3;
	if (left > 0) {
		buf = (uint8_t *)data->data + (sc->data_offset & ~0x3);
		word = read4(sc, SDHC_DATPORT);
		while (left > 0) {
			*(buf++) = word;
			word >>= 8;
			--left;
		}
	}
}

/**
 * Write a fifo buffer.
 * @warning Assumes data buffer size is 32-bit aligned.
 * @param sc
 */
static void
write_block_pio(struct fsl_sdhc_softc *sc)
{
	struct mmc_data *data = sc->request->cmd->data;
	size_t left = min(FSL_SDHC_FIFO_BUF_SIZE, data->len);
	uint8_t *buf = data->data;
	uint32_t word = 0;

	DPRINTF("sc->data_offset %d\n", sc->data_offset);

	buf += sc->data_offset;
	bus_space_write_multi_4(sc->bst, sc->bsh, SDHC_DATPORT, (uint32_t *)buf,
	    left >> 2);

	sc->data_offset += left;

	/* Handle 32-bit unaligned size case. */
	left &= 0x3;
	if (left > 0) {
		buf = (uint8_t *)data->data + (sc->data_offset & ~0x3);
		while (left > 0) {
			word += *(buf++);
			word <<= 8;
			--left;
		}
		write4(sc, SDHC_DATPORT, word);
	}
}

static void
pio_read_transfer(struct fsl_sdhc_softc *sc)
{

	while (read4(sc, SDHC_PRSSTAT) & PRSSTAT_BREN) {
		read_block_pio(sc);

		/*
		 * TODO: should we check here whether data_offset >= data->len?
		 */
	}
}

static void
pio_write_transfer(struct fsl_sdhc_softc *sc)
{

	while (read4(sc, SDHC_PRSSTAT) & PRSSTAT_BWEN) {
		write_block_pio(sc);

		/*
		 * TODO: should we check here whether data_offset >= data->len?
		 */
	}
}
#endif /* FSL_SDHC_USE_DMA */

static inline void
handle_command_intr(struct fsl_sdhc_softc *sc, uint32_t irq_stat)
{
	struct mmc_command *cmd = sc->request->cmd;

	/* Handle errors. */
	if (irq_stat & IRQ_CTOE) {
		cmd->error = MMC_ERR_TIMEOUT;
	} else if (irq_stat & IRQ_CCE) {
		cmd->error = MMC_ERR_BADCRC;
	} else if (irq_stat & (IRQ_CEBE | IRQ_CIE)) {
		cmd->error = MMC_ERR_FIFO;
	}

	if (cmd->error) {
		device_printf(sc->self, "Error interrupt occured\n");
		reset_controller_dat_cmd(sc);
		return;
	}

	if (sc->command_done)
		return;

	if (irq_stat & IRQ_CC) {
		sc->command_done = 1;

		if (cmd->flags & MMC_RSP_PRESENT)
			get_response(sc);
	}
}

static inline void
handle_data_intr(struct fsl_sdhc_softc *sc, uint32_t irq_stat)
{
	struct mmc_command *cmd = sc->request->cmd;

	/* Handle errors. */
	if (irq_stat & IRQ_DTOE) {
		cmd->error = MMC_ERR_TIMEOUT;
	} else if (irq_stat & (IRQ_DCE | IRQ_DEBE)) {
		cmd->error = MMC_ERR_BADCRC;
	} else if (irq_stat & IRQ_ERROR_DATA_MASK) {
		cmd->error = MMC_ERR_FAILED;
	}

	if (cmd->error) {
		device_printf(sc->self, "Error interrupt occured\n");
		sc->data_done = 1;
		reset_controller_dat_cmd(sc);
		return;
	}

	if (sc->data_done)
		return;

#ifdef FSL_SDHC_NO_DMA
	if (irq_stat & IRQ_BRR) {
		pio_read_transfer(sc);
	}

	if (irq_stat & IRQ_BWR) {
		pio_write_transfer(sc);
	}
#else
	if (irq_stat & IRQ_DINT) {
		struct mmc_data *data = sc->request->cmd->data;

		/* Synchronize DMA. */
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(sc->dma_tag, sc->dma_map,
			    BUS_DMASYNC_POSTREAD);
			memcpy(data->data, sc->dma_mem, data->len);
		} else {
			bus_dmamap_sync(sc->dma_tag, sc->dma_map,
			    BUS_DMASYNC_POSTWRITE);
		}

		/*
		 * TODO: For multiple block transfers, address of dma memory
		 * in DSADDR register should be set to the beginning of the
		 * segment here. Also offset to data pointer should be handled.
		 */
	}
#endif

	if (irq_stat & IRQ_TC)
		sc->data_done = 1;
}

static void
interrupt_handler(void *arg)
{
	struct fsl_sdhc_softc *sc = (struct fsl_sdhc_softc *)arg;
	uint32_t irq_stat;

	mtx_lock(&sc->mtx);

	irq_stat = read4(sc, SDHC_IRQSTAT);

	/* Card interrupt. */
	if (irq_stat & IRQ_CINT) {
		DPRINTF("Card interrupt recievied\n");

	}

	/* Card insertion interrupt. */
	if (irq_stat & IRQ_CINS) {
		clear_bit(sc, SDHC_IRQSIGEN, IRQ_CINS);
		clear_bit(sc, SDHC_IRQSTATEN, IRQ_CINS);
		set_bit(sc, SDHC_IRQSIGEN, IRQ_CRM);
		set_bit(sc, SDHC_IRQSTATEN, IRQ_CRM);

		callout_reset(&sc->card_detect_callout, hz / 2,
		    card_detect_delay, sc);
	}

	/* Card removal interrupt. */
	if (irq_stat & IRQ_CRM) {
		clear_bit(sc, SDHC_IRQSIGEN, IRQ_CRM);
		clear_bit(sc, SDHC_IRQSTATEN, IRQ_CRM);
		set_bit(sc, SDHC_IRQSIGEN, IRQ_CINS);
		set_bit(sc, SDHC_IRQSTATEN, IRQ_CINS);

		callout_stop(&sc->card_detect_callout);
		taskqueue_enqueue(taskqueue_swi_giant, &sc->card_detect_task);
	}

	/* Handle request interrupts. */
	if (sc->request) {
		handle_command_intr(sc, irq_stat);
		handle_data_intr(sc, irq_stat);

		/*
		 * Finalize request when transfer is done successfully
		 * or was interrupted due to error.
		 */  
		if ((sc->data_done && sc->command_done) ||
		    (sc->request->cmd->error))
			finalize_request(sc);
	}

	/* Clear status register. */
	write4(sc, SDHC_IRQSTAT, irq_stat);

	mtx_unlock(&sc->mtx);
}

#ifndef FSL_SDHC_NO_DMA
static void
dma_get_phys_addr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;

	/* Get first segment's physical address. */
	*(bus_addr_t *)arg = segs->ds_addr;
}

static int
init_dma(struct fsl_sdhc_softc *sc)
{
	device_t self = sc->self;
	int err;

	err = bus_dma_tag_create(bus_get_dma_tag(self),
	    FSL_SDHC_DMA_BLOCK_SIZE, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, FSL_SDHC_DMA_BLOCK_SIZE, 1,
	    FSL_SDHC_DMA_BLOCK_SIZE, BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sc->dma_tag);

	if (err) {
		device_printf(self, "Could not create DMA tag!\n");
		return (-1);
	}

	err = bus_dmamem_alloc(sc->dma_tag, (void **)&(sc->dma_mem),
	    BUS_DMA_NOWAIT | BUS_DMA_NOCACHE, &sc->dma_map);
	if (err) {
		device_printf(self, "Could not allocate DMA memory!\n");
		goto fail1;
	}

	err = bus_dmamap_load(sc->dma_tag, sc->dma_map, (void *)sc->dma_mem,
	    FSL_SDHC_DMA_BLOCK_SIZE, dma_get_phys_addr, &sc->dma_phys, 0);
	if (err) {
		device_printf(self, "Could not load DMA map!\n");
		goto fail2;
	}

	return (0);

fail2:
	bus_dmamem_free(sc->dma_tag, sc->dma_mem, sc->dma_map);
fail1:
	bus_dma_tag_destroy(sc->dma_tag);

	return (-1);
}
#endif /* FSL_SDHC_NO_DMA */

static uint32_t
set_xfertyp_register(const struct mmc_command *cmd)
{
	uint32_t xfertyp = 0;

	/* Set command index. */
	xfertyp |= cmd->opcode << CMDINX_SHIFT;

	/* Set command type. */
	if (cmd->opcode == MMC_STOP_TRANSMISSION)
		xfertyp |= CMDTYP_ABORT;

	/* Set data preset select. */
	if (cmd->data) {
		xfertyp |= XFERTYP_DPSEL;

		/* Set transfer direction. */
		if (cmd->data->flags & MMC_DATA_READ)
			xfertyp |= XFERTYP_DTDSEL;
	}

	/* Set command index check. */
	if (cmd->flags & MMC_RSP_OPCODE)
		xfertyp |= XFERTYP_CICEN;

	/* Set command CRC check. */
	if (cmd->flags & MMC_RSP_CRC)
		xfertyp |= XFERTYP_CCCEN;

	/* Set response type */
	if (!(cmd->flags & MMC_RSP_PRESENT))
		xfertyp |= RSPTYP_NONE;
	else if (cmd->flags & MMC_RSP_136)
		xfertyp |= RSPTYP_136;
	else if (cmd->flags & MMC_RSP_BUSY)
		xfertyp |= RSPTYP_48_BUSY;
	else
		xfertyp |= RSPTYP_48;

#ifndef FSL_SDHC_NO_DMA
	/* Enable DMA */
	xfertyp |= XFERTYP_DMAEN;
#endif

	return (xfertyp);
}

static uint32_t
set_blkattr_register(const struct mmc_data *data)
{

	if (data->len <= FSL_SDHC_MAX_BLOCK_SIZE) {
		/* One block transfer. */
		return (BLKATTR_BLOCK_COUNT(1) | ((data->len) &
		    BLKATTR_BLKSZE));
	}

	/* TODO: Write code here for multi-block transfers. */
	return (0);
}

/**
 * Initiate data transfer. Interrupt handler will finalize it.
 * @todo Implement multi-block transfers.
 * @param sc
 * @param cmd
 */
static int
start_data(struct fsl_sdhc_softc *sc, struct mmc_data *data)
{
	uint32_t reg;

	if ((uint32_t)data->data & 0x3) {
		device_printf(sc->self, "32-bit unaligned data pointer in "
		    "request\n");
		return (-1);
	}

	sc->data_done = 0;

#ifdef FSL_SDHC_NO_DMA
	sc->data_ptr = data->data;
	sc->data_offset = 0;
#else
	/* Write DMA address register. */
	write4(sc, SDHC_DSADDR, sc->dma_phys);

	/* Synchronize DMA. */
	if (data->flags & MMC_DATA_READ) {
		bus_dmamap_sync(sc->dma_tag, sc->dma_map,
		    BUS_DMASYNC_PREREAD);
	} else {
		memcpy(sc->dma_mem, data->data, data->len);
		bus_dmamap_sync(sc->dma_tag, sc->dma_map,
		    BUS_DMASYNC_PREWRITE);
	}
#endif
	/* Set block size and count. */
	reg = set_blkattr_register(data);
	if (reg == 0) {
		device_printf(sc->self, "Requested unsupported multi-block "
		    "transfer.\n");
		return (-1);
	}
	write4(sc, SDHC_BLKATTR, reg);

	return (0);
}

static int
start_command(struct fsl_sdhc_softc *sc, struct mmc_command *cmd)
{
	struct mmc_request *req = sc->request;
	uint32_t mask;
	uint32_t xfertyp;
	int err;

	DPRINTF("opcode %d, flags 0x%08x\n", cmd->opcode, cmd->flags);
	DPRINTF("PRSSTAT = 0x%08x\n", read4(sc, SDHC_PRSSTAT));

	sc->command_done = 0;

	cmd->error = MMC_ERR_NONE;

	/* TODO: should we check here for card presence and clock settings? */

	/* Always wait for free CMD line. */
	mask = SDHC_CMD_LINE;
	/* Wait for free DAT if we have data or busy signal. */
	if (cmd->data || (cmd->flags & MMC_RSP_BUSY))
		mask |= SDHC_DAT_LINE;
	/* We shouldn't wait for DAT for stop commands. */
	if (cmd == req->stop)
		mask &= ~SDHC_DAT_LINE;
	err = wait_for_free_line(sc, mask);
	if (err != 0) {
		device_printf(sc->self, "Controller never released inhibit "
		    "bit(s).\n");
		reset_controller_dat_cmd(sc);
		cmd->error = MMC_ERR_FAILED;
		sc->request = NULL;
		req->done(req);
		return (-1);
	}

	xfertyp = set_xfertyp_register(cmd);

	if (cmd->data != NULL) {
		err = start_data(sc, cmd->data);
		if (err != 0) {
			device_printf(sc->self,
			    "Data transfer request failed\n");
			reset_controller_dat_cmd(sc);
			cmd->error = MMC_ERR_FAILED;
			sc->request = NULL;
			req->done(req);
			return (-1);
		}
	}

	write4(sc, SDHC_CMDARG, cmd->arg);
	write4(sc, SDHC_XFERTYP, xfertyp);

	DPRINTF("XFERTYP = 0x%08x\n", xfertyp);
	DPRINTF("CMDARG = 0x%08x\n", cmd->arg);

	return (0);
}

#ifdef DEBUG
static void
dump_registers(struct fsl_sdhc_softc *sc)
{
	printf("PRSSTAT = 0x%08x\n", read4(sc, SDHC_PRSSTAT));
	printf("PROCTL = 0x%08x\n", read4(sc, SDHC_PROCTL));
	printf("HOSTCAPBLT = 0x%08x\n", read4(sc, SDHC_HOSTCAPBLT));
	printf("IRQSTAT = 0x%08x\n", read4(sc, SDHC_IRQSTAT));
	printf("IRQSTATEN = 0x%08x\n", read4(sc, SDHC_IRQSTATEN));
	printf("IRQSIGEN = 0x%08x\n", read4(sc, SDHC_IRQSIGEN));
	printf("WML = 0x%08x\n", read4(sc, SDHC_WML));
	printf("DSADDR = 0x%08x\n", read4(sc, SDHC_DSADDR));
	printf("XFERTYP = 0x%08x\n", read4(sc, SDHC_XFERTYP));
	printf("DCR = 0x%08x\n", read4(sc, SDHC_DCR));
}
#endif

/*****************************************************************************
 * Public methods
 *****************************************************************************/
/*
 * Device interface methods.
 */
static int
fsl_sdhc_probe(device_t self)
{
	static const char *desc =
	    "Freescale Enhanced Secure Digital Host Controller";

	if (!ofw_bus_is_compatible(self, "fsl,p2020-esdhc") &&
	    !ofw_bus_is_compatible(self, "fsl,esdhc"))
		return (ENXIO);

	device_set_desc(self, desc);

	return (BUS_PROBE_VENDOR);
}

static int
fsl_sdhc_attach(device_t self)
{
	struct fsl_sdhc_softc *sc;

	sc = device_get_softc(self);

	sc->self = self;

	mtx_init(&sc->mtx, device_get_nameunit(self), NULL, MTX_DEF);

	/* Setup memory resource */
	sc->mem_rid = 0;
	sc->mem_resource = bus_alloc_resource_any(self, SYS_RES_MEMORY,
	    &sc->mem_rid, RF_ACTIVE);
	if (sc->mem_resource == NULL) {
		device_printf(self, "Could not allocate memory.\n");
		goto fail;
	}
	sc->bst = rman_get_bustag(sc->mem_resource);
	sc->bsh = rman_get_bushandle(sc->mem_resource);

	/* Setup interrupt resource. */
	sc->irq_rid = 0;
	sc->irq_resource = bus_alloc_resource_any(self, SYS_RES_IRQ,
	    &sc->irq_rid, RF_ACTIVE);
	if (sc->irq_resource == NULL) {
		device_printf(self, "Could not allocate interrupt.\n");
		goto fail;
	}
	if (bus_setup_intr(self, sc->irq_resource, INTR_TYPE_MISC |
	    INTR_MPSAFE, NULL, interrupt_handler, sc, &sc->ihl) != 0) {
		device_printf(self, "Could not setup interrupt.\n");
		goto fail;
	}

	/* Setup DMA. */
#ifndef FSL_SDHC_NO_DMA
	if (init_dma(sc) != 0) {
		device_printf(self, "Could not setup DMA\n");
	}
#endif
	sc->bus_busy = 0;
	sc->platform_clock = get_platform_clock(sc);
	if (sc->platform_clock == 0) {
		device_printf(self, "Could not get platform clock.\n");
		goto fail;
	}
	sc->command_done = 1;
	sc->data_done = 1;

	/* Init card detection task. */
	TASK_INIT(&sc->card_detect_task, 0, card_detect_task, sc);
	callout_init(&sc->card_detect_callout, 1);

	reset_controller_all(sc);
	init_controller(sc);
	set_clock(sc, 400000);
	send_80_clock_ticks(sc);

#ifdef DEBUG
	dump_registers(sc);
#endif

	return (0);

fail:
	fsl_sdhc_detach(self);
	return (ENXIO);
}

static int
fsl_sdhc_detach(device_t self)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);
	int err;

	if (sc->child)
		device_delete_child(self, sc->child);

	taskqueue_drain(taskqueue_swi_giant, &sc->card_detect_task);

#ifndef FSL_SDHC_NO_DMA
	bus_dmamap_unload(sc->dma_tag, sc->dma_map);
	bus_dmamem_free(sc->dma_tag, sc->dma_mem, sc->dma_map);
	bus_dma_tag_destroy(sc->dma_tag);
#endif

	if (sc->ihl != NULL) {
		err = bus_teardown_intr(self, sc->irq_resource, sc->ihl);
		if (err)
			return (err);
	}
	if (sc->irq_resource != NULL) {
		err = bus_release_resource(self, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_resource);
		if (err)
			return (err);

	}
	if (sc->mem_resource != NULL) {
		err = bus_release_resource(self, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_resource);
		if (err)
			return (err);
	}

	mtx_destroy(&sc->mtx);

	return (0);
}


/*
 * Bus interface methods.
 */
static int
fsl_sdhc_read_ivar(device_t self, device_t child, int index,
    uintptr_t *result)
{
	struct mmc_host *host = device_get_ivars(child);

	switch (index) {
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = host->ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = host->ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = host->ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = host->ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = host->f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = host->f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = host->host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = host->mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = host->ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = host->ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = host->ios.vdd;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
fsl_sdhc_write_ivar(device_t self, device_t child, int index,
    uintptr_t value)
{
	struct mmc_host *host = device_get_ivars(child);

	switch (index) {
	case MMCBR_IVAR_BUS_MODE:
		host->ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		host->ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		host->ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		host->ios.clock = value;
		break;
	case MMCBR_IVAR_MODE:
		host->mode = value;
		break;
	case MMCBR_IVAR_OCR:
		host->ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		host->ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		host->ios.vdd = value;
		break;
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	default:
		/* Instance variable not writable. */
		return (EINVAL);
	}

	return (0);
}


/*
 * MMC bridge methods.
 */
static int
fsl_sdhc_update_ios(device_t self, device_t reqdev)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);
	struct mmc_host *host = device_get_ivars(reqdev);
	struct mmc_ios *ios = &host->ios;

	mtx_lock(&sc->mtx);

	/* Full reset on bus power down to clear from any state. */
	if (ios->power_mode == power_off) {
		reset_controller_all(sc);
		init_controller(sc);
	}

	set_clock(sc, ios->clock);
	set_bus_width(sc, ios->bus_width);

	mtx_unlock(&sc->mtx);

	return (0);
}

static int
fsl_sdhc_request(device_t self, device_t reqdev, struct mmc_request *req)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);
	int err;

	mtx_lock(&sc->mtx);

	sc->request = req;
	err = start_command(sc, req->cmd);

	mtx_unlock(&sc->mtx);

	return (err);
}

static int
fsl_sdhc_get_ro(device_t self, device_t reqdev)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);

	/* Wouldn't it be faster using branching (if {}) ?? */
	return (((read4(sc, SDHC_PRSSTAT) & PRSSTAT_WPSPL) >> 19) ^ 0x1);
}

static int
fsl_sdhc_acquire_host(device_t self, device_t reqdev)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);
	int retval = 0;

	mtx_lock(&sc->mtx);

	while (sc->bus_busy)
		retval = mtx_sleep(sc, &sc->mtx, PZERO, "sdhcah", 0);
	++(sc->bus_busy);

	mtx_unlock(&sc->mtx);

	return (retval);
}

static int
fsl_sdhc_release_host(device_t self, device_t reqdev)
{
	struct fsl_sdhc_softc *sc = device_get_softc(self);

	mtx_lock(&sc->mtx);
	--(sc->bus_busy);
	mtx_unlock(&sc->mtx);
	wakeup(sc);

	return (0);
}
