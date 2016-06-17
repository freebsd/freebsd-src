/*
 *      Copyright (C) 1993-1996 Bas Laarhoven,
 *                (C) 1996-1997 Claus-Justus Heine.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; see the file COPYING.  If not, write to
 the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 *
 *      This file contains the code that interfaces the kernel
 *      for the QIC-40/80/3010/3020 floppy-tape driver for Linux.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/major.h>

#include <linux/ftape.h>
#if LINUX_VERSION_CODE >= KERNEL_VER(2,1,16)
#include <linux/init.h>
#else
#define __initdata
#define __initfunc(__arg) __arg
#endif
#include <linux/qic117.h>
#ifdef CONFIG_ZFTAPE
#include <linux/zftape.h>
#endif

#include "../lowlevel/ftape-init.h"
#include "../lowlevel/ftape_syms.h"
#include "../lowlevel/ftape-io.h"
#include "../lowlevel/ftape-read.h"
#include "../lowlevel/ftape-write.h"
#include "../lowlevel/ftape-ctl.h"
#include "../lowlevel/ftape-rw.h"
#include "../lowlevel/fdc-io.h"
#include "../lowlevel/ftape-buffer.h"
#include "../lowlevel/ftape-proc.h"
#include "../lowlevel/ftape-tracing.h"

/*      Global vars.
 */
char ft_src[] __initdata = "$Source: /homes/cvs/ftape-stacked/ftape/lowlevel/ftape-init.c,v $";
char ft_rev[] __initdata = "$Revision: 1.8 $";
char ft_dat[] __initdata = "$Date: 1997/11/06 00:38:08 $";


/*  Called by modules package when installing the driver
 *  or by kernel during the initialization phase
 */
int __init ftape_init(void)
{
	TRACE_FUN(ft_t_flow);

#ifdef MODULE
	printk(KERN_INFO FTAPE_VERSION "\n");
        if (TRACE_LEVEL >= ft_t_info) {
		printk(
KERN_INFO "(c) 1993-1996 Bas Laarhoven (bas@vimec.nl)\n"
KERN_INFO "(c) 1995-1996 Kai Harrekilde-Petersen (khp@dolphinics.no)\n"
KERN_INFO "(c) 1996-1997 Claus-Justus Heine (claus@momo.math.rwth-aachen.de)\n"
KERN_INFO "QIC-117 driver for QIC-40/80/3010/3020 floppy tape drives\n"
KERN_INFO "Compiled for Linux version %s"
#ifdef MODVERSIONS
	       " with versioned symbols"
#endif
	       "\n", UTS_RELEASE);
        }
#else /* !MODULE */
	/* print a short no-nonsense boot message */
	printk(KERN_INFO FTAPE_VERSION " for Linux " UTS_RELEASE "\n");
#endif /* MODULE */
	TRACE(ft_t_info, "installing QIC-117 floppy tape hardware drive ... ");
	TRACE(ft_t_info, "ftape_init @ 0x%p", ftape_init);
	/*  Allocate the DMA buffers. They are deallocated at cleanup() time.
	 */
#if TESTING
#ifdef MODULE
	while (ftape_set_nr_buffers(CONFIG_FT_NR_BUFFERS) < 0) {
		ftape_sleep(FT_SECOND/20);
		if (signal_pending(current)) {
			(void)ftape_set_nr_buffers(0);
			TRACE(ft_t_bug,
			      "Killed by signal while allocating buffers.");
			TRACE_ABORT(-EINTR, 
				    ft_t_bug, "Free up memory and retry");
		}
	}
#else
	TRACE_CATCH(ftape_set_nr_buffers(CONFIG_FT_NR_BUFFERS),
		    (void)ftape_set_nr_buffers(0));
#endif
#else
	TRACE_CATCH(ftape_set_nr_buffers(CONFIG_FT_NR_BUFFERS),
		    (void)ftape_set_nr_buffers(0));
#endif
	ft_drive_sel = -1;
	ft_failure   = 1;         /* inhibit any operation but open */
	ftape_udelay_calibrate(); /* must be before fdc_wait_calibrate ! */
	fdc_wait_calibrate();
#if LINUX_VERSION_CODE < KERNEL_VER(2,1,18)
	register_symtab(&ftape_symbol_table); /* add global ftape symbols */
#endif
#if defined(CONFIG_PROC_FS) && defined(CONFIG_FT_PROC_FS)
	(void)ftape_proc_init();
#endif
#ifdef CONFIG_ZFTAPE
	(void)zft_init();
#endif
	TRACE_EXIT 0;
}

#ifdef MODULE

#ifndef CONFIG_FT_NO_TRACE_AT_ALL
static int ft_tracing = -1;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VER(2,1,18)
#define FT_MOD_PARM(var,type,desc) \
	MODULE_PARM(var,type); MODULE_PARM_DESC(var,desc)

FT_MOD_PARM(ft_fdc_base,       "i", "Base address of FDC controller.");
FT_MOD_PARM(ft_fdc_irq,        "i", "IRQ (interrupt channel) to use.");
FT_MOD_PARM(ft_fdc_dma,        "i", "DMA channel to use.");
FT_MOD_PARM(ft_fdc_threshold,  "i", "Threshold of the FDC Fifo.");
FT_MOD_PARM(ft_fdc_rate_limit, "i", "Maximal data rate for FDC.");
FT_MOD_PARM(ft_probe_fc10,     "i", 
	    "If non-zero, probe for a Colorado FC-10/FC-20 controller.");
FT_MOD_PARM(ft_mach2,          "i",
	    "If non-zero, probe for a Mountain MACH-2 controller.");
FT_MOD_PARM(ft_tracing,        "i", 
	    "Amount of debugging output, 0 <= tracing <= 8, default 3.");
MODULE_AUTHOR(
	"(c) 1993-1996 Bas Laarhoven (bas@vimec.nl), "
	"(c) 1995-1996 Kai Harrekilde-Petersen (khp@dolphinics.no), "
	"(c) 1996, 1997 Claus-Justus Heine (claus@momo.math.rwth-aachen.de)");
MODULE_DESCRIPTION(
	"QIC-117 driver for QIC-40/80/3010/3020 floppy tape drives.");
MODULE_LICENSE("GPL");
#endif

/*  Called by modules package when installing the driver
 */
int init_module(void)
{
#ifndef CONFIG_FT_NO_TRACE_AT_ALL
	if (ft_tracing != -1) {
		ftape_tracing = ft_tracing;
	}
#endif
	return ftape_init();
}

/*  Called by modules package when removing the driver
 */
void cleanup_module(void)
{
	TRACE_FUN(ft_t_flow);

#if defined(CONFIG_PROC_FS) && defined(CONFIG_FT_PROC_FS)
	ftape_proc_destroy();
#endif
	(void)ftape_set_nr_buffers(0);
        printk(KERN_INFO "ftape: unloaded.\n");
	TRACE_EXIT;
}
#endif /* MODULE */
