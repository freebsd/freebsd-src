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
 *
 *	$Id: autoconf.c,v 1.3 1998/07/05 12:10:10 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h> /* for BASE_SLICE, MAX_SLICES */
#include <sys/reboot.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <machine/cons.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/cpuconf.h>

#include "scbus.h"
#if NSCBUS > 0
#include <scsi/scsiconf.h>
#endif

static void	configure __P((void *));
SYSINIT(configure, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure, NULL)

static void	configure_finish __P((void));
static void	configure_start __P((void));

static void
configure_start()
{
#if NSCBUS > 0
	scsi_configure_start();
#endif
}

static void
configure_finish()
{
#if NSCBUS > 0
	scsi_configure_finish();
#endif
}

extern void pci_configure(void);

/*
 * Determine i/o configuration for a machine.
 */
static void
configure(void *dummy)
{
	configure_start();

	device_add_child(root_bus, platform.iobus, 0, 0);

	/* XXX hack until I implement ISA */
	if (!strcmp(platform.iobus, "cia"))
	    device_add_child(root_bus, "mcclock", 0, 0);
	/* XXX end hack */

	root_bus_configure();

	pci_configure();

	configure_finish();

	cninit_finish();

	/*
	 * Now we're ready to handle (pending) interrupts.
	 * XXX this is slightly misplaced.
	 */
	spl0();

	cold = 0;
}

void
cpu_rootconf()
{
    mountrootfsname = "ufs";
    rootdevs[0] = makedev(4, dkmakeminor(0, COMPATIBILITY_SLICE, 0));
    rootdevs[1] = makedev(4, dkmakeminor(1, COMPATIBILITY_SLICE, 0));
    rootdevnames[0] = "sd0a";
}

void
cpu_dumpconf()
{
}
