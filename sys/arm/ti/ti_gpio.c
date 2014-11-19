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
 *	Very simple GPIO (general purpose IO) driver module for TI OMAP SoC's.
 *
 *	Currently this driver only does the basics, get a value on a pin & set a
 *	value on a pin. Hopefully over time I'll expand this to be a bit more generic
 *	and support interrupts and other various bits on the SoC can do ... in the
 *	meantime this is all you get.
 *
 *	Beware the OMA datasheet(s) lists GPIO banks 1-6, whereas I've used 0-5 here
 *	in the code.
 *
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <arm/ti/ti_cpuid.h>
#include <arm/ti/ti_gpio.h>
#include <arm/ti/ti_scm.h>
#include <arm/ti/ti_prcm.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"
#include "ti_gpio_if.h"

/* Register definitions */
#define	TI_GPIO_REVISION		0x0000
#define	TI_GPIO_SYSCONFIG		0x0010
#if defined(SOC_OMAP4) || defined(SOC_TI_AM335X)
#define	TI_GPIO_IRQSTATUS_RAW_0		0x0024
#define	TI_GPIO_IRQSTATUS_RAW_1		0x0028
#define	TI_GPIO_IRQSTATUS_0		0x002C
#define	TI_GPIO_IRQSTATUS_1		0x0030
#define	TI_GPIO_IRQSTATUS_SET_0		0x0034
#define	TI_GPIO_IRQSTATUS_SET_1		0x0038
#define	TI_GPIO_IRQSTATUS_CLR_0		0x003C
#define	TI_GPIO_IRQSTATUS_CLR_1		0x0040
#define	TI_GPIO_IRQWAKEN_0		0x0044
#define	TI_GPIO_IRQWAKEN_1		0x0048
#define	TI_GPIO_SYSSTATUS		0x0114
#define	TI_GPIO_IRQSTATUS1		0x0118
#define	TI_GPIO_IRQENABLE1		0x011C
#define	TI_GPIO_WAKEUPENABLE		0x0120
#define	TI_GPIO_IRQSTATUS2		0x0128
#define	TI_GPIO_IRQENABLE2		0x012C
#define	TI_GPIO_CTRL			0x0130
#define	TI_GPIO_OE			0x0134
#define	TI_GPIO_DATAIN			0x0138
#define	TI_GPIO_DATAOUT			0x013C
#define	TI_GPIO_LEVELDETECT0		0x0140
#define	TI_GPIO_LEVELDETECT1		0x0144
#define	TI_GPIO_RISINGDETECT		0x0148
#define	TI_GPIO_FALLINGDETECT		0x014C
#define	TI_GPIO_DEBOUNCENABLE		0x0150
#define	TI_GPIO_DEBOUNCINGTIME		0x0154
#define	TI_GPIO_CLEARWKUPENA		0x0180
#define	TI_GPIO_SETWKUENA		0x0184
#define	TI_GPIO_CLEARDATAOUT		0x0190
#define	TI_GPIO_SETDATAOUT		0x0194
#else
#error "Unknown SoC"
#endif

/* Other SoC Specific definitions */
#define	OMAP4_MAX_GPIO_BANKS		6
#define	OMAP4_FIRST_GPIO_BANK		1
#define	OMAP4_INTR_PER_BANK		1
#define	OMAP4_GPIO_REV			0x50600801
#define	AM335X_MAX_GPIO_BANKS		4
#define	AM335X_FIRST_GPIO_BANK		0
#define	AM335X_INTR_PER_BANK		2
#define	AM335X_GPIO_REV			0x50600801
#define	PINS_PER_BANK			32

static u_int
ti_max_gpio_banks(void)
{
	switch(ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		return (OMAP4_MAX_GPIO_BANKS);
#endif
#ifdef SOC_TI_AM335X
	case CHIP_AM335X:
		return (AM335X_MAX_GPIO_BANKS);
#endif
	}
	return (0);
}

static u_int
ti_max_gpio_intrs(void)
{
	switch(ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		return (OMAP4_MAX_GPIO_BANKS * OMAP4_INTR_PER_BANK);
#endif
#ifdef SOC_TI_AM335X
	case CHIP_AM335X:
		return (AM335X_MAX_GPIO_BANKS * AM335X_INTR_PER_BANK);
#endif
	}
	return (0);
}

static u_int
ti_first_gpio_bank(void)
{
	switch(ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		return (OMAP4_FIRST_GPIO_BANK);
#endif
#ifdef SOC_TI_AM335X
	case CHIP_AM335X:
		return (AM335X_FIRST_GPIO_BANK);
#endif
	}
	return (0);
}

static uint32_t
ti_gpio_rev(void)
{
	switch(ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		return (OMAP4_GPIO_REV);
#endif
#ifdef SOC_TI_AM335X
	case CHIP_AM335X:
		return (AM335X_GPIO_REV);
#endif
	}
	return (0);
}

/**
 *	ti_gpio_mem_spec - Resource specification used when allocating resources
 *	ti_gpio_irq_spec - Resource specification used when allocating resources
 *
 *	This driver module can have up to six independent memory regions, each
 *	region typically controls 32 GPIO pins.
 *
 *	On OMAP3 and OMAP4 there is only one physical interrupt line per bank,
 *	but there are two set of registers which control the interrupt delivery
 *	to internal subsystems.  The first set of registers control the
 *	interrupts delivery to the MPU and the second set control the
 *	interrupts delivery to the DSP.
 *
 *	On AM335x there are two physical interrupt lines for each GPIO module.
 *	Each interrupt line is controlled by a set of registers.
 */
static struct resource_spec ti_gpio_mem_spec[] = {
	{ SYS_RES_MEMORY,   0,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   1,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   2,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   3,  RF_ACTIVE | RF_OPTIONAL },
#if !defined(SOC_TI_AM335X)
	{ SYS_RES_MEMORY,   4,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   5,  RF_ACTIVE | RF_OPTIONAL },
#endif
	{ -1,               0,  0 }
};
static struct resource_spec ti_gpio_irq_spec[] = {
	{ SYS_RES_IRQ,      0,  RF_ACTIVE },
	{ SYS_RES_IRQ,      1,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      2,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      3,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      4,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      5,  RF_ACTIVE | RF_OPTIONAL },
#if defined(SOC_TI_AM335X)
	{ SYS_RES_IRQ,      6,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      7,  RF_ACTIVE | RF_OPTIONAL },
#endif
	{ -1,               0,  0 }
};

/**
 *	Macros for driver mutex locking
 */
#define	TI_GPIO_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	TI_GPIO_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	TI_GPIO_LOCK_INIT(_sc)		\
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "ti_gpio", MTX_DEF)
#define	TI_GPIO_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx)
#define	TI_GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED)
#define	TI_GPIO_ASSERT_UNLOCKED(_sc)	mtx_assert(&_sc->sc_mtx, MA_NOTOWNED)

/**
 *	ti_gpio_read_4 - reads a 16-bit value from one of the PADCONFS registers
 *	@sc: GPIO device context
 *	@bank: The bank to read from
 *	@off: The offset of a register from the GPIO register address range
 *
 *
 *	RETURNS:
 *	32-bit value read from the register.
 */
static inline uint32_t
ti_gpio_read_4(struct ti_gpio_softc *sc, unsigned int bank, bus_size_t off)
{
	return (bus_read_4(sc->sc_mem_res[bank], off));
}

/**
 *	ti_gpio_write_4 - writes a 32-bit value to one of the PADCONFS registers
 *	@sc: GPIO device context
 *	@bank: The bank to write to
 *	@off: The offset of a register from the GPIO register address range
 *	@val: The value to write into the register
 *
 *	RETURNS:
 *	nothing
 */
static inline void
ti_gpio_write_4(struct ti_gpio_softc *sc, unsigned int bank, bus_size_t off,
                 uint32_t val)
{
	bus_write_4(sc->sc_mem_res[bank], off, val);
}

static inline void
ti_gpio_intr_clr(struct ti_gpio_softc *sc, unsigned int bank, uint32_t mask)
{

	/* We clear both set of registers. */
#if defined(SOC_OMAP4) || defined(SOC_TI_AM335X)
	ti_gpio_write_4(sc, bank, TI_GPIO_IRQSTATUS_CLR_0, mask);
	ti_gpio_write_4(sc, bank, TI_GPIO_IRQSTATUS_CLR_1, mask);
#else
	ti_gpio_write_4(sc, bank, TI_GPIO_CLEARIRQENABLE1, mask);
	ti_gpio_write_4(sc, bank, TI_GPIO_CLEARIRQENABLE2, mask);
#endif
}

/**
 *	ti_gpio_pin_max - Returns the maximum number of GPIO pins
 *	@dev: gpio device handle
 *	@maxpin: pointer to a value that upon return will contain the maximum number
 *	         of pins in the device.
 *
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise an error code
 */
static int
ti_gpio_pin_max(device_t dev, int *maxpin)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	unsigned int i;
	unsigned int banks = 0;

	TI_GPIO_LOCK(sc);

	/* Calculate how many valid banks we have and then multiply that by 32 to
	 * give use the total number of pins.
	 */
	for (i = 0; i < ti_max_gpio_banks(); i++) {
		if (sc->sc_mem_res[i] != NULL)
			banks++;
	}

	*maxpin = (banks * PINS_PER_BANK) - 1;

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_getcaps - Gets the capabilties of a given pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@caps: pointer to a value that upon return will contain the capabilities
 *
 *	Currently all pins have the same capability, notably:
 *	  - GPIO_PIN_INPUT
 *	  - GPIO_PIN_OUTPUT
 *	  - GPIO_PIN_PULLUP
 *	  - GPIO_PIN_PULLDOWN
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise an error code
 */
static int
ti_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	*caps = (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT |GPIO_PIN_PULLUP |
	    GPIO_PIN_PULLDOWN);

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_getflags - Gets the current flags of a given pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@flags: upon return will contain the current flags of the pin
 *
 *	Reads the current flags of a given pin, here we actually read the H/W
 *	registers to determine the flags, rather than storing the value in the
 *	setflags call.
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise an error code
 */
static int
ti_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Get the current pin state */
	TI_GPIO_GET_FLAGS(dev, pin, flags);

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_getname - Gets the name of a given pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@name: buffer to put the name in
 *
 *	The driver simply calls the pins gpio_n, where 'n' is obviously the number
 *	of the pin.
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise an error code
 */
static int
ti_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Set a very simple name */
	snprintf(name, GPIOMAXNAME, "gpio_%u", pin);
	name[GPIOMAXNAME - 1] = '\0';

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_setflags - Sets the flags for a given pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@flags: the flags to set
 *
 *	The flags of the pin correspond to things like input/output mode, pull-ups,
 *	pull-downs, etc.  This driver doesn't support all flags, only the following:
 *	  - GPIO_PIN_INPUT
 *	  - GPIO_PIN_OUTPUT
 *	  - GPIO_PIN_PULLUP
 *	  - GPIO_PIN_PULLDOWN
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise an error code
 */
static int
ti_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t reg_val;

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Set the GPIO mode and state */
	if (TI_GPIO_SET_FLAGS(dev, pin, flags) != 0) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* If configuring as an output set the "output enable" bit */
	reg_val = ti_gpio_read_4(sc, bank, TI_GPIO_OE);
	if (flags & GPIO_PIN_INPUT)
		reg_val |= mask;
	else
		reg_val &= ~mask;
	ti_gpio_write_4(sc, bank, TI_GPIO_OE, reg_val);

	TI_GPIO_UNLOCK(sc);
	
	return (0);
}

/**
 *	ti_gpio_pin_set - Sets the current level on a GPIO pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@value: non-zero value will drive the pin high, otherwise the pin is
 *	        driven low.
 *
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise a error code
 */
static int
ti_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	ti_gpio_write_4(sc, bank, (value == GPIO_PIN_LOW) ? TI_GPIO_CLEARDATAOUT
	    : TI_GPIO_SETDATAOUT, mask);

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_get - Gets the current level on a GPIO pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *	@value: pointer to a value that upond return will contain the pin value
 *
 *	The pin must be configured as an input pin beforehand, otherwise this
 *	function will fail.
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise a error code
 */
static int
ti_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t val = 0;

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Sanity check the pin is not configured as an output */
	val = ti_gpio_read_4(sc, bank, TI_GPIO_OE);

	/* Read the value on the pin */
	if (val & mask)
		*value = (ti_gpio_read_4(sc, bank, TI_GPIO_DATAIN) & mask) ? 1 : 0;
	else
		*value = (ti_gpio_read_4(sc, bank, TI_GPIO_DATAOUT) & mask) ? 1 : 0;

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_pin_toggle - Toggles a given GPIO pin
 *	@dev: gpio device handle
 *	@pin: the number of the pin
 *
 *
 *	LOCKING:
 *	Internally locks the context
 *
 *	RETURNS:
 *	Returns 0 on success otherwise a error code
 */
static int
ti_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t val;

	TI_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank >= ti_max_gpio_banks()) || (sc->sc_mem_res[bank] == NULL)) {
		TI_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Toggle the pin */
	val = ti_gpio_read_4(sc, bank, TI_GPIO_DATAOUT);
	if (val & mask)
		ti_gpio_write_4(sc, bank, TI_GPIO_CLEARDATAOUT, mask);
	else
		ti_gpio_write_4(sc, bank, TI_GPIO_SETDATAOUT, mask);

	TI_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	ti_gpio_intr - ISR for all GPIO modules
 *	@arg: the soft context pointer
 *
 *	Unsused
 *
 *	LOCKING:
 *	Internally locks the context
 *
 */
static void
ti_gpio_intr(void *arg)
{
	struct ti_gpio_softc *sc = arg;

	TI_GPIO_LOCK(sc);
	/* TODO: something useful */
	TI_GPIO_UNLOCK(sc);
}

static int
ti_gpio_attach_intr(device_t dev)
{
	int i;
	struct ti_gpio_softc *sc;

	sc = device_get_softc(dev);
	for (i = 0; i < ti_max_gpio_intrs(); i++) {
		if (sc->sc_irq_res[i] == NULL)
			break;

		/*
		 * Register our interrupt handler for each of the IRQ resources.
		 */
		if (bus_setup_intr(dev, sc->sc_irq_res[i],
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL, ti_gpio_intr, sc,
		    &sc->sc_irq_hdl[i]) != 0) {
			device_printf(dev,
			    "WARNING: unable to register interrupt handler\n");
			return (-1);
		}
	}

	return (0);
}

static int
ti_gpio_detach_intr(device_t dev)
{
	int i;
	struct ti_gpio_softc *sc;

	/* Teardown our interrupt handlers. */
	sc = device_get_softc(dev);
	for (i = 0; i < ti_max_gpio_intrs(); i++) {
		if (sc->sc_irq_res[i] == NULL)
			break;

		if (sc->sc_irq_hdl[i]) {
			bus_teardown_intr(dev, sc->sc_irq_res[i],
			    sc->sc_irq_hdl[i]);
		}
	}

	return (0);
}

static int
ti_gpio_bank_init(device_t dev, int bank)
{
	int pin;
	struct ti_gpio_softc *sc;
	uint32_t flags, reg_oe;

	sc = device_get_softc(dev);

	/* Enable the interface and functional clocks for the module. */
	ti_prcm_clk_enable(GPIO0_CLK + ti_first_gpio_bank() + bank);

	/*
	 * Read the revision number of the module.  TI don't publish the
	 * actual revision numbers, so instead the values have been
	 * determined by experimentation.
	 */
	sc->sc_revision[bank] = ti_gpio_read_4(sc, bank, TI_GPIO_REVISION);

	/* Check the revision. */
	if (sc->sc_revision[bank] != ti_gpio_rev()) {
		device_printf(dev, "Warning: could not determine the revision "
		    "of %u GPIO module (revision:0x%08x)\n",
		    bank, sc->sc_revision[bank]);
		return (EINVAL);
	}

	/* Disable interrupts for all pins. */
	ti_gpio_intr_clr(sc, bank, 0xffffffff);

	/* Init OE register based on pads configuration. */
	reg_oe = 0xffffffff;
	for (pin = 0; pin < PINS_PER_BANK; pin++) {
		TI_GPIO_GET_FLAGS(dev, PINS_PER_BANK * bank + pin, &flags);
		if (flags & GPIO_PIN_OUTPUT)
			reg_oe &= ~(1UL << pin);
	}
	ti_gpio_write_4(sc, bank, TI_GPIO_OE, reg_oe);

	return (0);
}

/**
 *	ti_gpio_attach - attach function for the driver
 *	@dev: gpio device handle
 *
 *	Allocates and sets up the driver context for all GPIO banks.  This function
 *	expects the memory ranges and IRQs to already be allocated to the driver.
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	Always returns 0
 */
static int
ti_gpio_attach(device_t dev)
{
	struct ti_gpio_softc *sc;
	unsigned int i;
	int err;

 	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	TI_GPIO_LOCK_INIT(sc);

	/* There are up to 6 different GPIO register sets located in different
	 * memory areas on the chip.  The memory range should have been set for
	 * the driver when it was added as a child.
	 */
	if (bus_alloc_resources(dev, ti_gpio_mem_spec, sc->sc_mem_res) != 0) {
		device_printf(dev, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}

	/* Request the IRQ resources */
	if (bus_alloc_resources(dev, ti_gpio_irq_spec, sc->sc_irq_res) != 0) {
		bus_release_resources(dev, ti_gpio_mem_spec, sc->sc_mem_res);
		device_printf(dev, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

	/* Setup the IRQ resources */
	if (ti_gpio_attach_intr(dev) != 0) {
		ti_gpio_detach_intr(dev);
		bus_release_resources(dev, ti_gpio_irq_spec, sc->sc_irq_res);
		bus_release_resources(dev, ti_gpio_mem_spec, sc->sc_mem_res);
		return (ENXIO);
	}

	/* We need to go through each block and ensure the clocks are running and
	 * the module is enabled.  It might be better to do this only when the
	 * pins are configured which would result in less power used if the GPIO
	 * pins weren't used ... 
	 */
	for (i = 0; i < ti_max_gpio_banks(); i++) {
		if (sc->sc_mem_res[i] != NULL) {
			/* Initialize the GPIO module. */
			err = ti_gpio_bank_init(dev, i);
			if (err != 0) {
				ti_gpio_detach_intr(dev);
				bus_release_resources(dev, ti_gpio_irq_spec,
				    sc->sc_irq_res);
				bus_release_resources(dev, ti_gpio_mem_spec,
				    sc->sc_mem_res);
				return (err);
			}
		}
	}

	/* Finish of the probe call */
	device_add_child(dev, "gpioc", -1);
	device_add_child(dev, "gpiobus", -1);

	return (bus_generic_attach(dev));
}

/**
 *	ti_gpio_detach - detach function for the driver
 *	@dev: scm device handle
 *
 *	Allocates and sets up the driver context, this simply entails creating a
 *	bus mappings for the SCM register set.
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	Always returns 0
 */
static int
ti_gpio_detach(device_t dev)
{
	struct ti_gpio_softc *sc = device_get_softc(dev);
	unsigned int i;

	KASSERT(mtx_initialized(&sc->sc_mtx), ("gpio mutex not initialized"));

	/* Disable all interrupts */
	for (i = 0; i < ti_max_gpio_banks(); i++) {
		if (sc->sc_mem_res[i] != NULL)
			ti_gpio_intr_clr(sc, i, 0xffffffff);
	}

	bus_generic_detach(dev);

	/* Release the memory and IRQ resources. */
	ti_gpio_detach_intr(dev);
	bus_release_resources(dev, ti_gpio_irq_spec, sc->sc_irq_res);
	bus_release_resources(dev, ti_gpio_mem_spec, sc->sc_mem_res);

	TI_GPIO_LOCK_DESTROY(sc);

	return (0);
}

static phandle_t
ti_gpio_get_node(device_t bus, device_t dev)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(bus));
}

static device_method_t ti_gpio_methods[] = {
	DEVMETHOD(device_attach, ti_gpio_attach),
	DEVMETHOD(device_detach, ti_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max, ti_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, ti_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, ti_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, ti_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, ti_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, ti_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, ti_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, ti_gpio_pin_toggle),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node, ti_gpio_get_node),

	{0, 0},
};

driver_t ti_gpio_driver = {
	"gpio",
	ti_gpio_methods,
	sizeof(struct ti_gpio_softc),
};
