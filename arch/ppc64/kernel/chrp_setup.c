/*
 *  linux/arch/ppc/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Adapted from 'alpha' version by Gary Thomas
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
 *  Modified by PPC64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*
 * bootup setup stuff..
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/blk.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/adb.h>
#include <linux/module.h>
#include <linux/delay.h>

#include <linux/irq.h>
#include <linux/seq_file.h>

#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/pci-bridge.h>
#include <asm/pci_dma.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/keyboard.h>
#include <asm/init.h>
#include <asm/naca.h>
#include <asm/time.h>

#include "local_irq.h"
#include "i8259.h"
#include "open_pic.h"
#include "xics.h"
#include <asm/ppcdebug.h>
#include <asm/cputable.h>

extern volatile unsigned char *chrp_int_ack_special;

void chrp_setup_pci_ptrs(void);
void chrp_progress(char *, unsigned short);
void chrp_request_regions(void);

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];
extern void openpic_init_IRQ(void);
extern void openpic_init_irq_desc(irq_desc_t *);
extern void init_ras_IRQ(void);

extern void find_and_init_phbs(void);
extern void pSeries_pcibios_fixup(void);
extern void iSeries_pcibios_fixup(void);

extern void pSeries_get_boot_time(struct rtc_time *rtc_time);
extern void pSeries_get_rtc_time(struct rtc_time *rtc_time);
extern int  pSeries_set_rtc_time(struct rtc_time *rtc_time);
void pSeries_calibrate_decr(void);
static void machine_check_init(void);
extern void SystemReset_FWNMI(void), MachineCheck_FWNMI(void);	/* from head.S */
int fwnmi_active = 0;  /* TRUE if an FWNMI handler is present */
int check_exception_flag = 0;  /* TRUE if a check-exception handler present */

kdev_t boot_dev;
unsigned long  virtPython0Facilities = 0;  // python0 facility area (memory mapped io) (64-bit format) VIRTUAL address.

extern HPTE *Hash, *Hash_end;
extern unsigned long Hash_size, Hash_mask;
extern int probingmem;
extern unsigned long loops_per_jiffy;

extern unsigned long ppc_proc_freq;
extern unsigned long ppc_tb_freq;
#ifdef CONFIG_BLK_DEV_RAM
extern int rd_doload;		/* 1 = load ramdisk, 0 = don't load */
extern int rd_prompt;		/* 1 = prompt for ramdisk, 0 = don't prompt */
extern int rd_image_start;	/* starting block # of image */
#endif

void 
chrp_get_cpuinfo(struct seq_file *m)
{
	struct device_node *root;
	const char *model = "";

	seq_printf(m, "timebase\t: %lu\n", ppc_tb_freq);

	root = find_path_device("/");
	if (root)
		model = get_property(root, "model", NULL);
	seq_printf(m, "machine\t\t: CHRP %s\n", model);
}

void __init chrp_request_regions(void) {
	request_region(0x20,0x20,"pic1");
	request_region(0xa0,0x20,"pic2");
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
}

void __init
chrp_setup_arch(void)
{
	struct device_node *root;
	unsigned int *opprop;
	
	/* openpic global configuration register (64-bit format). */
	/* openpic Interrupt Source Unit pointer (64-bit format). */
	/* python0 facility area (mmio) (64-bit format) REAL address. */

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000;

#ifdef CONFIG_BLK_DEV_INITRD
	/* this is fine for chrp */
	initrd_below_start_ok = 1;
	
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	else
#endif
	ROOT_DEV = to_kdev_t(0x0803); /* sda3 (sda1 is for the kernel or the bootloader) */

	machine_check_init();

#ifndef CONFIG_PPC_ISERIES
	/* Find and initialize PCI host bridges */
	/* iSeries needs to be done much later. */
	eeh_init();
	find_and_init_phbs();
#endif

	/* Find the Open PIC if present */
	root = find_path_device("/");
	opprop = (unsigned int *) get_property(root,
				"platform-open-pic", NULL);
	if (opprop != 0) {
		int n = prom_n_addr_cells(root);
		unsigned long openpic;

		for (openpic = 0; n > 0; --n)
			openpic = (openpic << 32) + *opprop++;
		printk(KERN_DEBUG "OpenPIC addr: %lx\n", openpic);
		OpenPIC_Addr = __ioremap(openpic, 0x40000, _PAGE_NO_CACHE);
	}

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif
}

void __init
chrp_init2(void)
{
	/*
	 * It is sensitive, when this is called (not too earlu)
	 * -- tibit
	 */
	chrp_request_regions();
	/* Manually leave the kernel version on the panel. */
	ppc_md.progress("Linux ppc64\n", 0);
	ppc_md.progress(UTS_RELEASE, 0);
}

/* Initialize firmware assisted non-maskable interrupts if
 * the firmware supports this feature.  If it does not,
 * look for the check-exception property.
 */
static void __init machine_check_init(void)
{
	long ret;
	int check_ex_token;
	int ibm_nmi_register = rtas_token("ibm,nmi-register");

	if (ibm_nmi_register != RTAS_UNKNOWN_SERVICE) {
		ret = rtas_call(ibm_nmi_register, 2, 1, NULL,
				__pa((unsigned long)SystemReset_FWNMI),
				__pa((unsigned long)MachineCheck_FWNMI));
		if (ret == 0)
			fwnmi_active = 1;
	} else {
		check_ex_token = rtas_token("check-exception");
		if (check_ex_token != RTAS_UNKNOWN_SERVICE)
			check_exception_flag = 1;
	}
}


/* Early initialization.  Relocation is on but do not reference unbolted pages */
void __init pSeries_init_early(void)
{
#ifdef CONFIG_PPC_PSERIES	/* This ifdef should go away */
	void *comport;

	hpte_init_pSeries();
	tce_init_pSeries();
	pSeries_pcibios_init_early();

#ifdef CONFIG_SMP
	smp_init_pSeries();
#endif

	/* Map the uart for udbg. */
	comport = (void *)__ioremap(naca->serialPortAddr, 16, _PAGE_NO_CACHE);
	udbg_init_uart(comport);

	ppc_md.udbg_putc = udbg_putc;
	ppc_md.udbg_getc = udbg_getc;
	ppc_md.udbg_getc_poll = udbg_getc_poll;
#endif
}

void __init
chrp_init(unsigned long r3, unsigned long r4, unsigned long r5,
	  unsigned long r6, unsigned long r7)
{
#if 0 /* PPPBBB remove this later... -Peter */
#ifdef CONFIG_BLK_DEV_INITRD
	/* take care of initrd if we have one */
	if ( r6 )
	{
		initrd_start = __va(r6);
		initrd_end = __va(r6 + r7);
	}
#endif /* CONFIG_BLK_DEV_INITRD */
#endif

	ppc_md.ppc_machine = systemcfg->platform;

	ppc_md.setup_arch     = chrp_setup_arch;
	ppc_md.setup_residual = NULL;
	ppc_md.get_cpuinfo    = chrp_get_cpuinfo;
	if(naca->interrupt_controller == IC_OPEN_PIC) {
		ppc_md.init_IRQ       = openpic_init_IRQ;
		ppc_md.init_irq_desc  = openpic_init_irq_desc;
		ppc_md.get_irq        = openpic_get_irq;
	} else {
		ppc_md.init_IRQ       = xics_init_IRQ;
		ppc_md.init_irq_desc  = xics_init_irq_desc;
		ppc_md.get_irq        = xics_get_irq;
	}
	ppc_md.init_ras_IRQ = init_ras_IRQ;

 	#ifndef CONFIG_PPC_ISERIES
 		ppc_md.pcibios_fixup = pSeries_pcibios_fixup;
		ppc_md.log_error     = pSeries_log_error;
 	#else 
 		ppc_md.pcibios_fixup = NULL;
 		// ppc_md.pcibios_fixup = iSeries_pcibios_fixup;
		ppc_md.log_error     = NULL;
 	#endif


	ppc_md.init           = chrp_init2;

	ppc_md.restart        = rtas_restart;
	ppc_md.power_off      = rtas_power_off;
	ppc_md.halt           = rtas_halt;

	ppc_md.time_init      = NULL;
	ppc_md.get_boot_time  = pSeries_get_boot_time;
	ppc_md.get_rtc_time   = pSeries_get_rtc_time;
	ppc_md.set_rtc_time   = pSeries_set_rtc_time;
	ppc_md.calibrate_decr = pSeries_calibrate_decr;

	ppc_md.progress = chrp_progress;

#ifdef CONFIG_VT
	ppc_md.kbd_setkeycode    = pckbd_setkeycode;
	ppc_md.kbd_getkeycode    = pckbd_getkeycode;
	ppc_md.kbd_translate     = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds          = pckbd_leds;
	ppc_md.kbd_init_hw       = pckbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x63;	/* Print Screen */
#endif
#endif
	/* Build up the firmware_features bitmask field
	 * using contents of device-tree/ibm,hypertas-functions.
	 * Ultimately this functionality may be moved into prom.c prom_init().
	 */
	struct device_node * dn;
	char * hypertas;
	unsigned int len;
	dn = find_path_device("/rtas");
	cur_cpu_spec->firmware_features=0;
	hypertas = get_property(dn, "ibm,hypertas-functions", &len);
	if (hypertas) {
		while (len > 0) {
			int i, hypertas_len;
			/* check value against table of strings */
			for(i=0; i < FIRMWARE_MAX_FEATURES; i++) {
				if ((firmware_features_table[i].name) &&
				    (strcmp(firmware_features_table[i].name,hypertas))==0) {
					/* we have a match */
					cur_cpu_spec->firmware_features |=
						(firmware_features_table[i].val);
					break;
				}
			}
			hypertas_len = strlen(hypertas);
			len -= hypertas_len +1;
			hypertas+= hypertas_len +1;
		}
	}

	printk(KERN_INFO "firmware_features = 0x%lx\n",
	       cur_cpu_spec->firmware_features);
}

void __chrp
chrp_progress(char *s, unsigned short hex)
{
	struct device_node *root;
	int width, *p;
	char *os;
	static int display_character, set_indicator;
	static int max_width;
	static spinlock_t progress_lock = SPIN_LOCK_UNLOCKED;
	static int pending_newline = 0;  /* did last write end with unprinted newline? */

	if (!rtas.base)
		return;

	if (max_width == 0) {
		if ( (root = find_path_device("/rtas")) &&
		     (p = (unsigned int *)get_property(root,
						       "ibm,display-line-length",
						       NULL)) )
			max_width = *p;
		else
			max_width = 0x10;
		display_character = rtas_token("display-character");
		set_indicator = rtas_token("set-indicator");
	}

	if(display_character == RTAS_UNKNOWN_SERVICE) {
		/* use hex display if available */
		if(set_indicator != RTAS_UNKNOWN_SERVICE)
			rtas_call(set_indicator, 3, 1, NULL, 6, 0, hex);
		return;
	}

	spin_lock(&progress_lock);

	/* Last write ended with newline, but we didn't print it since
	 * it would just clear the bottom line of output. Print it now
	 * instead.
	 *
	 * If no newline is pending, print a CR to start output at the
	 * beginning of the line.
	 */
	if(pending_newline) {
		rtas_call(display_character, 1, 1, NULL, '\r');
		rtas_call(display_character, 1, 1, NULL, '\n');
		pending_newline = 0;
	} else
		rtas_call(display_character, 1, 1, NULL, '\r');
 
	width = max_width;
	os = s;
	while (*os) {
		if(*os == '\n' || *os == '\r') {
			/* Blank to end of line. */
			while(width-- > 0)
				rtas_call(display_character, 1, 1, NULL, ' ');
 
			/* If newline is the last character, save it
			 * until next call to avoid bumping up the
			 * display output.
			 */
			if(*os == '\n' && !os[1]) {
				pending_newline = 1;
				spin_unlock(&progress_lock);
				return;
			}
 
			/* RTAS wants CR-LF, not just LF */
 
			if(*os == '\n') {
				rtas_call(display_character, 1, 1, NULL, '\r');
				rtas_call(display_character, 1, 1, NULL, '\n');
			} else {
				/* CR might be used to re-draw a line, so we'll
				 * leave it alone and not add LF.
				 */
				rtas_call(display_character, 1, 1, NULL, *os);
			}
 
			width = max_width;
		} else {
			width--;
			rtas_call(display_character, 1, 1, NULL, *os);
		}
 
		os++;
 
		/* if we overwrite the screen length */
		if ( width <= 0 )
			while ( (*os != 0) && (*os != '\n') && (*os != '\r') )
				os++;
	}
 
	/* Blank to end of line. */
	while ( width-- > 0 )
		rtas_call(display_character, 1, 1, NULL, ' ' );

	spin_unlock(&progress_lock);
}

extern void setup_default_decr(void);

void __init pSeries_calibrate_decr(void)
{
	struct device_node *cpu;
	struct div_result divres;
	int *fp;
	unsigned long freq, processor_freq;

	/*
	 * The cpu node should have a timebase-frequency property
	 * to tell us the rate at which the decrementer counts. 
	 */
	freq = 16666000;        /* hardcoded default */
	cpu = find_type_devices("cpu");
	if (cpu != 0) { 
		fp = (int *) get_property(cpu, "timebase-frequency", NULL);
		if (fp != 0)
			freq = *fp;
	}
	ppc_tb_freq = freq;
	processor_freq = freq;
	if (cpu != 0) {
		fp = (int *) get_property(cpu, "clock-frequency", NULL);
		if (fp != 0)
			processor_freq = *fp;
	}
	ppc_proc_freq = processor_freq;
	
        printk("time_init: decrementer frequency = %lu.%.6lu MHz\n", 
	       freq/1000000, freq%1000000 );
	printk("time_init: processor frequency   = %lu.%.6lu MHz\n",
		processor_freq/1000000, processor_freq%1000000 );

	tb_ticks_per_jiffy = freq / HZ;
	tb_ticks_per_sec = tb_ticks_per_jiffy * HZ;
	tb_ticks_per_usec = freq / 1000000;
	div128_by_32( 1024*1024, 0, tb_ticks_per_sec, &divres );
	tb_to_xs = divres.result_low;

	setup_default_decr();
}

