/*
 * linux/arch/arm/mach-footbridge/arch.c
 *
 * Architecture specific fixups.  This is where any
 * parameters in the params struct are fixed up, or
 * any additional architecture specific information
 * is pulled from the params struct.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/hardware/dec21285.h>
#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void footbridge_map_io(void);
extern void footbridge_init_irq(void);

unsigned int mem_fclk_21285 = 50000000;

EXPORT_SYMBOL(mem_fclk_21285);

static int __init parse_tag_memclk(const struct tag *tag)
{
	mem_fclk_21285 = tag->u.memclk.fmemclk;
	return 0;
}

__tagtable(ATAG_MEMCLK, parse_tag_memclk);

#ifdef CONFIG_ARCH_EBSA285

static void __init
fixup_ebsa285(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
#if defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_DUMMY_CONSOLE)
	ORIG_X		 = params->u1.s.video_x;
	ORIG_Y		 = params->u1.s.video_y;
	ORIG_VIDEO_COLS  = params->u1.s.video_num_cols;
	ORIG_VIDEO_LINES = params->u1.s.video_num_rows;
#endif
}

MACHINE_START(EBSA285, "EBSA285")
	MAINTAINER("Russell King")
	BOOT_MEM(0x00000000, DC21285_ARMCSR_BASE, 0xfe000000)
	BOOT_PARAMS(0x00000100)
	VIDEO(0x000a0000, 0x000bffff)
	FIXUP(fixup_ebsa285)
	MAPIO(footbridge_map_io)
	INITIRQ(footbridge_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_NETWINDER
/*
 * Older NeTTroms either do not provide a parameters
 * page, or they don't supply correct information in
 * the parameter page.
 */
static void __init
fixup_netwinder(struct machine_desc *desc, struct param_struct *params,
		char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_ISAPNP
	extern int isapnp_disable;

	/*
	 * We must not use the kernels ISAPnP code
	 * on the NetWinder - it will reset the settings
	 * for the WaveArtist chip and render it inoperable.
	 */
	isapnp_disable = 1;
#endif

	if (params->u1.s.nr_pages != 0x02000 &&
	    params->u1.s.nr_pages != 0x04000 &&
	    params->u1.s.nr_pages != 0x08000 &&
	    params->u1.s.nr_pages != 0x10000) {
		printk(KERN_WARNING "Warning: bad NeTTrom parameters "
		       "detected, using defaults\n");

		params->u1.s.nr_pages = 0x1000;	/* 16MB */
		params->u1.s.ramdisk_size = 0;
		params->u1.s.flags = FLAG_READONLY;
		params->u1.s.initrd_start = 0;
		params->u1.s.initrd_size = 0;
		params->u1.s.rd_start = 0;
	}
}

MACHINE_START(NETWINDER, "Rebel-NetWinder")
	MAINTAINER("Russell King/Rebel.com")
	BOOT_MEM(0x00000000, DC21285_ARMCSR_BASE, 0xfe000000)
	BOOT_PARAMS(0x00000100)
	VIDEO(0x000a0000, 0x000bffff)
	DISABLE_PARPORT(0)
	DISABLE_PARPORT(2)
	FIXUP(fixup_netwinder)
	MAPIO(footbridge_map_io)
	INITIRQ(footbridge_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_CATS
/*
 * CATS uses soft-reboot by default, since
 * hard reboots fail on early boards.
 */
static void __init
fixup_cats(struct machine_desc *desc, struct param_struct *unused,
	   char **cmdline, struct meminfo *mi)
{
	ORIG_VIDEO_LINES  = 25;
	ORIG_VIDEO_POINTS = 16;
	ORIG_Y = 24;
}

MACHINE_START(CATS, "Chalice-CATS")
	MAINTAINER("Philip Blundell")
	BOOT_MEM(0x00000000, DC21285_ARMCSR_BASE, 0xfe000000)
	BOOT_PARAMS(0x00000100)
	SOFT_REBOOT
	FIXUP(fixup_cats)
	MAPIO(footbridge_map_io)
	INITIRQ(footbridge_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_CO285

static void __init
fixup_coebsa285(struct machine_desc *desc, struct param_struct *unused,
		char **cmdline, struct meminfo *mi)
{
	extern unsigned long boot_memory_end;
	extern char boot_command_line[];

	mi->nr_banks      = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size  = boot_memory_end;
	mi->bank[0].node  = 0;

	*cmdline = boot_command_line;
}

MACHINE_START(CO285, "co-EBSA285")
	MAINTAINER("Mark van Doesburg")
	BOOT_MEM(0x00000000, DC21285_ARMCSR_BASE, 0x7cf00000)
	FIXUP(fixup_coebsa285)
	MAPIO(footbridge_map_io)
	INITIRQ(footbridge_init_irq)
MACHINE_END
#endif

#ifdef CONFIG_ARCH_PERSONAL_SERVER
MACHINE_START(PERSONAL_SERVER, "Compaq-PersonalServer")
	MAINTAINER("Jamey Hicks / George France")
	BOOT_MEM(0x00000000, DC21285_ARMCSR_BASE, 0xfe000000)
	BOOT_PARAMS(0x00000100)
	MAPIO(footbridge_map_io)
	INITIRQ(footbridge_init_irq)
MACHINE_END
#endif
