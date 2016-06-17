/*
 * ip22-ksyms.c: IP22 specific exports
 */

#include <linux/module.h>

#include <asm/sgi/mc.h>
#include <asm/sgi/hpc3.h>
#include <asm/sgi/ioc.h>
#include <asm/sgi/ip22.h>

EXPORT_SYMBOL(sgimc);
EXPORT_SYMBOL(hpc3c0);
EXPORT_SYMBOL(hpc3c1);
EXPORT_SYMBOL(sgioc);

extern void (*indy_volume_button)(int);
EXPORT_SYMBOL(indy_volume_button);

EXPORT_SYMBOL(ip22_eeprom_read);
EXPORT_SYMBOL(ip22_nvram_read);
