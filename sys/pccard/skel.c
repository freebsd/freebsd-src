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
#include <sys/lkm.h>

#include <sys/select.h>
#include <pccard/card.h>
#include <pccard/driver.h>
#include <pccard/slot.h>


/*
 *	This defines the lkm_misc module use by modload
 *	to define the module name.
 */
 MOD_MISC( "skel")


static int skelinit(struct pccard_devinfo *);		/* init device */
static void skelunload(struct pccard_devinfo *);	/* Disable driver */
static int skelintr(struct pccard_devinfo *);		/* Interrupt handler */

static struct pccard_device skel_info = {
	"skel",
	skelinit,
	skelunload,
	skelintr,
	0,			/* Attributes - presently unused */
	&net_imask		/* Interrupt mask for device */
};

DATA_SET(pccarddrv_set, skel_info);

static int opened;	/* Rather minimal device state... */
	
/*
 *	Module handler that processes loads and unloads.
 *	Once the module is loaded, the add driver routine is called
 *	to register the driver.
 *	If an unload is requested the remove driver routine is
 *	called to deregister the driver before unloading.
 */
static int
skel_handle( lkmtp, cmd)
struct lkm_table	*lkmtp;
int			cmd;
{
	int			i;
	struct lkm_misc		*args = lkmtp->private.lkm_misc;
	int			err = 0;	/* default = success*/

	switch( cmd) {
	case LKM_E_LOAD:

		/*
		 * Don't load twice! (lkmexists() is exported by kern_lkm.c)
		 */
		if( lkmexists( lkmtp))
			return( EEXIST);
/*
 *	Now register the driver
 */
		pccard_add_driver(&skel_info);
		break;		/* Success*/
/*
 *	Attempt to deregister the driver.
 */
	case LKM_E_UNLOAD:
		pccard_remove_driver(&skel_info);
		break;		/* Success*/

	default:	/* we only understand load/unload*/
		err = EINVAL;
		break;
	}

	return( err);
}


/*
 * External entry point; should generally match name of .o file.  The
 * arguments are always the same for all loaded modules.  The "load",
 * "unload", and "stat" functions in "DISPATCH" will be called under
 * their respective circumstances unless their value is "nosys".  If
 * called, they are called with the same arguments (cmd is included to
 * allow the use of a single function, ver is included for version
 * matching between modules and the kernel loader for the modules).
 *
 * Since we expect to link in the kernel and add external symbols to
 * the kernel symbol name space in a future version, generally all
 * functions used in the implementation of a particular module should
 * be static unless they are expected to be seen in other modules or
 * to resolve unresolved symbols alread existing in the kernel (the
 * second case is not likely to ever occur).
 *
 * The entry point should return 0 unless it is refusing load (in which
 * case it should return an errno from errno.h).
 */
int
lkm_skel(lkmtp, cmd, ver)
struct lkm_table	*lkmtp;	
int			cmd;
int			ver;
{
	DISPATCH(lkmtp,cmd,ver,skel_handle,skel_handle,nosys)
}
/*
 *	Skeleton driver entry points for PCCARD configuration.
 */
/*
 *	Initialize the device.
 */
static int
skelinit(struct pccard_devinfo *devi)
{
	if ((1 << devi->unit) & opened)
		return(EBUSY);
	opened |= 1 << devi->unit;
	printf("skel%d: init\n", devi->unit);
	printf("iomem = 0x%x, iobase = 0x%x\n", devi->memory, devi->ioaddr);
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
	printf("skel%d: unload\n", devi->unit);
	opened &= ~(1 << devi->unit);
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
