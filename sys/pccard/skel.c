/*
 * Loadable kernel module skeleton driver
 * 11 July 1995 Andrew McRae
 *
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/sysent.h>
#include <sys/exec.h>

#include <i386/isa/isa_device.h>

#include <sys/select.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

static int skelinit(struct pccard_devinfo *);		/* init device */
static void skelunload(struct pccard_devinfo *);	/* Disable driver */
static int skelintr(struct pccard_devinfo *);		/* Interrupt handler */

PCCARD_MODULE(skel, skelinit, skelunload, skelintr, 0, net_imask);

static int opened;	/* Rather minimal device state... */
	
/*
 *	Skeleton driver entry points for PCCARD configuration.
 */
/*
 *	Initialize the device.
 */
static int
skelinit(struct pccard_devinfo *devi)
{
	int unit = devi->isahd.id_unit;

	if (opened & (1 << unit))
		return(EBUSY);
	opened |= 1 << unit;
	printf("%s%d: init\n", devi->drv->name, unit);
	printf("%s%d: irq %d iobase 0x%x maddr 0x%x memlen %d\n",
		devi->drv->name, unit, devi->isahd.id_irq,
		devi->isahd.id_iobase, devi->isahd.id_maddr,
		devi->isahd.id_msize);
	return(0);
}
/*
 *	The device entry is being removed. Shut it down,
 *	and turn off interrupts etc. Not called unless
 *	the device was successfully installed.
 */
static void
skelunload(struct pccard_devinfo *devi)
{
	int unit = devi->isahd.id_unit;

	printf("%s%d: unload\n", devi->drv->name, unit);
	opened &= ~(1 << unit);
}
/*
 *	Interrupt handler.
 *	Returns true if the interrupt is for us.
 */
static int
skelintr(struct pccard_devinfo *devi)
{
	return(0);
}
