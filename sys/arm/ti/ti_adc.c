/*-
 * Copyright 2014 Luiz Otavio O Souza <loos@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_prcm.h>
#include <arm/ti/ti_adcreg.h>
#include <arm/ti/ti_adcvar.h>

/* Define our 8 steps, one for each input channel. */
static struct ti_adc_input ti_adc_inputs[TI_ADC_NPINS] = {
	{ .stepconfig = ADC_STEPCFG1, .stepdelay = ADC_STEPDLY1 },
	{ .stepconfig = ADC_STEPCFG2, .stepdelay = ADC_STEPDLY2 },
	{ .stepconfig = ADC_STEPCFG3, .stepdelay = ADC_STEPDLY3 },
	{ .stepconfig = ADC_STEPCFG4, .stepdelay = ADC_STEPDLY4 },
	{ .stepconfig = ADC_STEPCFG5, .stepdelay = ADC_STEPDLY5 },
	{ .stepconfig = ADC_STEPCFG6, .stepdelay = ADC_STEPDLY6 },
	{ .stepconfig = ADC_STEPCFG7, .stepdelay = ADC_STEPDLY7 },
	{ .stepconfig = ADC_STEPCFG8, .stepdelay = ADC_STEPDLY8 },
};

static int ti_adc_samples[5] = { 0, 2, 4, 8, 16 };

static void
ti_adc_enable(struct ti_adc_softc *sc)
{

	TI_ADC_LOCK_ASSERT(sc);

	if (sc->sc_last_state == 1)
		return;

	/* Enable the FIFO0 threshold and the end of sequence interrupt. */
	ADC_WRITE4(sc, ADC_IRQENABLE_SET,
	    ADC_IRQ_FIFO0_THRES | ADC_IRQ_END_OF_SEQ);

	/* Enable the ADC.  Run thru enabled steps, start the conversions. */
	ADC_WRITE4(sc, ADC_CTRL, ADC_READ4(sc, ADC_CTRL) | ADC_CTRL_ENABLE);

	sc->sc_last_state = 1;
}

static void
ti_adc_disable(struct ti_adc_softc *sc)
{
	int count;
	uint32_t data;

	TI_ADC_LOCK_ASSERT(sc);

	if (sc->sc_last_state == 0)
		return;

	/* Disable all the enabled steps. */
	ADC_WRITE4(sc, ADC_STEPENABLE, 0);

	/* Disable the ADC. */
	ADC_WRITE4(sc, ADC_CTRL, ADC_READ4(sc, ADC_CTRL) & ~ADC_CTRL_ENABLE);

	/* Disable the FIFO0 threshold and the end of sequence interrupt. */
	ADC_WRITE4(sc, ADC_IRQENABLE_CLR,
	    ADC_IRQ_FIFO0_THRES | ADC_IRQ_END_OF_SEQ);

	/* ACK any pending interrupt. */
	ADC_WRITE4(sc, ADC_IRQSTATUS, ADC_READ4(sc, ADC_IRQSTATUS));

	/* Drain the FIFO data. */
	count = ADC_READ4(sc, ADC_FIFO0COUNT) & ADC_FIFO_COUNT_MSK;
	while (count > 0) {
		data = ADC_READ4(sc, ADC_FIFO0DATA);
		count = ADC_READ4(sc, ADC_FIFO0COUNT) & ADC_FIFO_COUNT_MSK;
	}

	sc->sc_last_state = 0;
}

static int
ti_adc_setup(struct ti_adc_softc *sc)
{
	int ain;
	uint32_t enabled;

	TI_ADC_LOCK_ASSERT(sc);

	/* Check for enabled inputs. */
	enabled = 0;
	for (ain = 0; ain < TI_ADC_NPINS; ain++) {
		if (ti_adc_inputs[ain].enable)
			enabled |= (1U << (ain + 1));
	}

	/* Set the ADC global status. */
	if (enabled != 0) {
		ti_adc_enable(sc);
		/* Update the enabled steps. */
		if (enabled != ADC_READ4(sc, ADC_STEPENABLE))
			ADC_WRITE4(sc, ADC_STEPENABLE, enabled);
	} else
		ti_adc_disable(sc);

	return (0);
}

static void
ti_adc_input_setup(struct ti_adc_softc *sc, int32_t ain)
{
	struct ti_adc_input *input;
	uint32_t reg, val;

	TI_ADC_LOCK_ASSERT(sc);

	input = &ti_adc_inputs[ain];
	reg = input->stepconfig;
	val = ADC_READ4(sc, reg);

	/* Set single ended operation. */
	val &= ~ADC_STEP_DIFF_CNTRL;

	/* Set the negative voltage reference. */
	val &= ~ADC_STEP_RFM_MSK;
	val |= ADC_STEP_RFM_VREFN << ADC_STEP_RFM_SHIFT;

	/* Set the positive voltage reference. */
	val &= ~ADC_STEP_RFP_MSK;
	val |= ADC_STEP_RFP_VREFP << ADC_STEP_RFP_SHIFT;

	/* Set the samples average. */
	val &= ~ADC_STEP_AVG_MSK;
	val |= input->samples << ADC_STEP_AVG_SHIFT;

	/* Select the desired input. */
	val &= ~ADC_STEP_INP_MSK;
	val |= ain << ADC_STEP_INP_SHIFT;

	/* Set the ADC to one-shot mode. */
	val &= ~ADC_STEP_MODE_MSK;

	ADC_WRITE4(sc, reg, val);
}

static void
ti_adc_reset(struct ti_adc_softc *sc)
{
	int ain;

	TI_ADC_LOCK_ASSERT(sc);

	/* Disable all the inputs. */
	for (ain = 0; ain < TI_ADC_NPINS; ain++)
		ti_adc_inputs[ain].enable = 0;
}

static int
ti_adc_clockdiv_proc(SYSCTL_HANDLER_ARGS)
{
	int error, reg;
	struct ti_adc_softc *sc;

	sc = (struct ti_adc_softc *)arg1;

	TI_ADC_LOCK(sc);
	reg = (int)ADC_READ4(sc, ADC_CLKDIV) + 1;
	TI_ADC_UNLOCK(sc);

	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/*
	 * The actual written value is the prescaler setting - 1.
	 * Enforce a minimum value of 10 (i.e. 9) which limits the maximum
	 * ADC clock to ~2.4Mhz (CLK_M_OSC / 10).
	 */
	reg--;
	if (reg < 9)
		reg = 9;
	if (reg > USHRT_MAX)
		reg = USHRT_MAX;

	TI_ADC_LOCK(sc);
	/* Disable the ADC. */
	ti_adc_disable(sc);
	/* Update the ADC prescaler setting. */
	ADC_WRITE4(sc, ADC_CLKDIV, reg);
	/* Enable the ADC again. */
	ti_adc_setup(sc);
	TI_ADC_UNLOCK(sc);

	return (0);
}

static int
ti_adc_enable_proc(SYSCTL_HANDLER_ARGS)
{
	int error;
	int32_t enable;
	struct ti_adc_softc *sc;
	struct ti_adc_input *input;

	input = (struct ti_adc_input *)arg1;
	sc = input->sc;

	enable = input->enable;
	error = sysctl_handle_int(oidp, &enable, sizeof(enable),
	    req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (enable)
		enable = 1;

	TI_ADC_LOCK(sc);
	/* Setup the ADC as needed. */
	if (input->enable != enable) {
		input->enable = enable;
		ti_adc_setup(sc);
		if (input->enable == 0)
			input->value = 0;
	}
	TI_ADC_UNLOCK(sc);

	return (0);
}

static int
ti_adc_open_delay_proc(SYSCTL_HANDLER_ARGS)
{
	int error, reg;
	struct ti_adc_softc *sc;
	struct ti_adc_input *input;

	input = (struct ti_adc_input *)arg1;
	sc = input->sc;

	TI_ADC_LOCK(sc);
	reg = (int)ADC_READ4(sc, input->stepdelay) & ADC_STEP_OPEN_DELAY;
	TI_ADC_UNLOCK(sc);

	error = sysctl_handle_int(oidp, &reg, sizeof(reg), req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (reg < 0)
		reg = 0;

	TI_ADC_LOCK(sc);
	ADC_WRITE4(sc, input->stepdelay, reg & ADC_STEP_OPEN_DELAY);
	TI_ADC_UNLOCK(sc);

	return (0);
}

static int
ti_adc_samples_avg_proc(SYSCTL_HANDLER_ARGS)
{
	int error, samples, i;
	struct ti_adc_softc *sc;
	struct ti_adc_input *input;

	input = (struct ti_adc_input *)arg1;
	sc = input->sc;

	if (input->samples > nitems(ti_adc_samples))
		input->samples = nitems(ti_adc_samples);
	samples = ti_adc_samples[input->samples];

	error = sysctl_handle_int(oidp, &samples, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	TI_ADC_LOCK(sc);
	if (samples != ti_adc_samples[input->samples]) {
		input->samples = 0;
		for (i = 0; i < nitems(ti_adc_samples); i++)
			if (samples >= ti_adc_samples[i])
				input->samples = i;
		ti_adc_input_setup(sc, input->input);
	}
	TI_ADC_UNLOCK(sc);

	return (error);
}

static void
ti_adc_read_data(struct ti_adc_softc *sc)
{
	int count, ain;
	struct ti_adc_input *input;
	uint32_t data;

	TI_ADC_LOCK_ASSERT(sc);

	/* Read the available data. */
	count = ADC_READ4(sc, ADC_FIFO0COUNT) & ADC_FIFO_COUNT_MSK;
	while (count > 0) {
		data = ADC_READ4(sc, ADC_FIFO0DATA);
		ain = (data & ADC_FIFO_STEP_ID_MSK) >> ADC_FIFO_STEP_ID_SHIFT;
		input = &ti_adc_inputs[ain];
		if (input->enable == 0)
			input->value = 0;
		else
			input->value = (int32_t)(data & ADC_FIFO_DATA_MSK);
		count = ADC_READ4(sc, ADC_FIFO0COUNT) & ADC_FIFO_COUNT_MSK;
	}
}

static void
ti_adc_intr(void *arg)
{
	struct ti_adc_softc *sc;
	uint32_t status;

	sc = (struct ti_adc_softc *)arg;

	status = ADC_READ4(sc, ADC_IRQSTATUS);
	if (status == 0)
		return;
	if (status & ~(ADC_IRQ_FIFO0_THRES | ADC_IRQ_END_OF_SEQ))
		device_printf(sc->sc_dev, "stray interrupt: %#x\n", status);

	TI_ADC_LOCK(sc);
	/* ACK the interrupt. */
	ADC_WRITE4(sc, ADC_IRQSTATUS, status);

	/* Read the available data. */
	if (status & ADC_IRQ_FIFO0_THRES)
		ti_adc_read_data(sc);

	/* Start the next conversion ? */
	if (status & ADC_IRQ_END_OF_SEQ)
		ti_adc_setup(sc);
	TI_ADC_UNLOCK(sc);
}

static void
ti_adc_sysctl_init(struct ti_adc_softc *sc)
{
	char pinbuf[3];
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node, *inp_node, *inpN_node;
	struct sysctl_oid_list *tree, *inp_tree, *inpN_tree;
	int ain;

	/*
	 * Add per-pin sysctl tree/handlers.
	 */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree_node = device_get_sysctl_tree(sc->sc_dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "clockdiv",
	    CTLFLAG_RW | CTLTYPE_UINT,  sc, 0,
	    ti_adc_clockdiv_proc, "IU", "ADC clock prescaler");
	inp_node = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO, "ain",
	    CTLFLAG_RD, NULL, "ADC inputs");
	inp_tree = SYSCTL_CHILDREN(inp_node);

	for (ain = 0; ain < TI_ADC_NPINS; ain++) {

		snprintf(pinbuf, sizeof(pinbuf), "%d", ain);
		inpN_node = SYSCTL_ADD_NODE(ctx, inp_tree, OID_AUTO, pinbuf,
		    CTLFLAG_RD, NULL, "ADC input");
		inpN_tree = SYSCTL_CHILDREN(inpN_node);

		SYSCTL_ADD_PROC(ctx, inpN_tree, OID_AUTO, "enable",
		    CTLFLAG_RW | CTLTYPE_UINT, &ti_adc_inputs[ain], 0,
		    ti_adc_enable_proc, "IU", "Enable ADC input");
		SYSCTL_ADD_PROC(ctx, inpN_tree, OID_AUTO, "open_delay",
		    CTLFLAG_RW | CTLTYPE_UINT,  &ti_adc_inputs[ain], 0,
		    ti_adc_open_delay_proc, "IU", "ADC open delay");
		SYSCTL_ADD_PROC(ctx, inpN_tree, OID_AUTO, "samples_avg",
		    CTLFLAG_RW | CTLTYPE_UINT,  &ti_adc_inputs[ain], 0,
		    ti_adc_samples_avg_proc, "IU", "ADC samples average");
		SYSCTL_ADD_INT(ctx, inpN_tree, OID_AUTO, "input",
		    CTLFLAG_RD, &ti_adc_inputs[ain].value, 0,
		    "Converted raw value for the ADC input");
	}
}

static void
ti_adc_inputs_init(struct ti_adc_softc *sc)
{
	int ain;
	struct ti_adc_input *input;

	TI_ADC_LOCK(sc);
	for (ain = 0; ain < TI_ADC_NPINS; ain++) {
		input = &ti_adc_inputs[ain];
		input->sc = sc;
		input->input = ain;
		input->value = 0;
		input->enable = 0;
		input->samples = 0;
		ti_adc_input_setup(sc, ain);
	}
	TI_ADC_UNLOCK(sc);
}

static void
ti_adc_idlestep_init(struct ti_adc_softc *sc)
{
	uint32_t val;

	val = ADC_READ4(sc, ADC_IDLECONFIG);

	/* Set single ended operation. */
	val &= ~ADC_STEP_DIFF_CNTRL;

	/* Set the negative voltage reference. */
	val &= ~ADC_STEP_RFM_MSK;
	val |= ADC_STEP_RFM_VREFN << ADC_STEP_RFM_SHIFT;

	/* Set the positive voltage reference. */
	val &= ~ADC_STEP_RFP_MSK;
	val |= ADC_STEP_RFP_VREFP << ADC_STEP_RFP_SHIFT;

	/* Connect the input to VREFN. */
	val &= ~ADC_STEP_INP_MSK;
	val |= ADC_STEP_IN_VREFN << ADC_STEP_INP_SHIFT;

	ADC_WRITE4(sc, ADC_IDLECONFIG, val);
}

static int
ti_adc_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "ti,adc"))
		return (ENXIO);
	device_set_desc(dev, "TI ADC controller");

	return (BUS_PROBE_DEFAULT);
}

static int
ti_adc_attach(device_t dev)
{
	int err, rid;
	struct ti_adc_softc *sc;
	uint32_t reg, rev;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		return (ENXIO);
	}

	rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->sc_irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "cannot allocate interrupt\n");
		return (ENXIO);
	}

	if (bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, ti_adc_intr, sc, &sc->sc_intrhand) != 0) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);
		device_printf(dev, "Unable to setup the irq handler.\n");
		return (ENXIO);
	}

	/* Activate the ADC_TSC module. */
	err = ti_prcm_clk_enable(TSC_ADC_CLK);
	if (err)
		return (err);

	/* Check the ADC revision. */
	rev = ADC_READ4(sc, ADC_REVISION);
	device_printf(dev,
	    "scheme: %#x func: %#x rtl: %d rev: %d.%d custom rev: %d\n",
	    (rev & ADC_REV_SCHEME_MSK) >> ADC_REV_SCHEME_SHIFT,
	    (rev & ADC_REV_FUNC_MSK) >> ADC_REV_FUNC_SHIFT,
	    (rev & ADC_REV_RTL_MSK) >> ADC_REV_RTL_SHIFT,
	    (rev & ADC_REV_MAJOR_MSK) >> ADC_REV_MAJOR_SHIFT,
	    rev & ADC_REV_MINOR_MSK,
	    (rev & ADC_REV_CUSTOM_MSK) >> ADC_REV_CUSTOM_SHIFT);

	/*
	 * Disable the step write protect and make it store the step ID for
	 * the captured data on FIFO.
	 */
	reg = ADC_READ4(sc, ADC_CTRL);
	ADC_WRITE4(sc, ADC_CTRL, reg | ADC_CTRL_STEP_WP | ADC_CTRL_STEP_ID);

	/*
	 * Set the ADC prescaler to 2400 (yes, the actual value written here
	 * is 2400 - 1).
	 * This sets the ADC clock to ~10Khz (CLK_M_OSC / 2400).
	 */
	ADC_WRITE4(sc, ADC_CLKDIV, 2399);

	TI_ADC_LOCK_INIT(sc);

	ti_adc_idlestep_init(sc);
	ti_adc_inputs_init(sc);
	ti_adc_sysctl_init(sc);

	return (0);
}

static int
ti_adc_detach(device_t dev)
{
	struct ti_adc_softc *sc;

	sc = device_get_softc(dev);

	/* Turn off the ADC. */
	TI_ADC_LOCK(sc);
	ti_adc_reset(sc);
	ti_adc_setup(sc);
	TI_ADC_UNLOCK(sc);

	TI_ADC_LOCK_DESTROY(sc);

	if (sc->sc_intrhand)
		bus_teardown_intr(dev, sc->sc_irq_res, sc->sc_intrhand);
	if (sc->sc_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq_res);
	if (sc->sc_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->sc_mem_res);

	return (bus_generic_detach(dev));
}

static device_method_t ti_adc_methods[] = {
	DEVMETHOD(device_probe,		ti_adc_probe),
	DEVMETHOD(device_attach,	ti_adc_attach),
	DEVMETHOD(device_detach,	ti_adc_detach),

	DEVMETHOD_END
};

static driver_t ti_adc_driver = {
	"ti_adc",
	ti_adc_methods,
	sizeof(struct ti_adc_softc),
};

static devclass_t ti_adc_devclass;

DRIVER_MODULE(ti_adc, simplebus, ti_adc_driver, ti_adc_devclass, 0, 0);
MODULE_VERSION(ti_adc, 1);
MODULE_DEPEND(ti_adc, simplebus, 1, 1, 1);
