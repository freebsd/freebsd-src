/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * IDE routines for typical pc-like standard configurations
 * for the ZAO Networks Capcella.
 *
 * Copyright (C) 1998, 1999, 2001 by Ralf Baechle
 */
/*
 * Changes:
 *  Yoichi Yuasa <yuasa@hh.iij4u.or.jp>  Fri, 23 Aug 2002
 *  - Added Victor MP-C303/304 support.
 */
#include <linux/sched.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>
#include <asm/ptrace.h>
#include <asm/hdreg.h>

static int mpc30x_ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static ide_ioreg_t mpc30x_ide_default_io_base(int index)
{
	return 0;
}

static void mpc30x_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
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

static int mpc30x_ide_request_irq(unsigned int irq,
                                  void (*handler)(int,void *, struct pt_regs *),
                                  unsigned long flags, const char *device,
                                  void *dev_id)
{
	return request_irq(irq, handler, flags, device, dev_id);
}

static void mpc30x_ide_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

static int mpc30x_ide_check_region(ide_ioreg_t from, unsigned int extent)
{
	return check_region(from, extent);
}

static void mpc30x_ide_request_region(ide_ioreg_t from, unsigned int extent,
                                      const char *name)
{
	request_region(from, extent, name);
}

static void mpc30x_ide_release_region(ide_ioreg_t from, unsigned int extent)
{
	release_region(from, extent);
}

struct ide_ops mpc30x_ide_ops = {
	&mpc30x_ide_default_irq,
	&mpc30x_ide_default_io_base,
	&mpc30x_ide_init_hwif_ports,
	&mpc30x_ide_request_irq,
	&mpc30x_ide_free_irq,
	&mpc30x_ide_check_region,
	&mpc30x_ide_request_region,
	&mpc30x_ide_release_region
};
