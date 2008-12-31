/*-
 * Copyright (c) 2006 - 2007 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/ata/ata-usb.c,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/usb/usb_port.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

/* Command Block Wrapper */
struct bbb_cbw {
    u_int8_t	signature[4];
#define		CBWSIGNATURE		0x43425355

    u_int8_t	tag[4];
    u_int8_t	transfer_length[4];
    u_int8_t	flags;
#define		CBWFLAGS_OUT  		0x00
#define		CBWFLAGS_IN  		0x80

    u_int8_t	lun;
    u_int8_t	length;
#define		CBWCDBLENGTH     	16

    u_int8_t	cdb[CBWCDBLENGTH];
};

/* Command Status Wrapper */
struct bbb_csw {
    u_int8_t	signature[4];
#define		CSWSIGNATURE     	0x53425355

    u_int8_t	tag[4];
    u_int8_t	residue[4];
    u_int8_t    status;
#define 	CSWSTATUS_GOOD   	0x0
#define		CSWSTATUS_FAILED 	0x1
#define		CSWSTATUS_PHASE  	0x2
};

/* USB-ATA 'controller' softc */
struct atausb_softc {
    device_t		dev;		/* base device */
    usbd_interface_handle iface;	/* interface */
    int			ifaceno;	/* interface number */
    u_int8_t		bulkin;		/* endpoint address's */
    u_int8_t		bulkout;	
    u_int8_t		bulkirq;	
    usbd_pipe_handle	bulkin_pipe;	/* pipe handle's */
    usbd_pipe_handle	bulkout_pipe;
    usbd_pipe_handle	bulkirq_pipe;
    int			maxlun;
    int			timeout;
    struct ata_request	*ata_request;
    usb_device_request_t usb_request;
    struct bbb_cbw	cbw;
    struct bbb_csw	csw;

#define ATAUSB_T_BBB_CBW		0
#define ATAUSB_T_BBB_DATA		1
#define ATAUSB_T_BBB_DCLEAR		2
#define ATAUSB_T_BBB_CSW1		3
#define ATAUSB_T_BBB_CSW2		4
#define ATAUSB_T_BBB_SCLEAR		5
#define ATAUSB_T_BBB_RESET1		6
#define ATAUSB_T_BBB_RESET2		7
#define ATAUSB_T_BBB_RESET3		8
#define ATAUSB_T_MAX			9
    usbd_xfer_handle	transfer[ATAUSB_T_MAX];

    int			state;
#define	ATAUSB_S_ATTACH			0
#define	ATAUSB_S_IDLE			1
#define	ATAUSB_S_BBB_COMMAND		2
#define	ATAUSB_S_BBB_DATA		3
#define	ATAUSB_S_BBB_DCLEAR		4
#define	ATAUSB_S_BBB_STATUS1		5
#define	ATAUSB_S_BBB_SCLEAR		6
#define	ATAUSB_S_BBB_STATUS2		7
#define	ATAUSB_S_BBB_RESET1		8
#define	ATAUSB_S_BBB_RESET2		9
#define	ATAUSB_S_BBB_RESET3		10
#define	ATAUSB_S_DETACH			11

    struct mtx		locked_mtx;
    struct ata_channel	*locked_ch;
    struct ata_channel	*restart_ch;
};

static int atausbdebug = 0;

/* prototypes*/
static usbd_status atausb_start(struct atausb_softc *sc, usbd_pipe_handle pipe, void *buffer, int buflen, int flags, usbd_xfer_handle xfer);
static usbd_status atausb_ctl_start(struct atausb_softc *sc, usbd_device_handle udev, usb_device_request_t *req, void *buffer, int buflen, int flags, usbd_xfer_handle xfer);
static void atausb_clear_stall(struct atausb_softc *sc, u_int8_t endpt, usbd_pipe_handle pipe, int state, usbd_xfer_handle xfer);
static void atausb_bbb_reset(struct atausb_softc *sc);
static int atausb_bbb_start(struct ata_request *request);
static void atausb_bbb_finish(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status err);
int ata_usbchannel_begin_transaction(struct ata_request *request);
int ata_usbchannel_end_transaction(struct ata_request *request);


/*
 * USB frontend part
 */
USB_DECLARE_DRIVER(atausb);
DRIVER_MODULE(atausb, uhub, atausb_driver, atausb_devclass, 0, 0);
MODULE_VERSION(atausb, 1);

static int
atausb_match(device_t dev)
{
    struct usb_attach_arg *uaa = device_get_ivars(dev);
    usb_interface_descriptor_t *id;

    if (uaa->iface == NULL)
	return UMATCH_NONE;

    id = usbd_get_interface_descriptor(uaa->iface);
    if (!id || id->bInterfaceClass != UICLASS_MASS)
	return UMATCH_NONE;

    switch (id->bInterfaceSubClass) {
    case UISUBCLASS_QIC157:
    case UISUBCLASS_RBC:
    case UISUBCLASS_SCSI:
    case UISUBCLASS_SFF8020I:
    case UISUBCLASS_SFF8070I:
    case UISUBCLASS_UFI:
	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_CBI:
	case UIPROTO_MASS_CBI_I:
	case UIPROTO_MASS_BBB:
	case UIPROTO_MASS_BBB_OLD:
	    return UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO;
	default:
	    return UMATCH_IFACECLASS_IFACESUBCLASS;
	}
	break;
    default:
	return UMATCH_IFACECLASS;
    }
}

static int
atausb_attach(device_t dev)
{
    struct atausb_softc *sc = device_get_softc(dev);
    struct usb_attach_arg *uaa = device_get_ivars(dev);
    usb_interface_descriptor_t *id;
    usb_endpoint_descriptor_t *ed;
    usbd_device_handle udev;
    usb_device_request_t request;
    char devinfo[1024], *proto, *subclass;
    u_int8_t maxlun;
    int err, i;

    sc->dev = dev;
    usbd_devinfo(uaa->device, 0, devinfo);
    device_set_desc_copy(dev, devinfo);
    sc->bulkin = sc->bulkout = sc->bulkirq = -1;
    sc->bulkin_pipe = sc->bulkout_pipe= sc->bulkirq_pipe = NULL;
    sc->iface = uaa->iface;
    sc->ifaceno = uaa->ifaceno;
    sc->maxlun = 0;
    sc->timeout = 5000;
    sc->locked_ch = NULL;
    sc->restart_ch = NULL;
    mtx_init(&sc->locked_mtx, "ATAUSB lock", NULL, MTX_DEF); 

    id = usbd_get_interface_descriptor(sc->iface);
    switch (id->bInterfaceProtocol) {
    case UIPROTO_MASS_BBB:
    case UIPROTO_MASS_BBB_OLD:
	    proto = "Bulk-Only";
	    break;
    case UIPROTO_MASS_CBI:
	    proto = "CBI";
	    break;
    case UIPROTO_MASS_CBI_I:
	    proto = "CBI with CCI";
	    break;
    default:
	    proto = "Unknown";
    }
    switch (id->bInterfaceSubClass) {
    case UISUBCLASS_RBC:
	    subclass = "RBC";
	    break;
    case UISUBCLASS_QIC157:
    case UISUBCLASS_SFF8020I:
    case UISUBCLASS_SFF8070I:
	    subclass = "ATAPI";
	    break;
    case UISUBCLASS_SCSI:
	    subclass = "SCSI";
	    break;
    case UISUBCLASS_UFI:
	    subclass = "UFI";
	    break;
    default:
	    subclass = "Unknown";
    }
    device_printf(dev, "using %s over %s\n", subclass, proto);
    if (strcmp(proto, "Bulk-Only") ||
	(strcmp(subclass, "ATAPI") && strcmp(subclass, "SCSI")))
	return ENXIO;

    for (i = 0 ; i < id->bNumEndpoints ; i++) {
	if (!(ed = usbd_interface2endpoint_descriptor(sc->iface, i))) {
	    device_printf(sc->dev, "could not read endpoint descriptor\n");
	    return ENXIO;
	}
	if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
	    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
	    sc->bulkin = ed->bEndpointAddress;
	}
	if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
	           (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
	    sc->bulkout = ed->bEndpointAddress;
	}
	if (id->bInterfaceProtocol == UIPROTO_MASS_CBI_I &&
	           UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
	           (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
	    sc->bulkirq = ed->bEndpointAddress;
	}
    }

    /* check whether we found at least the endpoints we need */
    if (!sc->bulkin || !sc->bulkout) {
	device_printf(sc->dev, "needed endpoints not found (%d,%d)\n",
		      sc->bulkin, sc->bulkout);
	atausb_detach(dev);
	return ENXIO;
    }

    /* open the pipes */
    if (usbd_open_pipe(sc->iface, sc->bulkout,
		       USBD_EXCLUSIVE_USE, &sc->bulkout_pipe)) {
	device_printf(sc->dev, "cannot open bulkout pipe (%d)\n", sc->bulkout);
	atausb_detach(dev);
	return ENXIO;
    }
    if (usbd_open_pipe(sc->iface, sc->bulkin,
		       USBD_EXCLUSIVE_USE, &sc->bulkin_pipe)) {
	device_printf(sc->dev, "cannot open bulkin pipe (%d)\n", sc->bulkin);
	atausb_detach(dev);
	return ENXIO;
    }
    if (id->bInterfaceProtocol == UIPROTO_MASS_CBI_I) {
	if (usbd_open_pipe(sc->iface, sc->bulkirq,
			   USBD_EXCLUSIVE_USE, &sc->bulkirq_pipe)) {
	    device_printf(sc->dev, "cannot open bulkirq pipe (%d)\n",
	    		  sc->bulkirq);
	    atausb_detach(dev);
	    return ENXIO;
	}
    }
    sc->state = ATAUSB_S_ATTACH;

    /* alloc needed number of transfer handles */
    for (i = 0; i < ATAUSB_T_MAX; i++) {
	sc->transfer[i] = usbd_alloc_xfer(uaa->device);
	if (!sc->transfer[i]) {
	    device_printf(sc->dev, "out of memory\n");
	    atausb_detach(dev);
	    return ENXIO;
	}
    }

    /* driver is ready to process requests here */
    sc->state = ATAUSB_S_IDLE;

    /* get number of devices so we can add matching channels */
    usbd_interface2device_handle(sc->iface, &udev);
    request.bmRequestType = UT_READ_CLASS_INTERFACE;
    request.bRequest = 0xfe; //GET_MAX_LUN;
    USETW(request.wValue, 0);
    USETW(request.wIndex, sc->ifaceno);
    USETW(request.wLength, sizeof(maxlun));
    switch ((err = usbd_do_request(udev, &request, &maxlun))) {
    case USBD_NORMAL_COMPLETION:
	if (bootverbose)
	    device_printf(sc->dev, "maxlun=%d\n", maxlun);
	sc->maxlun = maxlun;
	break;
    default:
	if (bootverbose)
	    device_printf(sc->dev, "get maxlun not supported %s\n",
	usbd_errstr(err));
    }

    /* ata channels are children to this USB control device */
    for (i = 0; i <= sc->maxlun; i++) {
	if (!device_add_child(sc->dev, "ata",
			      devclass_find_free_unit(ata_devclass, 2))) {
	    device_printf(sc->dev, "failed to attach ata child device\n");
	    atausb_detach(dev);
	    return ENXIO;
	}
    }
    bus_generic_attach(sc->dev);
    return 0;
}

static int
atausb_detach(device_t dev)
{
    struct atausb_softc *sc = device_get_softc(dev);
    usbd_device_handle udev;
    device_t *children;
    int nchildren, i;

    /* signal that device is going away */
    sc->state = ATAUSB_S_DETACH;

    /* abort all the pipes in case there are active transfers */
    usbd_interface2device_handle(sc->iface, &udev);
    usbd_abort_default_pipe(udev);
    if (sc->bulkout_pipe)
        usbd_abort_pipe(sc->bulkout_pipe);
    if (sc->bulkin_pipe)
        usbd_abort_pipe(sc->bulkin_pipe);
    if (sc->bulkirq_pipe)
        usbd_abort_pipe(sc->bulkirq_pipe);

    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
        for (i = 0; i < nchildren; i++)
            device_delete_child(dev, children[i]);
        free(children, M_TEMP);
    }

    /* free the transfers */
    for (i = 0; i < ATAUSB_T_MAX; i++)
	if (sc->transfer[i])
	    usbd_free_xfer(sc->transfer[i]);

    /* remove all the pipes */
    if (sc->bulkout_pipe)
        usbd_close_pipe(sc->bulkout_pipe);
    if (sc->bulkin_pipe)
        usbd_close_pipe(sc->bulkin_pipe);
    if (sc->bulkirq_pipe)
        usbd_close_pipe(sc->bulkirq_pipe);

    mtx_destroy(&sc->locked_mtx);
    return 0;
}


/*
 * Generic USB transfer routines 
 */
static usbd_status
atausb_start(struct atausb_softc *sc, usbd_pipe_handle pipe,
	     void *buffer, int buflen, int flags, usbd_xfer_handle xfer)
{
    usbd_status err;

    if (sc->state == ATAUSB_S_DETACH)
	return USBD_NOT_STARTED;

    usbd_setup_xfer(xfer, pipe, (void *)sc, buffer, buflen, flags,
		    sc->timeout, atausb_bbb_finish);
    err = usbd_transfer(xfer);
    if (err && (err != USBD_IN_PROGRESS)) {
	if (atausbdebug)
	    device_printf(sc->dev, "failed to setup transfer, %s\n",
		          usbd_errstr(err));
	return err;
    }
    return USBD_NORMAL_COMPLETION;
}

static usbd_status
atausb_ctl_start(struct atausb_softc *sc, usbd_device_handle udev,
	 	 usb_device_request_t *req, void *buffer, int buflen, int flags,
 		 usbd_xfer_handle xfer)
{
    usbd_status err;

    if (sc->state == ATAUSB_S_DETACH)
	return USBD_NOT_STARTED;

    usbd_setup_default_xfer(xfer, udev, (void *)sc, sc->timeout, req,
   			    buffer, buflen, flags, atausb_bbb_finish);
    err = usbd_transfer(xfer);
    if (err && (err != USBD_IN_PROGRESS)) {
	if (atausbdebug)
	    device_printf(sc->dev, "failed to setup ctl transfer, %s\n",
	                  usbd_errstr(err));
	return err;
    }
    return USBD_NORMAL_COMPLETION;
}

static void
atausb_clear_stall(struct atausb_softc *sc, u_int8_t endpt,
		   usbd_pipe_handle pipe, int state, usbd_xfer_handle xfer)
{
    usbd_device_handle udev;

    if (atausbdebug)
	device_printf(sc->dev, "clear endpoint 0x%02x stall\n", endpt);
    usbd_interface2device_handle(sc->iface, &udev);
    sc->state = state;
    usbd_clear_endpoint_toggle(pipe);
    sc->usb_request.bmRequestType = UT_WRITE_ENDPOINT;
    sc->usb_request.bRequest = UR_CLEAR_FEATURE;
    USETW(sc->usb_request.wValue, UF_ENDPOINT_HALT);
    USETW(sc->usb_request.wIndex, endpt);
    USETW(sc->usb_request.wLength, 0);
    atausb_ctl_start(sc, udev, &sc->usb_request, NULL, 0, 0, xfer);
}


/*
 * Bulk-Only transport part
 */
static void
atausb_bbb_reset(struct atausb_softc *sc)
{
    usbd_device_handle udev;

    if (atausbdebug)
	device_printf(sc->dev, "Bulk Reset\n");
    sc->timeout = 5000;
    sc->state = ATAUSB_S_BBB_RESET1;
    usbd_interface2device_handle(sc->iface, &udev);
    sc->usb_request.bmRequestType = UT_WRITE_CLASS_INTERFACE;
    sc->usb_request.bRequest = 0xff; /* bulk-only reset */
    USETW(sc->usb_request.wValue, 0);
    USETW(sc->usb_request.wIndex, sc->ifaceno);
    USETW(sc->usb_request.wLength, 0);
    atausb_ctl_start(sc, udev, &sc->usb_request, NULL,
    		     0, 0, sc->transfer[ATAUSB_T_BBB_RESET1]);
}

static int
atausb_bbb_start(struct ata_request *request)
{
    struct atausb_softc *sc = 
	device_get_softc(device_get_parent(request->parent));
    struct ata_channel *ch = device_get_softc(request->parent);

    sc->timeout = (request->timeout * 1000) + 5000;
    USETDW(sc->cbw.signature, CBWSIGNATURE);
    USETDW(sc->cbw.tag, UGETDW(sc->cbw.tag) + 1);
    USETDW(sc->cbw.transfer_length, request->bytecount);
    sc->cbw.flags = (request->flags & ATA_R_READ) ? CBWFLAGS_IN : CBWFLAGS_OUT;
    sc->cbw.lun = ch->unit;
    sc->cbw.length = 16;
    bzero(sc->cbw.cdb, 16);
    bcopy(request->u.atapi.ccb, sc->cbw.cdb, 12); /* XXX SOS */
    sc->state = ATAUSB_S_BBB_COMMAND;
    if (atausb_start(sc, sc->bulkout_pipe, &sc->cbw, sizeof(struct bbb_cbw),
		     0, sc->transfer[ATAUSB_T_BBB_CBW])) {
	request->result = EIO;
        if (atausbdebug)
	    device_printf(request->dev, "cannot setup USB transfer\n");
        atausb_bbb_reset(sc);
	return ATA_OP_FINISHED;
    }
    return ATA_OP_CONTINUES;
}

static void
atausb_bbb_finish(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status err)
{
    struct atausb_softc *sc = (struct atausb_softc *)priv;
    struct ata_request *request = sc->ata_request;
    usbd_xfer_handle next_xfer;

    //device_printf(sc->dev, "BBB state %d: %s\n", sc->state, usbd_errstr(err));

    if (sc->state == ATAUSB_S_DETACH) {
        device_printf(sc->dev, "WARNING - device has been removed\n");
	return;
    }

    switch (sc->state) {
    case ATAUSB_S_BBB_COMMAND:	/* command transport phase */
	if (err) {
            if (atausbdebug)
	        device_printf(sc->dev, "failed to send CBW\n");
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}

	/* next is data transport phase, setup transfer */
	sc->state = ATAUSB_S_BBB_DATA;
	if (request->flags & ATA_R_READ) {
	    if (atausb_start(sc, sc->bulkin_pipe,
			     request->data, request->bytecount,
			     USBD_SHORT_XFER_OK,
			     sc->transfer[ATAUSB_T_BBB_DATA])) {
	        request->result = EIO;
		atausb_bbb_reset(sc);
	    }
	    return;
	}
	if (request->flags & ATA_R_WRITE) {
	    if (atausb_start(sc, sc->bulkout_pipe,
			     request->data, request->bytecount,
			     0, sc->transfer[ATAUSB_T_BBB_DATA])) {
	        request->result = EIO;
		atausb_bbb_reset(sc);
	    }
	    return;
	}
	/* FALLTHROUGH */

    case ATAUSB_S_BBB_DATA:	/* data transport phase */
	if (request->flags & (ATA_R_READ | ATA_R_WRITE)) {
	    usbd_get_xfer_status(xfer, NULL, NULL, &request->donecount, NULL);
	    if (err) {
                if (atausbdebug)
		    device_printf(sc->dev, "data %s count %d failed: %s\n",
		       	          (request->flags & ATA_R_READ?"read":"write"),
		                  request->bytecount, usbd_errstr(err));
		if (err == USBD_STALLED) {
		    atausb_clear_stall(sc,
				       (request->flags & ATA_R_READ ?
					sc->bulkin : sc->bulkout),
			  	       (request->flags & ATA_R_READ ?
			   	        sc->bulkin_pipe : sc->bulkout_pipe),
			  	       ATAUSB_S_BBB_DCLEAR,
			  	       sc->transfer[ATAUSB_T_BBB_DCLEAR]);
		}
		else {
	            request->result = EIO;
		    atausb_bbb_reset(sc);
		}
		return;
	    }
	}
	/* FALLTHROUGH */

    case ATAUSB_S_BBB_DCLEAR:	/* stall clear after data phase */
    case ATAUSB_S_BBB_SCLEAR:	/* stall clear after status phase */
	if (err) {
            if (atausbdebug)
	        device_printf(sc->dev, "bulk%s stall clear failed %s\n",
		   	      (request->flags & ATA_R_READ ? "in" : "out"),
		   	      usbd_errstr(err));
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}

	if (sc->state == ATAUSB_S_BBB_COMMAND ||
	    sc->state == ATAUSB_S_BBB_DATA ||
	    sc->state == ATAUSB_S_BBB_DCLEAR) {
	    /* first attempt on status transport phase setup transfer */
	    sc->state = ATAUSB_S_BBB_STATUS1;
	    next_xfer = sc->transfer[ATAUSB_T_BBB_CSW1];
	}
	else {
	    /* second attempt of fetching status */
	    sc->state = ATAUSB_S_BBB_STATUS2;
	    next_xfer = sc->transfer[ATAUSB_T_BBB_CSW2];
	}
	if (atausb_start(sc, sc->bulkin_pipe, &sc->csw, sizeof(struct bbb_csw),
			 USBD_SHORT_XFER_OK, next_xfer)) {
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	}
	return;

    case ATAUSB_S_BBB_STATUS1:	/* status transfer first attempt */
    case ATAUSB_S_BBB_STATUS2:	/* status transfer second attempt */
	if (err) {
            if (atausbdebug)
	        device_printf(sc->dev, "cannot get CSW, %s%s\n",
			      usbd_errstr(err),
		   	      sc->state == ATAUSB_S_BBB_STATUS1 ? ", retry":"");
	    if (sc->state == ATAUSB_S_BBB_STATUS1) {
		atausb_clear_stall(sc, sc->bulkin, sc->bulkin_pipe,
				   ATAUSB_S_BBB_SCLEAR,
				   sc->transfer[ATAUSB_T_BBB_SCLEAR]);
	    }
	    else {
	        request->result = EIO;
		atausb_bbb_reset(sc);
	    }
	    return;
	}

	int residue = UGETDW(sc->csw.residue);

	if (!residue &&
	    (request->bytecount - request->donecount))
	    residue = request->bytecount - request->donecount;

	/* check CSW and handle eventual error */
	if (UGETDW(sc->csw.signature) != CSWSIGNATURE) {
            if (atausbdebug)
	        device_printf(sc->dev, "bad CSW signature 0x%08x != 0x%08x\n",
		              UGETDW(sc->csw.signature), CSWSIGNATURE);
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}
	else if (UGETDW(sc->csw.tag) != UGETDW(sc->cbw.tag)) {
            if (atausbdebug)
	        device_printf(sc->dev, "bad CSW tag %d != %d\n",
		              UGETDW(sc->csw.tag), UGETDW(sc->cbw.tag));
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}
	else if (sc->csw.status > CSWSTATUS_PHASE) {
            if (atausbdebug)
	        device_printf(sc->dev, "bad CSW status %d > %d\n",
		    	      sc->csw.status, CSWSTATUS_PHASE);
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}
	else if (sc->csw.status == CSWSTATUS_PHASE) {
            if (atausbdebug)
	        device_printf(sc->dev, "phase error residue = %d\n", residue);
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}
	else if (request->donecount > request->bytecount) {
            if (atausbdebug)
	        device_printf(sc->dev, "buffer overrun %d > %d",
		             request->donecount, request->bytecount);
	    request->result = EIO;
	    atausb_bbb_reset(sc);
	    return;
	}
	else if (sc->csw.status == CSWSTATUS_FAILED) {
            if (atausbdebug)
	        device_printf(sc->dev, "CSWSTATUS_FAILED\n");
	    request->error = ATA_E_ATAPI_SENSE_MASK ;
	    sc->state = ATAUSB_S_IDLE;
	    ata_interrupt(device_get_softc(request->parent));
	    return;
	}
	else {
	    sc->state = ATAUSB_S_IDLE;
	    ata_interrupt(device_get_softc(request->parent));
	    return;
	}
	/* NOT REACHED */

    case ATAUSB_S_BBB_RESET1:
	if (err)
            if (atausbdebug)
	        device_printf(sc->dev,
			      "BBB reset failure: %s\n", usbd_errstr(err));
	atausb_clear_stall(sc, sc->bulkin, sc->bulkin_pipe,
			   ATAUSB_S_BBB_RESET2,
			   sc->transfer[ATAUSB_T_BBB_RESET2]);
	return;

    case ATAUSB_S_BBB_RESET2:
	if (err)
            if (atausbdebug)
	        device_printf(sc->dev, "BBB bulkin clear stall failure: %s\n",
		  	      usbd_errstr(err));
	atausb_clear_stall(sc, sc->bulkout, sc->bulkout_pipe,
			   ATAUSB_S_BBB_RESET3,
			   sc->transfer[ATAUSB_T_BBB_RESET3]);
	return;

    case ATAUSB_S_BBB_RESET3:
	if (err)
            if (atausbdebug)
	        device_printf(sc->dev, "BBB bulk-out clear stall failure: %s\n",
		   	      usbd_errstr(err));
	sc->state = ATAUSB_S_IDLE;
	if (request) {
	    if (err)
	        request->result = ENXIO;
	    else
	        request->result = EIO;
	    ata_interrupt(device_get_softc(request->parent));
	}
	return;

    default:
        if (atausbdebug)
	    device_printf(sc->dev, "unknown state %d", sc->state);
    }
}


/*
 * ATA backend part
 */
struct atapi_inquiry {
    u_int8_t    device_type;
    u_int8_t    device_modifier;
    u_int8_t    version;
    u_int8_t    response_format;
    u_int8_t    length;
    u_int8_t    reserved[2];
    u_int8_t    flags;
    u_int8_t    vendor[8];
    u_int8_t    product[16];
    u_int8_t    revision[4];
    //u_int8_t    crap[60];
};

int
ata_usbchannel_begin_transaction(struct ata_request *request)
{
    struct atausb_softc *sc = 
    	device_get_softc(device_get_parent(request->parent));

    if (atausbdebug > 1)
	device_printf(request->dev, "begin_transaction %s\n",
		      ata_cmd2str(request));

    /* sanity just in case */
    if (sc->state != ATAUSB_S_IDLE) {
	printf("begin is busy (%d)\n", sc->state);
	request->result = EBUSY;
	return ATA_OP_FINISHED;
    }

    /* XXX SOS convert the request into the format used, only BBB for now*/
    sc->ata_request = request;

    /* ATA/ATAPI IDENTIFY needs special treatment */
    if (!(request->flags & ATA_R_ATAPI)) {
	if (request->u.ata.command != ATA_ATAPI_IDENTIFY) {
	    device_printf(request->dev,"%s unsupported\n",ata_cmd2str(request));
	    request->result = EIO;
	    return ATA_OP_FINISHED;
	}
	request->flags |= ATA_R_ATAPI;
	bzero(request->u.atapi.ccb, 16);
	request->u.atapi.ccb[0] = ATAPI_INQUIRY;
	request->u.atapi.ccb[4] =  255; //sizeof(struct atapi_inquiry);
	request->data += 256;	/* arbitrary offset into ata_param */
	request->bytecount = 255; //sizeof(struct atapi_inquiry);
    }
    return atausb_bbb_start(request);
}

int
ata_usbchannel_end_transaction(struct ata_request *request)
{
    if (atausbdebug > 1)
	device_printf(request->dev, "end_transaction %s\n",
		      ata_cmd2str(request));
    
    /* XXX SOS convert the request from the format used, only BBB for now*/

    /* ATA/ATAPI IDENTIFY needs special treatment */
    if ((request->flags & ATA_R_ATAPI) &&
	(request->u.atapi.ccb[0] == ATAPI_INQUIRY)) {
	struct ata_device *atadev = device_get_softc(request->dev);
	struct atapi_inquiry *inquiry = (struct atapi_inquiry *)request->data;
	u_int16_t *ptr;

	/* convert inquiry data into simple ata_param like format */
	atadev->param.config = ATA_PROTO_ATAPI | ATA_PROTO_ATAPI_12;
	atadev->param.config |= (inquiry->device_type & 0x1f) << 8;
	bzero(atadev->param.model, sizeof(atadev->param.model));
	strncpy(atadev->param.model, inquiry->vendor, 8);
	strcpy(atadev->param.model, "  ");
	strncpy(atadev->param.model, inquiry->product, 16);
	ptr = (u_int16_t*)(atadev->param.model + sizeof(atadev->param.model));
	while (--ptr >= (u_int16_t*)atadev->param.model)
	    *ptr = ntohs(*ptr);
	strncpy(atadev->param.revision, inquiry->revision, 4);
	ptr=(u_int16_t*)(atadev->param.revision+sizeof(atadev->param.revision));
	while (--ptr >= (u_int16_t*)atadev->param.revision)
	    *ptr = ntohs(*ptr);
	request->result = 0;
    }
    return ATA_OP_FINISHED;
}

static int
ata_usbchannel_probe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int count, i;
    char buffer[32];

    /* take care of green memory */
    bzero(ch, sizeof(struct ata_channel));

    /* find channel number on this controller */
    device_get_children(device_get_parent(dev), &children, &count);
    for (i = 0; i < count; i++) {
        if (children[i] == dev)
            ch->unit = i;
    }
    free(children, M_TEMP);

    sprintf(buffer, "USB lun %d", ch->unit);
    device_set_desc_copy(dev, buffer);

    return 0;
}

static int
ata_usbchannel_attach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    /* initialize the softc basics */
    ch->dev = dev;
    ch->state = ATA_IDLE;
    ch->hw.begin_transaction = ata_usbchannel_begin_transaction;
    ch->hw.end_transaction = ata_usbchannel_end_transaction;
    ch->hw.status = NULL;
    ch->hw.command = NULL;
    bzero(&ch->state_mtx, sizeof(struct mtx));
    mtx_init(&ch->state_mtx, "ATA state lock", NULL, MTX_DEF);
    bzero(&ch->queue_mtx, sizeof(struct mtx));
    mtx_init(&ch->queue_mtx, "ATA queue lock", NULL, MTX_DEF);
    TAILQ_INIT(&ch->ata_queue);

    /* XXX SOS reset the controller HW, the channel and device(s) */
    //ATA_RESET(dev);

    /* probe and attach device on this channel */
    ch->devices = ATA_ATAPI_MASTER;
    if (!ata_delayed_attach)
        ata_identify(dev);
    return 0;
}

static int
ata_usbchannel_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int nchildren, i;

    /* detach & delete all children */
    if (!device_get_children(dev, &children, &nchildren)) {
        for (i = 0; i < nchildren; i++)
            if (children[i])
                device_delete_child(dev, children[i]);
        free(children, M_TEMP);
    }
    mtx_destroy(&ch->state_mtx);
    mtx_destroy(&ch->queue_mtx);
    return 0;
}

static void
ata_usbchannel_setmode(device_t parent, device_t dev)
{
    struct atausb_softc *sc = device_get_softc(GRANDPARENT(dev));
    struct ata_device *atadev = device_get_softc(dev);
    usbd_device_handle udev;

    usbd_interface2device_handle(sc->iface, &udev);
    if (usbd_get_speed(udev) == USB_SPEED_HIGH)
	atadev->mode = ATA_USB2;
    else
	atadev->mode = ATA_USB1;
}

static int
ata_usbchannel_locking(device_t dev, int flags)
{
    struct atausb_softc *sc = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int res = -1;


    mtx_lock(&sc->locked_mtx);
    switch (flags) {
    case ATA_LF_LOCK:
	if (sc->locked_ch == NULL)
	    sc->locked_ch = ch;
	if (sc->locked_ch != ch)
	    sc->restart_ch = ch;
	break;

    case ATA_LF_UNLOCK:
	if (sc->locked_ch == ch) {
	    sc->locked_ch = NULL;
	    if (sc->restart_ch) {
		ch = sc->restart_ch;
		sc->restart_ch = NULL;
		mtx_unlock(&sc->locked_mtx);
		ata_start(ch->dev);
		return res;
	    }
	}
	break;

    case ATA_LF_WHICH:
	break;
    }
    if (sc->locked_ch)
	res = sc->locked_ch->unit;
    mtx_unlock(&sc->locked_mtx);
    return res;
}

static device_method_t ata_usbchannel_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,         ata_usbchannel_probe),
    DEVMETHOD(device_attach,        ata_usbchannel_attach),
    DEVMETHOD(device_detach,        ata_usbchannel_detach),

    /* ATA methods */
    DEVMETHOD(ata_setmode,        ata_usbchannel_setmode),
    DEVMETHOD(ata_locking,        ata_usbchannel_locking),
    //DEVMETHOD(ata_reset,        ata_usbchannel_reset),

    { 0, 0 }
};

static driver_t ata_usbchannel_driver = {
    "ata",
    ata_usbchannel_methods,
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, atausb, ata_usbchannel_driver, ata_devclass, 0, 0);
MODULE_DEPEND(atausb, ata, 1, 1, 1);
