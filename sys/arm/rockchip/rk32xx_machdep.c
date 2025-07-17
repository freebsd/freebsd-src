/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devmap.h>
#include <sys/lock.h>
#include <sys/reboot.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/platformvar.h>

#include <dev/ofw/openfirm.h>

#include <arm/rockchip/rk32xx_mp.h>

#include "platform_if.h"
#define CRU_PHYSBASE		0xFF760000
#define CRU_SIZE		0x00010000
#define	 CRU_GLB_SRST_FST_VALUE	 0x1B0

static platform_def_t rk3288w_platform;

static void
rk32xx_late_init(platform_t plat)
{

}

/*
 * Set up static device mappings.
 */
static int
rk32xx_devmap_init(platform_t plat)
{

	devmap_add_entry(0xFF000000, 0x00E00000);
	return (0);
}

static void
rk32xx_cpu_reset(platform_t plat)
{
	bus_space_handle_t cru;

	printf("Resetting...\n");
	bus_space_map(fdtbus_bs_tag, CRU_PHYSBASE, CRU_SIZE, 0, &cru);

	spinlock_enter();
	dsb();
	/* Generate 'first global software reset' */
	bus_space_write_4(fdtbus_bs_tag, cru, CRU_GLB_SRST_FST_VALUE, 0xfdb9);
	while(1)
		;
}

/*
 * Early putc routine for EARLY_PRINTF support.  To use, add to kernel config:
 *   option SOCDEV_PA=0xFF600000
 *   option SOCDEV_VA=0x70000000
 *   option EARLY_PRINTF
 */
#if 0
#ifdef EARLY_PRINTF
static void
rk32xx_early_putc(int c)
{

	volatile uint32_t * UART_STAT_REG = (uint32_t *)(0x7009007C);
	volatile uint32_t * UART_TX_REG   = (uint32_t *)(0x70090000);
	const uint32_t      UART_TXRDY    = (1 << 2);
	while ((*UART_STAT_REG & UART_TXRDY) == 0)
		continue;
	*UART_TX_REG = c;
}
early_putc_t *early_putc = rk32xx_early_putc;
#endif
#endif
static platform_method_t rk32xx_methods[] = {
	PLATFORMMETHOD(platform_devmap_init,	rk32xx_devmap_init),
	PLATFORMMETHOD(platform_late_init,	rk32xx_late_init),
	PLATFORMMETHOD(platform_cpu_reset,	rk32xx_cpu_reset),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	rk32xx_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	rk32xx_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF2(rk32xx, rk3288,  "RK3288",  0, "rockchip,rk3288",  200);
FDT_PLATFORM_DEF2(rk32xx, rk3288w, "RK3288W", 0, "rockchip,rk3288w", 200);
