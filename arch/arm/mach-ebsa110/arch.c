/*
 *  linux/arch/arm/mach-ebsa110/arch.c
 *
 *  Architecture specific fixups.
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/hardware/dec21285.h>

extern void ebsa110_map_io(void);
extern void ebsa110_init_irq(void);

MACHINE_START(EBSA110, "EBSA110")
	MAINTAINER("Russell King")
	BOOT_MEM(0x00000000, 0xe0000000, 0xe0000000)
	BOOT_PARAMS(0x00000400)
	DISABLE_PARPORT(0)
	DISABLE_PARPORT(2)
	SOFT_REBOOT
	MAPIO(ebsa110_map_io)
	INITIRQ(ebsa110_init_irq)
MACHINE_END
