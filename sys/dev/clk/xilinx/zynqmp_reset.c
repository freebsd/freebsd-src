/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Beckhoff Automation GmbH & Co. KG
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/hwreset/hwreset.h>

#include <dev/firmware/xilinx/pm_defs.h>

#include "hwreset_if.h"
#include "zynqmp_firmware_if.h"

#define	ZYNQMP_RESET_PCIE_CFG		0
#define	ZYNQMP_RESET_PCIE_BRIDGE	1
#define	ZYNQMP_RESET_PCIE_CTRL		2
#define	ZYNQMP_RESET_DP			3
#define	ZYNQMP_RESET_SWDT_CRF		4
#define	ZYNQMP_RESET_AFI_FM5		5
#define	ZYNQMP_RESET_AFI_FM4		6
#define	ZYNQMP_RESET_AFI_FM3		7
#define	ZYNQMP_RESET_AFI_FM2		8
#define	ZYNQMP_RESET_AFI_FM1		9
#define	ZYNQMP_RESET_AFI_FM0		10
#define	ZYNQMP_RESET_GDMA		11
#define	ZYNQMP_RESET_GPU_PP1		12
#define	ZYNQMP_RESET_GPU_PP0		13
#define	ZYNQMP_RESET_GPU		14
#define	ZYNQMP_RESET_GT			15
#define	ZYNQMP_RESET_SATA		16
#define	ZYNQMP_RESET_ACPU3_PWRON	17
#define	ZYNQMP_RESET_ACPU2_PWRON	18
#define	ZYNQMP_RESET_ACPU1_PWRON	19
#define	ZYNQMP_RESET_ACPU0_PWRON	20
#define	ZYNQMP_RESET_APU_L2		21
#define	ZYNQMP_RESET_ACPU3		22
#define	ZYNQMP_RESET_ACPU2		23
#define	ZYNQMP_RESET_ACPU1		24
#define	ZYNQMP_RESET_ACPU0		25
#define	ZYNQMP_RESET_DDR		26
#define	ZYNQMP_RESET_APM_FPD		27
#define	ZYNQMP_RESET_SOFT		28
#define	ZYNQMP_RESET_GEM0		29
#define	ZYNQMP_RESET_GEM1		30
#define	ZYNQMP_RESET_GEM2		31
#define	ZYNQMP_RESET_GEM3		32
#define	ZYNQMP_RESET_QSPI		33
#define	ZYNQMP_RESET_UART0		34
#define	ZYNQMP_RESET_UART1		35
#define	ZYNQMP_RESET_SPI0		36
#define	ZYNQMP_RESET_SPI1		37
#define	ZYNQMP_RESET_SDIO0		38
#define	ZYNQMP_RESET_SDIO1		39
#define	ZYNQMP_RESET_CAN0		40
#define	ZYNQMP_RESET_CAN1		41
#define	ZYNQMP_RESET_I2C0		42
#define	ZYNQMP_RESET_I2C1		43
#define	ZYNQMP_RESET_TTC0		44
#define	ZYNQMP_RESET_TTC1		45
#define	ZYNQMP_RESET_TTC2		46
#define	ZYNQMP_RESET_TTC3		47
#define	ZYNQMP_RESET_SWDT_CRL		48
#define	ZYNQMP_RESET_NAND		49
#define	ZYNQMP_RESET_ADMA		50
#define	ZYNQMP_RESET_GPIO		51
#define	ZYNQMP_RESET_IOU_CC		52
#define	ZYNQMP_RESET_TIMESTAMP		53
#define	ZYNQMP_RESET_RPU_R50		54
#define	ZYNQMP_RESET_RPU_R51		55
#define	ZYNQMP_RESET_RPU_AMBA		56
#define	ZYNQMP_RESET_OCM		57
#define	ZYNQMP_RESET_RPU_PGE		58
#define	ZYNQMP_RESET_USB0_CORERESET	59
#define	ZYNQMP_RESET_USB1_CORERESET	60
#define	ZYNQMP_RESET_USB0_HIBERRESET	61
#define	ZYNQMP_RESET_USB1_HIBERRESET	62
#define	ZYNQMP_RESET_USB0_APB		63
#define	ZYNQMP_RESET_USB1_APB		64
#define	ZYNQMP_RESET_IPI		65
#define	ZYNQMP_RESET_APM_LPD		66
#define	ZYNQMP_RESET_RTC		67
#define	ZYNQMP_RESET_SYSMON		68
#define	ZYNQMP_RESET_AFI_FM6		69
#define	ZYNQMP_RESET_LPD_SWDT		70
#define	ZYNQMP_RESET_FPD		71
#define	ZYNQMP_RESET_RPU_DBG1		72
#define	ZYNQMP_RESET_RPU_DBG0		73
#define	ZYNQMP_RESET_DBG_LPD		74
#define	ZYNQMP_RESET_DBG_FPD		75
#define	ZYNQMP_RESET_APLL		76
#define	ZYNQMP_RESET_DPLL		77
#define	ZYNQMP_RESET_VPLL		78
#define	ZYNQMP_RESET_IOPLL		79
#define	ZYNQMP_RESET_RPLL		80
#define	ZYNQMP_RESET_GPO3_PL_0		81
#define	ZYNQMP_RESET_GPO3_PL_1		82
#define	ZYNQMP_RESET_GPO3_PL_2		83
#define	ZYNQMP_RESET_GPO3_PL_3		84
#define	ZYNQMP_RESET_GPO3_PL_4		85
#define	ZYNQMP_RESET_GPO3_PL_5		86
#define	ZYNQMP_RESET_GPO3_PL_6		87
#define	ZYNQMP_RESET_GPO3_PL_7		88
#define	ZYNQMP_RESET_GPO3_PL_8		89
#define	ZYNQMP_RESET_GPO3_PL_9		90
#define	ZYNQMP_RESET_GPO3_PL_10		91
#define	ZYNQMP_RESET_GPO3_PL_11		92
#define	ZYNQMP_RESET_GPO3_PL_12		93
#define	ZYNQMP_RESET_GPO3_PL_13		94
#define	ZYNQMP_RESET_GPO3_PL_14		95
#define	ZYNQMP_RESET_GPO3_PL_15		96
#define	ZYNQMP_RESET_GPO3_PL_16		97
#define	ZYNQMP_RESET_GPO3_PL_17		98
#define	ZYNQMP_RESET_GPO3_PL_18		99
#define	ZYNQMP_RESET_GPO3_PL_19		100
#define	ZYNQMP_RESET_GPO3_PL_20		101
#define	ZYNQMP_RESET_GPO3_PL_21		102
#define	ZYNQMP_RESET_GPO3_PL_22		103
#define	ZYNQMP_RESET_GPO3_PL_23		104
#define	ZYNQMP_RESET_GPO3_PL_24		105
#define	ZYNQMP_RESET_GPO3_PL_25		106
#define	ZYNQMP_RESET_GPO3_PL_26		107
#define	ZYNQMP_RESET_GPO3_PL_27		108
#define	ZYNQMP_RESET_GPO3_PL_28		109
#define	ZYNQMP_RESET_GPO3_PL_29		110
#define	ZYNQMP_RESET_GPO3_PL_30		111
#define	ZYNQMP_RESET_GPO3_PL_31		112
#define	ZYNQMP_RESET_RPU_LS		113
#define	ZYNQMP_RESET_PS_ONLY		114
#define	ZYNQMP_RESET_PL			115
#define	ZYNQMP_RESET_PS_PL0		116
#define	ZYNQMP_RESET_PS_PL1		117
#define	ZYNQMP_RESET_PS_PL2		118
#define	ZYNQMP_RESET_PS_PL3		119
#define	ZYNQMP_RESET_MAX		ZYNQMP_RESET_PS_PL3

struct zynqmp_reset_softc {
	device_t	dev;
	device_t	parent;
};

static int
zynqmp_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct zynqmp_reset_softc *sc;
	int rv;

	if (id > ZYNQMP_RESET_MAX)
		return (EINVAL);
	sc = device_get_softc(dev);
	device_printf(dev, "%s called for id = %ld, reset =%d\n", __func__, id, reset);
	rv = ZYNQMP_FIRMWARE_RESET_ASSERT(sc->parent, id, reset);
	return (rv);
}

static int
zynqmp_reset_is_asserted(device_t dev, intptr_t id, bool *reset)
{
	struct zynqmp_reset_softc *sc;
	int rv;

	if (id > ZYNQMP_RESET_MAX)
		return (EINVAL);
	sc = device_get_softc(dev);
	device_printf(dev, "%s called for id = %ld\n", __func__, id);
	rv = ZYNQMP_FIRMWARE_RESET_GET_STATUS(sc->parent, id, reset);

	return (rv);
}

static int
zynqmp_reset_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "xlnx,zynqmp-reset"))
		return (ENXIO);
	device_set_desc(dev, "ZynqMP Reset Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
zynqmp_reset_attach(device_t dev)
{
	struct zynqmp_reset_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->parent = device_get_parent(dev);

	/* register our self as a reset provider */
	hwreset_register_ofw_provider(dev);

	return (0);
}

static device_method_t zynqmp_reset_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	zynqmp_reset_probe),
	DEVMETHOD(device_attach, 	zynqmp_reset_attach),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	zynqmp_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	zynqmp_reset_is_asserted),

	DEVMETHOD_END
};

static driver_t zynqmp_reset_driver = {
	"zynqmp_reset",
	zynqmp_reset_methods,
	sizeof(struct zynqmp_reset_softc),
};

EARLY_DRIVER_MODULE(zynqmp_reset, simplebus, zynqmp_reset_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_LAST);
