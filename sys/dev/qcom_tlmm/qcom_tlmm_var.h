/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>.
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
 *
 * $FreeBSD$
 *
 */

#ifndef	__QCOM_TLMM_VAR_H__
#define	__QCOM_TLMM_VAR_H__

#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->gpio_mtx)
#define GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->gpio_mtx)
#define GPIO_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->gpio_mtx, MA_OWNED)

/*
 * register space access macros
 */
#define GPIO_WRITE(sc, reg, val)	do {	\
		bus_write_4(sc->gpio_mem_res, (reg), (val)); \
	} while (0)

#define GPIO_READ(sc, reg)	 bus_read_4(sc->gpio_mem_res, (reg))

#define GPIO_SET_BITS(sc, reg, bits)	\
	GPIO_WRITE(sc, reg, GPIO_READ(sc, (reg)) | (bits))

#define GPIO_CLEAR_BITS(sc, reg, bits)	\
	GPIO_WRITE(sc, reg, GPIO_READ(sc, (reg)) & ~(bits))


enum prop_id {
	PIN_ID_BIAS_DISABLE = 0,
	PIN_ID_BIAS_HIGH_IMPEDANCE,
	PIN_ID_BIAS_BUS_HOLD,
	PIN_ID_BIAS_PULL_UP,
	PIN_ID_BIAS_PULL_DOWN,
	PIN_ID_BIAS_PULL_PIN_DEFAULT,
	PIN_ID_DRIVE_PUSH_PULL,
	PIN_ID_DRIVE_OPEN_DRAIN,
	PIN_ID_DRIVE_OPEN_SOURCE,
	PIN_ID_DRIVE_STRENGTH,
	PIN_ID_INPUT_ENABLE,
	PIN_ID_INPUT_DISABLE,
	PIN_ID_INPUT_SCHMITT_ENABLE,
	PIN_ID_INPUT_SCHMITT_DISABLE,
	PIN_ID_INPUT_DEBOUNCE,
	PIN_ID_POWER_SOURCE,
	PIN_ID_SLEW_RATE,
	PIN_ID_LOW_POWER_MODE_ENABLE,
	PIN_ID_LOW_POWER_MODE_DISABLE,
	PIN_ID_OUTPUT_LOW,
	PIN_ID_OUTPUT_HIGH,
	PIN_ID_VM_ENABLE,
	PIN_ID_VM_DISABLE,
	PROP_ID_MAX_ID
};

struct qcom_tlmm_prop_name {
	const char		*name;
	enum prop_id		id;
	int			have_value;
};

/*
 * Pull-up / pull-down configuration.
 */
typedef enum {
	QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE = 0,
	QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN = 1,
	QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP = 2,
	QCOM_TLMM_PIN_PUPD_CONFIG_BUS_HOLD = 3,
} qcom_tlmm_pin_pupd_config_t;


/*
 * Pull-up / pull-down resistor configuration.
 */
typedef enum {
	QCOM_TLMM_PIN_RESISTOR_PUPD_CONFIG_10K = 0,
	QCOM_TLMM_PIN_RESISTOR_PUPD_CONFIG_1K5 = 1,
	QCOM_TLMM_PIN_RESISTOR_PUPD_CONFIG_35K = 2,
	QCOM_TLMM_PIN_RESISTOR_PUPD_CONFIG_20K = 3,
} qcom_tlmm_pin_resistor_pupd_config_t;

/*
 * configuration for one pin group.
 */
struct qcom_tlmm_pinctrl_cfg {
	char		*function;
	int		params[PROP_ID_MAX_ID];
};

#define GDEF(_id, ...)							\
{									\
	.id = _id,							\
	.name = "gpio" #_id,						\
	.functions = {"gpio", __VA_ARGS__}				\
}

struct qcom_tlmm_gpio_mux {
	int		id;
	char		*name;
	char		*functions[16]; /* XXX */
};

#define SDEF(n, r, ps, hs...)						\
{									\
	.name = n,							\
	.reg = r,							\
	.pull_shift = ps,						\
	.hdrv_shift = hs,						\
}


struct qcom_tlmm_spec_pin {
	char *name;
	uint32_t reg;
	uint32_t pull_shift;
	uint32_t hdrv_shift;
};

struct qcom_tlmm_softc {
	device_t		dev;
	device_t		busdev;
	struct mtx		gpio_mtx;
	struct resource		*gpio_mem_res;
	int			gpio_mem_rid;
	struct resource		*gpio_irq_res;
	int			gpio_irq_rid;
	void			*gpio_ih;
	int			gpio_npins;
	struct gpio_pin		*gpio_pins;
	uint32_t		sc_debug;

	const struct qcom_tlmm_gpio_mux	*gpio_muxes;
	const struct qcom_tlmm_spec_pin	*spec_pins;
};

/*
 * qcom_tlmm_pinmux.c
 */
extern	int qcom_tlmm_pinctrl_configure(device_t dev, phandle_t cfgxref);

#endif	/* __QCOM_TLMM_PINMUX_VAR_H__ */
