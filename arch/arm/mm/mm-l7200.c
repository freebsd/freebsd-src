/*
 *  linux/arch/arm/mm/mm-lusl7200.c
 *
 *  Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *
 *  Extra MM routines for L7200 architecture
 */
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/proc/domain.h>

#include <asm/mach/map.h>

static struct map_desc l7200_io_desc[] __initdata = {
	{ IO_BASE,	IO_START,	IO_SIZE,	DOMAIN_IO, 0, 1 ,0 ,0},
	{ IO_BASE_2,	IO_START_2,	IO_SIZE_2,	DOMAIN_IO, 0, 1 ,0 ,0},
	{ AUX_BASE,     AUX_START,      AUX_SIZE,       DOMAIN_IO, 0, 1 ,0 ,0},
	{ FLASH1_BASE,  FLASH1_START,   FLASH1_SIZE,    DOMAIN_IO, 0, 1 ,0 ,0},
	{ FLASH2_BASE,  FLASH2_START,   FLASH2_SIZE,    DOMAIN_IO, 0, 1 ,0 ,0},
	LAST_DESC
};

void __init l7200_map_io(void)
{
	iotable_init(l7200_io_desc);
}
