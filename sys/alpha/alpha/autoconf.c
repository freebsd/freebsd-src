/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bootp.h"
#include "opt_isa.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/kernel.h>


#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/cons.h>

#include <machine/md_var.h>
#include <machine/cpuconf.h>
#include <machine/rpb.h>
#include <machine/bootinfo.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

static void	configure_first(void *);
static void	configure(void *);
static void	configure_final(void *);

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
/* SI_ORDER_SECOND is hookable */
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
/* SI_ORDER_MIDDLE is hookable */
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

#ifdef DEV_ISA
#include <isa/isavar.h>
device_t isa_bus_device = 0;
#endif

#if 0

static int
atoi(const char *s)
{
    int n = 0;
    while (*s >= '0' && *s <= '9')
	n = n * 10 + (*s++ - '0');
    return n;
}

static const char *
bootdev_field(int which)
{
	char *p = bootinfo.booted_dev;
	char *q;
	static char field[128];

	/* Skip characters to find the right field */
	for (; which; which--) {
		while (*p != ' ' && *p != '\0')
			p++;
		if (*p)
			p++;
	}

	/* Copy out the field and return it */
	q = field;
	while (*p != ' ' && *p != '\0')
		*q++ = *p++;
	*q = '\0';

	return field;
}

static const char *
bootdev_protocol(void)
{
	return bootdev_field(0);
}

static int
bootdev_slot(void)
{
	return atoi(bootdev_field(2));
}

static int
bootdev_unit(void)
{
	return atoi(bootdev_field(5));
}

static int
bootdev_bus(void)
{
	return atoi(bootdev_field(1));
}

static int
bootdev_channel(void)
{
	return atoi(bootdev_field(3));
}

static const char *
bootdev_remote_address(void)
{
	return bootdev_field(4);
}

static int
bootdev_boot_dev_type(void)
{
	return atoi(bootdev_field(6));
}

static const char *
bootdev_ctrl_dev_type(void)
{
	return bootdev_field(7);
}

#endif

/*
 * Determine i/o configuration for a machine.
 */
static void
configure_first(void *dummy)
{
}

static void
configure(void *dummy)
{

	device_add_child(root_bus, platform.iobus, 0);

	root_bus_configure();

	/*
	 * Probe ISA devices after everything.
	 */
#ifdef DEV_ISA
	if (isa_bus_device)
		isa_probe_children(isa_bus_device);
#endif
}

static void
configure_final(void *dummy)
{

	/*
	 * Now we're ready to handle (pending) interrupts.
	 * XXX this is slightly misplaced.
	 */
	alpha_pal_swpipl(ALPHA_PSL_IPL_0);

	cold = 0;
}
