#define AUTOSENSE
#define PSEUDO_DMA

/*
 * Trantor T128/T128F/T228 driver
 *	Note : architecturally, the T100 and T130 are different and won't 
 * 	work
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 440-4894
 *
 * DISTRIBUTION RELEASE 3.
 *
 * For more information, please consult 
 *
 * Trantor Systems, Ltd.
 * T128/T128F/T228 SCSI Host Adapter
 * Hardware Specifications
 * 
 * Trantor Systems, Ltd. 
 * 5415 Randall Place
 * Fremont, CA 94538
 * 1+ (415) 770-1400, FAX 1+ (415) 770-9910
 * 
 * and 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

/*
 * Options : 
 * AUTOSENSE - if defined, REQUEST SENSE will be performed automatically
 *      for commands that return with a CHECK CONDITION status. 
 *
 * PSEUDO_DMA - enables PSEUDO-DMA hardware, should give a 3-4X performance
 * increase compared to polled I/O.
 *
 * PARITY - enable parity checking.  Not supported.
 *
 * UNSAFE - leave interrupts enabled during pseudo-DMA transfers.  You
 *          only really want to use this if you're having a problem with
 *          dropped characters during high speed communications, and even
 *          then, you're going to be better off twiddling with transfersize.
 *
 * The card is detected and initialized in one of several ways : 
 * 1.  Autoprobe (default) - since the board is memory mapped, 
 *     a BIOS signature is scanned for to locate the registers.
 *     An interrupt is triggered to autoprobe for the interrupt
 *     line.
 *
 * 2.  With command line overrides - t128=address,irq may be 
 *     used on the LILO command line to override the defaults.
 *
 * 3.  With the T128_OVERRIDE compile time define.  This is 
 *     specified as an array of address, irq tuples.  Ie, for
 *     one board at the default 0xcc000 address, IRQ5, I could say 
 *     -DT128_OVERRIDE={{0xcc000, 5}}
 *	
 *     Note that if the override methods are used, place holders must
 *     be specified for other boards in the system.
 * 
 * T128/T128F jumper/dipswitch settings (note : on my sample, the switches 
 * were epoxy'd shut, meaning I couldn't change the 0xcc000 base address) :
 *
 * T128    Sw7 Sw8 Sw6 = 0ws Sw5 = boot 
 * T128F   Sw6 Sw7 Sw5 = 0ws Sw4 = boot Sw8 = floppy disable
 * cc000   off off      
 * c8000   off on
 * dc000   on  off
 * d8000   on  on
 *
 * 
 * Interrupts 
 * There is a 12 pin jumper block, jp1, numbered as follows : 
 *   T128 (JP1)  	 T128F (J5)
 * 2 4 6 8 10 12	11 9  7 5 3 1
 * 1 3 5 7 9  11	12 10 8 6 4 2
 *
 * 3   2-4
 * 5   1-3
 * 7   3-5
 * T128F only 
 * 10 8-10
 * 12 7-9
 * 14 10-12
 * 15 9-11
 */

#include <asm/system.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "t128.h"
#define AUTOPROBE_IRQ
#include "NCR5380.h"
#include "constants.h"
#include "sd.h"
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/module.h>

static struct override {
	unsigned long address;
	int irq;
} overrides
#ifdef T128_OVERRIDE
[] __initdata = T128_OVERRIDE;
#else
[4] __initdata = { 
	{ 0, IRQ_AUTO},
	{ 0, IRQ_AUTO},
	{ 0, IRQ_AUTO},
	{ 0, IRQ_AUTO}
};
#endif

#define NO_OVERRIDES (sizeof(overrides) / sizeof(struct override))

static struct base {
	unsigned int address;
	int noauto;
} bases[] __initdata = {
	{0xcc000, 0},
	{0xc8000, 0},
	{0xdc000, 0},
	{0xd8000, 0}
};

#define NO_BASES (sizeof (bases) / sizeof (struct base))

static struct signature {
	const char *string;
	int offset;
} signatures[] __initdata = {
	{"TSROM: SCSI BIOS, Version 1.12", 0x36},
};

#define NO_SIGNATURES (sizeof (signatures) /  sizeof (struct signature))

/**
 *	t128_setup
 *	@str: command line 
 *
 *	LILO command line initialization of the overrides array,
 */

int __init t128_setup(char *str)
{
	static int commandline_current = 0;
	int ints[10];
	int i;
	
	get_options(str, sizeof(ints) / sizeof(int), ints);

	if (ints[0] != 2)
		printk(KERN_ERR "t128_setup : usage t128=address,irq\n");
	else if (commandline_current < NO_OVERRIDES) {
		overrides[commandline_current].address = ints[1];
		overrides[commandline_current].irq = ints[2];
		for (i = 0; i < NO_BASES; ++i)
			if (bases[i].address == ints[1]) {
				bases[i].noauto = 1;
				break;
			}
		++commandline_current;
	}
	return 1;
}

__setup("t128=", t128_setup);

/**
 *	t128_detect	-	detect controllers
 *	@tpnt: SCSI template
 *
 *	Detects and initializes T128,T128F, or T228 controllers
 *	that were autoprobed, overridden on the LILO command line, 
 *	or specified at compile time.
 */

int __init t128_detect(Scsi_Host_Template * tpnt)
{
	static int current_override = 0, current_base = 0;
	struct Scsi_Host *instance;
	unsigned long base;
	int sig, count;

	tpnt->proc_name = "t128";
	tpnt->proc_info = &t128_proc_info;

	for (count = 0; current_override < NO_OVERRIDES; ++current_override) 
	{
		base = 0;

		if (overrides[current_override].address)
			base = overrides[current_override].address;
		else
			for (; !base && (current_base < NO_BASES); ++current_base) {
				for (sig = 0; sig < NO_SIGNATURES; ++sig)
					if (!bases[current_base].noauto &&
					    isa_check_signature(bases[current_base].address + signatures[sig].offset,
								signatures[sig].string,
								strlen(signatures[sig].string)))
					{
						base = bases[current_base].address;
						break;
					}
			}

		if (!base)
			break;

		instance = scsi_register(tpnt, sizeof(struct NCR5380_hostdata));
		if (instance == NULL)
			break;

		instance->base = base;

		NCR5380_init(instance, 0);

		if (overrides[current_override].irq != IRQ_AUTO)
			instance->irq = overrides[current_override].irq;
		else
			instance->irq = NCR5380_probe_irq(instance, T128_IRQS);

		if (instance->irq != SCSI_IRQ_NONE)
			if (request_irq(instance->irq, do_t128_intr, SA_INTERRUPT, "t128", NULL)) 
			{
				printk(KERN_WARNING "scsi%d : IRQ%d not free, interrupts disabled\n", instance->host_no, instance->irq);
				instance->irq = SCSI_IRQ_NONE;
			}

		if (instance->irq == SCSI_IRQ_NONE) {
			printk(KERN_INFO "scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
			printk(KERN_INFO "scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
		}

		printk(KERN_INFO "scsi%d : at 0x%08lx", instance->host_no,instance->base);
		if (instance->irq == SCSI_IRQ_NONE)
			printk(" interrupts disabled");
		else
			printk(" irq %d", instance->irq);
		printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d", CAN_QUEUE, CMD_PER_LUN, T128_PUBLIC_RELEASE);
		NCR5380_print_options(instance);
		printk("\n");

		++current_override;
		++count;
	}
	return count;
}

/**
 *	t128_biosparam		-	disk geometry
 *	@disk: device 
 *	@dev: device major/minor
 *	@ip: array to return results
 *
 *	Generates a BIOS / DOS compatible H-C-S mapping for 
 *	the specified device / size.
 * 
 *	Most SCSI boards use this mapping, I could be incorrect.  Some one
 *	using hard disks on a trantor should verify that this mapping
 *	corresponds to that used by the BIOS / ASPI driver by running the
 *	linux fdisk program and matching the H_C_S coordinates to those
 *	that DOS uses.
 */

int t128_biosparam(Disk * disk, kdev_t dev, int *ip)
{
	int size = disk->capacity;
	ip[0] = 64;
	ip[1] = 32;
	ip[2] = size >> 11;
	return 0;
}

/**
 *	NCR5380_pread		-	pseudo DMA read
 *	@instance: controller
 *	@dst: buffer to write to
 *	@len: expect/max length
 *
 *	Fast 5380 pseudo-dma read function, transfers len bytes to 
 *	dst from the controller.
 */

static inline int NCR5380_pread(struct Scsi_Host *instance,
				unsigned char *dst, int len)
{
	unsigned long reg = instance->base + T_DATA_REG_OFFSET;
	unsigned char *d = dst;
	int i = len;

	while (!(isa_readb(instance->base + T_STATUS_REG_OFFSET) & T_ST_RDY))
	       	barrier();
	for (; i; --i) {
		*d++ = isa_readb(reg);
	}

	if (isa_readb(instance->base + T_STATUS_REG_OFFSET) & T_ST_TIM) {
		unsigned char tmp;
		unsigned long foo;
		foo = instance->base + T_CONTROL_REG_OFFSET;
		tmp = isa_readb(foo);
		isa_writeb(tmp | T_CR_CT, foo);
		isa_writeb(tmp, foo);
		printk(KERN_ERR "scsi%d : watchdog timer fired in t128 NCR5380_pread.\n", instance->host_no);
		return -1;
	} else
		return 0;
}

/**
 *	NCR5380_pwrite		-	pseudo DMA write
 *	@instance: controller
 *	@dst: buffer to write to
 *	@len: expect/max length
 *
 *	Fast 5380 pseudo-dma write function, transfers len bytes from
 *	dst to the controller.
 */


static inline int NCR5380_pwrite(struct Scsi_Host *instance,
				 unsigned char *src, int len)
{
	unsigned long reg = instance->base + T_DATA_REG_OFFSET;
	unsigned char *s = src;
	int i = len;

	while (!(isa_readb(instance->base + T_STATUS_REG_OFFSET) & T_ST_RDY))
		barrier();
	for (; i; --i) {
		isa_writeb(*s++, reg);
	}

	if (isa_readb(instance->base + T_STATUS_REG_OFFSET) & T_ST_TIM) {
		unsigned char tmp;
		unsigned long foo;
		foo = instance->base + T_CONTROL_REG_OFFSET;
		tmp = isa_readb(foo);
		isa_writeb(tmp | T_CR_CT, foo);
		isa_writeb(tmp, foo);
		printk(KERN_ERR "scsi%d : watchdog timer fired in t128 NCR5380_pwrite()\n", instance->host_no);
		return -1;
	} else
		return 0;
}

MODULE_LICENSE("GPL");

#include "NCR5380.c"

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = TRANTOR_T128;

#include "scsi_module.c"
