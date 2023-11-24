/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

/*
 * This is a pinmux/gpio controller for the IPQ4018/IPQ4019.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include "qcom_tlmm_var.h"

#include "qcom_tlmm_ipq4018_reg.h"
#include "qcom_tlmm_ipq4018_hw.h"

#include "gpio_if.h"

/*
 * Set the pin function.  This is a hardware and pin specific mapping.
 *
 * Returns 0 if OK, an errno if an error was encountered.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_function(struct qcom_tlmm_softc *sc,
    int pin, int function)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));
	reg &= ~(QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_MASK
	    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_SHIFT);
	reg |= (function & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_MASK)
	    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_SHIFT;
	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Get the pin function.  This is a hardware and pin specific mapping.
 *
 * Returns 0 if OK, an errno if a nerror was encountered.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_function(struct qcom_tlmm_softc *sc,
    int pin, int *function)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);


	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));
	reg = reg >> QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_SHIFT;
	reg &= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_MASK;
	*function = reg;

	return (0);
}

/*
 * Set the OE bit to be output.  This assumes the port is configured
 * as a GPIO port.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_oe_output(struct qcom_tlmm_softc *sc,
    int pin)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));
	reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OE_ENABLE;
	GPIO_WRITE(sc,
	    QCOM_TLMM_IPQ4018_REG_PIN(pin, QCOM_TLMM_IPQ4018_REG_PIN_CONTROL),
	    reg);

	return (0);
}

/*
 * Set the OE bit to be input.  This assumes the port is configured
 * as a GPIO port.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_oe_input(struct qcom_tlmm_softc *sc,
    int pin)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));
	reg &= ~QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OE_ENABLE;
	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Get the GPIO pin direction.  is_output is set to true if the pin
 * is an output pin, false if it's set to an input pin.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_oe_state(struct qcom_tlmm_softc *sc,
    int pin, bool *is_output)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));
	*is_output = !! (reg & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OE_ENABLE);

	return (0);
}


/*
 * Set the given GPIO pin to the given value.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_output_value(struct qcom_tlmm_softc *sc,
    uint32_t pin, unsigned int value)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO));
	if (value)
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN;
	else
		reg &= ~QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN;
	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO), reg);

	return (0);
}

/*
 * Get the input state of the current GPIO pin.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_output_value(struct qcom_tlmm_softc *sc,
    uint32_t pin, unsigned int *val)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO));

	*val = !! (reg & QCOM_TLMM_IPQ4018_REG_PIN_IO_INPUT_STATUS);

	return (0);
}


/*
 * Get the input state of the current GPIO pin.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_input_value(struct qcom_tlmm_softc *sc,
    uint32_t pin, unsigned int *val)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO));

	*val = !! (reg & QCOM_TLMM_IPQ4018_REG_PIN_IO_INPUT_STATUS);

	return (0);
}

/*
 * Toggle the current output pin value.
 */
int
qcom_tlmm_ipq4018_hw_pin_toggle_output_value(
    struct qcom_tlmm_softc *sc, uint32_t pin)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO));
	if ((reg & QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN) == 0)
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN;
	else
		reg &= ~QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN;
	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_IO), reg);

	return (0);
}

/*
 * Configure the pull-up / pull-down top-level configuration.
 *
 * This doesn't configure the resistor values, just what's enabled/disabled.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_pupd_config(
    struct qcom_tlmm_softc *sc, uint32_t pin,
    qcom_tlmm_pin_pupd_config_t pupd)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	reg &= ~(QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_MASK
	    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT);

	switch (pupd) {
	case QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE:
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_DISABLE
		    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT;
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN:
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLDOWN
		    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT;
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP:
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLUP
		    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT;
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_BUS_HOLD:
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_BUSHOLD
		    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT;
		break;
	}

	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Fetch the current pull-up / pull-down configuration.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_pupd_config(
    struct qcom_tlmm_softc *sc, uint32_t pin,
    qcom_tlmm_pin_pupd_config_t *pupd)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	reg >>= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT;
	reg &= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_MASK;

	switch (reg) {
	case QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_DISABLE:
		*pupd = QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE;
		break;
	case QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLDOWN:
		*pupd = QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN;
		break;
	case QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLUP:
		*pupd = QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP;
		break;
	default:
		*pupd = QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE;
		break;
	}

	return (0);
}

/*
 * Set the drive strength in mA.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_drive_strength(
    struct qcom_tlmm_softc *sc, uint32_t pin, uint8_t drv)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	/* Convert mA to hardware */
	if (drv > 16 || drv < 2)
		return (EINVAL);
	drv = (drv / 2) - 1;

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	reg &= ~(QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_SHIFT
	    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_MASK);
	reg |= (drv & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_MASK)
	    << QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_SHIFT;

	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Get the drive strength in mA.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_drive_strength(
    struct qcom_tlmm_softc *sc, uint32_t pin, uint8_t *drv)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	*drv = (reg >> QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_SHIFT)
	    & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_MASK;

	*drv = (*drv + 1) * 2;

	return (0);
}


/*
 * Enable/disable whether this pin is passed through to a VM.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_vm(
    struct qcom_tlmm_softc *sc, uint32_t pin, bool enable)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	reg &= ~QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_VM_ENABLE;
	if (enable)
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_VM_ENABLE;

	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Get the VM configuration bit.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_vm(
    struct qcom_tlmm_softc *sc, uint32_t pin, bool *enable)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	*enable = !! (reg & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_VM_ENABLE);

	return (0);
}

/*
 * Enable/disable open drain.
 */
int
qcom_tlmm_ipq4018_hw_pin_set_open_drain(
    struct qcom_tlmm_softc *sc, uint32_t pin, bool enable)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	reg &= ~QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OD_ENABLE;
	if (enable)
		reg |= QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OD_ENABLE;

	GPIO_WRITE(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL), reg);

	return (0);
}

/*
 * Get the open drain configuration bit.
 */
int
qcom_tlmm_ipq4018_hw_pin_get_open_drain(
    struct qcom_tlmm_softc *sc, uint32_t pin, bool *enable)
{
	uint32_t reg;

	GPIO_LOCK_ASSERT(sc);

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	reg = GPIO_READ(sc, QCOM_TLMM_IPQ4018_REG_PIN(pin,
	    QCOM_TLMM_IPQ4018_REG_PIN_CONTROL));

	*enable = !! (reg & QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OD_ENABLE);

	return (0);
}
