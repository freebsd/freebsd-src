#define AUTOSENSE
#define PSEUDO_DMA
#define BOARD_REQUIRES_NO_UDELAY	/* PAS16 needs no I/O recovery delays */
#define UNSAFE				/* Not unsafe for PAS16 -- use it */

/*
 * This driver adapted from Drew Eckhardt's Trantor T128 driver
 *
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 666-5836
 *
 *  ( Based on T128 - DISTRIBUTION RELEASE 3. ) 
 *
 * Modified to work with the Pro Audio Spectrum/Studio 16
 * by John Weidman.
 *
 *
 * For more information, please consult 
 *
 * Media Vision
 * (510) 770-8600
 * (800) 348-7116
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
 * LIMIT_TRANSFERSIZE - if defined, limit the pseudo-dma transfers to 512
 *      bytes at a time.  Since interrupts are disabled by default during
 *      these transfers, we might need this to give reasonable interrupt
 *      service time if the transfer size gets too large.
 *
 * PSEUDO_DMA - enables PSEUDO-DMA hardware, should give a 3-4X performance
 * increase compared to polled I/O.
 *
 * PARITY - enable parity checking.  Not supported.
 * 
 * UNSAFE - leave interrupts enabled during pseudo-DMA transfers.  This
 *	    parameter comes from the NCR5380 code.  It is NOT unsafe with
 *	    the PAS16 and you should use it.  If you don't you will have
 *	    a problem with dropped characters during high speed
 *	    communications during SCSI transfers.  If you really don't
 *	    want to use UNSAFE you can try defining LIMIT_TRANSFERSIZE or
 *	    twiddle with the transfer size in the high level code.
 *
 * The card is detected and initialized in one of several ways : 
 * 1.  Autoprobe (default) - There are many different models of
 *     the Pro Audio Spectrum/Studio 16, and I only have one of
 *     them, so this may require a little tweaking.  An interrupt
 *     is triggered to autoprobe for the interrupt line.  Note:
 *     with the newer model boards, the interrupt is set via
 *     software after reset using the default_irq for the
 *     current board number.
 *
 * 2.  With command line overrides - pas16=port,irq may be 
 *     used on the LILO command line to override the defaults.
 *
 * 3.  With the PAS16_OVERRIDE compile time define.  This is 
 *     specified as an array of address, irq tuples.  Ie, for
 *     one board at the default 0x388 address, IRQ10, I could say 
 *     -DPAS16_OVERRIDE={{0x388, 10}}
 *     NOTE:  Untested.
 *	
 * 4.  When included as a module, with arguments passed on the command line:
 *         pas16_irq=xx		the interrupt
 *         pas16_addr=xx	the port
 *     e.g. "modprobe pas16 pas16_addr=0x388 pas16_irq=5"
 *
 *     Note that if the override methods are used, place holders must
 *     be specified for other boards in the system.
 *
 *
 * Configuration notes :
 *   The current driver does not support interrupt sharing with the
 *   sound portion of the card.  If you use the same irq for the
 *   scsi port and sound you will have problems.  Either use
 *   a different irq for the scsi port or don't use interrupts
 *   for the scsi port.
 *
 *   If you have problems with your card not being recognized, use
 *   the LILO command line override.  Try to get it recognized without
 *   interrupts.  Ie, for a board at the default 0x388 base port,
 *   boot: linux pas16=0x388,255
 *
 *   SCSI_IRQ_NONE (255) should be specified for no interrupt,
 *   IRQ_AUTO (254) to autoprobe for an IRQ line if overridden
 *   on the command line.
 *
 *   (IRQ_AUTO == 254, SCSI_IRQ_NONE == 255 in NCR5380.h)
 */

#include <linux/module.h>

#include <asm/system.h>
#include <linux/signal.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "pas16.h"
#define AUTOPROBE_IRQ
#include "NCR5380.h"
#include "constants.h"
#include "sd.h"

#include <linux/stat.h>
#include <linux/init.h>

static int pas_maxi = 0;
static int pas_wmaxi = 0;
static unsigned short pas16_addr = 0;
static int pas16_irq = 0;

static int scsi_irq_translate[] = { 0, 0, 1, 2, 3, 4, 5, 6, 0, 0, 7, 8, 9, 0, 10, 11 };

/* The default_irqs array contains values used to set the irq into the
 * board via software (as must be done on newer model boards without
 * irq jumpers on the board).  The first value in the array will be
 * assigned to logical board 0, the next to board 1, etc.
 */

static int default_irqs[] __initdata = {
	PAS16_DEFAULT_BOARD_1_IRQ,
	PAS16_DEFAULT_BOARD_2_IRQ,
	PAS16_DEFAULT_BOARD_3_IRQ,
	PAS16_DEFAULT_BOARD_4_IRQ
};

static struct override {
	unsigned short io_port;
	int irq;
} overrides
#ifdef PAS16_OVERRIDE
[] __initdata = PAS16_OVERRIDE;
#else
[4] __initdata = { 
	{0, IRQ_AUTO},
	{0, IRQ_AUTO},
	{0, IRQ_AUTO},
	{0, IRQ_AUTO}
};
#endif

#define NO_OVERRIDES (sizeof(overrides) / sizeof(struct override))

static struct base {
	unsigned short io_port;
	int noauto;
} bases[] __initdata = {
	{PAS16_DEFAULT_BASE_1, 0},
	{PAS16_DEFAULT_BASE_2, 0},
	{PAS16_DEFAULT_BASE_3, 0},
	{PAS16_DEFAULT_BASE_4, 0}
};

#define NO_BASES (sizeof (bases) / sizeof (struct base))

unsigned short pas16_offset[8] = {
	0x1c00,			/* OUTPUT_DATA_REG */
	0x1c01,			/* INITIATOR_COMMAND_REG */
	0x1c02,			/* MODE_REG */
	0x1c03,			/* TARGET_COMMAND_REG */
	0x3c00,			/* STATUS_REG ro, SELECT_ENABLE_REG wo */
	0x3c01,			/* BUS_AND_STATUS_REG ro, START_DMA_SEND_REG wo */
	0x3c02,			/* INPUT_DATA_REGISTER ro, (N/A on PAS16 ?)
				 * START_DMA_TARGET_RECEIVE_REG wo
				 */
	0x3c03,			/* RESET_PARITY_INTERRUPT_REG ro,
				 * START_DMA_INITIATOR_RECEIVE_REG wo
				 */
};
/*----------------------------------------------------------------*/

/**
 * 	enable_board	-	enable a pas16 board
 *	@board_num: logical board id
 *	@port: port to assign it
 *
 *	Sets the address on new model PAS16 hardware
 */

static void __init enable_board(int board_num, unsigned short port)
{
	outb(0xbc + board_num, MASTER_ADDRESS_PTR);
	outb(port >> 2, MASTER_ADDRESS_PTR);
}

/**
 *	init_board	-	set up board
 *	@port: board address
 *	@irq: interrupt line
 *	@force_irq: if true allow scsi/audio on the same int
 *
 *	Set the board up to handle the SCSI interface
 */

static void __init init_board(unsigned short io_port, int irq, int force_irq)
{
	unsigned int tmp;
	unsigned int pas_irq_code;

	/* Initialize the SCSI part of the board */

	outb(0x30, io_port + P_TIMEOUT_COUNTER_REG);		/* Timeout counter */
	outb(0x01, io_port + P_TIMEOUT_STATUS_REG_OFFSET);	/* Reset TC */
	outb(0x01, io_port + WAIT_STATE);			/* 1 Wait state */

	NCR5380_read(RESET_PARITY_INTERRUPT_REG);

	/*
	 *	Set the SCSI interrupt pointer without mucking up the sound
	 *	interrupt pointer in the same byte.
	 */
	 
	pas_irq_code = (irq < 16) ? scsi_irq_translate[irq] : 0;
	tmp = inb(io_port + IO_CONFIG_3);

	if (((tmp & 0x0f) == pas_irq_code) && pas_irq_code > 0 && !force_irq) {
		printk(KERN_WARNING "pas16: WARNING: Can't use same irq as sound " "driver -- interrupts disabled\n");
		/* Set up the drive parameters, disable 5380 interrupts */
		outb(0x4d, io_port + SYS_CONFIG_4);
	} else {
		tmp = (tmp & 0x0f) | (pas_irq_code << 4);
		outb(tmp, io_port + IO_CONFIG_3);

		/* Set up the drive parameters and enable 5380 interrupts */
		outb(0x6d, io_port + SYS_CONFIG_4);
	}
}


/**
 *	pas16_hw_detect		-	probe for hardware
 *	@board_num: logical board ID to probe for
 *
 *	Determine if a pas16 board is present
 */

static int __init pas16_hw_detect(unsigned short board_num)
{
	unsigned char board_rev, tmp;
	unsigned short io_port = bases[board_num].io_port;

	/* See if we can find a PAS16 board at the address associated
	 * with this logical board number.
	 */

	/* First, attempt to take a newer model board out of reset and
	 * give it a base address.  This shouldn't affect older boards.
	 */
	enable_board(board_num, io_port);

	/* Now see if it looks like a PAS16 board */
	board_rev = inb(io_port + PCB_CONFIG);

	if (board_rev == 0xff)
		return 0;

	tmp = board_rev ^ 0xe0;

	outb(tmp, io_port + PCB_CONFIG);
	tmp = inb(io_port + PCB_CONFIG);
	outb(board_rev, io_port + PCB_CONFIG);

	if (board_rev != tmp)	/* Not a PAS-16 */
		return 0;

	if ((inb(io_port + OPERATION_MODE_1) & 0x03) != 0x03)
		return 0;	/* return if no SCSI interface found */

	/* Mediavision has some new model boards that return ID bits
	 * that indicate a SCSI interface, but they're not (LMS).  We'll
	 * put in an additional test to try to weed them out.
	 */

	outb(0x01, io_port + WAIT_STATE);		/* 1 Wait state       */
	NCR5380_write(MODE_REG, 0x20);			/* Is it really SCSI? */
	if (NCR5380_read(MODE_REG) != 0x20)		/* Write to a reg.    */
		return 0;				/* and try to read    */
	NCR5380_write(MODE_REG, 0x00);			/* it back.           */
	if (NCR5380_read(MODE_REG) != 0x00)
		return 0;

	return 1;
}

/**
 *	pas16_setup	-	parse command line
 *	@str: command line block
 *
 *	LILO command line initialization of the overrides array from
 *	the passed in pas16= options
 */

int __init pas16_setup(char *str)
{
	static int commandline_current = 0;
	int i;
	int ints[10];

	get_options(str, sizeof(ints) / sizeof(int), ints);
	if (ints[0] != 2)
		printk(KERN_ERR "pas16_setup : usage pas16=io_port,irq\n");
	else if (commandline_current < NO_OVERRIDES) {
		overrides[commandline_current].io_port = (unsigned short) ints[1];
		overrides[commandline_current].irq = ints[2];
		for (i = 0; i < NO_BASES; ++i)
			if (bases[i].io_port == (unsigned short) ints[1]) {
				bases[i].noauto = 1;
				break;
			}
		++commandline_current;
	}
	return 1;
}

__setup("pas16=", pas16_setup);

/**
 *	pas16_detect	-	detect and configure a pas16
 *	@tpnt: template
 *
 *	Detects and initializes PAS16 controllers that were autoprobed, 
 *	overridden on the LILO command line, or specified at compile time.
 */

int __init pas16_detect(Scsi_Host_Template * tpnt)
{
	static int current_override = 0;
	static unsigned short current_base = 0;
	struct Scsi_Host *instance;
	unsigned short io_port;
	int count;

	tpnt->proc_name = "pas16";
	tpnt->proc_info = &pas16_proc_info;

	if (pas16_addr != 0) {
		overrides[0].io_port = pas16_addr;
		/*
		 *  This is how we avoid seeing more than
		 *  one host adapter at the same I/O port.
		 *  Cribbed shamelessly from pas16_setup().
		 */
		for (count = 0; count < NO_BASES; ++count)
			if (bases[count].io_port == pas16_addr) {
				bases[count].noauto = 1;
				break;
			}
	}
	if (pas16_irq != 0)
		overrides[0].irq = pas16_irq;

	for (count = 0; current_override < NO_OVERRIDES; ++current_override) {
		io_port = 0;

		if (overrides[current_override].io_port) {
			io_port = overrides[current_override].io_port;
			enable_board(current_override, io_port);
			init_board(io_port, overrides[current_override].irq, 1);
		}
		else
		{
			for (; !io_port && (current_base < NO_BASES); ++current_base) {
				if (!bases[current_base].noauto && pas16_hw_detect(current_base)) {
					io_port = bases[current_base].io_port;
					init_board(io_port, default_irqs[current_base], 0);
				}
			}
		}

		if (!io_port)
			break;

		instance = scsi_register(tpnt, sizeof(struct NCR5380_hostdata));
		if (instance == NULL)
			break;

		instance->io_port = io_port;

		NCR5380_init(instance, 0);

		if (overrides[current_override].irq != IRQ_AUTO)
			instance->irq = overrides[current_override].irq;
		else
			instance->irq = NCR5380_probe_irq(instance, PAS16_IRQS);

		if (instance->irq != SCSI_IRQ_NONE)
			if (request_irq(instance->irq, do_pas16_intr, SA_INTERRUPT, "pas16", NULL)) {
				printk(KERN_WARNING "scsi%d : IRQ%d not free, interrupts disabled\n", instance->host_no, instance->irq);
				instance->irq = SCSI_IRQ_NONE;
			}

		if (instance->irq == SCSI_IRQ_NONE) {
			printk(KERN_INFO "scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
			printk(KERN_INFO "scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
			/* Disable 5380 interrupts, leave drive params the same */
			outb(0x4d, io_port + SYS_CONFIG_4);
			outb((inb(io_port + IO_CONFIG_3) & 0x0f), io_port + IO_CONFIG_3);
		}

		printk(KERN_INFO "scsi%d : at 0x%04x", instance->host_no, (int)
		       instance->io_port);
		if (instance->irq == SCSI_IRQ_NONE)
			printk(" interrupts disabled");
		else
			printk(" irq %d", instance->irq);
		printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d", CAN_QUEUE, CMD_PER_LUN, PAS16_PUBLIC_RELEASE);
		NCR5380_print_options(instance);
		printk("\n");

		++current_override;
		++count;
	}
	return count;
}

/**
 *	pas16_biosparam		-	generate C/H/S data
 *	@disk: Disk to set up
 *	@dev: device ident of disk
 *	@ip: array to return C/H/S mapping
 *
 *	Generates a BIOS / DOS compatible H-C-S mapping for 
 *	the specified device / size.
 */

int pas16_biosparam(Disk * disk, kdev_t dev, int *ip)
{
	int size = disk->capacity;
	ip[0] = 64;
	ip[1] = 32;
	ip[2] = size >> 11;	/* I think I have it as /(32*64) */
	if (ip[2] > 1024) {	/* yes, >, not >= */
		ip[0] = 255;
		ip[1] = 63;
		ip[2] = size / (63 * 255);
		if (ip[2] > 1023)	/* yes >1023... */
			ip[2] = 1023;
	}

	return 0;
}

/**
 *	NCR5380_pread 		-	pseudo DMA read
 *	@instance: board to read from
 *	@dst: destination for data
 *	@len: expected/max block length
 *
 *	Fast 5380 pseudo-dma read function, transfers len bytes to 
 *	dst. Unlike most boards the PAS has IRQs enabled here, which
 *	helps no end.
 */

static inline int NCR5380_pread(struct Scsi_Host *instance, unsigned char *dst, int len)
{
	unsigned char *d = dst;
	unsigned short reg = (unsigned short) (instance->io_port + P_DATA_REG_OFFSET);
	int i = len;
	int ii = 0;

	while (!(inb(instance->io_port + P_STATUS_REG_OFFSET) & P_ST_RDY))
		++ii;

	insb(reg, d, i);

	if (inb(instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET) & P_TS_TIM) {
		outb(P_TS_CT, instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET);
		printk(KERN_ERR "scsi%d : watchdog timer fired in NCR5380_pread()\n", instance->host_no);
		return -1;
	}
	if (ii > pas_maxi)
		pas_maxi = ii;
	return 0;
}

/**
 *	NCR5380_pwrite 		-	pseudo DMA write
 *	@instance: board to write to
 *	@src: source for data
 *	@len: expected/max block length
 *
 *	Fast 5380 pseudo-dma write function, transfers len bytes from
 *	src. Unlike most boards the PAS has IRQs enabled here, which
 *	helps no end.
 */

static inline int NCR5380_pwrite(struct Scsi_Host *instance, unsigned char *src, int len)
{
	unsigned char *s = src;
	unsigned short reg = (instance->io_port + P_DATA_REG_OFFSET);
	int i = len;
	int ii = 0;

	while (!((inb(instance->io_port + P_STATUS_REG_OFFSET)) & P_ST_RDY))
		++ii;

	outsb(reg, s, i);

	if (inb(instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET) & P_TS_TIM) {
		outb(P_TS_CT, instance->io_port + P_TIMEOUT_STATUS_REG_OFFSET);
		printk(KERN_ERR "scsi%d : watchdog timer fired in NCR5380_pwrite()\n", instance->host_no);
		return -1;
	}
	if (ii > pas_maxi)
		pas_wmaxi = ii;
	return 0;
}

#include "NCR5380.c"

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = MV_PAS16;

#include "scsi_module.c"

#ifdef MODULE
MODULE_PARM(pas16_addr, "h");
MODULE_PARM(pas16_irq, "i");
#endif
MODULE_LICENSE("GPL");
