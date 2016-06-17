/*
 * Platform dependent support for HP simulator.
 *
 * Copyright (C) 1998, 1999 Hewlett-Packard Co
 * Copyright (C) 1998, 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 Vijay Chander <vijay@engr.sgi.com>
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/console.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/pal.h>
#include <asm/machvec.h>
#include <asm/pgtable.h>
#include <asm/sal.h>

#include "hpsim_ssc.h"

extern struct console hpsim_cons;

/*
 * Simulator system call.
 */
asm (".text\n"
     ".align 32\n"
     ".global ia64_ssc\n"
     ".proc ia64_ssc\n"
     "ia64_ssc:\n"
     "mov r15=r36\n"
     "break 0x80001\n"
     "br.ret.sptk.many rp\n"
     ".endp\n");

void
ia64_ssc_connect_irq (long intr, long irq)
{
	ia64_ssc(intr, irq, 0, 0, SSC_CONNECT_INTERRUPT);
}

void
ia64_ctl_trace (long on)
{
	ia64_ssc(on, 0, 0, 0, SSC_CTL_TRACE);
}

void __init
hpsim_setup (char **cmdline_p)
{
	ROOT_DEV = to_kdev_t(0x0801);		/* default to first SCSI drive */

	register_console (&hpsim_cons);
}
