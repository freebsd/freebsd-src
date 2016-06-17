/* Special Initializers for certain USB Mass Storage devices
 *
 * $Id: initializers.c,v 1.2 2000/09/06 22:35:57 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "initializers.h"
#include "debug.h"
#include "transport.h"

/* This places the Shuttle/SCM USB<->SCSI bridge devices in multi-target
 * mode */
int usb_stor_euscsi_init(struct us_data *us)
{
	unsigned char data = 0x1;
	int result;

	US_DEBUGP("Attempting to init eUSCSI bridge...\n");
	result = usb_control_msg(us->pusb_dev, usb_sndctrlpipe(us->pusb_dev, 0),
			0x0C, USB_RECIP_INTERFACE | USB_TYPE_VENDOR,
			0x01, 0x0, &data, 0x1, 5*HZ);
	US_DEBUGP("-- result is %d\n", result);
	US_DEBUGP("-- data afterwards is %d\n", data);

	return 0;
}

/* This function is required to activate all four slots on the UCR-61S2B
 * flash reader */

int usb_stor_ucr61s2b_init(struct us_data *us)
{
	int pipe;
	struct bulk_cb_wrap *bcb;
	struct bulk_cs_wrap *bcs;
	int res, partial;

	bcb = kmalloc(sizeof *bcb, in_interrupt() ? GFP_ATOMIC : GFP_NOIO);
	if (!bcb) {
		return(-1);
	}
	bcs = kmalloc(sizeof *bcs, in_interrupt() ? GFP_ATOMIC : GFP_NOIO);
	if (!bcs) {
		kfree(bcb);
		return(-1);
	}

	US_DEBUGP("Sending UCR-61S2B initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = ++(us->tag);
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(UCR61S2B_INIT);
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, UCR61S2B_INIT, sizeof(UCR61S2B_INIT));

	pipe = usb_sndbulkpipe(us->pusb_dev, us->ep_out);
	res = usb_stor_bulk_msg(us, bcb, pipe, US_BULK_CB_WRAP_LEN, &partial);
	US_DEBUGP("-- result is %d\n", res);
	kfree(bcb);

	if(res) {
		kfree(bcs);
		return(res);
	}

	pipe = usb_rcvbulkpipe(us->pusb_dev, us->ep_in);
	res = usb_stor_bulk_msg(us, bcs, pipe, US_BULK_CS_WRAP_LEN, &partial);
	US_DEBUGP("-- result of status read is %d\n", res);

	kfree(bcs);

	return(res ? -1 : 0);
}
