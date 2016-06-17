/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Stub IDE routines to keep Linux from crashing on machine which don't
 * have IDE like the Indy.
 *
 * Copyright (C) 1998, 1999 by Ralf Baechle
 */
#include <linux/hdreg.h>
#include <linux/kernel.h>
#include <linux/ide.h>
#include <asm/hdreg.h>
#include <asm/ptrace.h>

static int no_ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static ide_ioreg_t no_ide_default_io_base(int index)
{
	return 0;
}

static void no_ide_init_hwif_ports (hw_regs_t *hw, ide_ioreg_t data_port,
                                    ide_ioreg_t ctrl_port, int *irq)
{
}

struct ide_ops no_ide_ops = {
	&no_ide_default_irq,
	&no_ide_default_io_base,
	&no_ide_init_hwif_ports
};
