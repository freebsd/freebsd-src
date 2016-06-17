/*
 *  arch/ppc/platforms/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Adapted from 'alpha' version by Gary Thomas
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
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
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/adb.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/seq_file.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/gg2.h>
#include <asm/pci-bridge.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/hydra.h>
#include <asm/keyboard.h>
#include <asm/sections.h>
#include <asm/time.h>
#include <asm/btext.h>
#include <asm/i8259.h>
#include <asm/open_pic.h>

#ifdef CONFIG_SERIAL
#include <linux/serial.h>
#include <linux/serialP.h>
#endif

unsigned long chrp_get_rtc_time(void);
int chrp_set_rtc_time(unsigned long nowtime);
void chrp_calibrate_decr(void);
long chrp_time_init(void);

void chrp_find_bridges(void);
void chrp_event_scan(void);
void rtas_display_progress(char *, unsigned short);
void rtas_indicator_progress(char *, unsigned short);
void btext_progress(char *, unsigned short);

extern unsigned long pmac_find_end_of_memory(void);
extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];
extern void select_adb_keyboard(void);
extern int of_show_percpuinfo(struct seq_file *, int);

extern kdev_t boot_dev;

extern unsigned long loops_per_jiffy;

static int max_width __chrpdata;
int chrp_is_briq __chrpdata;
unsigned int* briq_SPOR __chrpdata; /* To be replaced by RTAS when available */

#ifdef CONFIG_SMP
extern struct smp_ops_t chrp_smp_ops;
#endif

static const char *gg2_memtypes[4] = {
	"FPM", "SDRAM", "EDO", "BEDO"
};
static const char *gg2_cachesizes[4] = {
	"256 KB", "512 KB", "1 MB", "Reserved"
};
static const char *gg2_cachetypes[4] = {
	"Asynchronous", "Reserved", "Flow-Through Synchronous",
	"Pipelined Synchronous"
};
static const char *gg2_cachemodes[4] = {
	"Disabled", "Write-Through", "Copy-Back", "Transparent Mode"
};

int __chrp
chrp_show_cpuinfo(struct seq_file *m)
{
	int i, sdramen;
	unsigned int t;
	struct device_node *root;
	const char *model = "";

	root = find_path_device("/");
	if (root)
		model = get_property(root, "model", NULL);
	seq_printf(m, "machine\t\t: CHRP %s\n", model);

	/* longtrail (goldengate) stuff */
	if (!strncmp(model, "IBM,LongTrail", 13)) {
		/* VLSI VAS96011/12 `Golden Gate 2' */
		/* Memory banks */
		sdramen = (in_le32((unsigned *)(gg2_pci_config_base+
						GG2_PCI_DRAM_CTRL))
			   >>31) & 1;
		for (i = 0; i < (sdramen ? 4 : 6); i++) {
			t = in_le32((unsigned *)(gg2_pci_config_base+
						 GG2_PCI_DRAM_BANK0+
						 i*4));
			if (!(t & 1))
				continue;
			switch ((t>>8) & 0x1f) {
			case 0x1f:
				model = "4 MB";
				break;
			case 0x1e:
				model = "8 MB";
				break;
			case 0x1c:
				model = "16 MB";
				break;
			case 0x18:
				model = "32 MB";
				break;
			case 0x10:
				model = "64 MB";
				break;
			case 0x00:
				model = "128 MB";
				break;
			default:
				model = "Reserved";
				break;
			}
			seq_printf(m, "memory bank %d\t: %s %s\n", i, model,
				   gg2_memtypes[sdramen ? 1 : ((t>>1) & 3)]);
		}
		/* L2 cache */
		t = in_le32((unsigned *)(gg2_pci_config_base+GG2_PCI_CC_CTRL));
		seq_printf(m, "board l2\t: %s %s (%s)\n",
			   gg2_cachesizes[(t>>7) & 3],
			   gg2_cachetypes[(t>>2) & 3],
			   gg2_cachemodes[t & 3]);
	}
	return 0;
}

/*
 *  Fixes for the National Semiconductor PC78308VUL SuperI/O
 *
 *  Some versions of Open Firmware incorrectly initialize the IRQ settings
 *  for keyboard and mouse
 */
static inline void __init sio_write(u8 val, u8 index)
{
	outb(index, 0x15c);
	outb(val, 0x15d);
}

static inline u8 __init sio_read(u8 index)
{
	outb(index, 0x15c);
	return inb(0x15d);
}

static void __init sio_fixup_irq(const char *name, u8 device, u8 level,
				     u8 type)
{
	u8 level0, type0, active;

	/* select logical device */
	sio_write(device, 0x07);
	active = sio_read(0x30);
	level0 = sio_read(0x70);
	type0 = sio_read(0x71);
	if (level0 != level || type0 != type || !active) {
		printk(KERN_WARNING "sio: %s irq level %d, type %d, %sactive: "
		       "remapping to level %d, type %d, active\n",
		       name, level0, type0, !active ? "in" : "", level, type);
		sio_write(0x01, 0x30);
		sio_write(level, 0x70);
		sio_write(type, 0x71);
	}
}

static void __init sio_init(void)
{
	struct device_node *root;

	if ((root = find_path_device("/")) &&
	    !strncmp(get_property(root, "model", NULL), "IBM,LongTrail", 13)) {
		/* logical device 0 (KBC/Keyboard) */
		sio_fixup_irq("keyboard", 0, 1, 2);
		/* select logical device 1 (KBC/Mouse) */
		sio_fixup_irq("mouse", 1, 12, 2);
	}
}


void __init
chrp_setup_arch(void)
{
	struct device_node *device;

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000/HZ;

#ifdef CONFIG_BLK_DEV_INITRD
	/* this is fine for chrp */
	initrd_below_start_ok = 1;

	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	else
#endif
		ROOT_DEV = to_kdev_t(0x0802); /* sda2 (sda1 is for the kernel) */

	/* Lookup PCI host bridges */
	chrp_find_bridges();

#ifndef CONFIG_PPC64BRIDGE
	/*
	 *  Temporary fixes for PCI devices.
	 *  -- Geert
	 */
	hydra_init();		/* Mac I/O */

#endif /* CONFIG_PPC64BRIDGE */

	/*
	 *  Fix the Super I/O configuration
	 */
	sio_init();

	/*
	 *  Setup the console operations
	 */
#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

	/* Get the event scan rate for the rtas so we know how
	 * often it expects a heartbeat. -- Cort
	 */
	if (rtas_data) {
		struct property *p;
		unsigned long rate, count;
		device = find_devices("rtas");
		for ( p = device->properties;
		      p && strncmp(p->name, "rtas-event-scan-rate", 20);
		      p = p->next )
			/* nothing */ ;
		if (p && (rate = *(unsigned long *)p->value) > 0) {
			/*
			 * The value is the number of times per minute.
			 * For now assign the full workload here to cpu 0.
			 *
			 * We now split the rate and spread the heartbeats
			 * when we kick secondary cpus so we can spread
			 * the calls evenly.
			 */
			ppc_md.heartbeat = chrp_event_scan;

			count = (60*HZ) / rate;
			if (!count)        /* XXX insane */
				count = 1;

			heartbeat_reset(0) = count;
			heartbeat_count(0) = 1;

			printk("RTAS Event Scan Rate: %lu calls/minute "
			       "(every %lu jiffies)\n", rate, count );
		}
	}

	pci_create_OF_bus_map();
}

void __chrp
chrp_event_scan(void)
{
	unsigned char log[1024];
	unsigned long ret = 0;
	/* XXX: we should loop until the hardware says no more error logs -- Cort */
	call_rtas( "event-scan", 4, 1, &ret, 0xffffffff, 0,
		   __pa(log), 1024 );
}

void __chrp
chrp_restart(char *cmd)
{
	printk("RTAS system-reboot returned %d\n",
	       call_rtas("system-reboot", 0, 1, NULL));
	for (;;);
}

void __chrp
chrp_power_off(void)
{
	/* allow power on only with power button press */
	printk("RTAS power-off returned %d\n",
	       call_rtas("power-off", 2, 1, NULL,0xffffffff,0xffffffff));
	for (;;);
}

void __chrp
chrp_halt(void)
{
	chrp_power_off();
}

u_int __chrp
chrp_irq_cannonicalize(u_int irq)
{
	if (irq == 2)
		return 9;
	return irq;
}

static void __chrp
briq_restart(char *cmd)
{
	cli();
	if (briq_SPOR)
		out_be32(briq_SPOR, 0);
	for(;;) ;
}

/*
 * Finds the open-pic node and sets OpenPIC_Addr based on its reg property.
 * Then checks if it has an interrupt-ranges property.  If it does then
 * we have a distributed open-pic, so call openpic_set_sources to tell
 * the openpic code where to find the interrupt source registers.
 */
static void __init chrp_find_openpic(void)
{
	struct device_node *np;
	int len, i;
	unsigned int *iranges;
	void *isu;

	np = find_type_devices("open-pic");
	if (np == NULL || np->n_addrs == 0)
		return;
	printk(KERN_INFO "OpenPIC at %x (size %x)\n",
	       np->addrs[0].address, np->addrs[0].size);
	OpenPIC_Addr = ioremap(np->addrs[0].address, 0x40000);
	if (OpenPIC_Addr == NULL) {
		printk(KERN_ERR "Failed to map OpenPIC!\n");
		return;
	}

	iranges = (unsigned int *) get_property(np, "interrupt-ranges", &len);
	if (iranges == NULL || len < 2 * sizeof(unsigned int))
		return;		/* not distributed */

	/*
	 * The first pair of cells in interrupt-ranges refers to the
	 * IDU; subsequent pairs refer to the ISUs.
	 */
	len /= 2 * sizeof(unsigned int);
	if (np->n_addrs < len) {
		printk(KERN_ERR "Insufficient addresses for distributed"
		       " OpenPIC (%d < %d)\n", np->n_addrs, len);
		return;
	}
	if (iranges[1] != 0) {
		printk(KERN_INFO "OpenPIC irqs %d..%d in IDU\n",
		       iranges[0], iranges[0] + iranges[1] - 1);
		openpic_set_sources(iranges[0], iranges[1], NULL);
	}
	for (i = 1; i < len; ++i) {
		iranges += 2;
		printk(KERN_INFO "OpenPIC irqs %d..%d in ISU at %x (%x)\n",
		       iranges[0], iranges[0] + iranges[1] - 1,
		       np->addrs[i].address, np->addrs[i].size);
		isu = ioremap(np->addrs[i].address, np->addrs[i].size);
		if (isu != NULL)
			openpic_set_sources(iranges[0], iranges[1], isu);
		else
			printk(KERN_ERR "Failed to map OpenPIC ISU at %x!\n",
			       np->addrs[i].address);
	}
}

static void __init
chrp_init_irq_openpic(unsigned long intack)
{
	int i;
	unsigned char init_senses[NR_IRQS - NUM_8259_INTERRUPTS];

	chrp_find_openpic();

	prom_get_irq_senses(init_senses, NUM_8259_INTERRUPTS, NR_IRQS);
	OpenPIC_InitSenses = init_senses;
	OpenPIC_NumInitSenses = NR_IRQS - NUM_8259_INTERRUPTS;

	openpic_init(NUM_8259_INTERRUPTS);
	/* We have a cascade on OpenPIC IRQ 0, Linux IRQ 16 */
	openpic_hookup_cascade(NUM_8259_INTERRUPTS, "82c59 cascade",
			       i8259_irq);

	for (i = 0; i < NUM_8259_INTERRUPTS; i++)
		irq_desc[i].handler = &i8259_pic;
	i8259_init(intack);
}

static void __init
chrp_init_irq_8259(unsigned long intack)
{
	int i;

	ppc_md.get_irq = i8259_irq;
	for (i = 0; i < NUM_8259_INTERRUPTS; i++)
		irq_desc[i].handler = &i8259_pic;
	i8259_init(intack);
}

void __init chrp_init_IRQ(void)
{
	struct device_node *np;
	unsigned long intack = 0;
	struct device_node *main_irq_ctrler = NULL;
#if defined(CONFIG_VT) && defined(CONFIG_ADB_KEYBOARD) && defined(XMON)
	struct device_node *kbd;
#endif

	for (np = find_devices("pci"); np != NULL; np = np->next) {
		unsigned int *addrp = (unsigned int *)
			get_property(np, "8259-interrupt-acknowledge", NULL);
		if (addrp == NULL)
			continue;
		intack = addrp[prom_n_addr_cells(np)-1];
		break;
	}
	if (np == NULL)
		printk("Cannot find pci to get ack address\n");

	/* Look for the node of the toplevel interrupt controller.
	 * If we don't find it, we assume openpic
	 */
	np = find_path_device("/chosen");
	if (np) {
		phandle *irq_ctrler_ph =
			(phandle *)get_property(np, "interrupt-controller", NULL);
		if (irq_ctrler_ph)
			main_irq_ctrler = find_phandle(*irq_ctrler_ph);
	}

	if (main_irq_ctrler && device_is_compatible(main_irq_ctrler, "8259"))
		chrp_init_irq_8259(intack);
	else
		chrp_init_irq_openpic(intack);

#if defined(CONFIG_VT) && defined(CONFIG_ADB_KEYBOARD) && defined(XMON)
	/* see if there is a keyboard in the device tree
	   with a parent of type "adb" */
	for (kbd = find_devices("keyboard"); kbd; kbd = kbd->next)
		if (kbd->parent && kbd->parent->type
		    && strcmp(kbd->parent->type, "adb") == 0)
			break;
	if (kbd)
		request_irq(HYDRA_INT_ADB_NMI, xmon_irq, 0, "XMON break", 0);
#endif
}

void __init
chrp_init2(void)
{

	if (chrp_is_briq)
		briq_SPOR = (unsigned int *)ioremap(0xff0000e8, 4);
#ifdef CONFIG_NVRAM
/* Fix me: currently, a lot of pmac_nvram routines are marked __pmac, and
 * blindly calling pmac_nvram_init() on chrp cause bad results.
 * Among others, it cracks on briQ.
 * Please implement a CHRP specific version. --BenH
 */
	if (!chrp_is_briq)
		pmac_nvram_init();
#endif
	/* This is to be replaced by RTAS when available */

	request_region(0x20,0x20,"pic1");
	request_region(0xa0,0x20,"pic2");
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");

	if (ppc_md.progress)
		ppc_md.progress("  Have fun!    ", 0x7777);

#if defined(CONFIG_VT) && (defined(CONFIG_ADB_KEYBOARD) || defined(CONFIG_INPUT))
	/* see if there is a keyboard in the device tree
	   with a parent of type "adb" */
	{
		struct device_node *kbd;

		for (kbd = find_devices("keyboard"); kbd; kbd = kbd->next) {
			if (kbd->parent && kbd->parent->type
			    && strcmp(kbd->parent->type, "adb") == 0) {
				select_adb_keyboard();
				break;
			}
		}
	}
#endif /* CONFIG_VT && (CONFIG_ADB_KEYBOARD || CONFIG_INPUT) */
}

void __init
chrp_init(unsigned long r3, unsigned long r4, unsigned long r5,
	  unsigned long r6, unsigned long r7)
{
	struct device_node *root = find_path_device("/");
	char *machine;

#ifdef CONFIG_BLK_DEV_INITRD
	/* take care of initrd if we have one */
	if ( r6 )
	{
		initrd_start = r6 + KERNELBASE;
		initrd_end = r6 + r7 + KERNELBASE;
	}
#endif /* CONFIG_BLK_DEV_INITRD */

	ISA_DMA_THRESHOLD = ~0L;
	DMA_MODE_READ = 0x44;
	DMA_MODE_WRITE = 0x48;
	isa_io_base = CHRP_ISA_IO_BASE;		/* default value */

	/* Check if it's a briq */
	machine = get_property(root, "model", NULL);
	chrp_is_briq = machine && strncmp(machine, "TotalImpact,BRIQ-1", 18) == 0;
#ifdef CONFIG_SERIAL
	if (chrp_is_briq) {
		/* briQ has a different serial clock */
		extern struct serial_state rs_table[];
		rs_table[0].baud_base = (7372800 / 16);
		rs_table[1].baud_base = (7372800 / 16);
	}
#endif /* CONFIG_SERIAL */

	ppc_md.setup_arch     = chrp_setup_arch;
	ppc_md.show_percpuinfo = of_show_percpuinfo;
	ppc_md.show_cpuinfo   = chrp_show_cpuinfo;
	ppc_md.irq_cannonicalize = chrp_irq_cannonicalize;
	ppc_md.init_IRQ       = chrp_init_IRQ;
	ppc_md.get_irq        = openpic_get_irq;

	ppc_md.init           = chrp_init2;

	ppc_md.restart        = chrp_is_briq ? briq_restart : chrp_restart;
	ppc_md.power_off      = chrp_power_off;
	ppc_md.halt           = chrp_halt;

	ppc_md.time_init      = chrp_time_init;
	ppc_md.set_rtc_time   = chrp_set_rtc_time;
	ppc_md.get_rtc_time   = chrp_get_rtc_time;
	ppc_md.calibrate_decr = chrp_calibrate_decr;

	ppc_md.find_end_of_memory = pmac_find_end_of_memory;

#ifdef CONFIG_VT
	/* these are adjusted in chrp_init2 if we have an ADB keyboard */
	ppc_md.kbd_setkeycode    = pckbd_setkeycode;
	ppc_md.kbd_getkeycode    = pckbd_getkeycode;
	ppc_md.kbd_translate     = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	if (!chrp_is_briq) {
		ppc_md.kbd_leds          = pckbd_leds;
		ppc_md.kbd_init_hw       = pckbd_init_hw;
	}
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate	 = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x54;
#endif /* CONFIG_MAGIC_SYSRQ */
#endif /* CONFIG_VT */

	if (rtas_data) {
		struct device_node *rtas;
		unsigned int *p;

		rtas = find_devices("rtas");
		if (rtas != NULL) {
			if (get_property(rtas, "display-character", NULL)) {
				ppc_md.progress = rtas_display_progress;
				p = (unsigned int *) get_property
				       (rtas, "ibm,display-line-length", NULL);
				if (p)
					max_width = *p;
			} else if (get_property(rtas, "set-indicator", NULL))
				ppc_md.progress = rtas_indicator_progress;
		}
	}
#ifdef CONFIG_BOOTX_TEXT
	if (ppc_md.progress == NULL && boot_text_mapped)
		ppc_md.progress = btext_progress;
#endif

#ifdef CONFIG_SMP
	ppc_md.smp_ops = &chrp_smp_ops;
#endif /* CONFIG_SMP */

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
#endif

	/*
	 * Print the banner, then scroll down so boot progress
	 * can be printed.  -- Cort
	 */
	if ( ppc_md.progress ) ppc_md.progress("Linux/PPC "UTS_RELEASE"\n", 0x0);
}

void __chrp
rtas_display_progress(char *s, unsigned short hex)
{
	int width;
	char *os = s;

	if ( call_rtas( "display-character", 1, 1, NULL, '\r' ) )
		return;

	width = max_width;
	while ( *os )
	{
		if ( (*os == '\n') || (*os == '\r') )
			width = max_width;
		else
			width--;
		call_rtas( "display-character", 1, 1, NULL, *os++ );
		/* if we overwrite the screen length */
		if ( width == 0 )
			while ( (*os != 0) && (*os != '\n') && (*os != '\r') )
				os++;
	}

	/*while ( width-- > 0 )*/
	call_rtas( "display-character", 1, 1, NULL, ' ' );
}

void __chrp
rtas_indicator_progress(char *s, unsigned short hex)
{
	call_rtas("set-indicator", 3, 1, NULL, 6, 0, hex);
}

#ifdef CONFIG_BOOTX_TEXT
void
btext_progress(char *s, unsigned short hex)
{
	prom_print(s);
	prom_print("\n");
}
#endif /* CONFIG_BOOTX_TEXT */
