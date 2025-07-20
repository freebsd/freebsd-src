/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sx.h>

#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/gpio/gpiobusvar.h>

#include "max77620.h"

MALLOC_DEFINE(M_MAX77620_GPIO, "MAX77620 gpio", "MAX77620 GPIO");

#define	NGPIO		8

#define	GPIO_LOCK(_sc)	sx_slock(&(_sc)->gpio_lock)
#define	GPIO_UNLOCK(_sc)	sx_unlock(&(_sc)->gpio_lock)
#define	GPIO_ASSERT(_sc)	sx_assert(&(_sc)->gpio_lock, SA_LOCKED)

enum prop_id {
	CFG_BIAS_PULL_UP,
	CFG_BIAS_PULL_DOWN,
	CFG_OPEN_DRAIN,
	CFG_PUSH_PULL,

	CFG_ACTIVE_FPS_SRC,
	CFG_ACTIVE_PWRUP_SLOT,
	CFG_ACTIVE_PWRDOWN_SLOT,
	CFG_SUSPEND_FPS_SRC,
	CFG_SUSPEND_PWRUP_SLOT,
	CFG_SUSPEND_PWRDOWN_SLOT,

	PROP_ID_MAX_ID
};

static const struct {
	const char	*name;
	enum prop_id  	id;
} max77620_prop_names[] = {
	{"bias-pull-up",			CFG_BIAS_PULL_UP},
	{"bias-pull-down",			CFG_BIAS_PULL_DOWN},
	{"drive-open-drain",			CFG_OPEN_DRAIN},
	{"drive-push-pull",			CFG_PUSH_PULL},
	{"maxim,active-fps-source",		CFG_ACTIVE_FPS_SRC},
	{"maxim,active-fps-power-up-slot",	CFG_ACTIVE_PWRUP_SLOT},
	{"maxim,active-fps-power-down-slot",	CFG_ACTIVE_PWRDOWN_SLOT},
	{"maxim,suspend-fps-source",		CFG_SUSPEND_FPS_SRC},
	{"maxim,suspend-fps-power-up-slot",	CFG_SUSPEND_PWRUP_SLOT},
	{"maxim,suspend-fps-power-down-slot",	CFG_SUSPEND_PWRDOWN_SLOT},
};

/* Configuration for one pin group. */
struct max77620_pincfg {
	bool	alt_func;
	int	params[PROP_ID_MAX_ID];
};

static char *altfnc_table[] = {
	"lpm-control-in",
	"fps-out",
	"32k-out1",
	"sd0-dvs-in",
	"sd1-dvs-in",
	"reference-out",
};

struct max77620_gpio_pin {
	int		pin_caps;
	char		pin_name[GPIOMAXNAME];
	uint8_t		reg;

	/* Runtime data  */
	bool		alt_func;	/* GPIO or alternate function */
};

/* --------------------------------------------------------------------------
 *
 *  Pinmux functions.
 */
static int
max77620_pinmux_get_function(struct max77620_softc *sc, char *name,
    struct max77620_pincfg *cfg)
{
	int i;

	if (strcmp("gpio", name) == 0) {
		cfg->alt_func = false;
		return (0);
	}
	for (i = 0; i < nitems(altfnc_table); i++) {
		if (strcmp(altfnc_table[i], name) == 0) {
			cfg->alt_func = true;
			return (0);
		}
	}
	return (-1);
}

static int
max77620_pinmux_set_fps(struct max77620_softc *sc, int pin_num,
    struct max77620_gpio_pin *pin)
{
#if 0
	struct max77620_fps_config *fps_config = &mpci->fps_config[pin];
	int addr, ret;
	int param_val;
	int mask, shift;

	if ((pin < 1) || (pin > 3))
		return (0);

	switch (param) {
	case MAX77620_ACTIVE_FPS_SOURCE:
	case MAX77620_SUSPEND_FPS_SOURCE:
		mask = MAX77620_FPS_SRC_MASK;
		shift = MAX77620_FPS_SRC_SHIFT;
		param_val = fps_config->active_fps_src;
		if (param == MAX77620_SUSPEND_FPS_SOURCE)
			param_val = fps_config->suspend_fps_src;
		break;

	case MAX77620_ACTIVE_FPS_POWER_ON_SLOTS:
	case MAX77620_SUSPEND_FPS_POWER_ON_SLOTS:
		mask = MAX77620_FPS_PU_PERIOD_MASK;
		shift = MAX77620_FPS_PU_PERIOD_SHIFT;
		param_val = fps_config->active_power_up_slots;
		if (param == MAX77620_SUSPEND_FPS_POWER_ON_SLOTS)
			param_val = fps_config->suspend_power_up_slots;
		break;

	case MAX77620_ACTIVE_FPS_POWER_DOWN_SLOTS:
	case MAX77620_SUSPEND_FPS_POWER_DOWN_SLOTS:
		mask = MAX77620_FPS_PD_PERIOD_MASK;
		shift = MAX77620_FPS_PD_PERIOD_SHIFT;
		param_val = fps_config->active_power_down_slots;
		if (param == MAX77620_SUSPEND_FPS_POWER_DOWN_SLOTS)
			param_val = fps_config->suspend_power_down_slots;
		break;

	default:
		dev_err(mpci->dev, "Invalid parameter %d for pin %d\n",
			param, pin);
		return -EINVAL;
	}

	if (param_val < 0)
		return 0;

	ret = regmap_update_bits(mpci->rmap, addr, mask, param_val << shift);
	if (ret < 0)
		dev_err(mpci->dev, "Reg 0x%02x update failed %d\n", addr, ret);

	return ret;
#endif
	return (0);
}

static int
max77620_pinmux_config_node(struct max77620_softc *sc, char *pin_name,
    struct max77620_pincfg *cfg)
{
	struct max77620_gpio_pin *pin;
	uint8_t reg;
	int pin_num, rv;

	for (pin_num = 0; pin_num < sc->gpio_npins; pin_num++) {
		if (strcmp(sc->gpio_pins[pin_num]->pin_name, pin_name) == 0)
			 break;
	}
	if (pin_num >= sc->gpio_npins) {
		device_printf(sc->dev, "Unknown pin: %s\n", pin_name);
		return (ENXIO);
	}
	pin = sc->gpio_pins[pin_num];

	rv = max77620_pinmux_set_fps(sc, pin_num, pin);
	if (rv != 0)
		return (rv);

	rv = RD1(sc, pin->reg, &reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read GIPO_CFG register\n");
		return (ENXIO);
	}

	if (cfg->alt_func) {
		pin->alt_func = true;
		sc->gpio_reg_ame |=  1 << pin_num;
	} else {
		pin->alt_func = false;
		sc->gpio_reg_ame &=  ~(1 << pin_num);
	}

	/* Pull up/down. */
	switch (cfg->params[CFG_BIAS_PULL_UP]) {
	case 1:
		sc->gpio_reg_pue |= 1 << pin_num;
		break;
	case 0:
		sc->gpio_reg_pue &= ~(1 << pin_num);
		break;
	default:
		break;
	}

	switch (cfg->params[CFG_BIAS_PULL_DOWN]) {
	case 1:
		sc->gpio_reg_pde |= 1 << pin_num;
		break;
	case 0:
		sc->gpio_reg_pde &= ~(1 << pin_num);
		break;
	default:
		break;
	}

	/* Open drain/push-pull modes. */
	if (cfg->params[CFG_OPEN_DRAIN] == 1) {
		reg &= ~MAX77620_REG_GPIO_DRV(~0);
		reg |= MAX77620_REG_GPIO_DRV(MAX77620_REG_GPIO_DRV_OPENDRAIN);
	}

	if (cfg->params[CFG_PUSH_PULL] == 1) {
		reg &= ~MAX77620_REG_GPIO_DRV(~0);
		reg |= MAX77620_REG_GPIO_DRV(MAX77620_REG_GPIO_DRV_PUSHPULL);
	}

	rv = WR1(sc, pin->reg, reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read GIPO_CFG register\n");
		return (ENXIO);
	}

	return (0);
}

static int
max77620_pinmux_read_node(struct max77620_softc *sc, phandle_t node,
     struct max77620_pincfg *cfg, char **pins, int *lpins)
{
	char *function;
	int rv, i;

	*lpins = OF_getprop_alloc(node, "pins", (void **)pins);
	if (*lpins <= 0)
		return (ENOENT);

	/* Read function (mux) settings. */
	rv = OF_getprop_alloc(node, "function", (void **)&function);
	if (rv >  0) {
		rv = max77620_pinmux_get_function(sc, function, cfg);
		if (rv == -1) {
			device_printf(sc->dev,
			    "Unknown function %s\n", function);
			OF_prop_free(function);
			return (ENXIO);
		}
	}

	/* Read numeric properties. */
	for (i = 0; i < PROP_ID_MAX_ID; i++) {
		rv = OF_getencprop(node, max77620_prop_names[i].name,
		    &cfg->params[i], sizeof(cfg->params[i]));
		if (rv <= 0)
			cfg->params[i] = -1;
	}

	OF_prop_free(function);
	return (0);
}

static int
max77620_pinmux_process_node(struct max77620_softc *sc, phandle_t node)
{
	struct max77620_pincfg cfg;
	char *pins, *pname;
	int i, len, lpins, rv;

	rv = max77620_pinmux_read_node(sc, node, &cfg, &pins, &lpins);
	if (rv != 0)
		return (rv);

	len = 0;
	pname = pins;
	do {
		i = strlen(pname) + 1;
		rv = max77620_pinmux_config_node(sc, pname, &cfg);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot configure pin: %s: %d\n", pname, rv);
		}
		len += i;
		pname += i;
	} while (len < lpins);

	if (pins != NULL)
		OF_prop_free(pins);

	return (rv);
}

int max77620_pinmux_configure(device_t dev, phandle_t cfgxref)
{
	struct max77620_softc *sc;
	phandle_t node, cfgnode;
	uint8_t	old_reg_pue, old_reg_pde, old_reg_ame;
	int rv;

	sc = device_get_softc(dev);
	cfgnode = OF_node_from_xref(cfgxref);

	old_reg_pue =  sc->gpio_reg_pue;
	old_reg_pde = sc->gpio_reg_pde;
	old_reg_ame = sc->gpio_reg_ame;

	for (node = OF_child(cfgnode); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;
		rv = max77620_pinmux_process_node(sc, node);
		if (rv != 0)
			device_printf(dev, "Failed to process pinmux");

	}

	if (old_reg_pue != sc->gpio_reg_pue) {
		rv = WR1(sc, MAX77620_REG_PUE_GPIO, sc->gpio_reg_pue);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot update PUE_GPIO register\n");
			return (ENXIO);
		}
	}

	if (old_reg_pde != sc->gpio_reg_pde) {
		rv = WR1(sc, MAX77620_REG_PDE_GPIO, sc->gpio_reg_pde);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot update PDE_GPIO register\n");
			return (ENXIO);
		}
	}

	if (old_reg_ame != sc->gpio_reg_ame) {
		rv = WR1(sc, MAX77620_REG_AME_GPIO, sc->gpio_reg_ame);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot update PDE_GPIO register\n");
			return (ENXIO);
		}
	}

	return (0);
}

/* --------------------------------------------------------------------------
 *
 *  GPIO
 */
device_t
max77620_gpio_get_bus(device_t dev)
{
	struct max77620_softc *sc;

	sc = device_get_softc(dev);
	return (sc->gpio_busdev);
}

int
max77620_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = NGPIO - 1;
	return (0);
}

int
max77620_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct max77620_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);
	GPIO_LOCK(sc);
	*caps = sc->gpio_pins[pin]->pin_caps;
	GPIO_UNLOCK(sc);
	return (0);
}

int
max77620_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct max77620_softc *sc;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);
	GPIO_LOCK(sc);
	memcpy(name, sc->gpio_pins[pin]->pin_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);
	return (0);
}

static int
max77620_gpio_get_mode(struct max77620_softc *sc, uint32_t pin_num,
 uint32_t *out_flags)
{
	struct max77620_gpio_pin *pin;
	uint8_t reg;
	int rv;

	pin = sc->gpio_pins[pin_num];
	*out_flags = 0;

	rv = RD1(sc, pin->reg, &reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read GIPO_CFG register\n");
		return (ENXIO);
	}

	/* Pin function */
	pin->alt_func = sc->gpio_reg_ame & (1 << pin_num);

	/* Pull up/down. */
	if (sc->gpio_reg_pue & (1 << pin_num))
	    *out_flags |=  GPIO_PIN_PULLUP;
	if (sc->gpio_reg_pde & (1 << pin_num))
	    *out_flags |=  GPIO_PIN_PULLDOWN;

	/* Open drain/push-pull modes. */
	if (MAX77620_REG_GPIO_DRV_GET(reg) == MAX77620_REG_GPIO_DRV_PUSHPULL)
		*out_flags |= GPIO_PIN_PUSHPULL;
	else
		*out_flags |= GPIO_PIN_OPENDRAIN;

	/* Input/output modes. */
	if (MAX77620_REG_GPIO_DRV_GET(reg) == MAX77620_REG_GPIO_DRV_PUSHPULL)
		*out_flags |= GPIO_PIN_OUTPUT;
	else
		*out_flags |= GPIO_PIN_OUTPUT | GPIO_PIN_INPUT;
	return (0);
}

int
max77620_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *out_flags)
{
	struct max77620_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
#if 0 /* It colide with GPIO regulators */
	/* Is pin in GPIO mode ? */
	if (sc->gpio_pins[pin]->alt_func) {
		GPIO_UNLOCK(sc);
		return (ENXIO);
	}
#endif
	rv = max77620_gpio_get_mode(sc, pin, out_flags);
	GPIO_UNLOCK(sc);

	return (rv);
}

int
max77620_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	struct max77620_softc *sc;
	struct max77620_gpio_pin *pin;
	uint8_t reg;
	uint8_t	old_reg_pue, old_reg_pde;
	int rv;

	sc = device_get_softc(dev);
	if (pin_num >= sc->gpio_npins)
		return (EINVAL);

	pin = sc->gpio_pins[pin_num];

	GPIO_LOCK(sc);

#if 0 /* It colide with GPIO regulators */
	/* Is pin in GPIO mode ? */
	if (pin->alt_func) {
		GPIO_UNLOCK(sc);
		return (ENXIO);
	}
#endif

	old_reg_pue =  sc->gpio_reg_pue;
	old_reg_pde = sc->gpio_reg_pde;

	rv = RD1(sc, pin->reg, &reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read GIPO_CFG register\n");
		GPIO_UNLOCK(sc);
		return (ENXIO);
	}

	if (flags & GPIO_PIN_PULLUP)
		sc->gpio_reg_pue |= 1 << pin_num;
	else
		sc->gpio_reg_pue &= ~(1 << pin_num);

	if (flags & GPIO_PIN_PULLDOWN)
		sc->gpio_reg_pde |= 1 << pin_num;
	else
		sc->gpio_reg_pde &= ~(1 << pin_num);

	if (flags & GPIO_PIN_INPUT) {
		reg &= ~MAX77620_REG_GPIO_DRV(~0);
		reg |= MAX77620_REG_GPIO_DRV(MAX77620_REG_GPIO_DRV_OPENDRAIN);
		reg &= ~MAX77620_REG_GPIO_OUTPUT_VAL(~0);
		reg |= MAX77620_REG_GPIO_OUTPUT_VAL(1);

	} else if (((flags & GPIO_PIN_OUTPUT) &&
	    (flags & GPIO_PIN_OPENDRAIN) == 0) ||
	    (flags & GPIO_PIN_PUSHPULL)) {
		reg &= ~MAX77620_REG_GPIO_DRV(~0);
		reg |= MAX77620_REG_GPIO_DRV(MAX77620_REG_GPIO_DRV_PUSHPULL);
	} else {
		reg &= ~MAX77620_REG_GPIO_DRV(~0);
		reg |= MAX77620_REG_GPIO_DRV(MAX77620_REG_GPIO_DRV_OPENDRAIN);
	}

	rv = WR1(sc, pin->reg, reg);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read GIPO_CFG register\n");
		return (ENXIO);
	}
	if (old_reg_pue != sc->gpio_reg_pue) {
		rv = WR1(sc, MAX77620_REG_PUE_GPIO, sc->gpio_reg_pue);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot update PUE_GPIO register\n");
			GPIO_UNLOCK(sc);
			return (ENXIO);
		}
	}

	if (old_reg_pde != sc->gpio_reg_pde) {
		rv = WR1(sc, MAX77620_REG_PDE_GPIO, sc->gpio_reg_pde);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot update PDE_GPIO register\n");
			GPIO_UNLOCK(sc);
			return (ENXIO);
		}
	}

	GPIO_UNLOCK(sc);
	return (0);
}

int
max77620_gpio_pin_set(device_t dev, uint32_t pin, uint32_t val)
{
	struct max77620_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	rv = RM1(sc, sc->gpio_pins[pin]->reg, MAX77620_REG_GPIO_OUTPUT_VAL(~0),
	     MAX77620_REG_GPIO_OUTPUT_VAL(val));
	GPIO_UNLOCK(sc);
	return (rv);
}

int
max77620_gpio_pin_get(device_t dev, uint32_t pin, uint32_t *val)
{
	struct max77620_softc *sc;
	uint8_t tmp;
	int rv;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	rv = RD1(sc, sc->gpio_pins[pin]->reg, &tmp);

	if (MAX77620_REG_GPIO_DRV_GET(tmp) == MAX77620_REG_GPIO_DRV_PUSHPULL)
		*val = MAX77620_REG_GPIO_OUTPUT_VAL_GET(tmp);
	else
		*val = MAX77620_REG_GPIO_INPUT_VAL_GET(tmp);
	GPIO_UNLOCK(sc);
	if (rv != 0)
		return (rv);

	return (0);
}

int
max77620_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct max77620_softc *sc;
	uint8_t tmp;
	int rv;

	sc = device_get_softc(dev);
	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	rv = RD1(sc, sc->gpio_pins[pin]->reg, &tmp);
	if (rv != 0) {
		GPIO_UNLOCK(sc);
		return (rv);
	}
	tmp ^= MAX77620_REG_GPIO_OUTPUT_VAL(~0);
	rv = RM1(sc, sc->gpio_pins[pin]->reg, MAX77620_REG_GPIO_OUTPUT_VAL(~0),
	   tmp);
	GPIO_UNLOCK(sc);
	return (0);
}

int
max77620_gpio_map_gpios(device_t dev, phandle_t pdev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags)
{

	if (gcells != 2)
		return (ERANGE);
	*pin = gpios[0];
	*flags= gpios[1];
	return (0);
}

int
max77620_gpio_attach(struct max77620_softc *sc, phandle_t node)
{
	struct max77620_gpio_pin *pin;
	int i, rv;

	sx_init(&sc->gpio_lock, "MAX77620 GPIO lock");

	sc->gpio_busdev = gpiobus_add_bus(sc->dev);
	if (sc->gpio_busdev == NULL)
		return (ENXIO);

	rv = RD1(sc, MAX77620_REG_PUE_GPIO, &sc->gpio_reg_pue);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read PUE_GPIO register\n");
		return (ENXIO);
	}

	rv = RD1(sc, MAX77620_REG_PDE_GPIO, &sc->gpio_reg_pde);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read PDE_GPIO register\n");
		return (ENXIO);
	}

	rv = RD1(sc, MAX77620_REG_AME_GPIO, &sc->gpio_reg_ame);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read AME_GPIO register\n");
		return (ENXIO);
	}

	sc->gpio_npins = NGPIO;
	sc->gpio_pins = malloc(sizeof(struct max77620_gpio_pin *) *
	    sc->gpio_npins, M_MAX77620_GPIO, M_WAITOK | M_ZERO);
	for (i = 0; i < sc->gpio_npins; i++) {
		sc->gpio_pins[i] = malloc(sizeof(struct max77620_gpio_pin),
		    M_MAX77620_GPIO, M_WAITOK | M_ZERO);
		pin = sc->gpio_pins[i];
		sprintf(pin->pin_name, "gpio%d", i);
		pin->pin_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT  |
		    GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL |
		    GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN;
		pin->reg = MAX77620_REG_GPIO0 + i;
	}

	return (0);
}
