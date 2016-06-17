/*
 *  linux/arch/arm/mm/mm-cl7500.c
 *
 *  Copyright (C) 1998 Russell King
 *  Copyright (C) 1999 Nexus Electronics Ltd
 *
 * Extra MM routines for CL7500 architecture
 */
#include <linux/types.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/proc/domain.h>
#include <asm/setup.h>

#include <asm/mach/map.h>

static struct map_desc cl7500_io_desc[] __initdata = {
	{ IO_BASE,	IO_START,	IO_SIZE	 , DOMAIN_IO, 0, 1 },	/* IO space	*/
	{ ISA_BASE,	ISA_START,	ISA_SIZE , DOMAIN_IO, 0, 1 },	/* ISA space	*/
	{ FLASH_BASE,	FLASH_START,	FLASH_SIZE, DOMAIN_IO, 0, 1 },	/* Flash	*/
	{ LED_BASE,	LED_START,	LED_SIZE , DOMAIN_IO, 0, 1 },	/* LED		*/
	LAST_DESC
};

void __init clps7500_map_io(void)
{
	iotable_init(cl7500_io_desc);
}
