/*
 * Copyright (C) 1999,2001-2003 Silicon Graphics, Inc. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy, 
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/timex.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/serial.h>
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/mmzone.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/compiler.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/sal.h>
#include <asm/machvec.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/pgalloc.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/arch.h>
#include <asm/sn/addrs.h>
#include <asm/sn/pda.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/simulator.h>
#include <asm/sn/leds.h>
#include <asm/sn/bte.h>
#include <asm/sn/clksupport.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/sn2/shub.h>

#define pxm_to_nasid(pxm) (((pxm)<<1) | (get_nasid() & ~0x1ff))

#define MAX_PHYS_MEMORY		(1UL << 49)	/* 1 TB */

extern void bte_init_node (nodepda_t *, cnodeid_t);
extern void bte_init_cpu (void);
extern void sn_timer_init(void);
extern unsigned long last_time_offset;
extern void init_platform_hubinfo(nodepda_t **nodepdaindr);
extern void (*ia64_mark_idle)(int);
extern void (*ia64_platform_timer_extras)(void);
extern void sn_timer_interrupt_extras(void);
extern void snidle(int);
extern unsigned char acpi_kbd_controller_present;

unsigned long sn_rtc_cycles_per_second;   

partid_t sn_partid = -1;
char sn_system_serial_number_string[128];
u64 sn_partition_serial_number;

short	physical_node_map[MAX_PHYSNODE_ID];

int	numionodes;
/*
 * This is the address of the RRegs in the HSpace of the global
 * master.  It is used by a hack in serial.c (serial_[in|out],
 * printk.c (early_printk), and kdb_io.c to put console output on that
 * node's Bedrock UART.  It is initialized here to 0, so that
 * early_printk won't try to access the UART before
 * master_node_bedrock_address is properly calculated.
 */
u64 master_node_bedrock_address;

static void sn_init_pdas(char **);


static nodepda_t	*nodepdaindr[MAX_COMPACT_NODES];

irqpda_t		*irqpdaindr;


/*
 * The format of "screen_info" is strange, and due to early i386-setup
 * code. This is just enough to make the console code think we're on a
 * VGA color display.
 */
struct screen_info sn_screen_info = {
	orig_x:			 0,
	orig_y:			 0,
	orig_video_mode:	 3,
	orig_video_cols:	80,
	orig_video_ega_bx:	 3,
	orig_video_lines:	25,
	orig_video_isVGA:	 1,
	orig_video_points:	16
};

/*
 * This is here so we can use the CMOS detection in ide-probe.c to
 * determine what drives are present.  In theory, we don't need this
 * as the auto-detection could be done via ide-probe.c:do_probe() but
 * in practice that would be much slower, which is painful when
 * running in the simulator.  Note that passing zeroes in DRIVE_INFO
 * is sufficient (the IDE driver will autodetect the drive geometry).
 */
#ifdef CONFIG_IA64_GENERIC
extern char drive_info[4*16];
#else
char drive_info[4*16];
#endif

/**
 * early_sn_setup - early setup routine for SN platforms
 *
 * Sets up an initial console to aid debugging.  Intended primarily
 * for bringup.  See start_kernel() in init/main.c.
 */
#if defined(CONFIG_IA64_EARLY_PRINTK_SGI_SN) || defined(CONFIG_IA64_SGI_SN_SIM)

void __init
early_sn_setup(void)
{
	void ia64_sal_handler_init (void *entry_point, void *gpval);
	efi_system_table_t			*efi_systab;
	efi_config_table_t 			*config_tables;
	struct ia64_sal_systab			*sal_systab;
	struct ia64_sal_desc_entry_point	*ep;
	char					*p;
	int					i;

	/*
	 * Parse enough of the SAL tables to locate the SAL entry point. Since, console
	 * IO on SN2 is done via SAL calls, early_printk won't work without this.
	 *
	 * This code duplicates some of the ACPI table parsing that is in efi.c & sal.c.
	 * Any changes to those file may have to be made hereas well.
	 */
	efi_systab = (efi_system_table_t*)__va(ia64_boot_param->efi_systab);
	config_tables = __va(efi_systab->tables);
	for (i = 0; i < efi_systab->nr_tables; i++) {
		if (efi_guidcmp(config_tables[i].guid, SAL_SYSTEM_TABLE_GUID) == 0) {
			sal_systab = __va(config_tables[i].table);
			p = (char*)(sal_systab+1);
			for (i = 0; i < sal_systab->entry_count; i++) {
				if (*p == SAL_DESC_ENTRY_POINT) {
					ep = (struct ia64_sal_desc_entry_point *) p;
					ia64_sal_handler_init(__va(ep->sal_proc), __va(ep->gp));
					break;
				}
				p += SAL_DESC_SIZE(*p);
			}
		}
	}

	if ( IS_RUNNING_ON_SIMULATOR() ) {
		master_node_bedrock_address = (u64)REMOTE_HUB(get_nasid(), SH_JUNK_BUS_UART0);
		printk(KERN_DEBUG "early_sn_setup: setting master_node_bedrock_address to 0x%lx\n", master_node_bedrock_address);
	}
}
#endif /* CONFIG_IA64_EARLY_PRINTK_SGI_SN */

#ifdef CONFIG_IA64_MCA
extern int platform_intr_list[];
#endif

extern nasid_t master_nasid;
static int shub_1_1_found __initdata;


/*
 * sn_check_for_wars
 *
 * Set flag for enabling shub specific wars
 */

static inline int __init
is_shub_1_1(int nasid)
{
	unsigned long id;
	int	rev;

	id = REMOTE_HUB_L(nasid, SH_SHUB_ID);
	rev =  (id & SH_SHUB_ID_REVISION_MASK) >> SH_SHUB_ID_REVISION_SHFT;
	return rev <= 2;
}

static void __init
sn_check_for_wars(void)
{
	int	cnode;

	for (cnode=0; cnode< numnodes; cnode++)
		if (is_shub_1_1(cnodeid_to_nasid(cnode)))
			shub_1_1_found = 1;
}

/*
 * SN2 requires very slightly different alternate data-TLB miss handle than what
 * the mainline linux kernel provides.  At some point this approach could be used
 * to allow the use of the low-memory thrown away on other platforms when VGA is
 * present.
 *
 * On SN2 we want to load small TCs for granule-0 (and aliases of) faulting
 * addresses.  The details of this are more sublte than at which they first
 * appear.
 */
static void __init
sn2_replace_ivt(void)
{
	extern unsigned char alt_dtlb_miss[], ia64_ivt_page_fault[];
	extern unsigned char sn2_alt_dtlb_miss[], sn2_alt_dtlb_miss_end[];
	extern unsigned char sn2_alt_dtlb_miss_patch1[];

	unsigned char *s, *d;
	u64 *p;
	u64 len = (u64)sn2_alt_dtlb_miss_end - (u64)sn2_alt_dtlb_miss;
	u64 broffs = (ia64_ivt_page_fault - alt_dtlb_miss) - (sn2_alt_dtlb_miss_patch1 - sn2_alt_dtlb_miss);
	u64 psr;
	int i;

	/* printk(KERN_DEBUG "Replacing alternate data-TLB miss handler.\n"); */

	/* Check the code isn't too large */
	if (len > 1024) {
		printk(KERN_ERR "SGI: Specific alt_dtlb_misse too large!  Not replacing\n");
		return;
	}

	/* check the offset is sane (should always be) */
	if ((broffs>>4) + (1<<20) >= (1<<21)) {
		printk(KERN_ERR "SGI: IVT patch ivt offset %ld invalid!   Not replacing!\n", broffs);
		return;
	}

	/* 2nd half of bundle to patch (has slot 2) */
	p = (u64*)sn2_alt_dtlb_miss_patch1 + 1;
	/* patch the offset into slot 2 (imm20b + s) */
	*p = (*p & ~(0x8fffff000000000)) | ((broffs & 0x1000000) << 35) | ((broffs & 0x0fffff0) << 32);

	/* don't want any interrupts when doing this */
	psr = ia64_clear_ic();

	/* copy over the existing code, flush i-cache as required */
	d = alt_dtlb_miss;
	s = sn2_alt_dtlb_miss;
	for (i=0; i<len; ++i, ++s) {
		*d++ = *s;
		if ((((u64)s) & 63) == 63) {
			ia64_insn_group_barrier();
			ia64_fc((void*)s);
		}
	}
	ia64_insn_group_barrier();
	ia64_fc((void*)s);

	/* sync & serialize instruction stream */
	ia64_sync_i();
	ia64_srlz_i();

	/* restore interrupt status */
	ia64_set_psr(psr);

	/* flush any TC's we have had previously loaded that could cause problems here */
	local_flush_tlb_all();

	printk(KERN_DEBUG "SGI: Replaced alt_dtlb_miss handler.\n");
}

/**
 * sn_setup - SN platform setup routine
 * @cmdline_p: kernel command line
 *
 * Handles platform setup for SN machines.  This includes determining
 * the RTC frequency (via a SAL call), initializing secondary CPUs, and
 * setting up per-node data areas.  The console is also initialized here.
 */
void __init
sn_setup(char **cmdline_p)
{
	long status, ticks_per_sec, drift;
	int pxm;
	int major = sn_sal_rev_major(), minor = sn_sal_rev_minor();
	extern void sn_cpu_init(void);

	/*
	 * If the generic code has enabled vga console support - lets
	 * get rid of it again. This is a kludge for the fact that ACPI
	 * currtently has no way of informing us if legacy VGA is available
	 * or not.
	 */
#if defined(CONFIG_VT) && defined(CONFIG_VGA_CONSOLE)
	if (conswitchp == &vga_con) {
		printk(KERN_DEBUG "SGI: Disabling VGA console\n");
#ifdef CONFIG_DUMMY_CONSOLE
		conswitchp = &dummy_con;
#else
		conswitchp = NULL;
#endif /* CONFIG_DUMMY_CONSOLE */
	}
#endif /* def(CONFIG_VT) && def(CONFIG_VGA_CONSOLE) */

	MAX_DMA_ADDRESS = PAGE_OFFSET + MAX_PHYS_MEMORY;

	memset(physical_node_map, -1, sizeof(physical_node_map));
	for (pxm=0; pxm<MAX_PXM_DOMAINS; pxm++)
		if (pxm_to_nid_map[pxm] != -1)
			physical_node_map[pxm_to_nasid(pxm)] = pxm_to_nid_map[pxm];


	/*
	 * Old PROMs do not provide an ACPI FADT. Disable legacy keyboard
	 * support here so we don't have to listen to failed keyboard probe
	 * messages.
	 */
	if ((major < 2 || (major == 2 && minor <= 9)) &&
	    acpi_kbd_controller_present) {
		printk(KERN_INFO "Disabling legacy keyboard support as prom "
		       "is too old and doesn't provide FADT\n");
		acpi_kbd_controller_present = 0;
	}

	printk("SGI SAL version %x.%02x\n", major, minor);

	/*
	 * Confirm the SAL we're running on is recent enough...
	 */
	if ((major < SN_SAL_MIN_MAJOR) || (major == SN_SAL_MIN_MAJOR &&
					   minor < SN_SAL_MIN_MINOR)) {
		printk(KERN_ERR "This kernel needs SGI SAL version >= "
		       "%x.%02x\n", SN_SAL_MIN_MAJOR, SN_SAL_MIN_MINOR);
		panic("PROM version too old\n");
	}

	/* Patch the ivt */
	sn2_replace_ivt();

	master_nasid = get_nasid();
	(void)snia_get_console_nasid();
	(void)snia_get_master_baseio_nasid();

	status = ia64_sal_freq_base(SAL_FREQ_BASE_REALTIME_CLOCK, &ticks_per_sec, &drift);
	if (status != 0 || ticks_per_sec < 100000) {
		printk(KERN_WARNING "unable to determine platform RTC clock frequency, guessing.\n");
		/* PROM gives wrong value for clock freq. so guess */
		sn_rtc_cycles_per_second = 1000000000000UL/30000UL;
	}
	else
		sn_rtc_cycles_per_second = ticks_per_sec;

	platform_intr_list[ACPI_INTERRUPT_CPEI] = IA64_CPE_VECTOR;


	if ( IS_RUNNING_ON_SIMULATOR() )
	{
		master_node_bedrock_address = (u64)REMOTE_HUB(get_nasid(), SH_JUNK_BUS_UART0);
		printk(KERN_DEBUG "sn_setup: setting master_node_bedrock_address to 0x%lx\n",
		       master_node_bedrock_address);
	}

	/*
	 * we set the default root device to /dev/hda
	 * to make simulation easy
	 */
	ROOT_DEV = to_kdev_t(0x0301);

	/*
	 * Create the PDAs and NODEPDAs for all the cpus.
	 */
	sn_init_pdas(cmdline_p);

	ia64_mark_idle = &snidle;

	/* 
	 * For the bootcpu, we do this here. All other cpus will make the
	 * call as part of cpu_init in slave cpu initialization.
	 */
	sn_cpu_init();

	/*
	 * Setup hubinfo stuff. Has to happen AFTER sn_cpu_init(),
	 * because it uses the cnode to nasid tables.
	 */
	init_platform_hubinfo(nodepdaindr);
#ifdef CONFIG_SMP
	init_smp_config();
#endif
	screen_info = sn_screen_info;

	ia64_platform_timer_extras = &sn_timer_interrupt_extras;

	sn_timer_init();
}

/**
 * sn_init_pdas - setup node data areas
 *
 * One time setup for Node Data Area.  Called by sn_setup().
 */
void
sn_init_pdas(char **cmdline_p)
{
	cnodeid_t	cnode;
	void scan_for_ionodes(void);

	/*
	 * Make sure that the PDA fits entirely in the same page as the 
	 * cpu_data area.
	 */
	if ((PDAADDR&~PAGE_MASK)+sizeof(pda_t) > PAGE_SIZE)
		panic("overflow of cpu_data page");

	memset(pda.cnodeid_to_nasid_table, -1, sizeof(pda.cnodeid_to_nasid_table));
	for (cnode=0; cnode<numnodes; cnode++)
		pda.cnodeid_to_nasid_table[cnode] = pxm_to_nasid(nid_to_pxm_map[cnode]);

	numionodes = numnodes;
	scan_for_ionodes();

        /*
         * Allocate & initalize the nodepda for each node.
         */
        for (cnode=0; cnode < numnodes; cnode++) {
		nodepdaindr[cnode] = alloc_bootmem_node(NODE_DATA(cnode), sizeof(nodepda_t));
		memset(nodepdaindr[cnode], 0, sizeof(nodepda_t));
        }

	/* 
	 * Allocate & initialize nodepda for TIOs.  For now, put them on node 0.
	 */
	for (cnode = numnodes; cnode < numionodes; cnode ++) {
		nodepdaindr[cnode] = alloc_bootmem_node(NODE_DATA(0), sizeof(nodepda_t));
		memset(nodepdaindr[cnode], 0, sizeof(nodepda_t));
	}

	/*
	 * Now copy the array of nodepda pointers to each nodepda.
	 */
        for (cnode=0; cnode < numionodes; cnode++)
		memcpy(nodepdaindr[cnode]->pernode_pdaindr, nodepdaindr, sizeof(nodepdaindr));


	/*
	 * Set up IO related platform-dependent nodepda fields.
	 * The following routine actually sets up the hubinfo struct
	 * in nodepda.
	 */
	for (cnode = 0; cnode < numnodes; cnode++) {
		init_platform_nodepda(nodepdaindr[cnode], cnode);
		spin_lock_init(&nodepdaindr[cnode]->bist_lock);
		bte_init_node (nodepdaindr[cnode], cnode);
	}

	/*
	 * Handle TIO differently .. we do not do BTE init ..
	 */
	for (cnode = numnodes; cnode < numionodes; cnode++) {
		init_platform_nodepda(nodepdaindr[cnode], cnode);
	}
}

/**
 * sn_cpu_init - initialize per-cpu data areas
 * @cpuid: cpuid of the caller
 *
 * Called during cpu initialization on each cpu as it starts.
 * Currently, initializes the per-cpu data area for SNIA.
 * Also sets up a few fields in the nodepda.  Also known as
 * platform_cpu_init() by the ia64 machvec code.
 */
void __init
sn_cpu_init(void)
{
	int	cpuid;
	int	cpuphyid;
	int	nasid;
	int	slice;
	int	cnode, i;
	static int	wars_have_been_checked = 0;

	/*
	 * The boot cpu makes this call again after platform initialization is
	 * complete.
	 */
	if (nodepdaindr[0] == NULL)
		return;

	cpuid = smp_processor_id();
	cpuphyid = ((ia64_get_lid() >> 16) & 0xffff);
	nasid = cpu_physical_id_to_nasid(cpuphyid);
	cnode = nasid_to_cnodeid(nasid);
	slice = cpu_physical_id_to_slice(cpuphyid);

	printk("CPU %d: nasid %d, slice %d, cnode %d\n",
			smp_processor_id(), nasid, slice, cnode);

	memset(&pda, 0, sizeof(pda));
	pda.p_nodepda = nodepdaindr[cnode];
	pda.led_address = (typeof(pda.led_address)) (LED0 + (slice<<LED_CPU_SHIFT));
	pda.led_state = LED_ALWAYS_SET;
	pda.hb_count = HZ/2;
	pda.hb_state = 0;
	pda.idle_flag = 0;
	
	memset(pda.cnodeid_to_nasid_table, -1, sizeof(pda.cnodeid_to_nasid_table));
	for (i=0; i<numnodes; i++)
		pda.cnodeid_to_nasid_table[i] = pxm_to_nasid(nid_to_pxm_map[i]);
	/*
	 * Check for WARs.
	 * Only needs to be done once, on BSP.
	 * Has to be done after loop above, because it uses pda.cnodeid_to_nasid_table[i].
	 * Has to be done before assignment below.
	 */
	if (!wars_have_been_checked) {
		sn_check_for_wars();
		wars_have_been_checked = 1;
	}

	pda.shub_1_1_found = shub_1_1_found;

	if (local_node_data->active_cpu_count == 1)
		nodepda->node_first_cpu = cpuid;



	/*
	 * We must use different memory allocators for first cpu (bootmem 
	 * allocator) than for the other cpus (regular allocator).
	 */
	if (cpuid == 0)
		irqpdaindr = alloc_bootmem_node(NODE_DATA(cpuid_to_cnodeid(cpuid)),sizeof(irqpda_t));

	memset(irqpdaindr, 0, sizeof(irqpda_t));
	irqpdaindr->irq_flags[SGI_PCIBR_ERROR] = SN2_IRQ_SHARED;
	irqpdaindr->irq_flags[SGI_PCIBR_ERROR] |= SN2_IRQ_RESERVED;
	irqpdaindr->irq_flags[SGI_II_ERROR] = SN2_IRQ_SHARED;
	irqpdaindr->irq_flags[SGI_II_ERROR] |= SN2_IRQ_RESERVED;

	pda.pio_write_status_addr = (volatile unsigned long *)
			LOCAL_MMR_ADDR((slice < 2 ? SH_PIO_WRITE_STATUS_0 : SH_PIO_WRITE_STATUS_1 ) );
	pda.mem_write_status_addr = (volatile u64 *)
			LOCAL_MMR_ADDR((slice < 2 ? SH_MEMORY_WRITE_STATUS_0 : SH_MEMORY_WRITE_STATUS_1 ) );

	if (nodepda->node_first_cpu == cpuid) {
		int	buddy_nasid;
		buddy_nasid = cnodeid_to_nasid(numa_node_id() == numnodes-1 ? 0 : numa_node_id()+ 1);
		pda.pio_shub_war_cam_addr = (volatile unsigned long*)GLOBAL_MMR_ADDR(nasid, SH_PI_CAM_CONTROL);
	}

	bte_init_cpu();
}

/*
 * Scan klconfig for TIO's.  Add the TIO nasids to the
 * physical_node_map and the pda and increment numionodes.
 */

void
scan_for_ionodes() {
	int nasid = 0;
	lboard_t *brd;

	/* Scan all compute nodes. */
	for (nasid = 0; nasid < MAX_PHYSNODE_ID; nasid +=2) {
		/* if there's no nasid, don't try to read the klconfig on the node */
		if (physical_node_map[nasid] == -1) continue;
		brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_TIO);
		while (brd) {
			pda.cnodeid_to_nasid_table[numionodes] = brd->brd_nasid;
			physical_node_map[brd->brd_nasid] = numionodes++;
			brd = KLCF_NEXT(brd);
			brd = find_lboard(brd, KLTYPE_TIO);
		}
	}
}
