/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 *      $Id: bt5xx-445.c,v 1.2 1995/12/14 14:19:15 peter Exp $
 */

/*
 * Bulogic/Bustek 32 bit Addressing Mode SCSI driver.
 *
 * NOTE: 1. Some bt5xx card can NOT handle 32 bit addressing mode.
 *       2. OLD bt445s Revision A,B,C,D(nowired) + any firmware version
 *          has broken busmaster for handling 32 bit addressing on H/W bus
 *	    side.
 *
 *       3. Extended probing still needs confirmation from our user base, due
 *	    to several H/W and firmware dependencies. If you have a problem
 *	    with extended probing, please contact 'amurai@spec.co.jp'
 *
 *						amurai@spec.co.jp 94/6/16
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/devconf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <i386/isa/isa_device.h>
#include <i386/scsi/btreg.h>

static	int bt_isa_probe __P((struct isa_device *dev));
static	int bt_isa_attach __P((struct isa_device *dev));

struct isa_driver btdriver =
{
    bt_isa_probe,
    bt_isa_attach,
    "bt"
};

static struct kern_devconf kdc_isa_bt = {
	0, 0, 0,		/* filled in by dev_attach */
	"bt", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start here */
	NULL,
	DC_CLS_MISC		/* host adapters aren't special */
};

static inline void
bt_isa_registerdev(struct isa_device *id)
{
#ifdef BOGUS
	if(id->id_unit)
		kdc_bt[id->id_unit] = kdc_bt[0];
	kdc_bt[id->id_unit].kdc_unit = id->id_unit;
	kdc_bt[id->id_unit].kdc_parentdata = id;
	dev_attach(&kdc_bt[id->id_unit]);
#endif
}

/*
 * Check if the device can be found at the port given
 * and if so, set it up ready for further work
 * as an argument, takes the isa_device structure from
 * autoconf.c
 */
static int
bt_isa_probe(dev)
	struct isa_device *dev;
{
	/*
	 * find unit and check we have that many defined
	 */
	int     unit = bt_unit;
	struct bt_data *bt;

	/*
	 * We ignore the unit number assigned by config to allow
	 * consistant numbering between PCI/EISA/ISA devices.
	 * This is a total kludge until we have a configuration
	 * manager.
	 */
	dev->id_unit = bt_unit;
	/*
	 * Allocate a storage area for us
	 */
	bt = bt_alloc(unit, dev->id_iobase);
	if (!bt) 
		return 0;

#ifndef DEV_LKM
	bt_isa_registerdev(dev);
#endif /* not DEV_LKM */

	/*
	 * Try initialise a unit at this location
	 * sets up dma and bus speed, loads bt->bt_int
	 */
	if (bt_init(bt) != 0) {
		bt_free(bt);
		return 0;
	}
	/*
	 * If it's there, put in it's interrupt vectors
	 */
	dev->id_unit = unit;
	dev->id_irq = (1 << bt->bt_int);
	dev->id_drq = bt->bt_dma;

	bt_unit++;
	return 1;
}

/*
 * Attach all the sub-devices we can find
 */
static int
bt_isa_attach(dev)
	struct isa_device *dev;
{
	int	unit = dev->id_unit;
	struct	bt_data *bt = btdata[unit];

	return( bt_attach(bt) );
}

/*
 * Handle an ISA interrupt.
 * XXX should go away as soon as ISA interrupt handlers
 * take a (void *) arg.
 */
void
bt_isa_intr(unit)
	int	unit;
{
	struct bt_data* arg = btdata[unit];
	bt_intr((void *)arg);
}
