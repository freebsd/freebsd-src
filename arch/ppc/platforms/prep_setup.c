/*
 *  arch/ppc/platforms/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Adapted from 'alpha' version by Gary Thomas
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
 *
 * Support for PReP (Motorola MTX/MVME)
 * by Troy Benjegerdes (hozer@drgw.net)
 */

/*
 * bootup setup stuff..
 */

#include <linux/config.h>
#include <linux/delay.h>
#include <linux/module.h>
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
#include <linux/timex.h>
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/seq_file.h>

#include <asm/sections.h>
#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/residual.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/cache.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/mk48t59.h>
#include <asm/prep_nvram.h>
#include <asm/raven.h>
#include <asm/keyboard.h>
#include <asm/vga.h>
#include <asm/time.h>
#include <asm/i8259.h>
#include <asm/open_pic.h>
#include <asm/pci-bridge.h>
#include <asm/bootinfo.h>

unsigned char ucSystemType;
unsigned char ucBoardRev;
unsigned char ucBoardRevMaj, ucBoardRevMin;

extern unsigned long mc146818_get_rtc_time(void);
extern int mc146818_set_rtc_time(unsigned long nowtime);
extern unsigned long mk48t59_get_rtc_time(void);
extern int mk48t59_set_rtc_time(unsigned long nowtime);

extern unsigned char prep_nvram_read_val(int addr);
extern void prep_nvram_write_val(int addr,
				 unsigned char val);
extern unsigned char rs_nvram_read_val(int addr);
extern void rs_nvram_write_val(int addr,
				 unsigned char val);
extern void ibm_prep_init(void);

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
		char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[];

extern void prep_find_bridges(void);
extern char saved_command_line[];

int _prep_type;

extern void prep_sandalfoot_setup_pci(char *irq_edge_mask_lo, char *irq_edge_mask_hi);
extern void prep_thinkpad_setup_pci(char *irq_edge_mask_lo, char *irq_edge_mask_hi);
extern void prep_carolina_setup_pci(char *irq_edge_mask_lo, char *irq_edge_mask_hi);
extern void prep_tiger1_setup_pci(char *irq_edge_mask_lo, char *irq_edge_mask_hi);


#define cached_21	(((char *)(ppc_cached_irq_mask))[3])
#define cached_A1	(((char *)(ppc_cached_irq_mask))[2])

/* for the mac fs */
kdev_t boot_dev;

#ifdef CONFIG_SOUND_CS4232
long ppc_cs4232_dma, ppc_cs4232_dma2;
#endif

extern PTE *Hash, *Hash_end;
extern unsigned long Hash_size, Hash_mask;
extern int probingmem;
extern unsigned long loops_per_jiffy;

#ifdef CONFIG_SOUND_CS4232
EXPORT_SYMBOL(ppc_cs4232_dma);
EXPORT_SYMBOL(ppc_cs4232_dma2);
#endif

/* useful ISA ports */
#define PREP_SYSCTL	0x81c
/* present in the IBM reference design; possibly identical in Mot boxes: */
#define PREP_IBM_SIMM_ID	0x803 /* SIMM size: 32 or 8 MiB */
#define PREP_IBM_SIMM_PRESENCE	0x804
#define PREP_IBM_EQUIPMENT	0x80c
#define PREP_IBM_L2INFO	0x80d
#define PREP_IBM_PM1	0x82a /* power management register 1 */
#define PREP_IBM_PLANAR	0x852 /* planar ID - identifies the motherboard */

/* Equipment Present Register masks: */
#define PREP_IBM_EQUIPMENT_RESERVED	0x80
#define PREP_IBM_EQUIPMENT_SCSIFUSE	0x40
#define PREP_IBM_EQUIPMENT_L2_COPYBACK	0x08
#define PREP_IBM_EQUIPMENT_L2_256	0x04
#define PREP_IBM_EQUIPMENT_CPU	0x02
#define PREP_IBM_EQUIPMENT_L2	0x01

/* planar ID values: */
/* Sandalfoot/Sandalbow (6015/7020) */
#define PREP_IBM_SANDALFOOT	0xfc
/* Woodfield, Thinkpad 850/860 (6042/7249) */
#define PREP_IBM_THINKPAD	0xff /* planar ID unimplemented */
/* PowerSeries 830/850 (6050/6070) */
#define PREP_IBM_CAROLINA_IDE_0	0xf0
#define PREP_IBM_CAROLINA_IDE_1	0xf1
#define PREP_IBM_CAROLINA_IDE_2	0xf2
/* 7248-43P */
#define PREP_IBM_CAROLINA_SCSI_0	0xf4
#define PREP_IBM_CAROLINA_SCSI_1	0xf5
#define PREP_IBM_CAROLINA_SCSI_2	0xf6
#define PREP_IBM_CAROLINA_SCSI_3	0xf7 /* missing from Carolina Tech Spec */
/* Tiger1 (7043-140) */
#define PREP_IBM_TIGER1_133		0xd1
#define PREP_IBM_TIGER1_166		0xd2
#define PREP_IBM_TIGER1_180		0xd3
#define PREP_IBM_TIGER1_xxx		0xd4 /* unknown, but probably exists */
#define PREP_IBM_TIGER1_333		0xd5 /* missing from Tiger Tech Spec */

/* setup_ibm_pci:
 * 	set Motherboard_map_name, Motherboard_map, Motherboard_routes.
 * 	return 8259 edge/level masks.
 */
void (*setup_ibm_pci)(char *irq_lo, char *irq_hi);

extern char *Motherboard_map_name; /* for use in *_cpuinfo */

/*
 * As found in the PReP reference implementation.
 * Used by Thinkpad, Sandalfoot (6015/7020), and all Motorola PReP.
 */
static void __init
prep_gen_enable_l2(void)
{
	outb(inb(PREP_SYSCTL) | 0x3, PREP_SYSCTL);
}

/* Used by Carolina and Tiger1 */
static void __init
prep_carolina_enable_l2(void)
{
	outb(inb(PREP_SYSCTL) | 0xc0, PREP_SYSCTL);
}

/* cpuinfo code common to all IBM PReP */
static void __prep
prep_ibm_cpuinfo(struct seq_file *m)
{
	unsigned int equip_reg = inb(PREP_IBM_EQUIPMENT);

	seq_printf(m, "machine\t\t: PReP %s\n", Motherboard_map_name);

	seq_printf(m, "upgrade cpu\t: ");
	if (equip_reg & PREP_IBM_EQUIPMENT_CPU) {
		seq_printf(m, "not ");
	}
	seq_printf(m, "present\n");

	/* print info about the SCSI fuse */
	seq_printf(m, "scsi fuse\t: ");
	if (equip_reg & PREP_IBM_EQUIPMENT_SCSIFUSE)
		seq_printf(m, "ok");
	else
		seq_printf(m, "bad");
	seq_printf(m, "\n");

#ifdef CONFIG_PREP_RESIDUAL
	/* print info about SIMMs */
	if (res->ResidualLength != 0) {
		int i;
		seq_printf(m, "simms\t\t: ");
		for (i = 0; (res->ActualNumMemories) && (i < MAX_MEMS); i++) {
			if (res->Memories[i].SIMMSize != 0)
				seq_printf(m, "%d:%ldMiB ", i,
					(res->Memories[i].SIMMSize > 1024) ?
					res->Memories[i].SIMMSize>>20 :
					res->Memories[i].SIMMSize);
		}
		seq_printf(m, "\n");
	}
#endif
}

static int __prep
prep_sandalfoot_cpuinfo(struct seq_file *m)
{
	unsigned int equip_reg = inb(PREP_IBM_EQUIPMENT);

	prep_ibm_cpuinfo(m);

	/* report amount and type of L2 cache present */
	seq_printf(m, "L2 cache\t: ");
	if (equip_reg & PREP_IBM_EQUIPMENT_L2) {
		seq_printf(m, "not present");
	} else {
		if (equip_reg & PREP_IBM_EQUIPMENT_L2_256)
			seq_printf(m, "256KiB");
		else
			seq_printf(m, "unknown size");

		if (equip_reg & PREP_IBM_EQUIPMENT_L2_COPYBACK)
			seq_printf(m, ", copy-back");
		else
			seq_printf(m, ", write-through");
	}
	seq_printf(m, "\n");

	return 0;
}

static int __prep
prep_thinkpad_cpuinfo(struct seq_file *m)
{
	unsigned int equip_reg = inb(PREP_IBM_EQUIPMENT);
	char *cpubus_speed, *pci_speed;

	prep_ibm_cpuinfo(m);

	/* report amount and type of L2 cache present */
	seq_printf(m, "l2 cache\t: ");
	if ((equip_reg & 0x1) == 0) {
		switch ((equip_reg & 0xc) >> 2) {
			case 0x0:
				seq_printf(m, "128KiB look-aside 2-way write-through\n");
				break;
			case 0x1:
				seq_printf(m, "512KiB look-aside direct-mapped write-back\n");
				break;
			case 0x2:
				seq_printf(m, "256KiB look-aside 2-way write-through\n");
				break;
			case 0x3:
				seq_printf(m, "256KiB look-aside direct-mapped write-back\n");
				break;
		}
	} else {
		seq_printf(m, "not present\n");
	}

	/* report bus speeds because we can */
	if ((equip_reg & 0x80) == 0) {
		switch ((equip_reg & 0x30) >> 4) {
			case 0x1:
				cpubus_speed = "50";
				pci_speed = "25";
				break;
			case 0x3:
				cpubus_speed = "66";
				pci_speed = "33";
				break;
			default:
				cpubus_speed = "unknown";
				pci_speed = "unknown";
				break;
		}
	} else {
		switch ((equip_reg & 0x30) >> 4) {
			case 0x1:
				cpubus_speed = "25";
				pci_speed = "25";
				break;
			case 0x2:
				cpubus_speed = "60";
				pci_speed = "30";
				break;
			case 0x3:
				cpubus_speed = "33";
				pci_speed = "33";
				break;
			default:
				cpubus_speed = "unknown";
				pci_speed = "unknown";
				break;
		}
	}
	seq_printf(m, "60x bus\t\t: %sMHz\n", cpubus_speed);
	seq_printf(m, "pci bus\t\t: %sMHz\n", pci_speed);

	return 0;
}

static int __prep
prep_carolina_cpuinfo(struct seq_file *m)
{
	unsigned int equip_reg = inb(PREP_IBM_EQUIPMENT);

	prep_ibm_cpuinfo(m);

	/* report amount and type of L2 cache present */
	seq_printf(m, "l2 cache\t: ");
	if ((equip_reg & 0x1) == 0) {
		unsigned int l2_reg = inb(PREP_IBM_L2INFO);

		/* L2 size */
		switch ((l2_reg & 0x60) >> 5) {
			case 0:
				seq_printf(m, "256KiB");
				break;
			case 1:
				seq_printf(m, "512KiB");
				break;
			default:
				seq_printf(m, "unknown size");
				break;
		}

		/* L2 type */
		if ((l2_reg & 0x3) == 0)
			seq_printf(m, ", async");
		else if ((l2_reg & 0x3) == 1)
			seq_printf(m, ", sync");
		else
			seq_printf(m, ", unknown type");

		seq_printf(m, "\n");
	} else {
		seq_printf(m, "not present\n");
	}

	return 0;
}

static int __prep
prep_tiger1_cpuinfo(struct seq_file *m)
{
	unsigned int l2_reg = inb(PREP_IBM_L2INFO);

	prep_ibm_cpuinfo(m);

	/* report amount and type of L2 cache present */
	seq_printf(m, "l2 cache\t: ");
	if ((l2_reg & 0xf) == 0xf) {
		seq_printf(m, "not present\n");
	} else {
		if (l2_reg & 0x8)
			seq_printf(m, "async, ");
		else
			seq_printf(m, "sync burst, ");

		if (l2_reg & 0x4)
			seq_printf(m, "parity, ");
		else
			seq_printf(m, "no parity, ");

		switch (l2_reg & 0x3) {
			case 0x0:
				seq_printf(m, "256KiB\n");
				break;
			case 0x1:
				seq_printf(m, "512KiB\n");
				break;
			case 0x2:
				seq_printf(m, "1MiB\n");
				break;
			default:
				seq_printf(m, "unknown size\n");
				break;
		}
	}

	return 0;
}


/* Used by all Motorola PReP */
static int __prep
prep_mot_cpuinfo(struct seq_file *m)
{
	unsigned int cachew = *((unsigned char *)CACHECRBA);

	seq_printf(m, "machine\t\t: PReP %s\n", Motherboard_map_name);

	/* report amount and type of L2 cache present */
	seq_printf(m, "l2 cache\t: ");
	switch (cachew & L2CACHE_MASK) {
		case L2CACHE_512KB:
			seq_printf(m, "512KiB");
			break;
		case L2CACHE_256KB:
			seq_printf(m, "256KiB");
			break;
		case L2CACHE_1MB:
			seq_printf(m, "1MiB");
			break;
		case L2CACHE_NONE:
			seq_printf(m, "none\n");
			goto no_l2;
			break;
		default:
			seq_printf(m, "%x\n", cachew);
	}

	seq_printf(m, ", parity %s",
			(cachew & L2CACHE_PARITY)? "enabled" : "disabled");

	seq_printf(m, " SRAM:");

	switch ( ((cachew & 0xf0) >> 4) & ~(0x3) ) {
		case 1: seq_printf(m, "synchronous, parity, flow-through\n");
				break;
		case 2: seq_printf(m, "asynchronous, no parity\n");
				break;
		case 3: seq_printf(m, "asynchronous, parity\n");
				break;
		default:seq_printf(m, "synchronous, pipelined, no parity\n");
				break;
	}

no_l2:
#ifdef CONFIG_PREP_RESIDUAL
	/* print info about SIMMs */
	if (res->ResidualLength != 0) {
		int i;
		seq_printf(m, "simms\t\t: ");
		for (i = 0; (res->ActualNumMemories) && (i < MAX_MEMS); i++) {
			if (res->Memories[i].SIMMSize != 0)
				seq_printf(m, "%d:%ldM ", i,
					(res->Memories[i].SIMMSize > 1024) ?
					res->Memories[i].SIMMSize>>20 :
					res->Memories[i].SIMMSize);
		}
		seq_printf(m, "\n");
	}
#endif

	return 0;
}

static void __prep
prep_restart(char *cmd)
{
#define PREP_SP92	0x92	/* Special Port 92 */
	__cli(); /* no interrupts */

	/* set exception prefix high - to the prom */
	_nmask_and_or_msr(0, MSR_IP);

	/* make sure bit 0 (reset) is a 0 */
	outb( inb(PREP_SP92) & ~1L , PREP_SP92);
	/* signal a reset to system control port A - soft reset */
	outb( inb(PREP_SP92) | 1 , PREP_SP92);

	while ( 1 ) ;
	/* not reached */
#undef PREP_SP92
}

static void __prep
prep_halt(void)
{
	__cli(); /* no interrupts */

	/* set exception prefix high - to the prom */
	_nmask_and_or_msr(0, MSR_IP);

	while ( 1 ) ;
	/* not reached */
}

/* Carrera is the power manager in the Thinkpads. Unfortunately not much is
 * known about it, so we can't power down.
 */
static void __prep
prep_carrera_poweroff(void)
{
	prep_halt();
}

/*
 * On most IBM PReP's, power management is handled by a Signetics 87c750
 * behind the Utah component on the ISA bus. To access the 750 you must write
 * a series of nibbles to port 0x82a (decoded by the Utah). This is described
 * somewhat in the IBM Carolina Technical Specification.
 * -Hollis
 */
static void __prep
utah_sig87c750_setbit(unsigned int bytenum, unsigned int bitnum, int value)
{
	/*
	 * byte1: 0 0 0 1 0  d  a5 a4
	 * byte2: 0 0 0 1 a3 a2 a1 a0
	 *
	 * d = the bit's value, enabled or disabled
	 * (a5 a4 a3) = the byte number, minus 20
	 * (a2 a1 a0) = the bit number
	 *
	 * example: set the 5th bit of byte 21 (21.5)
	 *     a5 a4 a3 = 001 (byte 1)
	 *     a2 a1 a0 = 101 (bit 5)
	 *
	 *     byte1 = 0001 0100 (0x14)
	 *     byte2 = 0001 1101 (0x1d)
	 */
	unsigned char byte1=0x10, byte2=0x10;

	/* the 750's '20.0' is accessed as '0.0' through Utah (which adds 20) */
	bytenum -= 20;

	byte1 |= (!!value) << 2;		/* set d */
	byte1 |= (bytenum >> 1) & 0x3;	/* set a5, a4 */

	byte2 |= (bytenum & 0x1) << 3;	/* set a3 */
	byte2 |= bitnum & 0x7;			/* set a2, a1, a0 */

	outb(byte1, PREP_IBM_PM1);	/* first nibble */
	mb();
	udelay(100);				/* important: let controller recover */

	outb(byte2, PREP_IBM_PM1);	/* second nibble */
	mb();
	udelay(100);				/* important: let controller recover */
}

static void __prep
prep_sig750_poweroff(void)
{
	/* tweak the power manager found in most IBM PRePs (except Thinkpads) */
	unsigned long flags;
	__cli();
	/* set exception prefix high - to the prom */
	save_flags( flags );
	restore_flags( flags|MSR_IP );

	utah_sig87c750_setbit(21, 5, 1); /* set bit 21.5, "PMEXEC_OFF" */

	while (1) ;
	/* not reached */
}

static int __prep
prep_show_percpuinfo(struct seq_file *m, int i)
{
	/* PREP's without residual data will give incorrect values here */
	seq_printf(m, "clock\t\t: ");
#ifdef CONFIG_PREP_RESIDUAL
	if (res->ResidualLength)
		seq_printf(m, "%ldMHz\n",
			   (res->VitalProductData.ProcessorHz > 1024) ?
			   res->VitalProductData.ProcessorHz / 1000000 :
			   res->VitalProductData.ProcessorHz);
	else
#endif /* CONFIG_PREP_RESIDUAL */
		seq_printf(m, "???\n");

	return 0;
}

#ifdef CONFIG_SOUND_CS4232
static long __init masktoint(unsigned int i)
{
	int t = -1;
	while (i >> ++t)
		;
	return (t-1);
}

/*
 * ppc_cs4232_dma and ppc_cs4232_dma2 are used in include/asm/dma.h
 * to distinguish sound dma-channels from others. This is because
 * blocksize on 16 bit dma-channels 5,6,7 is 128k, but
 * the cs4232.c uses 64k like on 8 bit dma-channels 0,1,2,3
 */

static void __init prep_init_sound(void)
{
	PPC_DEVICE *audiodevice = NULL;

	/*
	 * Get the needed resource informations from residual data.
	 *
	 */
#ifdef CONFIG_PREP_RESIDUAL
	audiodevice = residual_find_device(~0, NULL, MultimediaController,
			AudioController, -1, 0);
	if (audiodevice != NULL) {
		PnP_TAG_PACKET *pkt;

		pkt = PnP_find_packet((unsigned char *)&res->DevicePnPHeap[audiodevice->AllocatedOffset],
				S5_Packet, 0);
		if (pkt != NULL)
			ppc_cs4232_dma = masktoint(pkt->S5_Pack.DMAMask);
		pkt = PnP_find_packet((unsigned char*)&res->DevicePnPHeap[audiodevice->AllocatedOffset],
				S5_Packet, 1);
		if (pkt != NULL)
			ppc_cs4232_dma2 = masktoint(pkt->S5_Pack.DMAMask);
	}
#endif

	/*
	 * These are the PReP specs' defaults for the cs4231.  We use these
	 * as fallback incase we don't have residual data.
	 * At least the IBM Thinkpad 850 with IDE DMA Channels at 6 and 7
	 * will use the other values.
	 */
	if (audiodevice == NULL) {
		switch (_prep_type) {
		case _PREP_IBM:
			ppc_cs4232_dma = 1;
			ppc_cs4232_dma2 = -1;
			break;
		default:
			ppc_cs4232_dma = 6;
			ppc_cs4232_dma2 = 7;
		}
	}

	/*
	 * Find a way to push these informations to the cs4232 driver
	 * Give it out with printk, when not in cmd_line?
	 * Append it to  cmd_line and saved_command_line?
	 * Format is cs4232=io,irq,dma,dma2
	 */
}
#endif /* CONFIG_SOUND_CS4232 */

/*
 * Fill out screen_info according to the residual data. This allows us to use
 * at least vesafb.
 */
static void __init
prep_init_vesa(void)
{
#if defined(CONFIG_PREP_RESIDUAL) && \
	(defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_FB))
	PPC_DEVICE *vgadev;

	vgadev = residual_find_device(~0, NULL, DisplayController, SVGAController,
									-1, 0);
	if (vgadev != NULL) {
		PnP_TAG_PACKET *pkt;

		pkt = PnP_find_large_vendor_packet(
				(unsigned char *)&res->DevicePnPHeap[vgadev->AllocatedOffset],
				0x04, 0); /* 0x04 = Display Tag */
		if (pkt != NULL) {
			unsigned char *ptr = (unsigned char *)pkt;

			if (ptr[4]) {
				/* graphics mode */
				screen_info.orig_video_isVGA = VIDEO_TYPE_VLFB;

				screen_info.lfb_depth = ptr[4] * 8;

				screen_info.lfb_width = swab16(*(short *)(ptr+6));
				screen_info.lfb_height = swab16(*(short *)(ptr+8));
				screen_info.lfb_linelength = swab16(*(short *)(ptr+10));

				screen_info.lfb_base = swab32(*(long *)(ptr+12));
				screen_info.lfb_size = swab32(*(long *)(ptr+20)) / 65536;
			}
		}
	}
#endif /* CONFIG_PREP_RESIDUAL */
}

static void __init
prep_setup_arch(void)
{
	unsigned char reg;
	int is_ide=0;

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000;

	/* Lookup PCI host bridges */
	prep_find_bridges();

	/* Set up floppy in PS/2 mode */
	outb(0x09, SIO_CONFIG_RA);
	reg = inb(SIO_CONFIG_RD);
	reg = (reg & 0x3F) | 0x40;
	outb(reg, SIO_CONFIG_RD);
	outb(reg, SIO_CONFIG_RD);	/* Have to write twice to change! */

	switch ( _prep_type )
	{
	case _PREP_IBM:
		reg = inb(PREP_IBM_PLANAR);
		printk(KERN_INFO "IBM planar ID: %08x", reg);
		switch (reg) {
			case PREP_IBM_SANDALFOOT:
				prep_gen_enable_l2();
				setup_ibm_pci = prep_sandalfoot_setup_pci;
				ppc_md.power_off = prep_sig750_poweroff;
				ppc_md.show_cpuinfo = prep_sandalfoot_cpuinfo;
				break;
			case PREP_IBM_THINKPAD:
				prep_gen_enable_l2();
				setup_ibm_pci = prep_thinkpad_setup_pci;
				ppc_md.power_off = prep_carrera_poweroff;
				ppc_md.show_cpuinfo = prep_thinkpad_cpuinfo;
				break;
			default:
				printk(" -- unknown! Assuming Carolina");
			case PREP_IBM_CAROLINA_IDE_0:
			case PREP_IBM_CAROLINA_IDE_1:
			case PREP_IBM_CAROLINA_IDE_2:
				is_ide = 1;
			case PREP_IBM_CAROLINA_SCSI_0:
			case PREP_IBM_CAROLINA_SCSI_1:
			case PREP_IBM_CAROLINA_SCSI_2:
			case PREP_IBM_CAROLINA_SCSI_3:
				prep_carolina_enable_l2();
				setup_ibm_pci = prep_carolina_setup_pci;
				ppc_md.power_off = prep_sig750_poweroff;
				ppc_md.show_cpuinfo = prep_carolina_cpuinfo;
				break;
			case PREP_IBM_TIGER1_133:
			case PREP_IBM_TIGER1_166:
			case PREP_IBM_TIGER1_180:
			case PREP_IBM_TIGER1_xxx:
			case PREP_IBM_TIGER1_333:
				prep_carolina_enable_l2();
				setup_ibm_pci = prep_tiger1_setup_pci;
				ppc_md.power_off = prep_sig750_poweroff;
				ppc_md.show_cpuinfo = prep_tiger1_cpuinfo;
				break;
		}
		printk("\n");

		/* default root device */
		if (is_ide)
			ROOT_DEV = to_kdev_t(0x0303); /* hda3 */
		else
			ROOT_DEV = to_kdev_t(0x0803); /* sda3 */

		break;
	case _PREP_Motorola:
		prep_gen_enable_l2();
		ppc_md.power_off = prep_halt;
		ppc_md.show_cpuinfo = prep_mot_cpuinfo;

#ifdef CONFIG_BLK_DEV_INITRD
		if (initrd_start)
			ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0); /* /dev/ram */
		else
#endif
#ifdef CONFIG_ROOT_NFS
			ROOT_DEV = to_kdev_t(0x00ff); /* /dev/nfs */
#else
			ROOT_DEV = to_kdev_t(0x0802); /* /dev/sda2 */
#endif
		break;
	}

	/* Read in NVRAM data */
	init_prep_nvram();

	/* if no bootargs, look in NVRAM */
	if ( cmd_line[0] == '\0' ) {
		char *bootargs;
		 bootargs = prep_nvram_get_var("bootargs");
		 if (bootargs != NULL) {
			 strcpy(cmd_line, bootargs);
			 /* again.. */
			 strcpy(saved_command_line, cmd_line);
		}
	}

#ifdef CONFIG_SOUND_CS4232
	prep_init_sound();
#endif /* CONFIG_SOUND_CS4232 */

	prep_init_vesa();

	switch (_prep_type) {
	case _PREP_Motorola:
		raven_init();
		break;
	case _PREP_IBM:
		ibm_prep_init();
		break;
	}

#ifdef CONFIG_VGA_CONSOLE
	/* vgacon.c needs to know where we mapped IO memory in io_block_mapping() */
	vgacon_remap_base = 0xf0000000;
	conswitchp = &vga_con;
#elif defined(CONFIG_DUMMY_CONSOLE)
	conswitchp = &dummy_con;
#endif
}

/*
 * Determine the decrementer frequency from the residual data
 * This allows for a faster boot as we do not need to calibrate the
 * decrementer against another clock. This is important for embedded systems.
 */
static int __init
prep_res_calibrate_decr(void)
{
#ifdef CONFIG_PREP_RESIDUAL
	unsigned long freq, divisor = 4;

	if ( res->VitalProductData.ProcessorBusHz ) {
		freq = res->VitalProductData.ProcessorBusHz;
		printk("time_init: decrementer frequency = %lu.%.6lu MHz\n",
				(freq/divisor)/1000000,
				(freq/divisor)%1000000);
		tb_to_us = mulhwu_scale_factor(freq/divisor, 1000000);
		tb_ticks_per_jiffy = freq / HZ / divisor;
		return 0;
	} else
#endif
		return 1;
}

/*
 * Uses the on-board timer to calibrate the on-chip decrementer register
 * for prep systems.  On the pmac the OF tells us what the frequency is
 * but on prep we have to figure it out.
 * -- Cort
 */
/* Done with 3 interrupts: the first one primes the cache and the
 * 2 following ones measure the interval. The precision of the method
 * is still doubtful due to the short interval sampled.
 */
static volatile int calibrate_steps __initdata = 3;
static unsigned tbstamp __initdata = 0;

static void __init
prep_calibrate_decr_handler(int irq, void *dev, struct pt_regs *regs)
{
	unsigned long t, freq;
	int step=--calibrate_steps;

	t = get_tbl();
	if (step > 0) {
		tbstamp = t;
	} else {
		freq = (t - tbstamp)*HZ;
		printk("time_init: decrementer frequency = %lu.%.6lu MHz\n",
			 freq/1000000, freq%1000000);
		tb_ticks_per_jiffy = freq / HZ;
		tb_to_us = mulhwu_scale_factor(freq, 1000000);
	}
}

static void __init
prep_calibrate_decr(void)
{
	int res;

	/* Try and get this from the residual data. */
	res = prep_res_calibrate_decr();

	/* If we didn't get it from the residual data, try this. */
	if ( res ) {
		unsigned long flags;

		save_flags(flags);

#define TIMER0_COUNT 0x40
#define TIMER_CONTROL 0x43
		/* set timer to periodic mode */
		outb_p(0x34,TIMER_CONTROL);/* binary, mode 2, LSB/MSB, ch 0 */
		/* set the clock to ~100 Hz */
		outb_p(LATCH & 0xff , TIMER0_COUNT);	/* LSB */
		outb(LATCH >> 8 , TIMER0_COUNT);	/* MSB */

		if (request_irq(0, prep_calibrate_decr_handler, 0, "timer", NULL) != 0)
			panic("Could not allocate timer IRQ!");
		__sti();
		/* wait for calibrate */
		while ( calibrate_steps )
			;
		restore_flags(flags);
		free_irq( 0, NULL);
	}
}

static long __init
mk48t59_init(void) {
	unsigned char tmp;

	tmp = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLB);
	if (tmp & MK48T59_RTC_CB_STOP) {
		printk("Warning: RTC was stopped, date will be wrong.\n");
		ppc_md.nvram_write_val(MK48T59_RTC_CONTROLB,
					 tmp & ~MK48T59_RTC_CB_STOP);
		/* Low frequency crystal oscillators may take a very long
		 * time to startup and stabilize. For now just ignore the
		 * the issue, but attempting to calibrate the decrementer
		 * from the RTC just after this wakeup is likely to be very
		 * inaccurate. Firmware should not allow to load
		 * the OS with the clock stopped anyway...
		 */
	}
	/* Ensure that the clock registers are updated */
	tmp = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLA);
	tmp &= ~(MK48T59_RTC_CA_READ | MK48T59_RTC_CA_WRITE);
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA, tmp);
	return 0;
}

/* We use the NVRAM RTC to time a second to calibrate the decrementer,
 * the RTC registers have just been set up in the right state by the
 * preceding routine.
 */
static void __init
mk48t59_calibrate_decr(void)
{
	unsigned long freq;
	unsigned long t1;
	unsigned char save_control;
	long i;
	unsigned char sec;


	/* Make sure the time is not stopped. */
	save_control = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLB);

	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA,
			(save_control & (~MK48T59_RTC_CB_STOP)));

	/* Now make sure the read bit is off so the value will change. */
	save_control = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLA);
	save_control &= ~MK48T59_RTC_CA_READ;
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA, save_control);


	/* Read the seconds value to see when it changes. */
	sec = ppc_md.nvram_read_val(MK48T59_RTC_SECONDS);
	/* Actually this is bad for precision, we should have a loop in
	 * which we only read the seconds counter. nvram_read_val writes
	 * the address bytes on every call and this takes a lot of time.
	 * Perhaps an nvram_wait_change method returning a time
	 * stamp with a loop count as parameter would be the  solution.
	 */
	for (i = 0 ; i < 1000000 ; i++)	{ /* may take up to 1 second... */
		t1 = get_tbl();
		if (ppc_md.nvram_read_val(MK48T59_RTC_SECONDS) != sec) {
			break;
		}
	}

	sec = ppc_md.nvram_read_val(MK48T59_RTC_SECONDS);
	for (i = 0 ; i < 1000000 ; i++)	{ /* Should take up 1 second... */
		freq = get_tbl()-t1;
		if (ppc_md.nvram_read_val(MK48T59_RTC_SECONDS) != sec)
			break;
	}

	printk("time_init: decrementer frequency = %lu.%.6lu MHz\n",
		 freq/1000000, freq%1000000);
	tb_ticks_per_jiffy = freq / HZ;
	tb_to_us = mulhwu_scale_factor(freq, 1000000);
}

static unsigned int __prep
prep_irq_cannonicalize(u_int irq)
{
	if (irq == 2)
	{
		return 9;
	}
	else
	{
		return irq;
	}
}

static void __init
prep_init_IRQ(void)
{
	int i;
	unsigned int pci_viddid, pci_did;

	if (OpenPIC_Addr != NULL) {
		openpic_init(NUM_8259_INTERRUPTS);
		/* We have a cascade on OpenPIC IRQ 0, Linux IRQ 16 */
		openpic_hookup_cascade(NUM_8259_INTERRUPTS, "82c59 cascade",
				       i8259_irq);
	}
	for (i = 0; i < NUM_8259_INTERRUPTS; i++)
		irq_desc[i].handler = &i8259_pic;
	 /* If we have a Raven PCI bridge or a Hawk PCI bridge / Memory
	  * controller, we poll (as they have a different int-ack address). */
	early_read_config_dword(0, 0, 0, PCI_VENDOR_ID, &pci_viddid);
	pci_did = (pci_viddid & 0xffff0000) >> 16;
	if (((pci_viddid & 0xffff) == PCI_VENDOR_ID_MOTOROLA)
			&& ((pci_did == PCI_DEVICE_ID_MOTOROLA_RAVEN)
				|| (pci_did == PCI_DEVICE_ID_MOTOROLA_HAWK)))
		i8259_init(0);
	else
		/* PCI interrupt ack address given in section 6.1.8 of the
		 * PReP specification. */
		i8259_init(0xbffffff0);
}

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
/*
 * IDE stuff.
 */
static int __prep
prep_ide_default_irq(ide_ioreg_t base)
{
	switch (base) {
		case 0x1f0: return 13;
		case 0x170: return 13;
		case 0x1e8: return 11;
		case 0x168: return 10;
		case 0xfff0: return 14;		/* MCP(N)750 ide0 */
		case 0xffe0: return 15;		/* MCP(N)750 ide1 */
		default: return 0;
	}
}

static ide_ioreg_t __prep
prep_ide_default_io_base(int index)
{
	switch (index) {
		case 0: return 0x1f0;
		case 1: return 0x170;
		case 2: return 0x1e8;
		case 3: return 0x168;
		default:
			return 0;
	}
}
#endif

#ifdef CONFIG_SMP
/* PReP (MTX) support */
static int __init
smp_prep_probe(void)
{
	extern int mot_multi;

	if (mot_multi) {
		openpic_request_IPIs();
		smp_hw_index[1] = 1;
		return 2;
	}

	return 1;
}

static void __init
smp_prep_kick_cpu(int nr)
{
	*(unsigned long *)KERNELBASE = nr;
	asm volatile("dcbf 0,%0"::"r"(KERNELBASE):"memory");
	printk("CPU1 released, waiting\n");
}

static void __init
smp_prep_setup_cpu(int cpu_nr)
{
	if (OpenPIC_Addr)
		do_openpic_setup_cpu();
}

static struct smp_ops_t prep_smp_ops __prepdata = {
	smp_openpic_message_pass,
	smp_prep_probe,
	smp_prep_kick_cpu,
	smp_prep_setup_cpu,
};
#endif /* CONFIG_SMP */

/*
 * What ever boots us must pass in the ammount of memory.
 */
static unsigned long __init
prep_find_end_of_memory(void)
{
	return boot_mem_size;
}

/*
 * Setup the bat mappings we're going to load that cover
 * the io areas.  RAM was mapped by mapin_ram().
 * -- Cort
 */
static void __init
prep_map_io(void)
{
	io_block_mapping(0x80000000, PREP_ISA_IO_BASE, 0x10000000, _PAGE_IO);
	io_block_mapping(0xf0000000, PREP_ISA_MEM_BASE, 0x08000000, _PAGE_IO);
}

static void __init
prep_init2(void)
{
#ifdef CONFIG_NVRAM
	request_region(PREP_NVRAM_AS0, 0x8, "nvram");
#endif
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
}

void __init
prep_init(unsigned long r3, unsigned long r4, unsigned long r5,
		unsigned long r6, unsigned long r7)
{
#ifdef CONFIG_PREP_RESIDUAL
	/* make a copy of residual data */
	if ( r3 ) {
		memcpy((void *)res,(void *)(r3+KERNELBASE),
			 sizeof(RESIDUAL));
	}
#endif

	isa_io_base = PREP_ISA_IO_BASE;
	isa_mem_base = PREP_ISA_MEM_BASE;
	pci_dram_offset = PREP_PCI_DRAM_OFFSET;
	ISA_DMA_THRESHOLD = 0x00ffffff;
	DMA_MODE_READ = 0x44;
	DMA_MODE_WRITE = 0x48;

	/* figure out what kind of prep workstation we are */
#ifdef CONFIG_PREP_RESIDUAL
	if ( res->ResidualLength != 0 ) {
		if ( !strncmp(res->VitalProductData.PrintableModel,"IBM",3) )
			_prep_type = _PREP_IBM;
		else
			_prep_type = _PREP_Motorola;
	} else /* assume motorola if no residual (netboot?) */
#endif
	{
		_prep_type = _PREP_Motorola;
	}

	ppc_md.setup_arch     = prep_setup_arch;
	ppc_md.show_percpuinfo = prep_show_percpuinfo;
	ppc_md.show_cpuinfo   = NULL; /* set in prep_setup_arch() */
	ppc_md.irq_cannonicalize = prep_irq_cannonicalize;
	ppc_md.init_IRQ       = prep_init_IRQ;
	/* this gets changed later on if we have an OpenPIC -- Cort */
	ppc_md.get_irq        = i8259_irq;
	ppc_md.init           = prep_init2;

	ppc_md.restart        = prep_restart;
	ppc_md.power_off      = NULL; /* set in prep_setup_arch() */
	ppc_md.halt           = prep_halt;

	ppc_md.nvram_read_val = prep_nvram_read_val;
	ppc_md.nvram_write_val = prep_nvram_write_val;

	ppc_md.time_init      = NULL;
	if (_prep_type == _PREP_IBM) {
		ppc_md.set_rtc_time   = mc146818_set_rtc_time;
		ppc_md.get_rtc_time   = mc146818_get_rtc_time;
		ppc_md.calibrate_decr = prep_calibrate_decr;
	} else {
		ppc_md.set_rtc_time   = mk48t59_set_rtc_time;
		ppc_md.get_rtc_time   = mk48t59_get_rtc_time;
		ppc_md.calibrate_decr = mk48t59_calibrate_decr;
		ppc_md.time_init      = mk48t59_init;
	}

	ppc_md.find_end_of_memory = prep_find_end_of_memory;
	ppc_md.setup_io_mappings = prep_map_io;

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
	ppc_ide_md.default_irq = prep_ide_default_irq;
	ppc_ide_md.default_io_base = prep_ide_default_io_base;
#endif

#ifdef CONFIG_VT
	ppc_md.kbd_setkeycode    = pckbd_setkeycode;
	ppc_md.kbd_getkeycode    = pckbd_getkeycode;
	ppc_md.kbd_translate     = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds          = pckbd_leds;
	ppc_md.kbd_init_hw       = pckbd_init_hw;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate	 = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x54;
#endif
#endif

#ifdef CONFIG_SMP
	ppc_md.smp_ops		 = &prep_smp_ops;
#endif /* CONFIG_SMP */
}
