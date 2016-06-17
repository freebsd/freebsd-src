/*
 *
 *    Copyright (c) 1999-2000 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: oak_setup.c
 *
 *    Description:
 *      Architecture- / platform-specific boot-time initialization code for
 *      the IBM PowerPC 403GCX "Oak" evaluation board. Adapted from original
 *      code by Gary Thomas, Cort Dougan <cort@fsmlabs.com>, and Dan Malek
 *      <dan@net4x.com>.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/irq.h>
#include <linux/seq_file.h>

#include <asm/processor.h>
#include <asm/board.h>
#include <asm/machdep.h>
#include <asm/page.h>
#include <asm/bootinfo.h>
#include <asm/ppc4xx_pic.h>
#include <asm/time.h>

#include "oak_setup.h"

/* Function Prototypes */

extern void abort(void);

/* Global Variables */

unsigned char __res[sizeof(bd_t)];


/*
 * void __init oak_init()
 *
 * Description:
 *   This routine...
 *
 * Input(s):
 *   r3 - Optional pointer to a board information structure.
 *   r4 - Optional pointer to the physical starting address of the init RAM
 *        disk.
 *   r5 - Optional pointer to the physical ending address of the init RAM
 *        disk.
 *   r6 - Optional pointer to the physical starting address of any kernel
 *        command-line parameters.
 *   r7 - Optional pointer to the physical ending address of any kernel
 *        command-line parameters.
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   N/A
 *
 */
void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
	      unsigned long r6, unsigned long r7)
{
	parse_bootinfo(find_bootinfo());

	/*
	 * If we were passed in a board information, copy it into the
	 * residual data area.
	 */
	if (r3) {
		memcpy((void *)__res, (void *)(r3 + KERNELBASE), sizeof(bd_t));
	}

#if defined(CONFIG_BLK_DEV_INITRD)
	/*
	 * If the init RAM disk has been configured in, and there's a valid
	 * starting address for it, set it up.
	 */
	if (r4) {
		initrd_start = r4 + KERNELBASE;
		initrd_end = r5 + KERNELBASE;
	}
#endif /* CONFIG_BLK_DEV_INITRD */

	/* Copy the kernel command line arguments to a safe place. */

	if (r6) {
 		*(char *)(r7 + KERNELBASE) = 0;
		strcpy(cmd_line, (char *)(r6 + KERNELBASE));
	}

	/* Initialize machine-dependency vectors */

	ppc_md.setup_arch	 	= oak_setup_arch;
	ppc_md.show_percpuinfo	 	= oak_show_percpuinfo;
	ppc_md.irq_cannonicalize 	= NULL;
	ppc_md.init_IRQ		 	= oak_init_IRQ;
	ppc_md.get_irq		 	= oak_get_irq;
	ppc_md.init		 	= NULL;

	ppc_md.restart		 	= oak_restart;
	ppc_md.power_off	 	= oak_power_off;
	ppc_md.halt		 	= oak_halt;

	ppc_md.time_init	 	= oak_time_init;
	ppc_md.set_rtc_time	 	= oak_set_rtc_time;
	ppc_md.get_rtc_time	 	= oak_get_rtc_time;
	ppc_md.calibrate_decr	 	= oak_calibrate_decr;

	ppc_md.kbd_setkeycode    	= NULL;
	ppc_md.kbd_getkeycode    	= NULL;
	ppc_md.kbd_translate     	= NULL;
	ppc_md.kbd_unexpected_up 	= NULL;
	ppc_md.kbd_leds          	= NULL;
	ppc_md.kbd_init_hw       	= NULL;
	ppc_md.ppc_kbd_sysrq_xlate	= NULL;
}

/*
 * Document me.
 */
void __init
oak_setup_arch(void)
{
	/* XXX - Implement me */
}

/*
 * int oak_show_percpuinfo()
 *
 * Description:
 *   This routine pretty-prints the platform's internal CPU and bus clock
 *   frequencies into the buffer for usage in /proc/cpuinfo.
 *
 * Input(s):
 *  *buffer - Buffer into which CPU and bus clock frequencies are to be
 *            printed.
 *
 * Output(s):
 *  *buffer - Buffer with the CPU and bus clock frequencies.
 *
 * Returns:
 *   The number of bytes copied into 'buffer' if OK, otherwise zero or less
 *   on error.
 */
int
oak_show_percpuinfo(struct seq_file *m, int i)
{
	bd_t *bp = (bd_t *)__res;

	seq_printf(m, "clock\t\t: %dMHz\n"
		   "bus clock\t\t: %dMHz\n",
		   bp->bi_intfreq / 1000000,
		   bp->bi_busfreq / 1000000);

	return 0;
}

/*
 * Document me.
 */
void __init
oak_init_IRQ(void)
{
	int i;

	ppc4xx_pic_init();

	for (i = 0; i < NR_IRQS; i++) {
		irq_desc[i].handler = ppc4xx_pic;
	}

	return;
}

/*
 * Document me.
 */
int
oak_get_irq(struct pt_regs *regs)
{
	return (ppc4xx_pic_get_irq(regs));
}

/*
 * Document me.
 */
void
oak_restart(char *cmd)
{
	abort();
}

/*
 * Document me.
 */
void
oak_power_off(void)
{
	oak_restart(NULL);
}

/*
 * Document me.
 */
void
oak_halt(void)
{
	oak_restart(NULL);
}

/*
 * Document me.
 */
long __init
oak_time_init(void)
{
	/* XXX - Implement me */
	return 0;
}

/*
 * Document me.
 */
int __init
oak_set_rtc_time(unsigned long time)
{
	/* XXX - Implement me */

	return (0);
}

/*
 * Document me.
 */
unsigned long __init
oak_get_rtc_time(void)
{
	/* XXX - Implement me */

	return (0);
}

/*
 * void __init oak_calibrate_decr()
 *
 * Description:
 *   This routine retrieves the internal processor frequency from the board
 *   information structure, sets up the kernel timer decrementer based on
 *   that value, enables the 403 programmable interval timer (PIT) and sets
 *   it up for auto-reload.
 *
 * Input(s):
 *   N/A
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   N/A
 *
 */
void __init
oak_calibrate_decr(void)
{
	unsigned int freq;
	bd_t *bip = (bd_t *)__res;

	freq = bip->bi_intfreq;

	decrementer_count = freq / HZ;
	count_period_num = 1;
	count_period_den = freq;

	/* Enable the PIT and set auto-reload of its value */

	mtspr(SPRN_TCR, TCR_PIE | TCR_ARE);

	/* Clear any pending timer interrupts */

	mtspr(SPRN_TSR, TSR_ENW | TSR_WIS | TSR_PIS | TSR_FIS);
}
