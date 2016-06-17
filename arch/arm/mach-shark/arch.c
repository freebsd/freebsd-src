/*
 *  linux/arch/arm/mach-shark/arch.c
 *
 *  Architecture specific stuff.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/hardware/dec21285.h>
#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void shark_map_io(void);
extern void genarch_init_irq(void);

MACHINE_START(SHARK, "Shark")
	MAINTAINER("Alexander Schulz")
	BOOT_MEM(0x08000000, 0x40000000, 0xe0000000)
	BOOT_PARAMS(0x08003000)
	MAPIO(shark_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
