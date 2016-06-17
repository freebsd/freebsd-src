/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * IDE routines for typical pc-like standard configurations
 * for the CASIO CASSIOPEIA E-55/65.
 *
 * Copyright (C) 1998, 1999, 2001 by Ralf Baechle
 */
/*
 * Changes:
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>  Sun, 24 Feb 2002
 *  - Added CASIO CASSIOPEIA E-55/65 support.
 */
#include <linux/sched.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>
#include <asm/ptrace.h>
#include <asm/hdreg.h>

static int e55_ide_default_irq(ide_ioreg_t base)
{
	return 40;
}

static ide_ioreg_t e55_ide_default_io_base(int index)
{
	switch (index) {
		case 0: return 0xc1f0;
		case 1: return 0xc170;
		case 2: return 0xc1e8;
		case 3: return 0xc168;
		case 4: return 0xc1e0;
		case 5: return 0xc160;
	}
	return 0;
}

static void e55_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
                                    ide_ioreg_t ctrl_port, int *irq)
{
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET] + 0x206;
	}
	if (irq != NULL)
		*irq = 0;
	hw->io_ports[IDE_IRQ_OFFSET] = 0;
}

static int e55_ide_request_irq(unsigned int irq,
                               void (*handler)(int,void *, struct pt_regs *),
                               unsigned long flags, const char *device,
                               void *dev_id)
{
	return request_irq(irq, handler, flags, device, dev_id);
}			

static void e55_ide_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

static int e55_ide_check_region(ide_ioreg_t from, unsigned int extent)
{
	return check_region(from, extent);
}

static void e55_ide_request_region(ide_ioreg_t from, unsigned int extent,
                                   const char *name)
{
	request_region(from, extent, name);
}

static void e55_ide_release_region(ide_ioreg_t from, unsigned int extent)
{
	release_region(from, extent);
}

struct ide_ops e55_ide_ops = {
	&e55_ide_default_irq,
	&e55_ide_default_io_base,
	&e55_ide_init_hwif_ports,
	&e55_ide_request_irq,
	&e55_ide_free_irq,
	&e55_ide_check_region,
	&e55_ide_request_region,
	&e55_ide_release_region
};
