/*
 *    Disk Array driver for HP SA 5xxx and 6xxx Controllers, SCSI Tape module
 *    Copyright 2001, 2002 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to Cciss-discuss@lists.sourceforge.net
 *
 */
#ifdef CONFIG_CISS_SCSI_TAPE
#ifndef _CCISS_SCSI_H_
#define _CCISS_SCSI_H_

#include <scsi/scsicam.h> /* possibly irrelevant, since we don't show disks */

		// the scsi id of the adapter...
#define SELF_SCSI_ID -1 
		// In case we ever want to present controller so sg will 
		// bind to it.  The scsi bus that's presented by the
		// driver to the OS is fabricated.  The "real" scsi-3
		// bus the hardware presents is fabricated too.
		// The actual, honest-to-goodness physical
		// bus that the devices are attached to is not
		// addressible natively, and may in fact turn
		// out to be not scsi at all.

#define SCSI_CCISS_CAN_QUEUE 2

/* this notation works fine for static initializations (as is the usual
   case for linux scsi drivers), but not so well for dynamic settings,
   so, if you change this, you also have to change cciss_unregister_scsi()
   in cciss_scsi.c  */
#define CCISS_SCSI {    \
	name:			"",				\
	detect:			cciss_scsi_detect,		\
	release:		cciss_scsi_release,		\
	proc_info:		cciss_scsi_proc_info,		\
	queuecommand:   	cciss_scsi_queue_command,	\
	bios_param:     	scsicam_bios_param,		\
	can_queue:      	SCSI_CCISS_CAN_QUEUE,		\
	this_id:		SELF_SCSI_ID,			\
	sg_tablesize:   	MAXSGENTRIES, 			\
	cmd_per_lun:		1,				\
	use_new_eh_code:	1,				\
	use_clustering:		DISABLE_CLUSTERING,\
}

/*
	info:			cciss_scsi_info,		\

Note, cmd_per_lun could give us some trouble, so I'm setting it very low.
Likewise, SCSI_CCISS_CAN_QUEUE is set very conservatively.

If the upper scsi layer tries to track how many commands we have
outstanding, it will be operating under the misapprehension that it is
the only one sending us requests.  We also have the block interface,
which is where most requests must surely come from, so the upper layer's
notion of how many requests we have outstanding will be wrong most or
all of the time.

Note, the normal SCSI mid-layer error handling doesn't work well
for this driver because 1) it takes the io_request_lock before
calling error handlers and uses a local variable to store flags,
so the io_request_lock cannot be released and interrupts enabled
inside the error handlers, and, the error handlers cannot poll
for command completion because they might get commands from the
block half of the driver completing, and not know what to do
with them.  That's what we get for making a hybrid scsi/block
driver, I suppose.

*/

struct cciss_scsi_dev_t {
	int devtype;
	int bus, target, lun;		/* as presented to the OS */
	unsigned char scsi3addr[8];	/* as presented to the HW */
};

struct cciss_scsi_hba_t {
	char name[32];
	int ndevices;
#define CCISS_MAX_SCSI_DEVS_PER_HBA 16
	struct cciss_scsi_dev_t dev[CCISS_MAX_SCSI_DEVS_PER_HBA];
};

#endif /* _CCISS_SCSI_H_ */
#endif /* CONFIG_CISS_SCSI_TAPE */
