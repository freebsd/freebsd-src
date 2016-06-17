/*
 * init.c: PROM library initialisation code.
 *
 * Copyright (C) 1998 Harald Koerfgen
 * Copyright (C) 2002  Maciej W. Rozycki
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/types.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/processor.h>

#include <asm/dec/prom.h>


int (*__rex_bootinit)(void);
int (*__rex_bootread)(void);
int (*__rex_getbitmap)(memmap *);
unsigned long *(*__rex_slot_address)(int);
void *(*__rex_gettcinfo)(void);
int (*__rex_getsysid)(void);
void (*__rex_clear_cache)(void);

int (*__prom_getchar)(void);
char *(*__prom_getenv)(char *);
int (*__prom_printf)(char *, ...);

int (*__pmax_open)(char*, int);
int (*__pmax_lseek)(int, long, int);
int (*__pmax_read)(int, void *, int);
int (*__pmax_close)(int);


/*
 * Detect which PROM's the DECSTATION has, and set the callback vectors
 * appropriately.
 */
void __init which_prom(s32 magic, s32 *prom_vec)
{
	/*
	 * No sign of the REX PROM's magic number means we assume a non-REX
	 * machine (i.e. we're on a DS2100/3100, DS5100 or DS5000/2xx)
	 */
	if (prom_is_rex(magic)) {
		/*
		 * Set up prom abstraction structure with REX entry points.
		 */
		__rex_bootinit =
			(void *)(long)*(prom_vec + REX_PROM_BOOTINIT);
		__rex_bootread =
			(void *)(long)*(prom_vec + REX_PROM_BOOTREAD);
		__rex_getbitmap =
			(void *)(long)*(prom_vec + REX_PROM_GETBITMAP);
		__prom_getchar =
			(void *)(long)*(prom_vec + REX_PROM_GETCHAR);
		__prom_getenv =
			(void *)(long)*(prom_vec + REX_PROM_GETENV);
		__rex_getsysid =
			(void *)(long)*(prom_vec + REX_PROM_GETSYSID);
		__rex_gettcinfo =
			(void *)(long)*(prom_vec + REX_PROM_GETTCINFO);
		__prom_printf =
			(void *)(long)*(prom_vec + REX_PROM_PRINTF);
		__rex_slot_address =
			(void *)(long)*(prom_vec + REX_PROM_SLOTADDR);
		__rex_clear_cache =
			(void *)(long)*(prom_vec + REX_PROM_CLEARCACHE);
	} else {
		/*
		 * Set up prom abstraction structure with non-REX entry points.
		 */
		__prom_getchar = (void *)PMAX_PROM_GETCHAR;
		__prom_getenv = (void *)PMAX_PROM_GETENV;
		__prom_printf = (void *)PMAX_PROM_PRINTF;
		__pmax_open = (void *)PMAX_PROM_OPEN;
		__pmax_lseek = (void *)PMAX_PROM_LSEEK;
		__pmax_read = (void *)PMAX_PROM_READ;
		__pmax_close = (void *)PMAX_PROM_CLOSE;
	}
}

int __init prom_init(s32 argc, s32 *argv, u32 magic, s32 *prom_vec)
{
	extern void dec_machine_halt(void);

	/*
	 * Determine which PROM's we have
	 * (and therefore which machine we're on!)
	 */
	which_prom(magic, prom_vec);

	if (prom_is_rex(magic))
		rex_clear_cache();

	/* Were we compiled with the right CPU option? */
#if defined(CONFIG_CPU_R3000)
	if ((current_cpu_data.cputype == CPU_R4000SC) ||
	    (current_cpu_data.cputype == CPU_R4400SC)) {
		prom_printf("Sorry, this kernel is compiled for the wrong CPU type!\n");
		prom_printf("Please recompile with \"CONFIG_CPU_R4x00 = y\"\n");
		dec_machine_halt();
	}
#endif

#if defined(CONFIG_CPU_R4X00)
	if ((current_cpu_data.cputype == CPU_R3000) ||
	    (current_cpu_data.cputype == CPU_R3000A)) {
		prom_printf("Sorry, this kernel is compiled for the wrong CPU type!\n");
		prom_printf("Please recompile with \"CONFIG_CPU_R3000 = y\"\n");
		dec_machine_halt();
	}
#endif

	prom_meminit(magic);
	prom_identify_arch(magic);
	prom_init_cmdline(argc, argv, magic);

	return 0;
}
