/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <sys/gpio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/fdt/fdt_common.h>
#include <dev/fdt/fdt_pinctrl.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>
#include "spibus_if.h"

#include <dev/qcom_qup/qcom_spi_var.h>
#include <dev/qcom_qup/qcom_qup_reg.h>
#include <dev/qcom_qup/qcom_spi_reg.h>
#include <dev/qcom_qup/qcom_spi_debug.h>

static struct ofw_compat_data compat_data[] = {
	{ "qcom,spi-qup-v1.1.1",	QCOM_SPI_HW_QPI_V1_1 },
	{ "qcom,spi-qup-v2.1.1",	QCOM_SPI_HW_QPI_V2_1 },
	{ "qcom,spi-qup-v2.2.1",	QCOM_SPI_HW_QPI_V2_2 },
	{ NULL,				0 }
};

/*
 * Flip the CS GPIO line either active or inactive.
 *
 * Actually listen to the CS polarity.
 */
static void
qcom_spi_set_chipsel(struct qcom_spi_softc *sc, int cs, bool active)
{
	bool pinactive;
	bool invert = !! (cs & SPIBUS_CS_HIGH);

	cs = cs & ~SPIBUS_CS_HIGH;

	if (sc->cs_pins[cs] == NULL) {
		device_printf(sc->sc_dev,
		    "%s: cs=%u, active=%u, invert=%u, no gpio?\n",
		    __func__, cs, active, invert);
		return;
	}

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_CHIPSELECT,
	    "%s: cs=%u active=%u\n", __func__, cs, active);

	/*
	 * Default rule here is CS is active low.
	 */
	if (active)
		pinactive = false;
	else
		pinactive = true;

	/*
	 * Invert the CS line if required.
	 */
	if (invert)
		pinactive = !! pinactive;

	gpio_pin_set_active(sc->cs_pins[cs], pinactive);
	gpio_pin_is_active(sc->cs_pins[cs], &pinactive);
}

static void
qcom_spi_intr(void *arg)
{
	struct qcom_spi_softc *sc = arg;
	int ret;

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_INTR, "%s: called\n", __func__);


	QCOM_SPI_LOCK(sc);
	ret = qcom_spi_hw_interrupt_handle(sc);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to read intr status\n");
		goto done;
	}

	/*
	 * Handle spurious interrupts outside of an actual
	 * transfer.
	 */
	if (sc->transfer.active == false) {
		device_printf(sc->sc_dev,
		    "ERROR: spurious interrupt\n");
		qcom_spi_hw_ack_opmode(sc);
		goto done;
	}

	/* Now, handle interrupts */
	if (sc->intr.error) {
		sc->intr.error = false;
		device_printf(sc->sc_dev, "ERROR: intr\n");
	}

	if (sc->intr.do_rx) {
		sc->intr.do_rx = false;
		QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_INTR,
		    "%s: PIO_READ\n", __func__);
		if (sc->state.transfer_mode == QUP_IO_M_MODE_FIFO)
			ret = qcom_spi_hw_read_pio_fifo(sc);
		else
			ret = qcom_spi_hw_read_pio_block(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_read failed (%u)\n", ret);
			goto done;
		}
	}
	if (sc->intr.do_tx) {
		sc->intr.do_tx = false;
		QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_INTR,
		    "%s: PIO_WRITE\n", __func__);
		/*
		 * For FIFO operations we do not do a write here, we did
		 * it at the beginning of the transfer.
		 *
		 * For BLOCK operations yes, we call the routine.
		 */

		if (sc->state.transfer_mode == QUP_IO_M_MODE_FIFO)
			ret = qcom_spi_hw_ack_write_pio_fifo(sc);
		else
			ret = qcom_spi_hw_write_pio_block(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_write failed (%u)\n", ret);
			goto done;
		}
	}

	/*
	 * Do this last.  We may actually have completed the
	 * transfer in the PIO receive path above and it will
	 * set the done flag here.
	 */
	if (sc->intr.done) {
		sc->intr.done = false;
		sc->transfer.done = true;
		QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_INTR,
		    "%s: transfer done\n", __func__);
		wakeup(sc);
	}

done:
	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_INTR,
	    "%s: done\n", __func__);
	QCOM_SPI_UNLOCK(sc);
}

static int
qcom_spi_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "Qualcomm SPI Interface");
	return (BUS_PROBE_DEFAULT);
}

/*
 * Allocate GPIOs if provided in the SPI controller block.
 *
 * Some devices will use GPIO lines for chip select.
 * It's also quite annoying because some devices will want to use
 * the hardware provided CS gating for say, the first chipselect block,
 * and then use GPIOs for the later ones.
 *
 * So here we just assume for now that SPI index 0 uses the hardware
 * lines, and >0 use GPIO lines.  Revisit this if better hardware
 * shows up.
 *
 * And finally, iterating over the cs-gpios list to allocate GPIOs
 * doesn't actually tell us what the polarity is.  For that we need
 * to actually iterate over the list of child nodes and check what
 * their properties are (and look for "spi-cs-high".)
 */
static void
qcom_spi_attach_gpios(struct qcom_spi_softc *sc)
{
	phandle_t node;
	int idx, err;

	/* Allocate gpio pins for configured chip selects. */
	node = ofw_bus_get_node(sc->sc_dev);
	for (idx = 0; idx < nitems(sc->cs_pins); idx++) {
		err = gpio_pin_get_by_ofw_propidx(sc->sc_dev, node,
		    "cs-gpios", idx, &sc->cs_pins[idx]);
		if (err == 0) {
			err = gpio_pin_setflags(sc->cs_pins[idx],
			    GPIO_PIN_OUTPUT);
			if (err != 0) {
				device_printf(sc->sc_dev,
				    "error configuring gpio for"
				    " cs %u (%d)\n", idx, err);
			}
			/*
			 * We can't set this HIGH right now because
			 * we don't know if it needs to be set to
			 * high for inactive or low for inactive
			 * based on the child SPI device flags.
			 */
#if 0
			gpio_pin_set_active(sc->cs_pins[idx], 1);
			gpio_pin_is_active(sc->cs_pins[idx], &tmp);
#endif
		} else {
			device_printf(sc->sc_dev,
			    "cannot configure gpio for chip select %u\n", idx);
			sc->cs_pins[idx] = NULL;
		}
	}
}

static void
qcom_spi_sysctl_attach(struct qcom_spi_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0,
	    "control debugging printfs");
}

static int
qcom_spi_attach(device_t dev)
{
	struct qcom_spi_softc *sc = device_get_softc(dev);
	int rid, ret, i, val;

	sc->sc_dev = dev;

	/*
	 * Hardware version is stored in the ofw_compat_data table.
	 */
	sc->hw_version =
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "ERROR: Could not map memory\n");
		ret = ENXIO;
		goto error;
	}

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "ERROR: Could not map interrupt\n");
		ret = ENXIO;
		goto error;
	}

	ret = bus_setup_intr(dev, sc->sc_irq_res,
	    INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, qcom_spi_intr, sc, &sc->sc_irq_h);
	if (ret != 0) {
		device_printf(dev, "ERROR: could not configure interrupt "
		    "(%d)\n",
		    ret);
		goto error;
	}

	qcom_spi_attach_gpios(sc);

	ret = clk_get_by_ofw_name(dev, 0, "core", &sc->clk_core);
	if (ret != 0) {
		device_printf(dev, "ERROR: could not get %s clock (%d)\n",
		    "core", ret);
		goto error;
	}
	ret = clk_get_by_ofw_name(dev, 0, "iface", &sc->clk_iface);
	if (ret != 0) {
		device_printf(dev, "ERROR: could not get %s clock (%d)\n",
		    "iface", ret);
		goto error;
	}

	/* Bring up initial clocks if they're off */
	ret = clk_enable(sc->clk_core);
	if (ret != 0) {
		device_printf(dev, "ERROR: couldn't enable core clock (%u)\n",
		    ret);
		goto error;
	}
	ret = clk_enable(sc->clk_iface);
	if (ret != 0) {
		device_printf(dev, "ERROR: couldn't enable iface clock (%u)\n",
		    ret);
		goto error;
	}

	/*
	 * Read optional spi-max-frequency
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "spi-max-frequency",
	    &val, sizeof(val)) > 0)
		sc->config.max_frequency = val;
	else
		sc->config.max_frequency = SPI_MAX_RATE;

	/*
	 * Read optional cs-select
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "cs-select",
	    &val, sizeof(val)) > 0)
		sc->config.cs_select = val;
	else
		sc->config.cs_select = 0;

	/*
	 * Read optional num-cs
	 */
	if (OF_getencprop(ofw_bus_get_node(dev), "num-cs",
	    &val, sizeof(val)) > 0)
		sc->config.num_cs = val;
	else
		sc->config.num_cs = SPI_NUM_CHIPSELECTS;

	ret = fdt_pinctrl_configure_by_name(dev, "default");
	if (ret != 0) {
		device_printf(dev,
		    "ERROR: could not configure default pinmux\n");
		goto error;
	}

	ret = qcom_spi_hw_read_controller_transfer_sizes(sc);
	if (ret != 0) {
		device_printf(dev, "ERROR: Could not read transfer config\n");
		goto error;
	}


	device_printf(dev, "BLOCK: input=%u bytes, output=%u bytes\n",
	    sc->config.input_block_size,
	    sc->config.output_block_size);
	device_printf(dev, "FIFO: input=%u bytes, output=%u bytes\n",
	    sc->config.input_fifo_size,
	    sc->config.output_fifo_size);

	/* QUP config */
	QCOM_SPI_LOCK(sc);
	ret = qcom_spi_hw_qup_init_locked(sc);
	if (ret != 0) {
		device_printf(dev, "ERROR: QUP init failed (%d)\n", ret);
		QCOM_SPI_UNLOCK(sc);
		goto error;
	}

	/* Initial SPI config */
	ret = qcom_spi_hw_spi_init_locked(sc);
	if (ret != 0) {
		device_printf(dev, "ERROR: SPI init failed (%d)\n", ret);
		QCOM_SPI_UNLOCK(sc);
		goto error;
	}
	QCOM_SPI_UNLOCK(sc);

	sc->spibus = device_add_child(dev, "spibus", -1);

	/* We're done, so shut down the interface clock for now */
	device_printf(dev, "DONE: shutting down interface clock for now\n");
	clk_disable(sc->clk_iface);

	/* Register for debug sysctl */
	qcom_spi_sysctl_attach(sc);

	return (bus_generic_attach(dev));
error:
	if (sc->sc_irq_h)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_h);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->clk_core) {
		clk_disable(sc->clk_core);
		clk_release(sc->clk_core);
	}
	if (sc->clk_iface) {
		clk_disable(sc->clk_iface);
		clk_release(sc->clk_iface);
	}
	for (i = 0; i < CS_MAX; i++) {
		if (sc->cs_pins[i] != NULL)
			gpio_pin_release(sc->cs_pins[i]);
	}
	mtx_destroy(&sc->sc_mtx);
	return (ret);
}

/*
 * Do a PIO transfer.
 *
 * Note that right now the TX/RX lens need to match, I'm not doing
 * dummy reads / dummy writes as required if they're not the same
 * size.  The QUP hardware supports doing multi-phase transactions
 * where the FIFO isn't engaged for transmit or receive, but it's
 * not yet being done here.
 */
static int
qcom_spi_transfer_pio_block(struct qcom_spi_softc *sc, int mode,
    char *tx_buf, int tx_len, char *rx_buf, int rx_len)
{
	int ret = 0;

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_TRANSFER, "%s: start\n",
	    __func__);

	if (rx_len != tx_len) {
		device_printf(sc->sc_dev,
		    "ERROR: tx/rx len doesn't match (%d/%d)\n",
		    tx_len, rx_len);
		return (ENXIO);
	}

	QCOM_SPI_ASSERT_LOCKED(sc);

	/*
	 * Make initial choices for transfer configuration.
	 */
	ret = qcom_spi_hw_setup_transfer_selection(sc, tx_len);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to setup transfer selection (%d)\n",
		    ret);
		return (ret);
	}

	/* Now set suitable buffer/lengths */
	sc->transfer.tx_buf = tx_buf;
	sc->transfer.tx_len = tx_len;
	sc->transfer.rx_buf = rx_buf;
	sc->transfer.rx_len = rx_len;
	sc->transfer.done = false;
	sc->transfer.active = false;

	/*
	 * Loop until the full transfer set is done.
	 *
	 * qcom_spi_hw_setup_current_transfer() will take care of
	 * setting a maximum transfer size for the hardware and choose
	 * a suitable operating mode.
	 */
	while (sc->transfer.tx_offset < sc->transfer.tx_len) {
		/*
		 * Set transfer to false early; this covers
		 * it also finishing a sub-transfer and we're
		 * about the put the block into RESET state before
		 * starting a new transfer.
		 */
		sc->transfer.active = false;

		QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_TRANSFER,
		    "%s: tx=%d of %d bytes, rx=%d of %d bytes\n",
		    __func__,
		    sc->transfer.tx_offset, sc->transfer.tx_len,
		    sc->transfer.rx_offset, sc->transfer.rx_len);

		/*
		 * Set state to RESET before doing anything.
		 *
		 * Otherwise the second sub-transfer that we queue up
		 * will generate interrupts immediately when we start
		 * configuring it here and it'll start underflowing.
		 */
		ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RESET);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: can't transition to RESET (%u)\n", ret);
			goto done;
		}
		/* blank interrupt state; we'll do a RESET below */
		bzero(&sc->intr, sizeof(sc->intr));
		sc->transfer.done = false;

		/*
		 * Configure what the transfer configuration for this
		 * sub-transfer will be.
		 */
		ret = qcom_spi_hw_setup_current_transfer(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: failed to setup sub transfer (%d)\n",
			    ret);
			goto done;
		}

		/*
		 * For now since we're configuring up PIO, we only setup
		 * the PIO transfer size.
		 */
		ret = qcom_spi_hw_setup_pio_transfer_cnt(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_pio_transfer_cnt failed"
			    " (%u)\n", ret);
			goto done;
		}

#if 0
		/*
		 * This is what we'd do to setup the block transfer sizes.
		 */
		ret = qcom_spi_hw_setup_block_transfer_cnt(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_block_transfer_cnt failed"
			    " (%u)\n", ret);
			goto done;
		}
#endif

		ret = qcom_spi_hw_setup_io_modes(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_io_modes failed"
			    " (%u)\n", ret);
			goto done;
		}

		ret = qcom_spi_hw_setup_spi_io_clock_polarity(sc,
		    !! (mode & SPIBUS_MODE_CPOL));
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_spi_io_clock_polarity"
			    "    failed (%u)\n", ret);
			goto done;
		}

		ret = qcom_spi_hw_setup_spi_config(sc, sc->state.frequency,
		    !! (mode & SPIBUS_MODE_CPHA));
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_spi_config failed"
			    " (%u)\n", ret);
			goto done;
		}

		ret = qcom_spi_hw_setup_qup_config(sc, !! (tx_len > 0),
		    !! (rx_len > 0));
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_qup_config failed"
			    " (%u)\n", ret);
			goto done;
		}

		ret = qcom_spi_hw_setup_operational_mask(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_setup_operational_mask failed"
			    " (%u)\n", ret);
			goto done;
		}

		/*
		 * Setup is done; reset the controller and start the PIO
		 * write.
		 */

		/*
		 * Set state to RUN; we may start getting interrupts that
		 * are valid and we need to handle.
		 */
		sc->transfer.active = true;
		ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RUN);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: can't transition to RUN (%u)\n", ret);
			goto done;
		}

		/*
		 * Set state to PAUSE
		 */
		ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_PAUSE);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: can't transition to PAUSE (%u)\n", ret);
			goto done;
		}

		/*
		 * If FIFO mode, write data now.  Else, we'll get an
		 * interrupt when it's time to populate more data
		 * in BLOCK mode.
		 */
		if (sc->state.transfer_mode == QUP_IO_M_MODE_FIFO)
			ret = qcom_spi_hw_write_pio_fifo(sc);
		else
			ret = qcom_spi_hw_write_pio_block(sc);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: qcom_spi_hw_write failed (%u)\n", ret);
			goto done;
		}

		/*
		 * Set state to RUN
		 */
		ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RUN);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: can't transition to RUN (%u)\n", ret);
			goto done;
		}

		/*
		 * Wait for an interrupt notification (which will
		 * continue to drive the state machine for this
		 * sub-transfer) or timeout.
		 */
		ret = 0;
		while (ret == 0 && sc->transfer.done == false) {
			QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_TRANSFER,
			    "%s: waiting\n", __func__);
			ret = msleep(sc, &sc->sc_mtx, 0, "qcom_spi", 0);
		}
	}
done:
	/*
	 * Complete; put controller into reset.
	 *
	 * Don't worry about return value here; if we errored out above then
	 * we want to communicate that value to the caller.
	 */
	(void) qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RESET);
	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_TRANSFER,
	    "%s: completed\n", __func__);

	 /*
	  * Blank the transfer state so we don't use an old transfer
	  * state in a subsequent interrupt.
	  */
	(void) qcom_spi_hw_complete_transfer(sc);
	sc->transfer.active = false;

	return (ret);
}

static int
qcom_spi_transfer(device_t dev, device_t child, struct spi_command *cmd)
{
	struct qcom_spi_softc *sc = device_get_softc(dev);
	uint32_t cs_val, mode_val, clock_val;
	uint32_t ret = 0;

	spibus_get_cs(child, &cs_val);
	spibus_get_clock(child, &clock_val);
	spibus_get_mode(child, &mode_val);

	QCOM_SPI_DPRINTF(sc, QCOM_SPI_DEBUG_TRANSFER,
	    "%s: called; child cs=0x%08x, clock=%u, mode=0x%08x, "
	    "cmd=%u/%u bytes; data=%u/%u bytes\n",
	    __func__,
	    cs_val,
	    clock_val,
	    mode_val,
	    cmd->tx_cmd_sz, cmd->rx_cmd_sz,
	    cmd->tx_data_sz, cmd->rx_data_sz);

	QCOM_SPI_LOCK(sc);

	/*
	 * wait until the controller isn't busy
	 */
	while (sc->sc_busy == true)
		mtx_sleep(sc, &sc->sc_mtx, 0, "qcom_spi_wait", 0);

	/*
	 * it's ours now!
	 */
	sc->sc_busy = true;

	sc->state.cs_high = !! (cs_val & SPIBUS_CS_HIGH);
	sc->state.frequency = clock_val;

	/*
	 * We can't set the clock frequency and enable it
	 * with the driver lock held, as the SPI lock is non-sleepable
	 * and the clock framework is sleepable.
	 *
	 * No other transaction is going on right now, so we can
	 * unlock here and do the clock related work.
	 */
	QCOM_SPI_UNLOCK(sc);

	/*
	 * Set the clock frequency
	 */
	ret = clk_set_freq(sc->clk_iface, sc->state.frequency, 0);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to set frequency to %u\n",
		    sc->state.frequency);
		goto done2;
	}
	clk_enable(sc->clk_iface);

	QCOM_SPI_LOCK(sc);

	/*
	 * Set state to RESET
	 */
	ret = qcom_spi_hw_qup_set_state_locked(sc, QUP_STATE_RESET);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: can't transition to RESET (%u)\n", ret);
		goto done;
	}

	/* Assert hardware CS if set, else GPIO */
	if (sc->cs_pins[cs_val & ~SPIBUS_CS_HIGH] == NULL)
		qcom_spi_hw_spi_cs_force(sc, cs_val & SPIBUS_CS_HIGH, true);
	else
		qcom_spi_set_chipsel(sc, cs_val & ~SPIBUS_CS_HIGH, true);

	/*
	 * cmd buffer transfer
	 */
	ret = qcom_spi_transfer_pio_block(sc, mode_val, cmd->tx_cmd,
	    cmd->tx_cmd_sz, cmd->rx_cmd, cmd->rx_cmd_sz);
	if (ret != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to transfer cmd payload (%u)\n", ret);
		goto done;
	}

	/*
	 * data buffer transfer
	 */
	if (cmd->tx_data_sz > 0) {
		ret = qcom_spi_transfer_pio_block(sc, mode_val, cmd->tx_data,
		    cmd->tx_data_sz, cmd->rx_data, cmd->rx_data_sz);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: failed to transfer data payload (%u)\n",
			    ret);
			goto done;
		}
	}

done:
	/* De-assert GPIO/CS */
	if (sc->cs_pins[cs_val & ~SPIBUS_CS_HIGH] == NULL)
		qcom_spi_hw_spi_cs_force(sc, cs_val & ~SPIBUS_CS_HIGH, false);
	else
		qcom_spi_set_chipsel(sc, cs_val & ~SPIBUS_CS_HIGH, false);

	/*
	 * Similarly to when we enabled the clock, we can't hold it here
	 * across a clk API as that's a sleep lock and we're non-sleepable.
	 * So instead we unlock/relock here, but we still hold the busy flag.
	 */

	QCOM_SPI_UNLOCK(sc);
	clk_disable(sc->clk_iface);
	QCOM_SPI_LOCK(sc);
done2:
	/*
	 * We're done; so mark the bus as not busy and wakeup
	 * the next caller.
	 */
	sc->sc_busy = false;
	wakeup_one(sc);
	QCOM_SPI_UNLOCK(sc);
	return (ret);
}

static int
qcom_spi_detach(device_t dev)
{
	struct qcom_spi_softc *sc = device_get_softc(dev);
	int i;

	bus_generic_detach(sc->sc_dev);
	if (sc->spibus != NULL)
		device_delete_child(dev, sc->spibus);

	if (sc->sc_irq_h)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_irq_h);

	if (sc->clk_iface) {
		clk_disable(sc->clk_iface);
		clk_release(sc->clk_iface);
	}
	if (sc->clk_core) {
		clk_disable(sc->clk_core);
		clk_release(sc->clk_core);
	}

	for (i = 0; i < CS_MAX; i++) {
		if (sc->cs_pins[i] != NULL)
			gpio_pin_release(sc->cs_pins[i]);
	}

	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static phandle_t
qcom_spi_get_node(device_t bus, device_t dev)
{

	return ofw_bus_get_node(bus);
}


static device_method_t qcom_spi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		qcom_spi_probe),
	DEVMETHOD(device_attach,	qcom_spi_attach),
	DEVMETHOD(device_detach,	qcom_spi_detach),
	/* TODO: suspend */
	/* TODO: resume */

	DEVMETHOD(spibus_transfer,	qcom_spi_transfer),

	/* ofw_bus_if */
	DEVMETHOD(ofw_bus_get_node,     qcom_spi_get_node),

	DEVMETHOD_END
};

static driver_t qcom_spi_driver = {
	"qcom_spi",
	qcom_spi_methods,
	sizeof(struct qcom_spi_softc),
};

DRIVER_MODULE(qcom_spi, simplebus, qcom_spi_driver, 0, 0);
DRIVER_MODULE(ofw_spibus, qcom_spi, ofw_spibus_driver, 0, 0);
MODULE_DEPEND(qcom_spi, ofw_spibus, 1, 1, 1);
SIMPLEBUS_PNP_INFO(compat_data);
