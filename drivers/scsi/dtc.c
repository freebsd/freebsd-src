#define AUTOSENSE
#define PSEUDO_DMA
#define DONT_USE_INTR
#define UNSAFE			/* Leave interrupts enabled during pseudo-dma I/O */
#define xNDEBUG (NDEBUG_INTR+NDEBUG_RESELECTION+\
		 NDEBUG_SELECTION+NDEBUG_ARBITRATION)
#define DMA_WORKS_RIGHT

/*
 * DTC 3180/3280 driver, by
 *	Ray Van Tassle	rayvt@comm.mot.com
 *
 *	taken from ...
 *	Trantor T128/T128F/T228 driver by...
 *
 * 	Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 440-4894
 *
 * DISTRIBUTION RELEASE 1.
 *
 * For more information, please consult 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
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
 * UNSAFE - leave interrupts enabled during pseudo-DMA transfers. 
 *		You probably want this.
 *
 * The card is detected and initialized in one of several ways : 
 * 1.  Autoprobe (default) - since the board is memory mapped, 
 *     a BIOS signature is scanned for to locate the registers.
 *     An interrupt is triggered to autoprobe for the interrupt
 *     line.
 *
 * 2.  With command line overrides - dtc=address,irq may be 
 *     used on the LILO command line to override the defaults.
 * 
*/

/*----------------------------------------------------------------*/
/* the following will set the monitor border color (useful to find
 where something crashed or gets stuck at */
/* 1 = blue
 2 = green
 3 = cyan
 4 = red
 5 = magenta
 6 = yellow
 7 = white
*/

#if 0
#define rtrc(i) {inb(0x3da); outb(0x31, 0x3c0); outb((i), 0x3c0);}
#else
#define rtrc(i) {}
#endif


#include <asm/system.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/blk.h>
#include <asm/io.h>
#include "scsi.h"
#include "hosts.h"
#include "dtc.h"
#define AUTOPROBE_IRQ
#include "NCR5380.h"
#include "constants.h"
#include "sd.h"
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/init.h>


#define DTC_PUBLIC_RELEASE 2

/*#define DTCDEBUG 0x1*/
#define DTCDEBUG_INIT	0x1
#define DTCDEBUG_TRANSFER 0x2

/*
 * The DTC3180 & 3280 boards are memory mapped.
 * 
 */

/*
 */
/* Offset from DTC_5380_OFFSET */
#define DTC_CONTROL_REG		0x100	/* rw */
#define D_CR_ACCESS		0x80	/* ro set=can access 3280 registers */
#define CSR_DIR_READ		0x40	/* rw direction, 1 = read 0 = write */

#define CSR_RESET              0x80	/* wo  Resets 53c400 */
#define CSR_5380_REG           0x80	/* ro  5380 registers can be accessed */
#define CSR_TRANS_DIR          0x40	/* rw  Data transfer direction */
#define CSR_SCSI_BUFF_INTR     0x20	/* rw  Enable int on transfer ready */
#define CSR_5380_INTR          0x10	/* rw  Enable 5380 interrupts */
#define CSR_SHARED_INTR        0x08	/* rw  Interrupt sharing */
#define CSR_HOST_BUF_NOT_RDY   0x04	/* ro  Host buffer not ready */
#define CSR_SCSI_BUF_RDY       0x02	/* ro  SCSI buffer ready */
#define CSR_GATED_5380_IRQ     0x01	/* ro  Last block xferred */
#define CSR_INT_BASE (CSR_SCSI_BUFF_INTR | CSR_5380_INTR)


#define DTC_BLK_CNT		0x101	/* rw 
					 * # of 128-byte blocks to transfer */


#define D_CR_ACCESS             0x80	/* ro set=can access 3280 registers */

#define DTC_SWITCH_REG		0x3982	/* ro - DIP switches */
#define DTC_RESUME_XFER		0x3982	/* wo - resume data xfer 
					   * after disconnect/reconnect */

#define DTC_5380_OFFSET		0x3880	/* 8 registers here, see NCR5380.h */

/*!!!! for dtc, it's a 128 byte buffer at 3900 !!! */
#define DTC_DATA_BUF		0x3900	/* rw 128 bytes long */

static struct override {
	unsigned int address;
	int irq;
} overrides
#ifdef OVERRIDE
[] __initdata = OVERRIDE;
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
	unsigned long address;
	int noauto;
} bases[] __initdata = {
	{0xcc000, 0},
	{0xc8000, 0},
	{0xdc000, 0},
	{0xd8000, 0}
};

#define NO_BASES (sizeof (bases) / sizeof (struct base))

static const struct signature {
	const char *string;
	int offset;
} signatures[] = { 
	{"DATA TECHNOLOGY CORPORATION BIOS", 0x25},
};

#define NO_SIGNATURES (sizeof (signatures) /  sizeof (struct signature))

/**
 *	dtc_setup	-	option setup for dtc3x80
 *
 *	LILO command line initialization of the overrides array,
 */

static int __init dtc_setup(char *str)
{
	static int commandline_current = 0;
	int i;
	int ints[10];

	get_options(str, sizeof(ints) / sizeof(int), ints);
	
	if (ints[0] != 2)
		printk(KERN_ERR "dtc_setup: usage dtc=address,irq\n");
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

__setup("dtc=", dtc_setup);

/**
 *	dtc_detect	-	detect DTC 3x80 controllers
 *	@tpnt: controller template
 *
 *	Detects and initializes DTC 3180/3280 controllers
 *	that were autoprobed, overridden on the LILO command line, 
 *	or specified at compile time.
 */

int __init dtc_detect(Scsi_Host_Template * tpnt)
{
	static int current_override = 0, current_base = 0;
	struct Scsi_Host *instance;
	unsigned int base;
	int sig, count;

	tpnt->proc_name = "dtc3x80";
	tpnt->proc_info = &dtc_proc_info;

	for (count = 0; current_override < NO_OVERRIDES; ++current_override) 
	{
		base = 0;

		if (overrides[current_override].address)
			base = overrides[current_override].address;
		else
		{
			for (; !base && (current_base < NO_BASES); ++current_base) {
				for (sig = 0; sig < NO_SIGNATURES; ++sig)
				{
					if (!bases[current_base].noauto && isa_check_signature(bases[current_base].address + signatures[sig].offset, signatures[sig].string, strlen(signatures[sig].string))) {
						base = bases[current_base].address;
						break;
					}
				}
			}
		}

		if (!base)
			break;

		instance = scsi_register(tpnt, sizeof(struct NCR5380_hostdata));
		if (instance == NULL)
			break;

		instance->base = base;

		NCR5380_init(instance, 0);

		NCR5380_write(DTC_CONTROL_REG, CSR_5380_INTR);	/* Enable int's */
		if (overrides[current_override].irq != IRQ_AUTO)
			instance->irq = overrides[current_override].irq;
		else
			instance->irq = NCR5380_probe_irq(instance, DTC_IRQS);

#ifndef DONT_USE_INTR
		/* With interrupts enabled, it will sometimes hang when doing heavy
		 * reads. So better not enable them until I figure it out. */
		if (instance->irq != SCSI_IRQ_NONE)
			if (request_irq(instance->irq, do_dtc_intr, SA_INTERRUPT, "dtc")) 
			{
				printk(KERN_WARNING "scsi%d : IRQ%d not free, interrupts disabled\n", instance->host_no, instance->irq);
				instance->irq = SCSI_IRQ_NONE;
			}

		if (instance->irq == SCSI_IRQ_NONE) {
			printk(KERN_INFO "scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
			printk(KERN_INFO "scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
		}
#else
		if (instance->irq != SCSI_IRQ_NONE)
			printk(KERN_INFO "scsi%d : interrupts not used. Might as well not jumper it.\n", instance->host_no);
		instance->irq = SCSI_IRQ_NONE;
#endif
		printk(KERN_INFO "scsi%d : at 0x%05X", instance->host_no, (int) instance->base);
		if (instance->irq == SCSI_IRQ_NONE)
			printk(" interrupts disabled");
		else
			printk(" irq %d", instance->irq);
		printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d", CAN_QUEUE, CMD_PER_LUN, DTC_PUBLIC_RELEASE);
		NCR5380_print_options(instance);
		printk("\n");

		++current_override;
		++count;
	}
	return count;
}

/**
 *	dtc_biosparam		-	compute disk geometry
 *	@disk: disk to generate for
 *	@dev: major/minor of device
 *	@ip: returned geometry
 *
 * 	Generates a BIOS / DOS compatible H-C-S mapping for 
 *	the specified device / size.
 */

int dtc_biosparam(Disk * disk, kdev_t dev, int *ip)
{
	int size = disk->capacity;

	ip[0] = 64;
	ip[1] = 32;
	ip[2] = size >> 11;
	return 0;
}


static int dtc_maxi = 0;
static int dtc_wmaxi = 0;

/**
 *	NCR5380_pread 		-	fast pseudo DMA read
 *	@instance: controller
 *	@dst: destination buffer
 *	@len: expected/max size
 *
 *	Fast 5380 pseudo-dma read function, reads len bytes from the controller
 *	mmio area into dst.
 */

static inline int NCR5380_pread(struct Scsi_Host *instance, unsigned char *dst, int len)
{
	unsigned char *d = dst;
	int i;			/* For counting time spent in the poll-loop */
	NCR5380_local_declare();
	NCR5380_setup(instance);

	i = 0;
	NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	NCR5380_write(MODE_REG, MR_ENABLE_EOP_INTR | MR_DMA_MODE);
	if (instance->irq == SCSI_IRQ_NONE)
		NCR5380_write(DTC_CONTROL_REG, CSR_DIR_READ);
	else
		NCR5380_write(DTC_CONTROL_REG, CSR_DIR_READ | CSR_INT_BASE);
	NCR5380_write(DTC_BLK_CNT, len >> 7);	/* Block count */
	rtrc(1);
	while (len > 0) {
		rtrc(2);
		while (NCR5380_read(DTC_CONTROL_REG) & CSR_HOST_BUF_NOT_RDY)
			++i;
		rtrc(3);
		isa_memcpy_fromio(d, base + DTC_DATA_BUF, 128);
		d += 128;
		len -= 128;
		rtrc(7);
		/*** with int's on, it sometimes hangs after here.
		 * Looks like something makes HBNR go away. */
	}
	rtrc(4);
	while (!(NCR5380_read(DTC_CONTROL_REG) & D_CR_ACCESS))
		++i;
	NCR5380_write(MODE_REG, 0);	/* Clear the operating mode */
	rtrc(0);
	NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	if (i > dtc_maxi)
		dtc_maxi = i;
	return (0);
}

/**
 *	NCR5380_pwrite 		-	fast pseudo DMA write
 *	@instance: controller
 *	@dst: destination buffer
 *	@len: expected/max size
 *
 *	Fast 5380 pseudo-dma write function, writes len bytes to the
 *	controller mmio area from src.
 */

static inline int NCR5380_pwrite(struct Scsi_Host *instance, unsigned char *src, int len)
{
	int i;
	NCR5380_local_declare();
	NCR5380_setup(instance);

	NCR5380_read(RESET_PARITY_INTERRUPT_REG);
	NCR5380_write(MODE_REG, MR_ENABLE_EOP_INTR | MR_DMA_MODE);
	/* set direction (write) */
	if (instance->irq == SCSI_IRQ_NONE)
		NCR5380_write(DTC_CONTROL_REG, 0);
	else
		NCR5380_write(DTC_CONTROL_REG, CSR_5380_INTR);
	NCR5380_write(DTC_BLK_CNT, len >> 7);	/* Block count */
	for (i = 0; len > 0; ++i) {
		rtrc(5);
		/* Poll until the host buffer can accept data. */
		while (NCR5380_read(DTC_CONTROL_REG) & CSR_HOST_BUF_NOT_RDY)
			++i;
		rtrc(3);
		isa_memcpy_toio(base + DTC_DATA_BUF, src, 128);
		src += 128;
		len -= 128;
	}
	rtrc(4);
	while (!(NCR5380_read(DTC_CONTROL_REG) & D_CR_ACCESS))
		++i;
	rtrc(6);
	/* Wait until the last byte has been sent to the disk */
	while (!(NCR5380_read(TARGET_COMMAND_REG) & TCR_LAST_BYTE_SENT))
		++i;
	rtrc(7);
	/* Check for parity error here. fixme. */
	NCR5380_write(MODE_REG, 0);	/* Clear the operating mode */
	rtrc(0);
	if (i > dtc_wmaxi)
		dtc_wmaxi = i;
	return (0);
}

MODULE_LICENSE("GPL");

#include "NCR5380.c"

/* Eventually this will go into an include file, but this will be later */
static Scsi_Host_Template driver_template = DTC3x80;
#include "scsi_module.c"
