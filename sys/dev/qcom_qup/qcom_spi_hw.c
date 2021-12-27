/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021, Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>
#include "spibus_if.h"

#include <dev/qcom_qup/qcom_spi_var.h>
#include <dev/qcom_qup/qcom_spi_reg.h>
#include <dev/qcom_qup/qcom_qup_reg.h>
#include <dev/qcom_qup/qcom_spi_debug.h>

int
qcom_spi_hw_read_controller_transfer_sizes(struct qcom_spi_softc *sc)
{
	uint32_t reg, val;

	reg = QCOM_SPI_READ_4(sc, QUP_IO_M_MODES);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: QUP_IO_M_MODES=0x%08x\n", __func__, reg);

	/* Input block size */
	val = (reg >> QUP_IO_M_INPUT_BLOCK_SIZE_SHIFT)
	    & QUP_IO_M_INPUT_BLOCK_SIZE_MASK;
	if (val == 0)
		sc->config.input_block_size = 4;
	else
		sc->config.input_block_size = val * 16;

	/* Output block size */
	val = (reg >> QUP_IO_M_OUTPUT_BLOCK_SIZE_SHIFT)
	    & QUP_IO_M_OUTPUT_BLOCK_SIZE_MASK;
	if (val == 0)
		sc->config.output_block_size = 4;
	else
		sc->config.output_block_size = val * 16;

	/* Input FIFO size */
	val = (reg >> QUP_IO_M_INPUT_FIFO_SIZE_SHIFT)
	    & QUP_IO_M_INPUT_FIFO_SIZE_MASK;
	sc->config.input_fifo_size =
	    sc->config.input_block_size * (2 << val);

	/* Output FIFO size */
	val = (reg >> QUP_IO_M_OUTPUT_FIFO_SIZE_SHIFT)
	    & QUP_IO_M_OUTPUT_FIFO_SIZE_MASK;
	sc->config.output_fifo_size =
	    sc->config.output_block_size * (2 << val);

	return (0);
}

static bool
qcom_spi_hw_qup_is_state_valid_locked(struct qcom_spi_softc *sc)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	reg = QCOM_SPI_READ_4(sc, QUP_STATE);
	QCOM_SPI_BARRIER_READ(sc);

	return !! (reg & QUP_STATE_VALID);
}

static int
qcom_spi_hw_qup_wait_state_valid_locked(struct qcom_spi_softc *sc)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (qcom_spi_hw_qup_is_state_valid_locked(sc))
			break;
	}
	if (i >= 10) {
		device_printf(sc->sc_dev,
		    "ERROR: timeout waiting for valid state\n");
		return (ENXIO);
	}
	return (0);
}

static bool
qcom_spi_hw_is_opmode_dma_locked(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	if (sc->state.transfer_mode == QUP_IO_M_MODE_DMOV)
		return (true);
	if (sc->state.transfer_mode == QUP_IO_M_MODE_BAM)
		return (true);
	return (false);
}

int
qcom_spi_hw_qup_set_state_locked(struct qcom_spi_softc *sc, uint32_t state)
{
	uint32_t cur_state;
	int ret;

	QCOM_SPI_ASSERT_LOCKED(sc);

	/* Wait until the state becomes valid */
	ret = qcom_spi_hw_qup_wait_state_valid_locked(sc);
	if (ret != 0) {
		return (ret);
	}

	cur_state = QCOM_SPI_READ_4(sc, QUP_STATE);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_STATE_CHANGE,
	    "%s: target state=%d, cur_state=0x%08x\n",
	    __func__, state, cur_state);

	/*
	 * According to the QUP specification, when going
	 * from PAUSE to RESET, two writes are required.
	 */
	if ((state == QUP_STATE_RESET)
	    && ((cur_state & QUP_STATE_MASK) == QUP_STATE_PAUSE)) {
		QCOM_SPI_WRITE_4(sc, QUP_STATE, QUP_STATE_CLEAR);
		QCOM_SPI_BARRIER_WRITE(sc);
		QCOM_SPI_WRITE_4(sc, QUP_STATE, QUP_STATE_CLEAR);
		QCOM_SPI_BARRIER_WRITE(sc);
	} else {
		cur_state &= ~QUP_STATE_MASK;
		cur_state |= state;
		QCOM_SPI_WRITE_4(sc, QUP_STATE, cur_state);
		QCOM_SPI_BARRIER_WRITE(sc);
	}

	/* Wait until the state becomes valid */
	ret = qcom_spi_hw_qup_wait_state_valid_locked(sc);
	if (ret != 0) {
		return (ret);
	}

	cur_state = QCOM_SPI_READ_4(sc, QUP_STATE);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_STATE_CHANGE,
	    "%s: FINISH: target state=%d, cur_state=0x%08x\n",
	    __func__, state, cur_state);

	return (0);
}

/*
 * Do initial QUP setup.
 *
 * This is initially for the SPI driver; it would be interesting to see how
 * much of this is the same with the I2C/HSUART paths.
 */
int
qcom_spi_hw_qup_init_locked(struct qcom_spi_softc *sc)
{
	int ret;

	QCOM_SPI_ASSERT_LOCKED(sc);

	/* Full hardware reset */
	(void) qcom_spi_hw_do_full_reset(sc);

	ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RESET);
	if (ret != 0) {
		device_printf(sc->sc_dev, "ERROR: %s: couldn't reset\n",
		    __func__);
		goto error;
	}

	QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, 0);
	QCOM_SPI_WRITE_4(sc, QUP_IO_M_MODES, 0);
	/* Note: no QUP_OPERATIONAL_MASK in QUP v1 */
	if (! QCOM_SPI_QUP_VERSION_V1(sc))
		QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL_MASK, 0);

	/* Explicitly disable input overrun in QUP v1 */
	if (QCOM_SPI_QUP_VERSION_V1(sc))
		QCOM_SPI_WRITE_4(sc, QUP_ERROR_FLAGS_EN,
		    QUP_ERROR_OUTPUT_OVER_RUN
		    | QUP_ERROR_INPUT_UNDER_RUN
		    | QUP_ERROR_OUTPUT_UNDER_RUN);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
error:
	return (ret);
}

/*
 * Do initial SPI setup.
 */
int
qcom_spi_hw_spi_init_locked(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	/* Initial SPI error flags */
	QCOM_SPI_WRITE_4(sc, SPI_ERROR_FLAGS_EN,
	    QUP_ERROR_INPUT_UNDER_RUN
	    | QUP_ERROR_OUTPUT_UNDER_RUN);
	QCOM_SPI_BARRIER_WRITE(sc);

	/* Initial SPI config */
	QCOM_SPI_WRITE_4(sc, SPI_CONFIG, 0);
	QCOM_SPI_BARRIER_WRITE(sc);

	/* Initial CS/tri-state io control config */
	QCOM_SPI_WRITE_4(sc, SPI_IO_CONTROL,
	    SPI_IO_C_NO_TRI_STATE
	    | SPI_IO_C_CS_SELECT(sc->config.cs_select));
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Force the currently selected device CS line to be active
 * or inactive.
 *
 * This forces it to be active or inactive rather than letting
 * the SPI transfer machine do its thing.  If you want to be able
 * break up a big transaction into a handful of smaller ones,
 * without toggling /CS_n for that device, then you need it forced.
 * (If you toggle the /CS_n to the device to inactive then active,
 * NOR/NAND devices tend to stop a block transfer.)
 */
int
qcom_spi_hw_spi_cs_force(struct qcom_spi_softc *sc, int cs, bool enable)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_CHIPSELECT,
	    "%s: called, enable=%u\n",
	    __func__, enable);

	reg = QCOM_SPI_READ_4(sc, SPI_IO_CONTROL);
	if (enable)
		reg |= SPI_IO_C_FORCE_CS;
	else
		reg &= ~SPI_IO_C_FORCE_CS;
	reg &= ~SPI_IO_C_CS_SELECT_MASK;
	reg |= SPI_IO_C_CS_SELECT(cs);
	QCOM_SPI_WRITE_4(sc, SPI_IO_CONTROL, reg);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

/*
 * ACK/store current interrupt flag state.
 */
int
qcom_spi_hw_interrupt_handle(struct qcom_spi_softc *sc)
{
	uint32_t qup_error, spi_error, op_flags;

	QCOM_SPI_ASSERT_LOCKED(sc);

	/* Get QUP/SPI state */
	qup_error = QCOM_SPI_READ_4(sc, QUP_ERROR_FLAGS);
	spi_error = QCOM_SPI_READ_4(sc, SPI_ERROR_FLAGS);
	op_flags = QCOM_SPI_READ_4(sc, QUP_OPERATIONAL);

	/* ACK state */
	QCOM_SPI_WRITE_4(sc, QUP_ERROR_FLAGS, qup_error);
	QCOM_SPI_WRITE_4(sc, SPI_ERROR_FLAGS, spi_error);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_INTR,
	    "%s: called; qup=0x%08x, spi=0x%08x, op=0x%08x\n",
	    __func__,
	    qup_error,
	    spi_error,
	    op_flags);

	/* handle error flags */
	if (qup_error != 0) {
		device_printf(sc->sc_dev, "ERROR: (QUP) mask=0x%08x\n",
		    qup_error);
		sc->intr.error = true;
	}
	if (spi_error != 0) {
		device_printf(sc->sc_dev, "ERROR: (SPI) mask=0x%08x\n",
		    spi_error);
		sc->intr.error = true;
	}

	/* handle operational state */
	if (qcom_spi_hw_is_opmode_dma_locked(sc)) {
		/* ACK interrupts now */
		QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, op_flags);
		if ((op_flags & QUP_OP_IN_SERVICE_FLAG)
		    && (op_flags & QUP_OP_MAX_INPUT_DONE_FLAG))
			sc->intr.rx_dma_done = true;
		if ((op_flags & QUP_OP_OUT_SERVICE_FLAG)
		    && (op_flags & QUP_OP_MAX_OUTPUT_DONE_FLAG))
			sc->intr.tx_dma_done = true;
	} else {
		/* FIFO/Block */
		if (op_flags & QUP_OP_IN_SERVICE_FLAG)
			sc->intr.do_rx = true;
		if (op_flags & QUP_OP_OUT_SERVICE_FLAG)
			sc->intr.do_tx = true;
	}

	/* Check if we've finished transfers */
	if (op_flags & QUP_OP_MAX_INPUT_DONE_FLAG)
		sc->intr.done = true;
	if (sc->intr.error)
		sc->intr.done = true;

	return (0);
}

/*
 * Make initial transfer selections based on the transfer sizes
 * and alignment.
 *
 * For now this'll just default to FIFO until that works, and then
 * will grow to include BLOCK / DMA as appropriate.
 */
int
qcom_spi_hw_setup_transfer_selection(struct qcom_spi_softc *sc, uint32_t len)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	/*
	 * For now only support doing a single FIFO transfer.
	 * The main PIO transfer routine loop will break it up for us.
	 */
	sc->state.transfer_mode = QUP_IO_M_MODE_FIFO;
	sc->transfer.tx_offset = 0;
	sc->transfer.rx_offset = 0;
	sc->transfer.tx_len = 0;
	sc->transfer.rx_len = 0;
	sc->transfer.tx_buf = NULL;
	sc->transfer.rx_buf = NULL;

	/*
	 * If we're sending a DWORD multiple sized block (like IO buffers)
	 * then we can totally just use the DWORD size transfers.
	 *
	 * This is really only valid for PIO/block modes; I'm not yet
	 * sure what we should do for DMA modes.
	 */
	if (len > 0 && len % 4 == 0)
		sc->state.transfer_word_size = 4;
	else
		sc->state.transfer_word_size = 1;

	return (0);
}

/*
 * Blank the transfer state after a full transfer is completed.
 */
int
qcom_spi_hw_complete_transfer(struct qcom_spi_softc *sc)
{
	QCOM_SPI_ASSERT_LOCKED(sc);

	sc->state.transfer_mode = QUP_IO_M_MODE_FIFO;
	sc->transfer.tx_offset = 0;
	sc->transfer.rx_offset = 0;
	sc->transfer.tx_len = 0;
	sc->transfer.rx_len = 0;
	sc->transfer.tx_buf = NULL;
	sc->transfer.rx_buf = NULL;
	sc->state.transfer_word_size = 0;
	return (0);
}

/*
 * Configure up the transfer selection for the current transfer.
 *
 * This calculates how many words we can transfer in the current
 * transfer and what's left to transfer.
 */
int
qcom_spi_hw_setup_current_transfer(struct qcom_spi_softc *sc)
{
	uint32_t bytes_left;

	QCOM_SPI_ASSERT_LOCKED(sc);

	/*
	 * XXX For now, base this on the TX side buffer size, not both.
	 * Later on we'll want to configure it based on the MAX of
	 * either and just eat up the dummy values in the PIO
	 * routines.  (For DMA it's .. more annoyingly complicated
	 * if the transfer sizes are not symmetrical.)
	 */
	bytes_left = sc->transfer.tx_len - sc->transfer.tx_offset;

	if (sc->state.transfer_mode == QUP_IO_M_MODE_FIFO) {
		/*
		 * For FIFO transfers the num_words limit depends upon
		 * the word size, FIFO size and how many bytes are left.
		 * It definitely will be under SPI_MAX_XFER so don't
		 * worry about that here.
		 */
		sc->transfer.num_words = bytes_left / sc->state.transfer_word_size;
		sc->transfer.num_words = MIN(sc->transfer.num_words,
		    sc->config.input_fifo_size / sizeof(uint32_t));
	} else if (sc->state.transfer_mode == QUP_IO_M_MODE_BLOCK) {
		/*
		 * For BLOCK transfers the logic will be a little different.
		 * Instead of it being based on the maximum input_fifo_size,
		 * it'll be broken down into the 'words per block" size but
		 * our maximum transfer size will ACTUALLY be capped by
		 * SPI_MAX_XFER (65536-64 bytes.)  Each transfer
		 * will end up being in multiples of a block until the
		 * last transfer.
		 */
		sc->transfer.num_words = bytes_left / sc->state.transfer_word_size;
		sc->transfer.num_words = MIN(sc->transfer.num_words,
		    SPI_MAX_XFER);
	}


	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	"%s: transfer.tx_len=%u,"
	    "transfer.tx_offset=%u,"
	    " transfer_word_size=%u,"
	    " bytes_left=%u, num_words=%u, fifo_word_max=%u\n",
	    __func__,
	    sc->transfer.tx_len,
	    sc->transfer.tx_offset,
	    sc->state.transfer_word_size,
	    bytes_left,
	    sc->transfer.num_words,
	    sc->config.input_fifo_size / sizeof(uint32_t));

	return (0);
}

/*
 * Setup the PIO FIFO transfer count.
 *
 * Note that we get a /single/ TX/RX phase up to these num_words
 * transfers.
 */
int
qcom_spi_hw_setup_pio_transfer_cnt(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_WRITE_4(sc, QUP_MX_READ_CNT, sc->transfer.num_words);
	QCOM_SPI_WRITE_4(sc, QUP_MX_WRITE_CNT, sc->transfer.num_words);
	QCOM_SPI_WRITE_4(sc, QUP_MX_INPUT_CNT, 0);
	QCOM_SPI_WRITE_4(sc, QUP_MX_OUTPUT_CNT, 0);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: num_words=%u\n", __func__,
	    sc->transfer.num_words);

	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

/*
 * Setup the PIO BLOCK transfer count.
 *
 * This sets up the total transfer size, in TX/RX FIFO block size
 * chunks.  We will get multiple notifications when a block sized
 * chunk of data is avaliable or required.
 */
int
qcom_spi_hw_setup_block_transfer_cnt(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_WRITE_4(sc, QUP_MX_READ_CNT, 0);
	QCOM_SPI_WRITE_4(sc, QUP_MX_WRITE_CNT, 0);
	QCOM_SPI_WRITE_4(sc, QUP_MX_INPUT_CNT, sc->transfer.num_words);
	QCOM_SPI_WRITE_4(sc, QUP_MX_OUTPUT_CNT, sc->transfer.num_words);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

int
qcom_spi_hw_setup_io_modes(struct qcom_spi_softc *sc)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	reg = QCOM_SPI_READ_4(sc, QUP_IO_M_MODES);

	reg &= ~((QUP_IO_M_INPUT_MODE_MASK << QUP_IO_M_INPUT_MODE_SHIFT)
	    | (QUP_IO_M_OUTPUT_MODE_MASK << QUP_IO_M_OUTPUT_MODE_SHIFT));

	/*
	 * If it's being done using DMA then the hardware will
	 * need to pack and unpack the byte stream into the word/dword
	 * stream being expected by the SPI/QUP micro engine.
	 *
	 * For PIO modes we're doing the pack/unpack in software,
	 * see the pio/block transfer routines.
	 */
	if (qcom_spi_hw_is_opmode_dma_locked(sc))
		reg |= (QUP_IO_M_PACK_EN | QUP_IO_M_UNPACK_EN);
	else
		reg &= ~(QUP_IO_M_PACK_EN | QUP_IO_M_UNPACK_EN);

	/* Transfer mode */
	reg |= ((sc->state.transfer_mode & QUP_IO_M_INPUT_MODE_MASK)
	    << QUP_IO_M_INPUT_MODE_SHIFT);
	reg |= ((sc->state.transfer_mode & QUP_IO_M_OUTPUT_MODE_MASK)
	    << QUP_IO_M_OUTPUT_MODE_SHIFT);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: QUP_IO_M_MODES=0x%08x\n", __func__, reg);

	QCOM_SPI_WRITE_4(sc, QUP_IO_M_MODES, reg);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

int
qcom_spi_hw_setup_spi_io_clock_polarity(struct qcom_spi_softc *sc,
    bool cpol)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	reg = QCOM_SPI_READ_4(sc, SPI_IO_CONTROL);

	if (cpol)
		reg |= SPI_IO_C_CLK_IDLE_HIGH;
	else
		reg &= ~SPI_IO_C_CLK_IDLE_HIGH;

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: SPI_IO_CONTROL=0x%08x\n", __func__, reg);

	QCOM_SPI_WRITE_4(sc, SPI_IO_CONTROL, reg);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

int
qcom_spi_hw_setup_spi_config(struct qcom_spi_softc *sc, uint32_t clock_val,
    bool cpha)
{
	uint32_t reg;

	/*
	 * For now we don't have a way to configure loopback SPI for testing,
	 * or the clock/transfer phase.  When we do then here's where we
	 * would put that.
	 */

	QCOM_SPI_ASSERT_LOCKED(sc);

	reg = QCOM_SPI_READ_4(sc, SPI_CONFIG);
	reg &= ~SPI_CONFIG_LOOPBACK;

	if (cpha)
		reg &= ~SPI_CONFIG_INPUT_FIRST;
	else
		reg |= SPI_CONFIG_INPUT_FIRST;

	/*
	 * If the frequency is above SPI_HS_MIN_RATE then enable high speed.
	 * This apparently improves stability.
	 *
	 * Note - don't do this if SPI loopback is enabled!
	 */
	if (clock_val >= SPI_HS_MIN_RATE)
		reg |= SPI_CONFIG_HS_MODE;
	else
		reg &= ~SPI_CONFIG_HS_MODE;

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: SPI_CONFIG=0x%08x\n", __func__, reg);

	QCOM_SPI_WRITE_4(sc, SPI_CONFIG, reg);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

int
qcom_spi_hw_setup_qup_config(struct qcom_spi_softc *sc, bool is_tx, bool is_rx)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	reg = QCOM_SPI_READ_4(sc, QUP_CONFIG);
	reg &= ~(QUP_CONFIG_NO_INPUT | QUP_CONFIG_NO_OUTPUT | QUP_CONFIG_N);

	/* SPI mode */
	reg |= QUP_CONFIG_SPI_MODE;

	/* bitmask for number of bits per word being used in each FIFO slot */
	reg |= ((sc->state.transfer_word_size * 8) - 1) & QUP_CONFIG_N;

	/*
	 * When doing DMA we need to configure whether we are shifting
	 * data in, out, and/or both.  For PIO/block modes it must stay
	 * unset.
	 */
	if (qcom_spi_hw_is_opmode_dma_locked(sc)) {
		if (is_rx == false)
			reg |= QUP_CONFIG_NO_INPUT;
		if (is_tx == false)
			reg |= QUP_CONFIG_NO_OUTPUT;
	}

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
	    "%s: QUP_CONFIG=0x%08x\n", __func__, reg);

	QCOM_SPI_WRITE_4(sc, QUP_CONFIG, reg);
	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

int
qcom_spi_hw_setup_operational_mask(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	if (QCOM_SPI_QUP_VERSION_V1(sc)) {
		QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TRANSFER_SETUP,
		    "%s: skipping, qupv1\n", __func__);
		return (0);
	}

	if (qcom_spi_hw_is_opmode_dma_locked(sc))
		QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL_MASK,
		    QUP_OP_IN_SERVICE_FLAG | QUP_OP_OUT_SERVICE_FLAG);
	else
		QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL_MASK, 0);

	QCOM_SPI_BARRIER_WRITE(sc);

	return (0);
}

/*
 * ACK that we already have serviced the output FIFO.
 */
int
qcom_spi_hw_ack_write_pio_fifo(struct qcom_spi_softc *sc)
{

	QCOM_SPI_ASSERT_LOCKED(sc);
	QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, QUP_OP_OUT_SERVICE_FLAG);
	QCOM_SPI_BARRIER_WRITE(sc);
	return (0);
}

int
qcom_spi_hw_ack_opmode(struct qcom_spi_softc *sc)
{
	uint32_t reg;

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_BARRIER_READ(sc);
	reg = QCOM_SPI_READ_4(sc, QUP_OPERATIONAL);
	QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, QUP_OP_OUT_SERVICE_FLAG);
	QCOM_SPI_BARRIER_WRITE(sc);
	return (0);

}

/*
 * Read the value from the TX buffer into the given 32 bit DWORD,
 * pre-shifting it into the place requested.
 *
 * Returns true if there was a byte available, false otherwise.
 */
static bool
qcom_spi_hw_write_from_tx_buf(struct qcom_spi_softc *sc, int shift,
    uint32_t *val)
{

	QCOM_SPI_ASSERT_LOCKED(sc);

	if (sc->transfer.tx_buf == NULL)
		return false;

	if (sc->transfer.tx_offset < sc->transfer.tx_len) {
		*val |= (sc->transfer.tx_buf[sc->transfer.tx_offset] & 0xff)
		    << shift;
		sc->transfer.tx_offset++;
		return true;
	}

	return false;
}

int
qcom_spi_hw_write_pio_fifo(struct qcom_spi_softc *sc)
{
	uint32_t i;
	int num_bytes = 0;

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, QUP_OP_OUT_SERVICE_FLAG);
	QCOM_SPI_BARRIER_WRITE(sc);

	/*
	 * Loop over the transfer num_words, do complain if we are full.
	 */
	for (i = 0; i < sc->transfer.num_words; i++) {
		uint32_t reg;

		/* Break if FIFO is full */
		if ((QCOM_SPI_READ_4(sc, QUP_OPERATIONAL)
		    & QUP_OP_OUT_FIFO_FULL) != 0) {
			device_printf(sc->sc_dev, "%s: FIFO full\n", __func__);
			break;
		}

		/*
		 * Handle 1, 2, 4 byte transfer packing rules.
		 *
		 * Unlike read, where the shifting is done towards the MSB
		 * for us by default, we have to do it ourselves for transmit.
		 * There's a bit that one can set to do the preshifting
		 * (and u-boot uses it!) but I'll stick with what Linux is
		 * doing to make it easier for future maintenance.
		 *
		 * The format is the same as 4 byte RX - 0xaabbccdd;
		 * the byte ordering on the wire being aa, bb, cc, dd.
		 */
		reg = 0;
		if (sc->state.transfer_word_size == 1) {
			if (qcom_spi_hw_write_from_tx_buf(sc, 24, &reg))
				num_bytes++;
		} else if (sc->state.transfer_word_size == 2) {
			if (qcom_spi_hw_write_from_tx_buf(sc, 24, &reg))
				num_bytes++;
			if (qcom_spi_hw_write_from_tx_buf(sc, 16, &reg))
				num_bytes++;
		} else if (sc->state.transfer_word_size == 4) {
			if (qcom_spi_hw_write_from_tx_buf(sc, 24, &reg))
				num_bytes++;
			if (qcom_spi_hw_write_from_tx_buf(sc, 16, &reg))
				num_bytes++;
			if (qcom_spi_hw_write_from_tx_buf(sc, 8, &reg))
				num_bytes++;
			if (qcom_spi_hw_write_from_tx_buf(sc, 0, &reg))
				num_bytes++;
		}

		/*
		 * always shift out something in case we need phantom
		 * writes to finish things up whilst we read a reply
		 * payload.
		 */
		QCOM_SPI_WRITE_4(sc, QUP_OUTPUT_FIFO, reg);
		QCOM_SPI_BARRIER_WRITE(sc);
	}

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TX_FIFO,
	    "%s: wrote %d bytes (%d fifo slots)\n",
	    __func__, num_bytes, sc->transfer.num_words);

	return (0);
}

int
qcom_spi_hw_write_pio_block(struct qcom_spi_softc *sc)
{
	/* Not yet implemented */
	return (ENXIO);
}

/*
 * Read data into the the RX buffer and increment the RX offset.
 *
 * Return true if the byte was saved into the RX buffer, else
 * return false.
 */
static bool
qcom_spi_hw_read_into_rx_buf(struct qcom_spi_softc *sc, uint8_t val)
{
	QCOM_SPI_ASSERT_LOCKED(sc);

	if (sc->transfer.rx_buf == NULL)
		return false;

	/* Make sure we aren't overflowing the receive buffer */
	if (sc->transfer.rx_offset < sc->transfer.rx_len) {
		sc->transfer.rx_buf[sc->transfer.rx_offset] = val;
		sc->transfer.rx_offset++;
		return true;
	}
	return false;
}

/*
 * Read "n_words" transfers, and push those bytes into the receive buffer.
 * Make sure we have enough space, and make sure we don't overflow the
 * read buffer size too!
 */
int
qcom_spi_hw_read_pio_fifo(struct qcom_spi_softc *sc)
{
	uint32_t i;
	uint32_t reg;
	int num_bytes = 0;

	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_WRITE_4(sc, QUP_OPERATIONAL, QUP_OP_IN_SERVICE_FLAG);
	QCOM_SPI_BARRIER_WRITE(sc);

	for (i = 0; i < sc->transfer.num_words; i++) {
		/* Break if FIFO is empty */
		QCOM_SPI_BARRIER_READ(sc);
		reg = QCOM_SPI_READ_4(sc, QUP_OPERATIONAL);
		if ((reg & QUP_OP_IN_FIFO_NOT_EMPTY) == 0) {
			device_printf(sc->sc_dev, "%s: FIFO empty\n", __func__);
			break;
		}

		/*
		 * Always read num_words up to FIFO being non-empty; that way
		 * if we have mis-matching TX/RX buffer sizes for some reason
		 * we will read the needed phantom bytes.
		 */
		reg = QCOM_SPI_READ_4(sc, QUP_INPUT_FIFO);

		/*
		 * Unpack the receive buffer based on whether we are
		 * doing 1, 2, or 4 byte transfer words.
		 */
		if (sc->state.transfer_word_size == 1) {
			if (qcom_spi_hw_read_into_rx_buf(sc, reg & 0xff))
				num_bytes++;
		} else if (sc->state.transfer_word_size == 2) {
			if (qcom_spi_hw_read_into_rx_buf(sc, (reg >> 8) & 0xff))
				num_bytes++;
			if (qcom_spi_hw_read_into_rx_buf(sc, reg & 0xff))
				num_bytes++;
		} else if (sc->state.transfer_word_size == 4) {
			if (qcom_spi_hw_read_into_rx_buf(sc, (reg >> 24) & 0xff))
				num_bytes++;
			if (qcom_spi_hw_read_into_rx_buf(sc, (reg >> 16) & 0xff))
				num_bytes++;
			if (qcom_spi_hw_read_into_rx_buf(sc, (reg >> 8) & 0xff))
				num_bytes++;
			if (qcom_spi_hw_read_into_rx_buf(sc, reg & 0xff))
				num_bytes++;
		}
	}

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_HW_TX_FIFO,
	    "%s: read %d bytes (%d transfer words)\n",
	    __func__, num_bytes, sc->transfer.num_words);

#if 0
	/*
	 * This is a no-op for FIFO mode, it's only a thing for BLOCK
	 * transfers.
	 */
	QCOM_SPI_BARRIER_READ(sc);
	reg = QCOM_SPI_READ_4(sc, QUP_OPERATIONAL);
	if (reg & QUP_OP_MAX_INPUT_DONE_FLAG) {
		device_printf(sc->sc_dev, "%s: read complete (DONE)\n" ,
		    __func__);
		sc->intr.done = true;
	}
#endif

#if 0
	/*
	 * And see if we've finished the transfer and won't be getting
	 * any more.  Then treat it as done as well.
	 *
	 * In FIFO only mode we don't get a completion interrupt;
	 * we get an interrupt when the FIFO has enough data present.
	 */
	if ((sc->state.transfer_mode == QUP_IO_M_MODE_FIFO)
	    && (sc->transfer.rx_offset >= sc->transfer.rx_len)) {
		device_printf(sc->sc_dev, "%s: read complete (rxlen)\n",
		    __func__);
		sc->intr.done = true;
	}
#endif

	/*
	 * For FIFO transfers we get a /single/ result that complete
	 * the FIFO transfer.  We won't get any subsequent transfers;
	 * we'll need to schedule a new FIFO transfer.
	 */
	sc->intr.done = true;

	return (0);
}

int
qcom_spi_hw_read_pio_block(struct qcom_spi_softc *sc)
{

	/* Not yet implemented */
	return (ENXIO);
}

int
qcom_spi_hw_do_full_reset(struct qcom_spi_softc *sc)
{
	QCOM_SPI_ASSERT_LOCKED(sc);

	QCOM_SPI_WRITE_4(sc, QUP_SW_RESET, 1);
	QCOM_SPI_BARRIER_WRITE(sc);
	DELAY(100);

	return (0);
}
