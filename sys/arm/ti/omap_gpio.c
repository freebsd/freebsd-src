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

#include <arm/omap/omap_scm.h>
#include <arm/omap/omap_prcm.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "gpio_if.h"

#define OMAP_GPIO_REVISION          0x0000
#define OMAP_GPIO_SYSCONFIG         0x0010

/* Register offsets of the GPIO banks on OMAP3 devices */
#define OMAP3_GPIO_SYSSTATUS        0x0014
#define OMAP3_GPIO_IRQSTATUS1       0x0018
#define OMAP3_GPIO_IRQENABLE1       0x001C
#define OMAP3_GPIO_WAKEUPENABLE     0x0020
#define OMAP3_GPIO_IRQSTATUS2       0x0028
#define OMAP3_GPIO_IRQENABLE2       0x002C
#define OMAP3_GPIO_CTRL             0x0030
#define OMAP3_GPIO_OE               0x0034
#define OMAP3_GPIO_DATAIN           0x0038
#define OMAP3_GPIO_DATAOUT          0x003C
#define OMAP3_GPIO_LEVELDETECT0     0x0040
#define OMAP3_GPIO_LEVELDETECT1     0x0044
#define OMAP3_GPIO_RISINGDETECT     0x0048
#define OMAP3_GPIO_FALLINGDETECT    0x004C
#define OMAP3_GPIO_DEBOUNCENABLE    0x0050
#define OMAP3_GPIO_DEBOUNCINGTIME   0x0054
#define OMAP3_GPIO_CLEARIRQENABLE1  0x0060
#define OMAP3_GPIO_SETIRQENABLE1    0x0064
#define OMAP3_GPIO_CLEARIRQENABLE2  0x0070
#define OMAP3_GPIO_SETIRQENABLE2    0x0074
#define OMAP3_GPIO_CLEARWKUENA      0x0080
#define OMAP3_GPIO_SETWKUENA        0x0084
#define OMAP3_GPIO_CLEARDATAOUT     0x0090
#define OMAP3_GPIO_SETDATAOUT       0x0094

/* Register offsets of the GPIO banks on OMAP4 devices */
#define OMAP4_GPIO_IRQSTATUS_RAW_0  0x0024
#define OMAP4_GPIO_IRQSTATUS_RAW_1  0x0028
#define OMAP4_GPIO_IRQSTATUS_0      0x002C
#define OMAP4_GPIO_IRQSTATUS_1      0x0030
#define OMAP4_GPIO_IRQSTATUS_SET_0  0x0034
#define OMAP4_GPIO_IRQSTATUS_SET_1  0x0038
#define OMAP4_GPIO_IRQSTATUS_CLR_0  0x003C
#define OMAP4_GPIO_IRQSTATUS_CLR_1  0x0040
#define OMAP4_GPIO_IRQWAKEN_0       0x0044
#define OMAP4_GPIO_IRQWAKEN_1       0x0048
#define OMAP4_GPIO_SYSSTATUS        0x0114
#define OMAP4_GPIO_IRQSTATUS1       0x0118
#define OMAP4_GPIO_IRQENABLE1       0x011C
#define OMAP4_GPIO_WAKEUPENABLE     0x0120
#define OMAP4_GPIO_IRQSTATUS2       0x0128
#define OMAP4_GPIO_IRQENABLE2       0x012C
#define OMAP4_GPIO_CTRL             0x0130
#define OMAP4_GPIO_OE               0x0134
#define OMAP4_GPIO_DATAIN           0x0138
#define OMAP4_GPIO_DATAOUT          0x013C
#define OMAP4_GPIO_LEVELDETECT0     0x0140
#define OMAP4_GPIO_LEVELDETECT1     0x0144
#define OMAP4_GPIO_RISINGDETECT     0x0148
#define OMAP4_GPIO_FALLINGDETECT    0x014C
#define OMAP4_GPIO_DEBOUNCENABLE    0x0150
#define OMAP4_GPIO_DEBOUNCINGTIME   0x0154
#define OMAP4_GPIO_CLEARIRQENABLE1  0x0160
#define OMAP4_GPIO_SETIRQENABLE1    0x0164
#define OMAP4_GPIO_CLEARIRQENABLE2  0x0170
#define OMAP4_GPIO_SETIRQENABLE2    0x0174
#define OMAP4_GPIO_CLEARWKUPENA     0x0180
#define OMAP4_GPIO_SETWKUENA        0x0184
#define OMAP4_GPIO_CLEARDATAOUT     0x0190
#define OMAP4_GPIO_SETDATAOUT       0x0194

#define MAX_GPIO_BANKS    6
#define PINS_PER_BANK     32

/**
 *	The following H/W revision values were found be experimentation, TI don't
 *	publish the revision numbers.  The TRM says "TI internal Data".
 */
#define OMAP3_GPIO_REV  0x00000025
#define OMAP4_GPIO_REV  0x50600801

/**
 *	omap_gpio_mem_spec - Resource specification used when allocating resources
 *	omap_gpio_irq_spec - Resource specification used when allocating resources
 *
 *	This driver module can have up to six independent memory regions, each
 *	region typically controls 32 GPIO pins.
 */
static struct resource_spec omap_gpio_mem_spec[] = {
	{ SYS_RES_MEMORY,   0,  RF_ACTIVE },
	{ SYS_RES_MEMORY,   1,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   2,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   3,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   4,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_MEMORY,   5,  RF_ACTIVE | RF_OPTIONAL },
	{ -1,               0,  0 }
};
static struct resource_spec omap_gpio_irq_spec[] = {
	{ SYS_RES_IRQ,      0,  RF_ACTIVE },
	{ SYS_RES_IRQ,      1,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      2,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      3,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      4,  RF_ACTIVE | RF_OPTIONAL },
	{ SYS_RES_IRQ,      5,  RF_ACTIVE | RF_OPTIONAL },
	{ -1,               0,  0 }
};

/**
 *	Structure that stores the driver context.
 *
 *	This structure is allocated during driver attach.
 */
struct omap_gpio_softc {
	device_t			sc_dev;
	
	/* The memory resource(s) for the PRCM register set, when the device is
	 * created the caller can assign up to 4 memory regions.
	 */
	struct resource*    sc_mem_res[MAX_GPIO_BANKS];
	struct resource*    sc_irq_res[MAX_GPIO_BANKS];
	
	/* The handle for the register IRQ handlers */
	void*               sc_irq_hdl[MAX_GPIO_BANKS];
	
	/* The following describes the H/W revision of each of the GPIO banks */
	uint32_t            sc_revision[MAX_GPIO_BANKS];
	
	struct mtx			sc_mtx;
};

/**
 *	Macros for driver mutex locking
 */
#define OMAP_GPIO_LOCK(_sc)             mtx_lock(&(_sc)->sc_mtx)
#define	OMAP_GPIO_UNLOCK(_sc)           mtx_unlock(&(_sc)->sc_mtx)
#define OMAP_GPIO_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	         "omap_gpio", MTX_DEF)
#define OMAP_GPIO_LOCK_DESTROY(_sc)     mtx_destroy(&_sc->sc_mtx);
#define OMAP_GPIO_ASSERT_LOCKED(_sc)    mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define OMAP_GPIO_ASSERT_UNLOCKED(_sc)  mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);

/**
 *	omap_gpio_read_4 - reads a 16-bit value from one of the PADCONFS registers
 *	@sc: GPIO device context
 *	@bank: The bank to read from
 *	@off: The offset of a register from the GPIO register address range
 *
 *
 *	RETURNS:
 *	32-bit value read from the register.
 */
static inline uint32_t
omap_gpio_read_4(struct omap_gpio_softc *sc, unsigned int bank, bus_size_t off)
{
	return (bus_read_4(sc->sc_mem_res[bank], off));
}

/**
 *	omap_gpio_write_4 - writes a 32-bit value to one of the PADCONFS registers
 *	@sc: GPIO device context
 *	@bank: The bank to write to
 *	@off: The offset of a register from the GPIO register address range
 *	@val: The value to write into the register
 *
 *	RETURNS:
 *	nothing
 */
static inline void
omap_gpio_write_4(struct omap_gpio_softc *sc, unsigned int bank, bus_size_t off,
                 uint32_t val)
{
	bus_write_4(sc->sc_mem_res[bank], off, val);
}

/**
 *	omap_gpio_is_omap4 - returns 1 if the GPIO module is from an OMAP4xxx chip
 *	omap_gpio_is_omap3 - returns 1 if the GPIO module is from an OMAP3xxx chip
 *	@sc: GPIO device context
 *	@bank: The bank to test the type of
 *
 *	RETURNS:
 *	nothing
 */
static inline int
omap_gpio_is_omap4(struct omap_gpio_softc *sc, unsigned int bank)
{
	return (sc->sc_revision[bank] == OMAP4_GPIO_REV);
}

static inline int
omap_gpio_is_omap3(struct omap_gpio_softc *sc, unsigned int bank)
{
	return (sc->sc_revision[bank] == OMAP3_GPIO_REV);
}

/**
 *	omap_gpio_pin_max - Returns the maximum number of GPIO pins
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
omap_gpio_pin_max(device_t dev, int *maxpin)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	unsigned int i;
	unsigned int banks = 0;

	OMAP_GPIO_LOCK(sc);

	/* Calculate how many valid banks we have and then multiply that by 32 to
	 * give use the total number of pins.
	 */
	for (i = 0; i < MAX_GPIO_BANKS; i++) {
		if (sc->sc_mem_res[i] != NULL)
			banks++;
	}

	*maxpin = (banks * PINS_PER_BANK);
	
	OMAP_GPIO_UNLOCK(sc);
	
	return (0);
}

/**
 *	omap_gpio_pin_getcaps - Gets the capabilties of a given pin
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
omap_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	*caps = (GPIO_PIN_INPUT | 
	         GPIO_PIN_OUTPUT | 
	         GPIO_PIN_PULLUP |
	         GPIO_PIN_PULLDOWN); 

	OMAP_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	omap_gpio_pin_getflags - Gets the current flags of a given pin
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
omap_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	unsigned int state;

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Get the current pin state */
	if (omap_scm_padconf_get_gpiomode(pin, &state) != 0)
		*flags = 0;
	else {
		switch (state) {
			case PADCONF_PIN_OUTPUT:
				*flags = GPIO_PIN_OUTPUT;
				break;
			case PADCONF_PIN_INPUT:
				*flags = GPIO_PIN_INPUT;
				break;
			case PADCONF_PIN_INPUT_PULLUP:
				*flags = GPIO_PIN_INPUT | GPIO_PIN_PULLUP;
				break;
			case PADCONF_PIN_INPUT_PULLDOWN:
				*flags = GPIO_PIN_INPUT | GPIO_PIN_PULLDOWN;
				break;
			default:
				*flags = 0;
				break;
		}
	}

	OMAP_GPIO_UNLOCK(sc);
	
	return (0);
}

/**
 *	omap_gpio_pin_getname - Gets the name of a given pin
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
omap_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Set a very simple name */
	snprintf(name, GPIOMAXNAME, "gpio_%u", pin);
	name[GPIOMAXNAME - 1] = '\0';

	OMAP_GPIO_UNLOCK(sc);
	
	return (0);
}

/**
 *	omap_gpio_pin_setflags - Sets the flags for a given pin
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
omap_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	unsigned int state = 0;
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t reg_off;
	uint32_t reg_val;

	/* Sanity check the flags supplied are valid, i.e. not input and output */
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) == 0x0000)
		return (EINVAL);
	if ((flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) == 
	    (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT))
		return (EINVAL);
	if ((flags & (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN)) == 
	    (GPIO_PIN_PULLUP|GPIO_PIN_PULLDOWN))
		return (EINVAL);


	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* First the SCM driver needs to be told to put the pad into GPIO mode */
	if (flags & GPIO_PIN_OUTPUT)
		state = PADCONF_PIN_OUTPUT;
	else if (flags & GPIO_PIN_INPUT) {
		if (flags & GPIO_PIN_PULLUP)
			state = PADCONF_PIN_INPUT_PULLUP;
		else if (flags & GPIO_PIN_PULLDOWN)
			state = PADCONF_PIN_INPUT_PULLDOWN;
		else
			state = PADCONF_PIN_INPUT;
	}

	/* Set the GPIO mode and state */
	if (omap_scm_padconf_set_gpiomode(pin, state) != 0) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	
	
	/* Get the offset of the register to use */
	if (omap_gpio_is_omap3(sc, bank))
		reg_off = OMAP3_GPIO_OE;
	else if (omap_gpio_is_omap4(sc, bank))
		reg_off = OMAP4_GPIO_OE;
	else {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	

	/* If configuring as an output set the "output enable" bit */
	reg_val = omap_gpio_read_4(sc, bank, reg_off);
	if (flags & GPIO_PIN_INPUT)
		reg_val |= mask;
	else
		reg_val &= ~mask;
	omap_gpio_write_4(sc, bank, reg_off, reg_val);


	OMAP_GPIO_UNLOCK(sc);
	
	return (0);
}

/**
 *	omap_gpio_pin_set - Sets the current level on a GPIO pin
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
omap_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t reg_off;

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	
	/* Set the pin value */
	if (omap_gpio_is_omap3(sc, bank))
		reg_off = (value == GPIO_PIN_LOW) ? OMAP3_GPIO_CLEARDATAOUT :
		                                    OMAP3_GPIO_SETDATAOUT;
	else if (omap_gpio_is_omap4(sc, bank))
		reg_off = (value == GPIO_PIN_LOW) ? OMAP4_GPIO_CLEARDATAOUT :
		                                    OMAP4_GPIO_SETDATAOUT;
	else {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	omap_gpio_write_4(sc, bank, reg_off, mask);

	OMAP_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	omap_gpio_pin_get - Gets the current level on a GPIO pin
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
omap_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t val = 0;

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	
	/* Sanity check the pin is not configured as an output */
	if (omap_gpio_is_omap3(sc, bank))
		val = omap_gpio_read_4(sc, bank, OMAP3_GPIO_OE);
	else if (omap_gpio_is_omap4(sc, bank))
		val = omap_gpio_read_4(sc, bank, OMAP4_GPIO_OE);

	if ((val & mask) == mask) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	
	/* Read the value on the pin */
	if (omap_gpio_is_omap3(sc, bank))
		val = omap_gpio_read_4(sc, bank, OMAP3_GPIO_DATAIN);
	else if (omap_gpio_is_omap4(sc, bank))
		val = omap_gpio_read_4(sc, bank, OMAP4_GPIO_DATAIN);

	*value = (val & mask) ? 1 : 0;

	OMAP_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	omap_gpio_pin_toggle - Toggles a given GPIO pin
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
omap_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	uint32_t bank = (pin / PINS_PER_BANK);
	uint32_t mask = (1UL << (pin % PINS_PER_BANK));
	uint32_t val;

	OMAP_GPIO_LOCK(sc);

	/* Sanity check the pin number is valid */
	if ((bank > MAX_GPIO_BANKS) || (sc->sc_mem_res[bank] == NULL)) {
		OMAP_GPIO_UNLOCK(sc);
		return (EINVAL);
	}

	/* Toggle the pin */
	if (omap_gpio_is_omap3(sc, bank)) {
		val = omap_gpio_read_4(sc, bank, OMAP3_GPIO_DATAOUT);
		if (val & mask)
			omap_gpio_write_4(sc, bank, OMAP3_GPIO_CLEARDATAOUT, mask);
		else
			omap_gpio_write_4(sc, bank, OMAP3_GPIO_SETDATAOUT, mask);
	}
	else if (omap_gpio_is_omap4(sc, bank)) {
		val = omap_gpio_read_4(sc, bank, OMAP4_GPIO_DATAOUT);
		if (val & mask)
			omap_gpio_write_4(sc, bank, OMAP4_GPIO_CLEARDATAOUT, mask);
		else
			omap_gpio_write_4(sc, bank, OMAP4_GPIO_SETDATAOUT, mask);
	}

	OMAP_GPIO_UNLOCK(sc);

	return (0);
}

/**
 *	omap_gpio_intr - ISR for all GPIO modules
 *	@arg: the soft context pointer
 *
 *	Unsused
 *
 *	LOCKING:
 *	Internally locks the context
 *
 */
static void
omap_gpio_intr(void *arg)
{
	struct omap_gpio_softc *sc = arg;

	OMAP_GPIO_LOCK(sc);
	/* TODO: something useful */
	OMAP_GPIO_UNLOCK(sc);
}

/**
 *	omap_gpio_probe - probe function for the driver
 *	@dev: gpio device handle
 *
 *	Simply sets the name of the driver
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	Always returns 0
 */
static int
omap_gpio_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ti,omap_gpio"))
		return (ENXIO);

	device_set_desc(dev, "TI OMAP General Purpose I/O (GPIO)");
	return (0);
}

/**
 *	omap_gpio_attach - attach function for the driver
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
omap_gpio_attach(device_t dev)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	unsigned int i;
	int err = 0;

	sc->sc_dev = dev;

	OMAP_GPIO_LOCK_INIT(sc);


	/* There are up to 6 different GPIO register sets located in different
	 * memory areas on the chip.  The memory range should have been set for
	 * the driver when it was added as a child (for example in omap44xx.c).
	 */
	err = bus_alloc_resources(dev, omap_gpio_mem_spec, sc->sc_mem_res);
	if (err) {
		device_printf(dev, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}

	/* Request the IRQ resources */
	err = bus_alloc_resources(dev, omap_gpio_irq_spec, sc->sc_irq_res);
	if (err) {
		device_printf(dev, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

	/* Setup the IRQ resources */
	for (i = 0;  i < MAX_GPIO_BANKS; i++) {
		if (sc->sc_irq_res[i] == NULL)
			break;
		
		/* Register an interrupt handler for each of the IRQ resources */
		if ((bus_setup_intr(dev, sc->sc_irq_res[i], INTR_TYPE_MISC | INTR_MPSAFE, 
		                    NULL, omap_gpio_intr, sc, &(sc->sc_irq_hdl[i])))) {
			device_printf(dev, "WARNING: unable to register interrupt handler\n");
			return (ENXIO);
		}
	}

	/* Store the device handle back in the sc */
	sc->sc_dev = dev;

	/* We need to go through each block and ensure the clocks are running and
	 * the module is enabled.  It might be better to do this only when the
	 * pins are configured which would result in less power used if the GPIO
	 * pins weren't used ... 
	 */
	for (i = 0;  i < MAX_GPIO_BANKS; i++) {
		if (sc->sc_mem_res[i] != NULL) {
		
			/* Enable the interface and functional clocks for the module */
			omap_prcm_clk_enable(GPIO1_CLK + i);
			
			/* Read the revision number of the module. TI don't publish the
			 * actual revision numbers, so instead the values have been
			 * determined by experimentation on OMAP4430 and OMAP3530 chips.
			 */
			sc->sc_revision[i] = omap_gpio_read_4(sc, i, OMAP_GPIO_REVISION);
			
			/* Check the revision */
			if (!omap_gpio_is_omap4(sc, i) && !omap_gpio_is_omap3(sc, i)) {
				device_printf(dev, "Warning: could not determine the revision"
				              "of %u GPIO module (revision:0x%08x)\n",
				              i, sc->sc_revision[i]);
				continue;
			}
			
			/* Disable interrupts for all pins */
			if (omap_gpio_is_omap3(sc, i)) {
				omap_gpio_write_4(sc, i, OMAP3_GPIO_CLEARIRQENABLE1, 0xffffffff);
				omap_gpio_write_4(sc, i, OMAP3_GPIO_CLEARIRQENABLE2, 0xffffffff);
			}
			else if (omap_gpio_is_omap4(sc, i)) {
				omap_gpio_write_4(sc, i, OMAP4_GPIO_CLEARIRQENABLE1, 0xffffffff);
				omap_gpio_write_4(sc, i, OMAP4_GPIO_CLEARIRQENABLE2, 0xffffffff);
			}
		}
	}

	/* Finish of the probe call */
	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));
	return (bus_generic_attach(dev));
}

/**
 *	omap_gpio_detach - detach function for the driver
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
omap_gpio_detach(device_t dev)
{
	struct omap_gpio_softc *sc = device_get_softc(dev);
	unsigned int i;

	KASSERT(mtx_initialized(&sc->sc_mtx), ("gpio mutex not initialized"));

	/* Disable all interrupts */
	for (i = 0;  i < MAX_GPIO_BANKS; i++) {
		if (sc->sc_mem_res[i] != NULL) {
			if (omap_gpio_is_omap3(sc, i)) {
				omap_gpio_write_4(sc, i, OMAP3_GPIO_CLEARIRQENABLE1, 0xffffffff);
				omap_gpio_write_4(sc, i, OMAP3_GPIO_CLEARIRQENABLE2, 0xffffffff);
			} else if (omap_gpio_is_omap4(sc, i)) {
				omap_gpio_write_4(sc, i, OMAP4_GPIO_CLEARIRQENABLE1, 0xffffffff);
				omap_gpio_write_4(sc, i, OMAP4_GPIO_CLEARIRQENABLE2, 0xffffffff);
			}
		}
	}

	bus_generic_detach(dev);

	/* Release the memory and IRQ resources */
	for (i = 0;  i < MAX_GPIO_BANKS; i++) {
		if (sc->sc_mem_res[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, i, sc->sc_mem_res[i]);
		if (sc->sc_irq_res[i] != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, i, sc->sc_irq_res[i]);
	}

	OMAP_GPIO_LOCK_DESTROY(sc);

	return(0);
}

static device_method_t omap_gpio_methods[] = {
	DEVMETHOD(device_probe, omap_gpio_probe),
	DEVMETHOD(device_attach, omap_gpio_attach),
	DEVMETHOD(device_detach, omap_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max, omap_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname, omap_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, omap_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, omap_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, omap_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, omap_gpio_pin_get),
	DEVMETHOD(gpio_pin_set, omap_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, omap_gpio_pin_toggle),
	{0, 0},
};

static driver_t omap_gpio_driver = {
	"gpio",
	omap_gpio_methods,
	sizeof(struct omap_gpio_softc),
};
static devclass_t omap_gpio_devclass;

DRIVER_MODULE(omap_gpio, simplebus, omap_gpio_driver, omap_gpio_devclass, 0, 0);
