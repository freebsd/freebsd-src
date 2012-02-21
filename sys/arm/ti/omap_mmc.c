/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * Driver for the MMC/SD/SDIO module on the TI OMAP series of SoCs.
 *
 * This driver is heavily based on the SD/MMC driver for the AT91 (at91_mci.c).
 *
 * It's important to realise that the MMC state machine is already in the kernel
 * and this driver only exposes the specific interfaces of the controller.
 *
 * This driver is still very much a work in progress, I've verified that basic
 * sector reading can be performed. But I've yet to test it with a file system
 * or even writing.  In addition I've only tested the driver with an SD card,
 * I've no idea if MMC cards work.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/intr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#include "mmcbr_if.h"
#include "mmcbus_if.h"

#include <arm/ti/omap_dma.h>
#include <arm/ti/omap_mmc.h>
#include <arm/ti/ti_cpuid.h>
#include <arm/ti/ti_prcm.h>

#include <arm/ti/twl/twl.h>
#include <arm/ti/twl/twl_vreg.h>

#ifdef DEBUG
#define omap_mmc_dbg(sc, fmt, args...) \
	device_printf((sc)->sc_dev, fmt, ## args);
#else
#define omap_mmc_dbg(sc, fmt, args...)
#endif

/**
 *	Structure that stores the driver context
 */
struct omap_mmc_softc {
	device_t		sc_dev;
	struct resource*	sc_irq_res;
	struct resource*	sc_mem_res;

	void*			sc_irq_h;

	bus_dma_tag_t		sc_dmatag;
	bus_dmamap_t		sc_dmamap;
	int			sc_dmamapped;

	unsigned int		sc_dmach_rd;
	unsigned int		sc_dmach_wr;

	device_t		sc_gpio_dev;
	int			sc_wp_gpio_pin;  /* GPIO pin for MMC write protect */

	device_t		sc_vreg_dev;
	const char*		sc_vreg_name;

	struct mtx		sc_mtx;

	struct mmc_host		host;
	struct mmc_request*	req;
	struct mmc_command*	curcmd;

	int			flags;
#define CMD_STARTED     1
#define STOP_STARTED    2

	int			bus_busy;  /* TODO: Needed ? */

	void*			sc_cmd_data_vaddr;
	int			sc_cmd_data_len;

	/* The offset applied to each of the register base addresses, OMAP4
	 * register sets are offset 0x100 from the OMAP3 series.
	 */
	unsigned long		sc_reg_off;

	/* The physical address of the MMCHS_DATA register, used for the DMA xfers */
	unsigned long		sc_data_reg_paddr;

	/* The reference clock frequency */
	unsigned int		sc_ref_freq;

	enum mmc_power_mode	sc_cur_power_mode;
};

/**
 *	Macros for driver mutex locking
 */
#define OMAP_MMC_LOCK(_sc)              mtx_lock(&(_sc)->sc_mtx)
#define	OMAP_MMC_UNLOCK(_sc)            mtx_unlock(&(_sc)->sc_mtx)
#define OMAP_MMC_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	         "omap_mmc", MTX_DEF)
#define OMAP_MMC_LOCK_DESTROY(_sc)      mtx_destroy(&_sc->sc_mtx);
#define OMAP_MMC_ASSERT_LOCKED(_sc)     mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define OMAP_MMC_ASSERT_UNLOCKED(_sc)   mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

static void omap_mmc_start(struct omap_mmc_softc *sc);

/**
 *	omap_mmc_read_4 - reads a 32-bit value from a register
 *	omap_mmc_write_4 - writes a 32-bit value to a register
 *	@sc: pointer to the driver context
 *	@off: register offset to read from
 *	@val: the value to write into the register
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	The 32-bit value read from the register
 */
static inline uint32_t
omap_mmc_read_4(struct omap_mmc_softc *sc, bus_size_t off)
{
	return bus_read_4(sc->sc_mem_res, (sc->sc_reg_off + off));
}

static inline void
omap_mmc_write_4(struct omap_mmc_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->sc_mem_res, (sc->sc_reg_off + off), val);
}

/**
 *	omap_mmc_reset_controller -
 *	@arg: caller supplied arg
 *	@segs: array of segments (although in our case should only be one)
 *	@nsegs: number of segments (in our case should be 1)
 *	@error:
 *
 *
 *
 */
static void
omap_mmc_reset_controller(struct omap_mmc_softc *sc, uint32_t bit)
{
	unsigned long attempts;
	uint32_t sysctl;

	omap_mmc_dbg(sc, "reseting controller - bit 0x%08x\n", bit);

	sysctl = omap_mmc_read_4(sc, MMCHS_SYSCTL);
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl | bit);

	if ((ti_chip() == CHIP_OMAP_4) && (ti_revision() > OMAP4430_REV_ES1_0)) {
		/* OMAP4 ES2 and greater has an updated reset logic.
		 * Monitor a 0->1 transition first
		 */
		attempts = 10000;
		while (!(omap_mmc_read_4(sc, MMCHS_SYSCTL) & bit) && (attempts-- > 0))
			continue;
	}

	attempts = 10000;
	while ((omap_mmc_read_4(sc, MMCHS_SYSCTL) & bit) && (attempts-- > 0))
		continue;

	if (omap_mmc_read_4(sc, MMCHS_SYSCTL) & bit)
		device_printf(sc->sc_dev, "Error - Timeout waiting on controller reset\n");
}

/**
 *	omap_mmc_getaddr - called by the DMA function to simply return the phys addr
 *	@arg: caller supplied arg
 *	@segs: array of segments (although in our case should only be one)
 *	@nsegs: number of segments (in our case should be 1)
 *	@error:
 *
 *	This function is called by bus_dmamap_load() after it has compiled an array
 *	of segments, each segment is a phsyical chunk of memory. However in our case
 *	we should only have one segment, because we don't (yet?) support DMA scatter
 *	gather. To ensure we only have one segment, the DMA tag was created by
 *	bus_dma_tag_create() (called from omap_mmc_attach) with nsegments set to 1.
 *
 */
static void
omap_mmc_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/**
 *	omap_mmc_dma_intr - interrupt handler for DMA events triggered by the controller
 *	@ch: the dma channel number
 *	@status: bit field of the status bytes
 *	@data: callback data, in this case a pointer to the controller struct
 *
 *
 *	LOCKING:
 *	Called from interrupt context
 *
 */
static void
omap_mmc_dma_intr(unsigned int ch, uint32_t status, void *data)
{
	/* Ignore for now ... we don't need this interrupt as we already have the
	 * interrupt from the MMC controller.
	 */
}

/**
 *	omap_mmc_intr_xfer_compl - called if a 'transfer complete' IRQ was received
 *	@sc: pointer to the driver context
 *	@cmd: the command that was sent previously
 *
 *	This function is simply responsible for syncing up the DMA buffer.
 *
 *	LOCKING:
 *	Called from interrupt context
 *
 *	RETURNS:
 *	Return value indicates if the transaction is complete, not done = 0, done != 0
 */
static int
omap_mmc_intr_xfer_compl(struct omap_mmc_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmd_reg;

	/* Read command register to test whether this command was a read or write. */
	cmd_reg = omap_mmc_read_4(sc, MMCHS_CMD);

	/* Sync-up the DMA buffer so the caller can access the new memory */
	if (cmd_reg & MMCHS_CMD_DDIR) {
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_dmamap);
	}
	else {
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmatag, sc->sc_dmamap);
	}
	sc->sc_dmamapped--;

	/* Debugging dump of the data received */
#if 0
	{
		int i;
		uint8_t *p = (uint8_t*) sc->sc_cmd_data_vaddr;
		for (i=0; i<sc->sc_cmd_data_len; i++) {
			if ((i % 16) == 0)
				printf("\n0x%04x : ", i);
			printf("%02X ", *p++);
		}
		printf("\n");
	}
#endif

	/* We are done, transfer complete */
	return 1;
}

/**
 *	omap_mmc_intr_cmd_compl - called if a 'command complete' IRQ was received
 *	@sc: pointer to the driver context
 *	@cmd: the command that was sent previously
 *
 *
 *	LOCKING:
 *	Called from interrupt context
 *
 *	RETURNS:
 *	Return value indicates if the transaction is complete, not done = 0, done != 0
 */
static int
omap_mmc_intr_cmd_compl(struct omap_mmc_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmd_reg;

	/* Copy the response into the request struct ... if a response was
	 * expected */
	if (cmd != NULL && (cmd->flags & MMC_RSP_PRESENT)) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[3] = omap_mmc_read_4(sc, MMCHS_RSP10);
			cmd->resp[2] = omap_mmc_read_4(sc, MMCHS_RSP32);
			cmd->resp[1] = omap_mmc_read_4(sc, MMCHS_RSP54);
			cmd->resp[0] = omap_mmc_read_4(sc, MMCHS_RSP76);
		} else {
			cmd->resp[0] = omap_mmc_read_4(sc, MMCHS_RSP10);
		}
	}

	/* Check if the command was expecting some data transfer, if not
	 * we are done. */
	cmd_reg = omap_mmc_read_4(sc, MMCHS_CMD);
	return ((cmd_reg & MMCHS_CMD_DP) == 0);
}

/**
 *	omap_mmc_intr_error - handles error interrupts
 *	@sc: pointer to the driver context
 *	@cmd: the command that was sent previously
 *	@stat_reg: the value that was in the status register
 *
 *
 *	LOCKING:
 *	Called from interrupt context
 *
 *	RETURNS:
 *	Return value indicates if the transaction is complete, not done = 0, done != 0
 */
static int
omap_mmc_intr_error(struct omap_mmc_softc *sc, struct mmc_command *cmd,
					 uint32_t stat_reg)
{
	omap_mmc_dbg(sc, "error in xfer - stat 0x%08x\n", stat_reg);

	/* Ignore CRC errors on CMD2 and ACMD47, per relevant standards */
	if ((stat_reg & MMCHS_STAT_CCRC) && (cmd->opcode == MMC_SEND_OP_COND ||
	    cmd->opcode == ACMD_SD_SEND_OP_COND))
		cmd->error = MMC_ERR_NONE;
	else if (stat_reg & (MMCHS_STAT_CTO | MMCHS_STAT_DTO))
		cmd->error = MMC_ERR_TIMEOUT;
	else if (stat_reg & (MMCHS_STAT_CCRC | MMCHS_STAT_DCRC))
		cmd->error = MMC_ERR_BADCRC;
	else
		cmd->error = MMC_ERR_FAILED;

	/* If a dma transaction we should also stop the dma transfer */
	if (omap_mmc_read_4(sc, MMCHS_CMD) & MMCHS_CMD_DE) {

		/* Abort the DMA transfer (DDIR bit tells direction) */
		if (omap_mmc_read_4(sc, MMCHS_CMD) & MMCHS_CMD_DDIR)
			omap_dma_stop_xfer(sc->sc_dmach_rd);
		else
			omap_dma_stop_xfer(sc->sc_dmach_wr);

		/* If an error occure abort the DMA operation and free the dma map */
		if ((sc->sc_dmamapped > 0) && (cmd->error != MMC_ERR_NONE)) {
			bus_dmamap_unload(sc->sc_dmatag, sc->sc_dmamap);
			sc->sc_dmamapped--;
		}
	}

	/* Command error occured? ... if so issue a soft reset for the cmd fsm */
	if (stat_reg & (MMCHS_STAT_CCRC | MMCHS_STAT_CTO)) {
		omap_mmc_reset_controller(sc, MMCHS_SYSCTL_SRC);
	}

	/* Data error occured? ... if so issue a soft reset for the data line */
	if (stat_reg & (MMCHS_STAT_DEB | MMCHS_STAT_DCRC | MMCHS_STAT_DTO)) {
		omap_mmc_reset_controller(sc, MMCHS_SYSCTL_SRD);
	}

	/* On any error the command is cancelled ... so we are done */
	return 1;
}

/**
 *	omap_mmc_intr - interrupt handler for MMC/SD/SDIO controller
 *	@arg: pointer to the driver context
 *
 *	Interrupt handler for the MMC/SD/SDIO controller, responsible for handling
 *	the IRQ and clearing the status flags.
 *
 *	LOCKING:
 *	Called from interrupt context
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_intr(void *arg)
{
	struct omap_mmc_softc *sc = (struct omap_mmc_softc *) arg;
	uint32_t stat_reg;
	int done = 0;

	OMAP_MMC_LOCK(sc);

	stat_reg = omap_mmc_read_4(sc, MMCHS_STAT)
	    & (omap_mmc_read_4(sc, MMCHS_IE) | MMCHS_STAT_ERRI);

	if (sc->curcmd == NULL) {
		device_printf(sc->sc_dev, "Error: current cmd NULL, already done?\n");
		omap_mmc_write_4(sc, MMCHS_STAT, stat_reg);
		OMAP_MMC_UNLOCK(sc);
		return;
	}

	if (stat_reg & MMCHS_STAT_ERRI) {
		/* An error has been tripped in the status register */
		done = omap_mmc_intr_error(sc, sc->curcmd, stat_reg);

	} else {

		/* NOTE: This implementation could be a bit inefficent, I don't think
		 * it is necessary to handle both the 'command complete' and 'transfer
		 * complete' for data transfers ... presumably just transfer complete
		 * is enough.
		 */

		/* No error */
		sc->curcmd->error = MMC_ERR_NONE;

		/* Check if the command completed */
		if (stat_reg & MMCHS_STAT_CC) {
			done = omap_mmc_intr_cmd_compl(sc, sc->curcmd);
		}

		/* Check if the transfer has completed */
		if (stat_reg & MMCHS_STAT_TC) {
			done = omap_mmc_intr_xfer_compl(sc, sc->curcmd);
		}

	}

	/* Clear all the interrupt status bits by writing the value back */
	omap_mmc_write_4(sc, MMCHS_STAT, stat_reg);

	/* This may mark the command as done if there is no stop request */
	/* TODO: This is a bit ugly, needs fix-up */
	if (done) {
		omap_mmc_start(sc);
	}

	OMAP_MMC_UNLOCK(sc);
}

/**
 *	omap_mmc_start_cmd - starts the given command
 *	@sc: pointer to the driver context
 *	@cmd: the command to start
 *
 *	The call tree for this function is
 *		- omap_mmc_start_cmd
 *			- omap_mmc_start
 *				- omap_mmc_request
 *
 *	LOCKING:
 *	Caller should be holding the OMAP_MMC lock.
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_start_cmd(struct omap_mmc_softc *sc, struct mmc_command *cmd)
{
	uint32_t cmd_reg, con_reg, ise_reg;
	struct mmc_data *data;
	struct mmc_request *req;
	void *vaddr;
	bus_addr_t paddr;
	uint32_t pktsize;

	sc->curcmd = cmd;
	data = cmd->data;
	req = cmd->mrq;

	/* Ensure the STR and MIT bits are cleared, these are only used for special
	 * command types.
	 */
	con_reg = omap_mmc_read_4(sc, MMCHS_CON);
	con_reg &= ~(MMCHS_CON_STR | MMCHS_CON_MIT);

	/* Load the command into bits 29:24 of the CMD register */
	cmd_reg = (uint32_t)(cmd->opcode & 0x3F) << 24;

	/* Set the default set of interrupts */
	ise_reg = (MMCHS_STAT_CERR | MMCHS_STAT_CTO | MMCHS_STAT_CC | MMCHS_STAT_CEB);

	/* Enable CRC checking if requested */
	if (cmd->flags & MMC_RSP_CRC)
		ise_reg |= MMCHS_STAT_CCRC;

	/* Enable reply index checking if the response supports it */
	if (cmd->flags & MMC_RSP_OPCODE)
		ise_reg |= MMCHS_STAT_CIE;

	/* Set the expected response length */
	if (MMC_RSP(cmd->flags) == MMC_RSP_NONE) {
		cmd_reg |= MMCHS_CMD_RSP_TYPE_NO;
	} else {
		if (cmd->flags & MMC_RSP_136)
			cmd_reg |= MMCHS_CMD_RSP_TYPE_136;
		else if (cmd->flags & MMC_RSP_BUSY)
			cmd_reg |= MMCHS_CMD_RSP_TYPE_48_BSY;
		else
			cmd_reg |= MMCHS_CMD_RSP_TYPE_48;

		/* Enable command index/crc checks if necessary expected */
		if (cmd->flags & MMC_RSP_CRC)
			cmd_reg |= MMCHS_CMD_CCCE;
		if (cmd->flags & MMC_RSP_OPCODE)
			cmd_reg |= MMCHS_CMD_CICE;
	}

	/* Set the bits for the special commands CMD12 (MMC_STOP_TRANSMISSION) and
	 * CMD52 (SD_IO_RW_DIRECT) */
	if (cmd->opcode == MMC_STOP_TRANSMISSION)
		cmd_reg |= MMCHS_CMD_CMD_TYPE_IO_ABORT;

	/* Check if there is any data to write */
	if (data == NULL) {
		/* Clear the block count */
		omap_mmc_write_4(sc, MMCHS_BLK, 0);

		/* The no data case is fairly simple */
		omap_mmc_write_4(sc, MMCHS_CON, con_reg);
		omap_mmc_write_4(sc, MMCHS_IE, ise_reg);
		omap_mmc_write_4(sc, MMCHS_ISE, ise_reg);
		omap_mmc_write_4(sc, MMCHS_ARG, cmd->arg);
		omap_mmc_write_4(sc, MMCHS_CMD, cmd_reg);
		return;
	}

	/* Indicate that data is present */
	cmd_reg |= MMCHS_CMD_DP | MMCHS_CMD_MSBS | MMCHS_CMD_BCE;

	/* Indicate a read operation */
	if (data->flags & MMC_DATA_READ)
		cmd_reg |= MMCHS_CMD_DDIR;

	/* Streaming mode */
	if (data->flags & MMC_DATA_STREAM) {
		con_reg |= MMCHS_CON_STR;
	}

	/* Multi-block mode */
	if (data->flags & MMC_DATA_MULTI) {
		cmd_reg |= MMCHS_CMD_MSBS;
	}

	/* Enable extra interrupt sources for the transfer */
	ise_reg |= (MMCHS_STAT_TC | MMCHS_STAT_DTO | MMCHS_STAT_DEB | MMCHS_STAT_CEB);
	if (cmd->flags & MMC_RSP_CRC)
		ise_reg |= MMCHS_STAT_DCRC;

	/* Enable the DMA transfer bit */
	cmd_reg |= MMCHS_CMD_DE;

	/* Set the block size and block count */
	omap_mmc_write_4(sc, MMCHS_BLK, (1 << 16) | data->len);

	/* Setup the DMA stuff */
	if (data->flags & (MMC_DATA_READ | MMC_DATA_WRITE)) {

		vaddr = data->data;
		data->xfer_len = 0;

		/* Map the buffer buf into bus space using the dmamap map. */
		if (bus_dmamap_load(sc->sc_dmatag, sc->sc_dmamap, vaddr, data->len,
		    omap_mmc_getaddr, &paddr, 0) != 0) {

			if (req->cmd->flags & STOP_STARTED)
				req->stop->error = MMC_ERR_NO_MEMORY;
			else
				req->cmd->error = MMC_ERR_NO_MEMORY;
			sc->req = NULL;
			sc->curcmd = NULL;
			req->done(req);
			return;
		}

		/* Calculate the packet size, the max packet size is 512 bytes
		 * (or 128 32-bit elements).
		 */
		pktsize = min((data->len / 4), (512 / 4));

		/* Sync the DMA buffer and setup the DMA controller */
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap, BUS_DMASYNC_PREREAD);
			omap_dma_start_xfer_packet(sc->sc_dmach_rd, sc->sc_data_reg_paddr,
			    paddr, 1, (data->len / 4), pktsize);
		} else {
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_dmamap, BUS_DMASYNC_PREWRITE);
			omap_dma_start_xfer_packet(sc->sc_dmach_wr, paddr,
			    sc->sc_data_reg_paddr, 1, (data->len / 4), pktsize);
		}

		/* Increase the mapped count */
		sc->sc_dmamapped++;

		sc->sc_cmd_data_vaddr = vaddr;
		sc->sc_cmd_data_len = data->len;
	}

	/* Finally kick off the command */
	omap_mmc_write_4(sc, MMCHS_CON, con_reg);
	omap_mmc_write_4(sc, MMCHS_IE, ise_reg);
	omap_mmc_write_4(sc, MMCHS_ISE, ise_reg);
	omap_mmc_write_4(sc, MMCHS_ARG, cmd->arg);
	omap_mmc_write_4(sc, MMCHS_CMD, cmd_reg);

	/* and we're done */
}

/**
 *	omap_mmc_start - starts a request stored in the driver context
 *	@sc: pointer to the driver context
 *
 *	This function is called by omap_mmc_request() in response to a read/write
 *	request from the MMC core module.
 *
 *	LOCKING:
 *	Caller should be holding the OMAP_MMC lock.
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_start(struct omap_mmc_softc *sc)
{
	struct mmc_request *req;

	/* Sanity check we have a request */
	req = sc->req;
	if (req == NULL)
		return;

	/* assert locked */
	if (!(sc->flags & CMD_STARTED)) {
		sc->flags |= CMD_STARTED;
		omap_mmc_start_cmd(sc, req->cmd);
		return;
	}

	if (!(sc->flags & STOP_STARTED) && req->stop) {
		sc->flags |= STOP_STARTED;
		omap_mmc_start_cmd(sc, req->stop);
		return;
	}

	/* We must be done -- bad idea to do this while locked? */
	sc->req = NULL;
	sc->curcmd = NULL;
	req->done(req);
}

/**
 *	omap_mmc_request - entry point for all read/write/cmd requests
 *	@brdev: mmc bridge device handle
 *	@reqdev: the device doing the requesting ?
 *	@req: the action requested
 *
 *	LOCKING:
 *	None, internally takes the OMAP_MMC lock.
 *
 *	RETURNS:
 *	0 on success
 *	EBUSY if the driver is already performing a request
 */
static int
omap_mmc_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct omap_mmc_softc *sc = device_get_softc(brdev);

	OMAP_MMC_LOCK(sc);

	/*
	 * XXX do we want to be able to queue up multiple commands?
	 * XXX sounds like a good idea, but all protocols are sync, so
	 * XXX maybe the idea is naive...
	 */
	if (sc->req != NULL) {
		OMAP_MMC_UNLOCK(sc);
		return (EBUSY);
	}

	/* Store the request and start the command */
	sc->req = req;
	sc->flags = 0;
	omap_mmc_start(sc);

	OMAP_MMC_UNLOCK(sc);

	return (0);
}

/**
 *	omap_mmc_get_ro - returns the status of the read-only setting
 *	@brdev: mmc bridge device handle
 *	@reqdev: device doing the request
 *
 *	This function is relies on hint'ed values to determine which GPIO is used
 *	to determine if the write protect is enabled. On the BeagleBoard the pin
 *	is GPIO_23.
 *
 *	LOCKING:
 *	-
 *
 *	RETURNS:
 *	0 if not read-only
 *	1 if read only
 */
static int
omap_mmc_get_ro(device_t brdev, device_t reqdev)
{
	struct omap_mmc_softc *sc = device_get_softc(brdev);
	unsigned int readonly = 0;

	OMAP_MMC_LOCK(sc);

	if ((sc->sc_wp_gpio_pin != -1) && (sc->sc_gpio_dev != NULL)) {
		if (GPIO_PIN_GET(sc->sc_gpio_dev, sc->sc_wp_gpio_pin, &readonly) != 0)
			readonly = 0;
		else
			readonly = (readonly == 0) ? 0 : 1;
	}

	OMAP_MMC_UNLOCK(sc);

	return (readonly);
}

/**
 *	omap_mmc_send_init_stream - sets bus/controller settings
 *	@brdev: mmc bridge device handle
 *	@reqdev: device doing the request
 *
 *	Send init stream sequence to card before sending IDLE command
 *
 *	LOCKING:
 *
 *
 *	RETURNS:
 *	0 if function succeeded
 */
static void
omap_mmc_send_init_stream(struct omap_mmc_softc *sc)
{
	unsigned long timeout;
	uint32_t ie, ise, con;

	omap_mmc_dbg(sc, "Performing init sequence\n");

	/* Prior to issuing any command, the MMCHS controller has to execute a
	 * special INIT procedure. The MMCHS controller has to generate a clock
	 * during 1ms. During the INIT procedure, the MMCHS controller generates 80
	 * clock periods. In order to keep the 1ms gap, the MMCHS controller should
	 * be configured to generate a clock whose frequency is smaller or equal to
	 * 80 KHz. If the MMCHS controller divider bitfield width doesn't allow to
	 * choose big values, the MMCHS controller driver should perform the INIT
	 * procedure twice or three times. Twice is generally enough.
	 *
	 * The INIt procedure is executed by setting MMCHS1.MMCHS_CON[1] INIT
	 * bitfield to 1 and by sending a dummy command, writing 0x00000000 in
	 * MMCHS1.MMCHS_CMD register.
	 */

	/* Disable interrupt status events but enable interrupt generation.
	 * This doesn't seem right to me, but if the interrupt generation is not
	 * enabled the CC bit doesn't seem to be set in the STAT register.
	 */

	/* Enable interrupt generation */
	ie = omap_mmc_read_4(sc, MMCHS_IE);
	omap_mmc_write_4(sc, MMCHS_IE, 0x307F0033);

	/* Disable generation of status events (stops interrupt triggering) */
	ise = omap_mmc_read_4(sc, MMCHS_ISE);
	omap_mmc_write_4(sc, MMCHS_ISE, 0);

	/* Set the initialise stream bit */
	con = omap_mmc_read_4(sc, MMCHS_CON);
	con |= MMCHS_CON_INIT;
	omap_mmc_write_4(sc, MMCHS_CON, con);

	/* Write a dummy command 0x00 */
	omap_mmc_write_4(sc, MMCHS_CMD, 0x00000000);

	/* Loop waiting for the command to finish */
	timeout = hz;
	do {
		pause("MMCINIT", 1);
		if (timeout-- == 0) {
			device_printf(sc->sc_dev, "Error: first stream init timed out\n");
			break;
		}
	} while (!(omap_mmc_read_4(sc, MMCHS_STAT) & MMCHS_STAT_CC));

	/* Clear the command complete status bit */
	omap_mmc_write_4(sc, MMCHS_STAT, MMCHS_STAT_CC);

	/* Write another dummy command 0x00 */
	omap_mmc_write_4(sc, MMCHS_CMD, 0x00000000);

	/* Loop waiting for the second command to finish */
	timeout = hz;
	do {
		pause("MMCINIT", 1);
		if (timeout-- == 0) {
			device_printf(sc->sc_dev, "Error: second stream init timed out\n");
			break;
		}
	} while (!(omap_mmc_read_4(sc, MMCHS_STAT) & MMCHS_STAT_CC));

	/* Clear the stream init bit */
	con &= ~MMCHS_CON_INIT;
	omap_mmc_write_4(sc, MMCHS_CON, con);

	/* Clear the status register, then restore the IE and ISE registers */
	omap_mmc_write_4(sc, MMCHS_STAT, 0xffffffff);
	omap_mmc_read_4(sc, MMCHS_STAT);

	omap_mmc_write_4(sc, MMCHS_ISE, ise);
	omap_mmc_write_4(sc, MMCHS_IE, ie);
}

/**
 *	omap_mmc_update_ios - sets bus/controller settings
 *	@brdev: mmc bridge device handle
 *	@reqdev: device doing the request
 *
 *	Called to set the bus and controller settings that need to be applied to
 *	the actual HW.  Currently this function just sets the bus width and the
 *	clock speed.
 *
 *	LOCKING:
 *
 *
 *	RETURNS:
 *	0 if function succeeded
 */
static int
omap_mmc_update_ios(device_t brdev, device_t reqdev)
{
	struct omap_mmc_softc *sc;
	struct mmc_host *host;
	struct mmc_ios *ios;
	uint32_t clkdiv;
	uint32_t hctl_reg;
	uint32_t con_reg;
	uint32_t sysctl_reg;
	uint16_t mv;
	unsigned long timeout;
	int do_card_init = 0;

	sc = device_get_softc(brdev);
	host = &sc->host;
	ios = &host->ios;

	/* Read the initial values of the registers */
	hctl_reg = omap_mmc_read_4(sc, MMCHS_HCTL);
	con_reg = omap_mmc_read_4(sc, MMCHS_CON);

	/* Set the bus width */
	switch (ios->bus_width) {
		case bus_width_1:
			hctl_reg &= ~MMCHS_HCTL_DTW;
			con_reg &= ~MMCHS_CON_DW8;
			break;
		case bus_width_4:
			hctl_reg |= MMCHS_HCTL_DTW;
			con_reg &= ~MMCHS_CON_DW8;
			break;
		case bus_width_8:
			con_reg |= MMCHS_CON_DW8;
			break;
	}

	/* Finally write all these settings back to the registers */
	omap_mmc_write_4(sc, MMCHS_HCTL, hctl_reg);
	omap_mmc_write_4(sc, MMCHS_CON, con_reg);

	/* Check if we need to change the external voltage regulator */
	if (sc->sc_cur_power_mode != ios->power_mode) {

		if (ios->power_mode == power_up) {

			/* Set the power level */
			hctl_reg = omap_mmc_read_4(sc, MMCHS_HCTL);
			hctl_reg &= ~(MMCHS_HCTL_SDVS_MASK | MMCHS_HCTL_SDBP);

			if ((ios->vdd == -1) || (ios->vdd >= vdd_240)) {
				mv = 3000;
				hctl_reg |= MMCHS_HCTL_SDVS_V30;
			} else {
				mv = 1800;
				hctl_reg |= MMCHS_HCTL_SDVS_V18;
			}

			omap_mmc_write_4(sc, MMCHS_HCTL, hctl_reg);

			/* Set the desired voltage on the regulator */
			if (sc->sc_vreg_dev && sc->sc_vreg_name)
				twl_vreg_set_voltage(sc->sc_vreg_dev, sc->sc_vreg_name, mv);

			/* Enable the bus power */
			omap_mmc_write_4(sc, MMCHS_HCTL, (hctl_reg | MMCHS_HCTL_SDBP));
			timeout = hz;
			while (!(omap_mmc_read_4(sc, MMCHS_HCTL) & MMCHS_HCTL_SDBP)) {
				if (timeout-- == 0)
					break;
				pause("MMC_PWRON", 1);
			}

		} else if (ios->power_mode == power_off) {
			/* Disable the bus power */
			hctl_reg = omap_mmc_read_4(sc, MMCHS_HCTL);
			omap_mmc_write_4(sc, MMCHS_HCTL, (hctl_reg & ~MMCHS_HCTL_SDBP));

			/* Turn the power off on the voltage regulator */
			if (sc->sc_vreg_dev && sc->sc_vreg_name)
				twl_vreg_set_voltage(sc->sc_vreg_dev, sc->sc_vreg_name, 0);

		} else if (ios->power_mode == power_on) {
			/* Force a card re-initialisation sequence */
			do_card_init = 1;
		}

		/* Save the new power state */
		sc->sc_cur_power_mode = ios->power_mode;
	}

	/* need the MMCHS_SYSCTL register */
	sysctl_reg = omap_mmc_read_4(sc, MMCHS_SYSCTL);

	/* Just in case this hasn't been setup before, set the timeout to the default */
	sysctl_reg &= ~MMCHS_SYSCTL_DTO_MASK;
	sysctl_reg |= MMCHS_SYSCTL_DTO(0xe);

	/* Disable the clock output while configuring the new clock */
	sysctl_reg &= ~(MMCHS_SYSCTL_ICE | MMCHS_SYSCTL_CEN);
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl_reg);

	/* bus mode? */
	if (ios->clock == 0) {
		clkdiv = 0;
	} else {
		clkdiv = sc->sc_ref_freq / ios->clock;
		if (clkdiv < 1)
			clkdiv = 1;
		if ((sc->sc_ref_freq / clkdiv) > ios->clock)
			clkdiv += 1;
		if (clkdiv > 250)
			clkdiv = 250;
	}

	/* Set the new clock divider */
	sysctl_reg &= ~MMCHS_SYSCTL_CLKD_MASK;
	sysctl_reg |= MMCHS_SYSCTL_CLKD(clkdiv);

	/* Write the new settings ... */
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl_reg);
	/* ... write the internal clock enable bit ... */
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl_reg | MMCHS_SYSCTL_ICE);
	/* ... wait for the clock to stablise ... */
	while (((sysctl_reg = omap_mmc_read_4(sc, MMCHS_SYSCTL)) &
	    MMCHS_SYSCTL_ICS) == 0) {
		continue;
	}
	/* ... then enable */
	sysctl_reg |= MMCHS_SYSCTL_CEN;
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl_reg);

	/* If the power state has changed to 'power_on' then run the init sequence*/
	if (do_card_init) {
		omap_mmc_send_init_stream(sc);
	}

	/* Set the bus mode (opendrain or normal) */
	con_reg = omap_mmc_read_4(sc, MMCHS_CON);
	if (ios->bus_mode == opendrain)
		con_reg |= MMCHS_CON_OD;
	else
		con_reg &= ~MMCHS_CON_OD;
	omap_mmc_write_4(sc, MMCHS_CON, con_reg);

	return (0);
}

/**
 *	omap_mmc_acquire_host -
 *	@brdev: mmc bridge device handle
 *	@reqdev: device doing the request
 *
 *	TODO: Is this function needed ?
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 function succeeded
 *
 */
static int
omap_mmc_acquire_host(device_t brdev, device_t reqdev)
{
	struct omap_mmc_softc *sc = device_get_softc(brdev);
	int err = 0;

	OMAP_MMC_LOCK(sc);

	while (sc->bus_busy) {
		msleep(sc, &sc->sc_mtx, PZERO, "mmc", hz / 5);
	}

	sc->bus_busy++;

	OMAP_MMC_UNLOCK(sc);

	return (err);
}

/**
 *	omap_mmc_release_host -
 *	@brdev: mmc bridge device handle
 *	@reqdev: device doing the request
 *
 *	TODO: Is this function needed ?
 *
 *	LOCKING:
 *	none
 *
 *	RETURNS:
 *	0 function succeeded
 *
 */
static int
omap_mmc_release_host(device_t brdev, device_t reqdev)
{
	struct omap_mmc_softc *sc = device_get_softc(brdev);

	OMAP_MMC_LOCK(sc);

	sc->bus_busy--;
	wakeup(sc);

	OMAP_MMC_UNLOCK(sc);

	return (0);
}

/**
 *	omap_mmc_read_ivar - returns driver conf variables
 *	@bus:
 *	@child:
 *	@which: The variable to get the result for
 *	@result: Upon return will store the variable value
 *
 *
 *
 *	LOCKING:
 *	None, caller must hold locks
 *
 *	RETURNS:
 *	0 on success
 *	EINVAL if the variable requested is invalid
 */
static int
omap_mmc_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct omap_mmc_softc *sc = device_get_softc(bus);

	switch (which) {
		case MMCBR_IVAR_BUS_MODE:
			*(int *)result = sc->host.ios.bus_mode;
			break;
		case MMCBR_IVAR_BUS_WIDTH:
			*(int *)result = sc->host.ios.bus_width;
			break;
		case MMCBR_IVAR_CHIP_SELECT:
			*(int *)result = sc->host.ios.chip_select;
			break;
		case MMCBR_IVAR_CLOCK:
			*(int *)result = sc->host.ios.clock;
			break;
		case MMCBR_IVAR_F_MIN:
			*(int *)result = sc->host.f_min;
			break;
		case MMCBR_IVAR_F_MAX:
			*(int *)result = sc->host.f_max;
			break;
		case MMCBR_IVAR_HOST_OCR:
			*(int *)result = sc->host.host_ocr;
			break;
		case MMCBR_IVAR_MODE:
			*(int *)result = sc->host.mode;
			break;
		case MMCBR_IVAR_OCR:
			*(int *)result = sc->host.ocr;
			break;
		case MMCBR_IVAR_POWER_MODE:
			*(int *)result = sc->host.ios.power_mode;
			break;
		case MMCBR_IVAR_VDD:
			*(int *)result = sc->host.ios.vdd;
			break;
		case MMCBR_IVAR_CAPS:
			*(int *)result = sc->host.caps;
			break;
		case MMCBR_IVAR_MAX_DATA:
			*(int *)result = 1;
			break;
		default:
			return (EINVAL);
	}
	return (0);
}

/**
 *	omap_mmc_write_ivar - writes a driver conf variables
 *	@bus:
 *	@child:
 *	@which: The variable to set
 *	@value: The value to write into the variable
 *
 *
 *
 *	LOCKING:
 *	None, caller must hold locks
 *
 *	RETURNS:
 *	0 on success
 *	EINVAL if the variable requested is invalid
 */
static int
omap_mmc_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct omap_mmc_softc *sc = device_get_softc(bus);

	switch (which) {
		case MMCBR_IVAR_BUS_MODE:
			sc->host.ios.bus_mode = value;
			break;
		case MMCBR_IVAR_BUS_WIDTH:
			sc->host.ios.bus_width = value;
			break;
		case MMCBR_IVAR_CHIP_SELECT:
			sc->host.ios.chip_select = value;
			break;
		case MMCBR_IVAR_CLOCK:
			sc->host.ios.clock = value;
			break;
		case MMCBR_IVAR_MODE:
			sc->host.mode = value;
			break;
		case MMCBR_IVAR_OCR:
			sc->host.ocr = value;
			break;
		case MMCBR_IVAR_POWER_MODE:
			sc->host.ios.power_mode = value;
			break;
		case MMCBR_IVAR_VDD:
			sc->host.ios.vdd = value;
			break;
			/* These are read-only */
		case MMCBR_IVAR_CAPS:
		case MMCBR_IVAR_HOST_OCR:
		case MMCBR_IVAR_F_MIN:
		case MMCBR_IVAR_F_MAX:
		case MMCBR_IVAR_MAX_DATA:
			return (EINVAL);
		default:
			return (EINVAL);
	}
	return (0);
}

/**
 *	omap_mmc_hw_init - initialises the MMC/SD/SIO controller
 *	@dev: mmc device handle
 *
 *	Called by the driver attach function during driver initialisation. This
 *	function is responsibly to setup the controller ready for transactions.
 *
 *	LOCKING:
 *	No locking, assumed to only be called during initialisation.
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_hw_init(device_t dev)
{
	struct omap_mmc_softc *sc = device_get_softc(dev);
	clk_ident_t clk;
	unsigned long timeout;
	uint32_t sysctl;
	uint32_t capa;
	uint32_t con;

	/* 1: Enable the controller and interface/functional clocks */
	clk = MMC1_CLK + device_get_unit(dev);

	if (ti_prcm_clk_enable(clk) != 0) {
		device_printf(dev, "Error: failed to enable MMC clock\n");
		return;
	}

	/* 1a: Get the frequency of the source clock */
	if (ti_prcm_clk_get_source_freq(clk, &sc->sc_ref_freq) != 0) {
		device_printf(dev, "Error: failed to get source clock freq\n");
		return;
	}

	/* 2: Issue a softreset to the controller */
	omap_mmc_write_4(sc, MMCHS_SYSCONFIG, 0x0002);
	timeout = 100;
	while ((omap_mmc_read_4(sc, MMCHS_SYSSTATUS) & 0x01) == 0x0) {
		DELAY(1000);
		if (timeout-- == 0) {
			device_printf(dev, "Error: reset operation timed out\n");
			return;
		}
	}

	/* 3: Reset both the command and data state machines */
	sysctl = omap_mmc_read_4(sc, MMCHS_SYSCTL);
	omap_mmc_write_4(sc, MMCHS_SYSCTL, sysctl | MMCHS_SYSCTL_SRA);
	timeout = 100;
	while ((omap_mmc_read_4(sc, MMCHS_SYSCTL) & MMCHS_SYSCTL_SRA) != 0x0) {
		DELAY(1000);
		if (timeout-- == 0) {
			device_printf(dev, "Error: reset operation timed out\n");
			return;
		}
	}

	/* 4: Set initial host configuration (1-bit mode, pwroff) and capabilities */
	omap_mmc_write_4(sc, MMCHS_HCTL, MMCHS_HCTL_SDVS_V30);

	capa = omap_mmc_read_4(sc, MMCHS_CAPA);
	omap_mmc_write_4(sc, MMCHS_CAPA, capa | MMCHS_CAPA_VS30 | MMCHS_CAPA_VS18);

	/* 5: Set the initial bus configuration
	 *       0  CTPL_MMC_SD      : Control Power for DAT1 line
	 *       0  WPP_ACTIVE_HIGH  : Write protect polarity
	 *       0  CDP_ACTIVE_HIGH  : Card detect polarity
	 *       0  CTO_ENABLED      : MMC interrupt command
	 *       0  DW8_DISABLED     : 8-bit mode MMC select
	 *       0  MODE_FUNC        : Mode select
	 *       0  STREAM_DISABLED  : Stream command
	 *       0  HR_DISABLED      : Broadcast host response
	 *       0  INIT_DISABLED    : Send initialization stream
	 *       0  OD_DISABLED      : No Open Drain
	 */
	con = omap_mmc_read_4(sc, MMCHS_CON) & MMCHS_CON_DVAL_MASK;
	omap_mmc_write_4(sc, MMCHS_CON, con);

}

/**
 *	omap_mmc_fini - shutdown the MMC/SD/SIO controller
 *	@dev: mmc device handle
 *
 *	Responsible for shutting done the MMC controller, this function may be
 *	called as part of a reset sequence.
 *
 *	LOCKING:
 *	No locking, assumed to be called during tear-down/reset.
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_hw_fini(device_t dev)
{
	struct omap_mmc_softc *sc = device_get_softc(dev);

	/* Disable all interrupts */
	omap_mmc_write_4(sc, MMCHS_ISE, 0x00000000);
	omap_mmc_write_4(sc, MMCHS_IE, 0x00000000);

	/* Disable the functional and interface clocks */
	switch (device_get_unit(dev)) {
		case 0:
			ti_prcm_clk_disable(MMC1_CLK);
			break;
		case 1:
			ti_prcm_clk_disable(MMC2_CLK);
			break;
		case 2:
			ti_prcm_clk_disable(MMC3_CLK);
			break;
	}
}

/**
 *	omap_mmc_init_dma_channels - initalise the DMA channels
 *	@sc: driver soft context
 *
 *	Attempts to activate an RX and TX DMA channel for the MMC device.
 *
 *	LOCKING:
 *	No locking, assumed to be called during tear-down/reset.
 *
 *	RETURNS:
 *	0 on success, a negative error code on failure.
 */
static int
omap_mmc_init_dma_channels(struct omap_mmc_softc *sc)
{
	int err;
	int unit;
	uint32_t rev;
	int dma_rx_trig = -1;
	int dma_tx_trig = -1;

	/* Get the device unit number, needed for getting the DMA trigger number */
	unit = device_get_unit(sc->sc_dev);

	/* Get the current chip revision */
	rev = ti_revision();
	if ((OMAP_REV_DEVICE(rev) != OMAP4430_DEV) && (unit > 2))
		return(-EINVAL);

	/* Get the DMA MMC triggers */
	switch (unit) {
		case 0:
			dma_tx_trig = 60;
			dma_rx_trig = 61;
			break;
		case 1:
			dma_tx_trig = 46;
			dma_rx_trig = 47;
			break;
		case 2:
			dma_tx_trig = 76;
			dma_rx_trig = 77;
			break;
		/* The following are OMAP4 only */
		case 3:
			dma_tx_trig = 56;
			dma_rx_trig = 57;
			break;
		case 4:
			dma_tx_trig = 58;
			dma_rx_trig = 59;
			break;
		default:
			return(-EINVAL);
	}

	/* Activate a RX channel from the OMAP DMA driver */
	err = omap_dma_activate_channel(&sc->sc_dmach_rd, omap_mmc_dma_intr, sc);
	if (err != 0)
		return(err);

	/* Setup the RX channel for MMC data transfers */
	omap_dma_set_xfer_burst(sc->sc_dmach_rd, OMAP_SDMA_BURST_NONE,
	    OMAP_SDMA_BURST_64);
	omap_dma_set_xfer_data_type(sc->sc_dmach_rd, OMAP_SDMA_DATA_32BITS_SCALAR);
	omap_dma_sync_params(sc->sc_dmach_rd, dma_rx_trig,
	    OMAP_SDMA_SYNC_PACKET | OMAP_SDMA_SYNC_TRIG_ON_SRC);
	omap_dma_set_addr_mode(sc->sc_dmach_rd, OMAP_SDMA_ADDR_CONSTANT,
	    OMAP_SDMA_ADDR_POST_INCREMENT);

	/* Activate and configure the TX DMA channel */
	err = omap_dma_activate_channel(&sc->sc_dmach_wr, omap_mmc_dma_intr, sc);
	if (err != 0)
		return(err);

	/* Setup the TX channel for MMC data transfers */
	omap_dma_set_xfer_burst(sc->sc_dmach_wr, OMAP_SDMA_BURST_64,
	    OMAP_SDMA_BURST_NONE);
	omap_dma_set_xfer_data_type(sc->sc_dmach_wr, OMAP_SDMA_DATA_32BITS_SCALAR);
	omap_dma_sync_params(sc->sc_dmach_wr, dma_tx_trig,
	    OMAP_SDMA_SYNC_PACKET | OMAP_SDMA_SYNC_TRIG_ON_DST);
	omap_dma_set_addr_mode(sc->sc_dmach_wr, OMAP_SDMA_ADDR_POST_INCREMENT,
	    OMAP_SDMA_ADDR_CONSTANT);

	return(0);
}

/**
 *	omap_mmc_deactivate - deactivates the driver
 *	@dev: mmc device handle
 *
 *	Unmaps the register set and releases the IRQ resource.
 *
 *	LOCKING:
 *	None required
 *
 *	RETURNS:
 *	nothing
 */
static void
omap_mmc_deactivate(device_t dev)
{
	struct omap_mmc_softc *sc= device_get_softc(dev);

	/* Remove the IRQ handler */
	if (sc->sc_irq_h != NULL) {
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_h);
		sc->sc_irq_h = NULL;
	}

	/* Do the generic detach */
	bus_generic_detach(sc->sc_dev);

	/* Deactivate the DMA channels */
	omap_dma_deactivate_channel(sc->sc_dmach_rd);
	omap_dma_deactivate_channel(sc->sc_dmach_wr);

	/* Unmap the MMC controller registers */
	if (sc->sc_mem_res != 0) {
		bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->sc_irq_res),
		    sc->sc_mem_res);
		sc->sc_mem_res = NULL;
	}

	/* Release the IRQ resource */
	if (sc->sc_irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->sc_irq_res),
		    sc->sc_irq_res);
		sc->sc_irq_res = NULL;
	}

	return;
}

/**
 *	omap_mmc_activate - activates the driver
 *	@dev: mmc device handle
 *
 *	Maps in the register set and requests an IRQ handler for the MMC controller.
 *
 *	LOCKING:
 *	None required
 *
 *	RETURNS:
 *	0 on sucess
 *	ENOMEM if failed to map register set
 */
static int
omap_mmc_activate(device_t dev)
{
	struct omap_mmc_softc *sc = device_get_softc(dev);
	unsigned long addr;
	int rid;
	int err;

	/* Get the memory resource for the register mapping */
	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem_res == NULL)
		panic("%s: Cannot map registers", device_get_name(dev));

	/* Allocate an IRQ resource for the MMC controller */
	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sc_irq_res == NULL)
		goto errout;

	/* Allocate DMA tags and maps */
	err = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, MAXPHYS, 1, MAXPHYS, BUS_DMA_ALLOCNOW, NULL,
	    NULL, &sc->sc_dmatag);
	if (err != 0)
		goto errout;

	err = bus_dmamap_create(sc->sc_dmatag, 0,  &sc->sc_dmamap);
	if (err != 0)
		goto errout;

	/* Initialise the DMA channels to be used by the controller */
	err = omap_mmc_init_dma_channels(sc);
	if (err != 0)
		goto errout;

	/* Set the register offset */
	if (ti_chip() == CHIP_OMAP_3)
		sc->sc_reg_off = OMAP3_MMCHS_REG_OFFSET;
	else if (ti_chip() == CHIP_OMAP_4)
		sc->sc_reg_off = OMAP4_MMCHS_REG_OFFSET;
	else
		panic("Unknown OMAP device\n");

	/* Get the physical address of the MMC data register, needed for DMA */
	addr = vtophys(rman_get_start(sc->sc_mem_res));
	sc->sc_data_reg_paddr = addr + sc->sc_reg_off + MMCHS_DATA;

	/* Set the initial power state to off */
	sc->sc_cur_power_mode = power_off;

	return (0);

errout:
	omap_mmc_deactivate(dev);
	return (ENOMEM);
}

/**
 *	omap_mmc_probe - probe function for the driver
 *	@dev: mmc device handle
 *
 *
 *
 *	RETURNS:
 *	always returns 0
 */
static int
omap_mmc_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ti,omap_mmc"))
		return (ENXIO);

	device_set_desc(dev, "TI OMAP MMC/SD/SDIO host bridge");
	return (0);
}

/**
 *	omap_mmc_attach - attach function for the driver
 *	@dev: mmc device handle
 *
 *	Driver initialisation, sets-up the bus mappings, DMA mapping/channels and
 *	the actual controller by calling omap_mmc_init().
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
omap_mmc_attach(device_t dev)
{
	struct omap_mmc_softc *sc = device_get_softc(dev);
	int unit = device_get_unit(dev);
	int err;
	device_t child;

	/* Save the device and bus tag */
	sc->sc_dev = dev;

	/* Initiate the mtex lock */
	OMAP_MMC_LOCK_INIT(sc);

	/* Indicate the DMA channels haven't yet been allocated */
	sc->sc_dmach_rd = (unsigned int)-1;
	sc->sc_dmach_wr = (unsigned int)-1;

	/* Get the hint'ed write detect pin */
	if (resource_int_value("omap_mmc", unit, "wp_gpio", &sc->sc_wp_gpio_pin) != 0){
		sc->sc_wp_gpio_pin = -1;
	} else {
		/* Get the GPIO device, we need this for the write protect pin */
		sc->sc_gpio_dev = devclass_get_device(devclass_find("gpio"), 0);
		if (sc->sc_gpio_dev == NULL)
			device_printf(dev, "Error: failed to get the GPIO device\n");
		else
			GPIO_PIN_SETFLAGS(sc->sc_gpio_dev, sc->sc_wp_gpio_pin,
			                  GPIO_PIN_INPUT);
	}

	/* Get the TWL voltage regulator device, we need this to for setting the
	 * voltage of the bus on certain OMAP platforms.
	 */
	sc->sc_vreg_name = NULL;

	/* TODO: add voltage regulator knob to FDT */
#ifdef notyet
	sc->sc_vreg_dev = devclass_get_device(devclass_find("twl_vreg"), 0);
	if (sc->sc_vreg_dev == NULL) {
		device_printf(dev, "Error: failed to get the votlage regulator"
		    " device\n");
		sc->sc_vreg_name = NULL;
	}
#endif

	/* Activate the device */
	err = omap_mmc_activate(dev);
	if (err)
		goto out;

	/* Initialise the controller */
	omap_mmc_hw_init(dev);

	/* Activate the interrupt and attach a handler */
	err = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, omap_mmc_intr, sc, &sc->sc_irq_h);
	if (err != 0)
		goto out;

	/* Add host details */
	sc->host.f_min = sc->sc_ref_freq / 1023;
	sc->host.f_max = sc->sc_ref_freq;
	sc->host.host_ocr = MMC_OCR_290_300 | MMC_OCR_300_310;
	sc->host.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA;

	child = device_add_child(dev, "mmc", 0);

	device_set_ivars(dev, &sc->host);
	err = bus_generic_attach(dev);

out:
	if (err) {
		OMAP_MMC_LOCK_DESTROY(sc);
		omap_mmc_deactivate(dev);

		if (sc->sc_dmach_rd != (unsigned int)-1)
			omap_dma_deactivate_channel(sc->sc_dmach_rd);
		if (sc->sc_dmach_wr != (unsigned int)-1)
			omap_dma_deactivate_channel(sc->sc_dmach_wr);
	}

	return (err);
}

/**
 *	omap_mmc_detach - dettach function for the driver
 *	@dev: mmc device handle
 *
 *	Shutdowns the controll and release resources allocated by the driver.
 *
 *	RETURNS:
 *	Always returns 0.
 */
static int
omap_mmc_detach(device_t dev)
{
	struct omap_mmc_softc *sc = device_get_softc(dev);

	omap_mmc_hw_fini(dev);
	omap_mmc_deactivate(dev);

	omap_dma_deactivate_channel(sc->sc_dmach_wr);
	omap_dma_deactivate_channel(sc->sc_dmach_rd);

	return (0);
}

static device_method_t omap_mmc_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, omap_mmc_probe),
	DEVMETHOD(device_attach, omap_mmc_attach),
	DEVMETHOD(device_detach, omap_mmc_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	omap_mmc_read_ivar),
	DEVMETHOD(bus_write_ivar,	omap_mmc_write_ivar),

	/* mmcbr_if - MMC state machine callbacks */
	DEVMETHOD(mmcbr_update_ios, omap_mmc_update_ios),
	DEVMETHOD(mmcbr_request, omap_mmc_request),
	DEVMETHOD(mmcbr_get_ro, omap_mmc_get_ro),
	DEVMETHOD(mmcbr_acquire_host, omap_mmc_acquire_host),
	DEVMETHOD(mmcbr_release_host, omap_mmc_release_host),

	{0, 0},
};

static driver_t omap_mmc_driver = {
	"omap_mmc",
	omap_mmc_methods,
	sizeof(struct omap_mmc_softc),
};
static devclass_t omap_mmc_devclass;

DRIVER_MODULE(omap_mmc, simplebus, omap_mmc_driver, omap_mmc_devclass, 0, 0);
MODULE_DEPEND(omap_mmc, ti_prcm, 1, 1, 1);
MODULE_DEPEND(omap_mmc, omap_dma, 1, 1, 1);
MODULE_DEPEND(omap_mmc, ti_gpio, 1, 1, 1);

/* FIXME: MODULE_DEPEND(omap_mmc, twl_vreg, 1, 1, 1); */
