/*
 * linux/include/asm-arm/arch-sa1100/system.h
 *
 * Copyright (c) 1999 Nicolas Pitre <nico@cam.org>
 */
#include <linux/config.h>
#include <asm/arch/hardware.h>

static inline void arch_idle(void)
{
	cpu_do_idle();
}

#ifdef CONFIG_SA1100_VICTOR

/* power off unconditionally */
#define arch_reset(x) machine_power_off()

#else

static inline void arch_reset(char mode)
{
	if (mode == 's') {
		/* Jump into ROM at address 0 */
		cpu_reset(0);
	} else {
		/* Use on-chip reset capability */
		RSRR = RSRR_SWR;
	}
}

#endif
