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
 *
 * $FreeBSD$
 */

#ifndef _MAX77620_H_

#include <sys/clock.h>

#define	MAX77620_REG_CNFGGLBL1		0x00
#define	MAX77620_REG_CNFGGLBL2		0x01
#define	MAX77620_REG_CNFGGLBL3		0x02
#define	MAX77620_REG_CNFG1_32K		0x03
#define	MAX77620_REG_CNFGBBC		0x04
#define	MAX77620_REG_IRQTOP		0x05
#define	MAX77620_REG_INTLBT		0x06
#define	MAX77620_REG_IRQSD		0x07
#define	MAX77620_REG_IRQ_LVL2_L0_7	0x08
#define	MAX77620_REG_IRQ_LVL2_L8	0x09
#define	MAX77620_REG_IRQ_LVL2_GPIO	0x0A
#define	MAX77620_REG_ONOFFIRQ		0x0B
#define	MAX77620_REG_NVERC		0x0C
#define	MAX77620_REG_IRQTOPM		0x0D
#define	MAX77620_REG_INTENLBT		0x0E
#define	MAX77620_REG_IRQMASKSD		0x0F
#define	MAX77620_REG_IRQ_MSK_L0_7	0x10
#define	MAX77620_REG_IRQ_MSK_L8		0x11
#define	MAX77620_REG_ONOFFIRQM		0x12
#define	MAX77620_REG_STATLBT		0x13
#define	MAX77620_REG_STATSD		0x14
#define	MAX77620_REG_ONOFFSTAT		0x15
#define	MAX77620_REG_SD0		0x16
#define	 MAX77620_SD0_VSEL_MASK			0x3F

#define	MAX77620_REG_SD1		0x17
#define	 MAX77620_SD1_VSEL_MASK			0x7F

#define	MAX77620_REG_SD2		0x18
#define	MAX77620_REG_SD3		0x19
#define	MAX77620_REG_SD4		0x1A
#define	 MAX77620_SDX_VSEL_MASK			0xFF

#define	MAX77620_REG_DVSSD0		0x1B
#define	MAX77620_REG_DVSSD1		0x1C
#define	MAX77620_REG_CFG_SD0		0x1D
#define	MAX77620_REG_CFG_SD1		0x1E
#define	MAX77620_REG_CFG_SD2		0x1F
#define	MAX77620_REG_CFG_SD3		0x20
#define	MAX77620_REG_CFG_SD4		0x21
#define	 MAX77620_SD_SR_MASK			0xC0
#define	 MAX77620_SD_SR_SHIFT			6
#define	 MAX77620_SD_POWER_MODE_MASK		0x30
#define	 MAX77620_SD_POWER_MODE_SHIFT		4
#define	 MAX77620_SD_FPWM_MASK			0x04
#define	 MAX77620_SD_FPWM_SHIFT			2
#define	 MAX77620_SD_FSRADE_MASK		0x01
#define	 MAX77620_SD_FSRADE_SHIFT		0

#define	MAX77620_REG_CFG2_SD		0x22
#define	MAX77620_REG_CFG_LDO0		0x23
#define	MAX77620_REG_CFG2_LDO0		0x24
#define	MAX77620_REG_CFG_LDO1		0x25
#define	MAX77620_REG_CFG2_LDO1		0x26
#define	MAX77620_REG_CFG_LDO2		0x27
#define	MAX77620_REG_CFG2_LDO2		0x28
#define	MAX77620_REG_CFG_LDO3		0x29
#define	MAX77620_REG_CFG2_LDO3		0x2A
#define	MAX77620_REG_CFG_LDO4		0x2B
#define	MAX77620_REG_CFG2_LDO4		0x2C
#define	MAX77620_REG_CFG_LDO5		0x2D
#define	MAX77620_REG_CFG2_LDO5		0x2E
#define	MAX77620_REG_CFG_LDO6		0x2F
#define	MAX77620_REG_CFG2_LDO6		0x30
#define	MAX77620_REG_CFG_LDO7		0x31
#define	MAX77620_REG_CFG2_LDO7		0x32
#define	MAX77620_REG_CFG_LDO8		0x33
#define	 MAX77620_LDO_POWER_MODE_MASK		0xC0
#define	 MAX77620_LDO_POWER_MODE_SHIFT		6
#define	 MAX77620_LDO_VSEL_MASK			0x3F

#define	MAX77620_REG_CFG2_LDO8		0x34
#define	 MAX77620_LDO_SLEW_RATE_MASK		0x1
#define	 MAX77620_LDO_SLEW_RATE_SHIFT		0x0

#define	MAX77620_REG_CFG3_LDO		0x35

#define	MAX77620_REG_GPIO0		0x36
#define	MAX77620_REG_GPIO1		0x37
#define	MAX77620_REG_GPIO2		0x38
#define	MAX77620_REG_GPIO3		0x39
#define	MAX77620_REG_GPIO4		0x3A
#define	MAX77620_REG_GPIO5		0x3B
#define	MAX77620_REG_GPIO6		0x3C
#define	MAX77620_REG_GPIO7		0x3D
#define	 MAX77620_REG_GPIO_INT_GET(x)		(((x) >> 5) & 0x3)
#define	 MAX77620_REG_GPIO_INT(x)		(((x) & 0x3) << 5)
#define	  MAX77620_REG_GPIO_INT_NONE			0
#define	  MAX77620_REG_GPIO_INT_FALLING			1
#define	  MAX77620_REG_GPIO_INT_RISING			2
#define	  MAX77620_REG_GPIO_INT_BOTH			3
#define	 MAX77620_REG_GPIO_OUTPUT_VAL_GET(x)	(((x) >> 3) & 0x1)
#define	 MAX77620_REG_GPIO_OUTPUT_VAL(x)	(((x) & 0x1) << 3)
#define	 MAX77620_REG_GPIO_INPUT_VAL_GET(x)	(((x) << 2) & 0x1)
#define	 MAX77620_REG_GPIO_INPUT_VAL		(1 << 2)
#define	 MAX77620_REG_GPIO_DRV_GET(x)		(((x) >> 0) & 0x1)
#define	 MAX77620_REG_GPIO_DRV(x)		(((x) & 0x1) << 0)
#define	  MAX77620_REG_GPIO_DRV_PUSHPULL		1
#define	  MAX77620_REG_GPIO_DRV_OPENDRAIN		0

#define	MAX77620_REG_PUE_GPIO		0x3E
#define	MAX77620_REG_PDE_GPIO		0x3F
#define	MAX77620_REG_AME_GPIO		0x40
#define	MAX77620_REG_ONOFFCNFG1		0x41
#define	MAX77620_REG_ONOFFCNFG2		0x42

#define	MAX77620_REG_FPS_CFG0		0x43
#define	MAX77620_REG_FPS_CFG1		0x44
#define	MAX77620_REG_FPS_CFG2		0x45
#define	 MAX77620_FPS_TIME_PERIOD_MASK		0x38
#define	 MAX77620_FPS_TIME_PERIOD_SHIFT		3
#define	 MAX77620_FPS_EN_SRC_MASK		0x06
#define	 MAX77620_FPS_EN_SRC_SHIFT		1
#define	 MAX77620_FPS_ENFPS_SW_MASK		0x01
#define	 MAX77620_FPS_ENFPS_SW			0x01

#define	MAX77620_REG_FPS_LDO0		0x46
#define	MAX77620_REG_FPS_LDO1		0x47
#define	MAX77620_REG_FPS_LDO2		0x48
#define	MAX77620_REG_FPS_LDO3		0x49
#define	MAX77620_REG_FPS_LDO4		0x4A
#define	MAX77620_REG_FPS_LDO5		0x4B
#define	MAX77620_REG_FPS_LDO6		0x4C
#define	MAX77620_REG_FPS_LDO7		0x4D
#define	MAX77620_REG_FPS_LDO8		0x4E
#define	MAX77620_REG_FPS_SD0		0x4F
#define	MAX77620_REG_FPS_SD1		0x50
#define	MAX77620_REG_FPS_SD2		0x51
#define	MAX77620_REG_FPS_SD3		0x52
#define	MAX77620_REG_FPS_SD4		0x53
#define	MAX77620_REG_FPS_GPIO1		0x54
#define	MAX77620_REG_FPS_GPIO2		0x55
#define	MAX77620_REG_FPS_GPIO3		0x56
#define	MAX77620_REG_FPS_RSO		0x57
#define	 MAX77620_FPS_SRC_MASK			0xC0
#define	 MAX77620_FPS_SRC_SHIFT			6
#define	 MAX77620_FPS_PU_PERIOD_MASK		0x38
#define	 MAX77620_FPS_PU_PERIOD_SHIFT		3
#define	 MAX77620_FPS_PD_PERIOD_MASK		0x07
#define	 MAX77620_FPS_PD_PERIOD_SHIFT		0

#define	MAX77620_REG_CID0		0x58
#define	MAX77620_REG_CID1		0x59
#define	MAX77620_REG_CID2		0x5A
#define	MAX77620_REG_CID3		0x5B
#define	MAX77620_REG_CID4		0x5C
#define	MAX77620_REG_CID5		0x5D
#define	MAX77620_REG_DVSSD4		0x5E
#define	MAX20024_REG_MAX_ADD		0x70

/* MIsc FPS definitions. */
#define	MAX77620_FPS_COUNT			3
#define	MAX77620_FPS_PERIOD_MIN_US		40
#define	MAX77620_FPS_PERIOD_MAX_US		2560

/* Power modes */
#define	MAX77620_POWER_MODE_NORMAL		3
#define	MAX77620_POWER_MODE_LPM			2
#define	MAX77620_POWER_MODE_GLPM		1
#define	MAX77620_POWER_MODE_DISABLE		0


struct max77620_reg_sc;
struct max77620_gpio_pin;

struct max77620_softc {
	device_t			dev;
	struct sx			lock;
	int				bus_addr;
	struct resource			*irq_res;
	void				*irq_h;

	int 				shutdown_fps[MAX77620_FPS_COUNT];
	int 				suspend_fps[MAX77620_FPS_COUNT];
	int 				event_source[MAX77620_FPS_COUNT];

	/* Regulators. */
	struct max77620_reg_sc		**regs;
	int				nregs;

	/* GPIO */
	device_t			gpio_busdev;
	struct max77620_gpio_pin 	**gpio_pins;
	int				gpio_npins;
	struct sx			gpio_lock;
	uint8_t				gpio_reg_pue;	/* pull-up enables */
	uint8_t				gpio_reg_pde;	/* pull-down enables */
	uint8_t				gpio_reg_ame;	/* alternate fnc */


};

#define	RD1(sc, reg, val)	max77620_read(sc, reg, val)
#define	WR1(sc, reg, val)	max77620_write(sc, reg, val)
#define	RM1(sc, reg, clr, set)	max77620_modify(sc, reg, clr, set)

int max77620_read(struct max77620_softc *sc, uint8_t reg, uint8_t *val);
int max77620_write(struct max77620_softc *sc, uint8_t reg, uint8_t val);
int max77620_modify(struct max77620_softc *sc, uint8_t reg, uint8_t clear,
    uint8_t set);
int max77620_read_buf(struct max77620_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size);
int max77620_write_buf(struct max77620_softc *sc, uint8_t reg, uint8_t *buf,
    size_t size);

/* Regulators */
int max77620_regulator_attach(struct max77620_softc *sc, phandle_t node);
int max77620_regulator_map(device_t dev, phandle_t xref, int ncells,
    pcell_t *cells, intptr_t *num);

/* RTC */
int max77620_rtc_create(struct max77620_softc *sc, phandle_t node);

/* GPIO */
device_t max77620_gpio_get_bus(device_t dev);
int max77620_gpio_pin_max(device_t dev, int *maxpin);
int max77620_gpio_pin_getname(device_t dev, uint32_t pin, char *name);
int max77620_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags);
int max77620_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);
int max77620_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags);
int max77620_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value);
int max77620_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val);
int max77620_gpio_pin_toggle(device_t dev, uint32_t pin);
int max77620_gpio_map_gpios(device_t dev, phandle_t pdev, phandle_t gparent,
    int gcells, pcell_t *gpios, uint32_t *pin, uint32_t *flags);
int max77620_gpio_attach(struct max77620_softc *sc, phandle_t node);
int max77620_pinmux_configure(device_t dev, phandle_t cfgxref);

#endif /* _MAX77620_H_ */
