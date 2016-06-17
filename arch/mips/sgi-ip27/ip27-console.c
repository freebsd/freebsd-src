/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001, 2002 Ralf Baechle
 */
#include <linux/init.h>
#include <linux/console.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/serial.h>
#include <asm/page.h>
#include <asm/sn/addrs.h>
#include <asm/sn/sn0/hub.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/sn_private.h>

#define IOC3_BAUD (22000000 / (3*16))
#define IOC3_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

static inline struct ioc3_uartregs *console_uart(void)
{
	struct ioc3 *ioc3;

	ioc3 = (struct ioc3 *)KL_CONFIG_CH_CONS_INFO(get_nasid())->memory_base;

	return &ioc3->sregs.uarta;
}

void prom_putchar(char c)
{
	struct ioc3_uartregs *uart = console_uart();

	while ((uart->iu_lsr & 0x20) == 0);
	uart->iu_thr = c;
}

char __init prom_getchar(void)
{
	return 0;
}

static void inline ioc3_console_probe(void)
{
	struct serial_struct req;

	/* Register to interrupt zero because we share the interrupt with
	   the serial driver which we don't properly support yet.  */
	memset(&req, 0, sizeof(req));
	req.irq             = 0;
	req.flags           = IOC3_COM_FLAGS;
	req.io_type         = SERIAL_IO_MEM;
	req.iomem_reg_shift = 0;
	req.baud_base       = IOC3_BAUD;

	req.iomem_base      = (unsigned char *) console_uart();
	register_serial(&req);
}

__init void ip27_setup_console(void)
{
	ioc3_console_probe();
}
