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
 * $FreeBSD$
 */

#include "opt_bootp.h"
#include "opt_isa.h"
#include "opt_nfs.h"
#include "opt_nfsroot.h"

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
#include <sys/devicestat.h>
#include <sys/cons.h>

#include <machine/md_var.h>
#include <machine/bootinfo.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

static void	configure(void *);
extern void	bootpc_init(void);
SYSINIT(configure, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL)

#ifdef DEV_ISA
#include <isa/isavar.h>
device_t isa_bus_device = 0;
#endif

extern int nfs_diskless_valid;		/* XXX use include file */

/*
 * Determine i/o configuration for a machine.
 */
static void
configure(void *dummy)
{
	device_add_child(root_bus, "nexus", 0);

	root_bus_configure();

	/*
	 * Probe ISA devices after everything.
	 */
#ifdef DEV_ISA
	if (isa_bus_device)
		isa_probe_children(isa_bus_device);
#endif

	/*
	 * Now we're ready to handle (pending) interrupts.
	 * XXX this is slightly misplaced.
	 */
	enable_intr();

	cold = 0;
}

/*
 * Do legacy root filesystem discovery.  This isn't really
 * needed on the Alpha, which has always used the loader.
 */
void
cpu_rootconf()
{
#if defined(NFSCLIENT) && defined(NFS_ROOT)
	int	order = 0;
#endif

#ifdef BOOTP
	if (!ia64_running_in_simulator())
		bootpc_init();
#endif
#if defined(NFSCLIENT) && defined(NFS_ROOT)
#if !defined(BOOTP_NFSROOT)
	if (nfs_diskless_valid)
#endif
		rootdevnames[order++] = "nfs:";
#endif
}
SYSINIT(cpu_rootconf, SI_SUB_ROOT_CONF, SI_ORDER_FIRST, cpu_rootconf, NULL)
