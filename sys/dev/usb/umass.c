/*-
 * Copyright (c) 1999 MAEKAWA Masahide <bishop@rr.iij4u.or.jp>,
 *		      Nick Hibma <hibma@skylink.it>
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
 *	$FreeBSD$
 */

/*
 * Universal Serial Bus Mass Storage Class Bulk-Only Transport
 * http://www.usb.org/developers/usbmassbulk_09.pdf
 *
 * Relevant parts have been quoted in the source.
 */

/* To do:
 *	x The umass_usb_transfer routine uses synchroneous transfers. This
 *	  should be changed to async and state handling.
 */

/* Authors: (with short acronyms for comments)
 *   NWH - Nick Hibma <hibma@skylink.it>
 */

#include "opt_usb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <machine/clock.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdi_util.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>

#ifdef UMASS_DEBUG
#define	DPRINTF(m, x)	if (umassdebug & (m)) logprintf x
#define UDMASS_CAM	0x00010000
#define UDMASS_SCSI	0x00020000
#define UDMASS_USB	0x00040000
#define UDMASS_BULK	0x00080000
#define UDMASS_ALL	0xffff0000
int umassdebug = UDMASS_CAM|UDMASS_BULK|UDMASS_USB;
#else
#define	DPRINTF(m, x)
#endif

typedef struct umass_softc {
	bdevice			sc_dev;		/* base device */
	usbd_interface_handle	sc_iface;	/* the interface we use */

	u_int8_t		sc_bulkout;	/* bulk-out Endpoint Address */
	usbd_pipe_handle	sc_bulkout_pipe;
	u_int8_t		sc_bulkin;	/* bulk-in Endpoint Address */
	usbd_pipe_handle	sc_bulkin_pipe;

	struct cam_sim		*sim;		/* SCSI Interface Module */ 
	struct cam_path		*path;		/* XPT path */

	/* we count the number of soft retries that failed. If 5 have failed,
	 * the device does not support them for example, we revert to hard
	 * resets.
	 */
	int			soft_tries;	/* retries to do soft reset */
#	define MAX_SOFT_TRIES		5
} umass_softc_t;

#define USBD_COMMAND_FAILED	USBD_INVAL	/* redefine some errors for */

#define UPROTO_MASS_ZIP	80

#define UMASS_SCSIID_HOST	0x00
#define UMASS_SCSIID_DEVICE	0x01

#define DIR_OUT		0
#define DIR_IN		1
#define DIR_NONE	2

/* Bulk-Only specific request */
#define	UR_RESET	0xff
#define	URESET_HARD	0x00
#define	URESET_SOFT	0x01

/* Bulk-Only Mass Storage features */
/* Command Block Wrapper */
typedef struct {
	uDWord		dCBWSignature;
#define  CBWSIGNATURE		0x43425355
	uDWord		dCBWTag;
	uDWord		dCBWDataTransferLength;
	uByte		bCBWFlags;
#define	 CBWFLAGS_OUT	0x00
#define	 CBWFLAGS_IN	0x80
	uByte		bCBWLUN;
	uByte		bCDBLength;
	uByte		CBWCDB[16];
} usb_bulk_cbw_t;
#define	USB_BULK_CBW_SIZE	31

/* Command Status Wrapper */
typedef struct {
	uDWord		dCSWSignature;
#define	 CSWSIGNATURE		0x53425355
	uDWord		dCSWTag;
	uDWord		dCSWDataResidue;
	uByte		bCSWStatus;
#define  CSWSTATUS_GOOD		0x0
#define  CSWSTATUS_FAILED	0x1
#define  CSWSTATUS_PHASE	0x2
} usb_bulk_csw_t;
#define	USB_BULK_CSW_SIZE	13


USB_DECLARE_DRIVER(umass);

/* USB related functions */
usbd_status umass_usb_transfer __P((usbd_interface_handle iface,
				usbd_pipe_handle pipe,
		                void *buf, int buflen,
				int flags, int *xfer_size));

/* Bulk-Only related functions */
usbd_status umass_bulk_reset	__P((umass_softc_t *sc, int flag));
usbd_status umass_bulk_transfer	__P((umass_softc_t *sc, int lun,
				void *cmd, int cmdlen,
		    		void *data, int datalen,
				int dir, int *residue));

/* CAM related functions */
static void umass_cam_action	__P((struct cam_sim *sim, union ccb *ccb));
static void umass_cam_poll	__P((struct cam_sim *sim));

static int  umass_cam_attach	__P((umass_softc_t *sc));
static void umass_cam_detach	__P((umass_softc_t *sc));



USB_MATCH(umass)
{
	USB_MATCH_START(umass, uaa);
	usb_interface_descriptor_t *id;

	if (!uaa->iface)
		return(UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id
	    && id->bInterfaceClass == UCLASS_MASS
	    && id->bInterfaceSubClass == USUBCLASS_SCSI
	    && id->bInterfaceProtocol == UPROTO_MASS_ZIP)
	    	/* probe the Iomega USB Zip 100 drive */
		return(UMATCH_VENDOR_IFACESUBCLASS_IFACEPROTO);

	return(UMATCH_NONE);
}

USB_ATTACH(umass)
{
	USB_ATTACH_START(umass, sc, uaa);
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char devinfo[1024];
	int i;
	int err;

	sc->sc_iface = uaa->iface;
	sc->sc_bulkout_pipe = NULL;
	sc->sc_bulkin_pipe = NULL;
	sc->soft_tries = 0;

	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;

	id = usbd_get_interface_descriptor(sc->sc_iface);
	printf("%s: %s, iclass %d/%d/%d\n", USBDEVNAME(sc->sc_dev), devinfo,
	       id->bInterfaceClass, id->bInterfaceSubClass,
	       id->bInterfaceProtocol);

	/*
	 * A Bulk-Only Mass Storage device supports the following endpoints,
	 * in addition to the Endpoint 0 for Control transfer that is required
	 * of all USB devices:
	 * (a) bulk-in endpoint.
	 * (b) bulk-out endpoint.
	 *
	 * The endpoint addresses are not fixed, so we have to read them
	 * from the device descriptors of the current interface.
	 */
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (!ed) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_bulkin = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_bulkout = ed->bEndpointAddress;
		}
	}

	/* Open the bulk-in and -out pipe */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("cannot open bulk out pipe (address %d)\n",
			sc->sc_bulkout));
		USB_ATTACH_ERROR_RETURN;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin,
				USBD_EXCLUSIVE_USE, &sc->sc_bulkin_pipe);
	if (err) {
		DPRINTF(UDMASS_USB, ("cannot open bulk in pipe (address %d)\n",
			sc->sc_bulkin));
		usbd_close_pipe(sc->sc_bulkout_pipe);
		USB_ATTACH_ERROR_RETURN;
	}

	/* attach the device to the SCSI layer */
	err = umass_cam_attach(sc);
	if (err) {
		usbd_close_pipe(sc->sc_bulkout_pipe);
		usbd_close_pipe(sc->sc_bulkin_pipe);
		USB_ATTACH_ERROR_RETURN;
	}

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__FreeBSD__)
static int
umass_detach(device_t self)
{
	umass_softc_t *sc = device_get_softc(self);

	DPRINTF(UDMASS_USB, ("%s: detached\n", USBDEVNAME(sc->sc_dev)));

	/* detach from the SCSI host controller (completely) */
	umass_cam_detach(sc);

	/* remove the bulk pipes) */
	if (sc->sc_bulkout_pipe)
		usbd_close_pipe(sc->sc_bulkout_pipe);
	if (sc->sc_bulkin_pipe)
		usbd_close_pipe(sc->sc_bulkin_pipe);

	device_set_desc(sc->sc_dev, NULL);

	return(0);
}
#endif


/* Performs a request over a pipe.
 *
 * flags: Can be set to USBD_SHORT_XFER_OK
 * xfer_size: if not null returns the nr. of bytes transferred
 *
 * If the returned error is USBD_STALLED the pipe stall has
 * been cleared again.
 */

usbd_status
umass_usb_transfer(usbd_interface_handle iface, usbd_pipe_handle pipe,
		   void *buf, int buflen, int flags, int *xfer_size)
{
	usbd_request_handle reqh;
	usbd_private_handle priv;
	void *buffer;
	int size;
	usbd_status err;

	/* A transfer is done synchronously. We create and schedule the
	 * transfer and then wait for it to complete
	 */

	reqh = usbd_alloc_request();
	if (!reqh) {
		DPRINTF(UDMASS_USB, ("Not enough memory\n"));
		return USBD_NOMEM;
	}

	(void) usbd_setup_request(reqh, pipe, 0, buf, buflen,
				flags, 3000 /*ms*/, NULL);
	err = usbd_sync_transfer(reqh);
	if (err) {
		DPRINTF(UDMASS_USB, ("transfer failed, %s\n",
			usbd_errstr(err)));
		usbd_free_request(reqh);
		return(err);
	}

	usbd_get_request_status(reqh, &priv, &buffer, &size, &err);

	if (xfer_size)
		*xfer_size = size;

	usbd_free_request(reqh);
	return(USBD_NORMAL_COMPLETION);
}




/*
 * USB Mass Storage Bulk-Only specific request
 */

/*
 * The Reset request shall be sent via the Control endpoint to the device.
 *
 * There are two types of Bulk-Only Mass Storage Resets; soft and hard.
 * Implementation of the soft is optional. If the soft one fails, the hard one
 * is immediately tried.
 *
 * Soft reset shall clear all buffers and reset the interface to the device
 * without affecting the state of the device itself.
 * Hard reset shall clear all buffers and reset the interface and the
 * device without changing STALL or toggle conditions.
 */

usbd_status
umass_bulk_reset(umass_softc_t *sc, int flag)
{
	usbd_device_handle dev;
        usb_device_request_t req;
	usbd_status err;

	DPRINTF(UDMASS_BULK, ("%s: %s reset\n",
		USBDEVNAME(sc->sc_dev),
		(flag == URESET_SOFT? "Soft":"Hard")));

	/* Avoid useless attempts at soft-resetting the drive
	 * XXX Could be done with a quirk entry somewhere as well.
	 */
	if (flag == URESET_SOFT && sc->soft_tries > MAX_SOFT_TRIES)
		flag = URESET_HARD;

	usbd_interface2device_handle(sc->sc_iface, &dev);

	/* the reset command is a class specific interface request */
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_RESET;
	USETW(req.wValue, flag);		/* type of reset: hard/soft */
	USETW(req.wIndex, sc->sc_iface->idesc->bInterfaceNumber);
	USETW(req.wLength, 0);			/* no data stage */

	err = usbd_do_request(dev, &req, 0);
	if (err) {
		if (flag == URESET_SOFT) {
			/* reset again, but now hard. Soft reset is optional in the
			 * Bulk-Only spec.
			 */
			sc->soft_tries++;
			DPRINTF(UDMASS_USB, ("%s: Soft reset failed (%d), %s\n",
				USBDEVNAME(sc->sc_dev),
				sc->soft_tries, usbd_errstr(err)));
			return(umass_bulk_reset(sc, URESET_HARD));
		} else {
			printf("%s: Hard reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* XXX we should port_reset the device */
			return(err);
		}
	}

	/* we do not need to wait for the device to finish the reset.
	 * From the Bulk-Only spec (5.3.3):
	 * "For either Bulk-Only Mass Storage Reset, hard, or soft, the device
	 * shall NAK the status stage of Control request until the reset is
	 * complete."
	 *
	 * XXX (Iomega Zip 100) For some reason we get timeouts if we don't. :-(
	 *                      The 2.5sec. is a guessed value.
	 */
	
	DELAY(2500000 /*us*/);

	return(USBD_NORMAL_COMPLETION);
}

/*
 * Do a Bulk-Only transfer with cmdlen bytes from cmd, possibly
 * a data phase of datalen bytes from/to data and finally a csw read
 * phase.
 *
 * If the data direction was inbound a maximum of datalen bytes
 * is stored in the buffer pointed to by data.
 * The status returned is USBD_NORMAL_COMPLETION,
 * USBD_IOERROR, USBD_COMMAND_FAILED.
 * In the last case *residue is set to the residue from the CSW,
 * otherwise to 0.
 *
 * For the functionality of this subroutine see the Mass Storage
 * Spec., the graphs on page 14 and page 19 and beyong (v0.9 of
 * the spec).
 */

usbd_status
umass_bulk_transfer(umass_softc_t *sc, int lun, void *cmd, int cmdlen,
		    void *data, int datalen, int dir, int *residue)
{
	static int dCBWtag = 42;	/* tag to be used in transfers, 
					 * incremented at each transfer */
	usb_bulk_cbw_t cbw;		/* command block wrapper struct */
	usb_bulk_csw_t csw;		/* command status wrapper struct */
	u_int32_t n = 0;		/* number of bytes transported */
	usbd_status err;

#ifdef UMASS_DEBUG
	u_int8_t *c = cmd;

	/* check the given arguments */
	if (!data && datalen > 0) {	/* no buffer for transfer */
		DPRINTF(UDMASS_BULK, ("%s: no buffer, but datalen > 0 !\n",
			USBDEVNAME(sc->sc_dev)));
		return USBD_IOERROR;
	}

	DPRINTF(UDMASS_BULK, ("%s: cmd: %d bytes (0x%02x%02x%02x%02x%02x%02x%s)"
		", data: %d bytes, dir: %s\n",
		USBDEVNAME(sc->sc_dev),
		cmdlen, c[0], c[1], c[2], c[3], c[4], c[5],
		(cmdlen > 6? "...":""),
		datalen, (dir == DIR_IN? "in":"out")));
#endif

	if (dir == DIR_NONE || datalen == 0) {		/* make sure they correspond */
		datalen = 0;
		dir = DIR_NONE;
	}

	*residue = 0;			/* reset residue */

	/*
	 * Determine the direction of transferring data and data length.
	 *
	 * dCBWDataTransferLength (datalen) :
	 *   This field indicates the number of bytes of data that the host
	 *   intends to transfer on the IN or OUT Bulk endpoint(as indicated by
	 *   the Direction bit) during the execution of this command. If this
	 *   field is set to 0, the device will expect that no data will be
	 *   transferred IN or OUT during this command, regardless of the value
	 *   of the Direction bit defined in dCBWFlags.
	 *
	 * dCBWFlags (dir) :
	 *   The bits of the Flags field are defined as follows:
	 *     Bits 0-6  reserved
	 *     Bit  7    Direction - this bit shall be ignored if the
	 *                           dCBWDataTransferLength field is zero.
	 *               0 = data Out from host to device
	 *               1 = data In from device to host
	 */


	/*
	 * Command transport phase
	 */

	/* Fill in the Command Block Wrapper */
	USETDW(cbw.dCBWSignature, CBWSIGNATURE);
	USETDW(cbw.dCBWTag, dCBWtag++);
	USETDW(cbw.dCBWDataTransferLength, datalen);
	/* we do not check for DIR_NONE below (see text on dCBWFlags above) */
	cbw.bCBWFlags = (dir == DIR_IN? CBWFLAGS_IN:CBWFLAGS_OUT);
	cbw.bCBWLUN = lun;
	cbw.bCDBLength = cmdlen;
	bcopy(cmd, cbw.CBWCDB, cmdlen);

	/* Send the CBW from host to device via bulk-out endpoint. */
	err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkout_pipe,
				&cbw, USB_BULK_CBW_SIZE, 0, NULL);
	if (err) {
		DPRINTF(UDMASS_BULK, ("%s: failed to send CBW\n",
		         USBDEVNAME(sc->sc_dev)));
		/* If the device detects that the CBW is invalid, then the
		 * device shall STALL both bulk endpoints and require a
		 * Bulk-Only MS Reset (hard)
		 */
		umass_bulk_reset(sc, URESET_HARD);
		usbd_clear_endpoint_stall(sc->sc_bulkout_pipe);
		return(USBD_IOERROR);
	}


	/*
	 * Data transport phase (only if there is data to be sent/received)
	 */

	if (dir == DIR_IN) {
		/* we allow short transfers for bulk-in pipes */
		err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
					data, datalen,
					USBD_SHORT_XFER_OK, &n);
		if (err)
			DPRINTF(UDMASS_BULK, ("%s: failed to receive data, "
				"(%d bytes, n = %d), %d(%s)\n", 
				USBDEVNAME(sc->sc_dev),
				datalen, n, err, usbd_errstr(err)));
	} else if (dir == DIR_OUT) {
		err = umass_usb_transfer(sc->sc_iface,
					sc->sc_bulkout_pipe,
					data, datalen, 0, &n);
		if (err)
			DPRINTF(UDMASS_BULK, ("%s: failed to send data, "
				"(%d bytes, n = %d), %d(%s)\n", 
				USBDEVNAME(sc->sc_dev),
				datalen, n, err, usbd_errstr(err)));
	}
	if (err && err != USBD_STALLED)
		return(USBD_IOERROR);


	/*
	 * Status transport phase
	 */

	/* Read the Command Status Wrapper via bulk-in endpoint. */
	err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
				&csw, USB_BULK_CSW_SIZE, 0, NULL);
	/* Try again if the bulk-in pipe was stalled */
	if (err == USBD_STALLED) {
		err = usbd_clear_endpoint_stall(sc->sc_bulkin_pipe);
		if (!err) {
			err = umass_usb_transfer(sc->sc_iface, sc->sc_bulkin_pipe,
						&csw, USB_BULK_CSW_SIZE, 0, NULL);
		}
	}
	if (err && err != USBD_STALLED)
		return(USBD_IOERROR);

	/*
	 * Check the CSW for status and validity, and check for fatal errors
	 */

	/* Invalid CSW: Wrong signature or wrong tag might indicate
	 * that the device is confused -> reset it hard, to remove
	 * all its state.
	 * Other fatal errors: STALL on read of CSW and Phase error
	 * or unknown status.
	 */
	if (err == USBD_STALLED
	    || UGETDW(csw.dCSWSignature) != CSWSIGNATURE
	    || UGETDW(csw.dCSWTag) != UGETDW(cbw.dCBWTag)
	    || csw.bCSWStatus == CSWSTATUS_PHASE
	    || csw.bCSWStatus > CSWSTATUS_PHASE) {
		if (err) {
			printf("%s: failed to read CSW, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		} else if (csw.bCSWStatus == CSWSTATUS_PHASE) {
			printf("%s: Phase Error, residue = %d, n = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(csw.dCSWDataResidue), n);
		} else if (csw.bCSWStatus > CSWSTATUS_PHASE) {
			printf("%s: Unknown status %d in CSW\n",
				USBDEVNAME(sc->sc_dev), csw.bCSWStatus);
		} else {
			printf("%s: invalid CSW, sig = 0x%08x, tag = %d (!= %d)\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(csw.dCSWSignature),
				UGETDW(csw.dCSWTag), UGETDW(cbw.dCBWTag));
		}
		umass_bulk_reset(sc, URESET_HARD);
		usbd_clear_endpoint_stall(sc->sc_bulkout_pipe);
		usbd_clear_endpoint_stall(sc->sc_bulkin_pipe);
		return(USBD_IOERROR);
	}

	if (csw.bCSWStatus == CSWSTATUS_FAILED) {
		DPRINTF(UDMASS_BULK, ("%s: Command Failed, "
			"residue = %d, n = %d\n",
			USBDEVNAME(sc->sc_dev),
			UGETDW(csw.dCSWDataResidue), n));
		umass_bulk_reset(sc, URESET_SOFT);
		*residue = UGETDW(csw.dCSWDataResidue);
		return(USBD_COMMAND_FAILED);
	}

	return(USBD_NORMAL_COMPLETION);
}





/*
 * CAM specific functions
 */

static int
umass_cam_attach(umass_softc_t *sc)
{
	struct cam_devq *devq;

	/* A HBA is attached to the CAM layer.
	 *
	 * The CAM layer will then after a while start probing for
	 * devices on the bus. The number of devices is limitted to one.
	 */

	/* SCSI transparent command set */

	devq = cam_simq_alloc(1);	/* Maximum Openings */
	if (devq == NULL)
		return(ENOMEM);
	sc->sim = cam_sim_alloc(umass_cam_action, umass_cam_poll, "umass", sc,
				device_get_unit(sc->sc_dev), 1, 0, devq);
	if (sc->sim == NULL) {
		cam_simq_free(devq);
		return(ENOMEM);
	}

	/* spec-ed sustainable xfer speed */
	cam_sim_set_basexfer_speed(sc->sim, 700/*kb/s*/);

	if(xpt_bus_register(sc->sim, 0) != CAM_SUCCESS) {
		cam_sim_free(sc->sim, 1);
		return(ENOMEM);
	}

	if (xpt_create_path(&sc->path, NULL, cam_sim_path(sc->sim),
	                    CAM_TARGET_WILDCARD,
	                    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sc->sim));
		cam_sim_free(sc->sim, 1);
		return(ENOMEM);
	}

	return(0);
}

/* umass_cam_detach
 */
static void
umass_cam_detach(umass_softc_t *sc)
{
	/* Detach from CAM layer
	 * XXX do we need to delete the device first?
	 */

	xpt_free_path(sc->path);
	xpt_bus_deregister(cam_sim_path(sc->sim));
	cam_sim_free(sc->sim, /*free_devq*/TRUE);
}


/* umass_cam_action
 * 	CAM requests for action come through here
 */

static void
umass_cam_action(struct cam_sim *sim, union ccb *ccb)
{
	umass_softc_t *sc = (umass_softc_t *) sim->softc;
	usbd_status err;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio = &ccb->csio;	/* deref union */
		int dir;
		int residue;

		DPRINTF(UDMASS_CAM, ("%s: XPT_SCSI_IO %d:%d:%d\n",
		       USBDEVNAME(sc->sc_dev),
		       ccb->ccb_h.path_id, ccb->ccb_h.target_id,
		       ccb->ccb_h.target_lun));

#ifdef UMASS_DEBUG
		if (ccb->ccb_h.target_id != UMASS_SCSIID_DEVICE
		    || ccb->ccb_h.target_lun != 0) {
			DPRINTF(UDMASS_CAM, ("%s: Wrong SCSI ID %d or LUN %d",
				USBDEVNAME(sc->sc_dev),
				ccb->ccb_h.target_id, ccb->ccb_h.target_lun));
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			return;
		}
#endif

		if (ccb->ccb_h.flags == CAM_DIR_NONE)
			dir = DIR_NONE;
		else if (ccb->ccb_h.flags & CAM_DIR_IN)
			dir = DIR_IN;
		else
			dir = DIR_OUT;

		err = umass_bulk_transfer(sc, ccb->ccb_h.target_lun,
					  csio->cdb_io.cdb_bytes, csio->cdb_len,
					  (void *)csio->data_ptr, csio->dxfer_len,
					  dir, &residue);
		/* FAILED commands are supposed to be SCSI failed commands
		 * and are therefore considered to be successfull CDW/CSW
		 * transfers. PHASE errors are more serious and should return
		 * an error to the CAM system.
		 *
		 * XXX This is however more based on empirical evidence than on
		 * hard proof from the Bulk-Only spec.
		 */
		if (!err) {
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else if (err == USBD_COMMAND_FAILED) {
			csio->resid = residue;
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else if (err) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		DPRINTF(UDMASS_CAM, ("%s: XPT_PATH_INQ\n",
			USBDEVNAME(sc->sc_dev)));

		/* information copied from vpo driver. Needs to be checked */
		cpi->version_num = 1;
		cpi->hba_inquiry = 0;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = 1;	/* one target: the drive */
		cpi->max_lun = 0;	/* no LUN's */
		cpi->initiator_id = UMASS_SCSIID_HOST;
		cpi->bus_id = cam_sim_bus(sim);
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Iomega", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);

		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:
	case XPT_RESET_DEV:
	{
		DPRINTF(UDMASS_CAM, ("%s: XPT_RESET_{BUS,DEV}\n",
			USBDEVNAME(sc->sc_dev)));

		err = umass_bulk_reset(sc, URESET_HARD);
		if (err)
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else
			ccb->ccb_h.status = CAM_REQ_CMP;

		xpt_done(ccb);
		break;
	} 
	case XPT_GET_TRAN_SETTINGS:
	{
		DPRINTF(UDMASS_CAM, ("%s: XPT_GET_TRAN_SETTINGS (XXX)\n",
			USBDEVNAME(sc->sc_dev)));
		/* XXX not implemented */

		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	case XPT_SET_TRAN_SETTINGS:
	{
		DPRINTF(UDMASS_CAM, ("%s: XPT_SET_TRAN_SETTINGS (XXX)\n",
			USBDEVNAME(sc->sc_dev)));
		/* XXX not implemented */

		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct ccb_calc_geometry *ccg = &ccb->ccg;

		DPRINTF(UDMASS_CAM, ("%s: XPT_CALC_GEOMETRY volume size %d\n",
			USBDEVNAME(sc->sc_dev), ccg->volume_size));

		/* fill in the missing fields in the struct */
		ccg->heads = 64;
		ccg->secs_per_track = 32;
		ccg->cylinders = ccg->volume_size / ccg->heads
				  / ccg->secs_per_track;

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		DPRINTF(UDMASS_CAM, ("%s: CAM function 0x%x not implemented\n",
		       USBDEVNAME(sc->sc_dev), ccb->ccb_h.func_code));
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

/* umass_cam_poll
 *	all requests are handled through umass_cam_action, requests
 *	are never pending. So, nothing to do here.
 */
static void
umass_cam_poll(struct cam_sim *sim)
{
	/* nop */
}


DRIVER_MODULE(umass, uhub, umass_driver, umass_devclass, usbd_driver_load, 0);
