/*	$NetBSD: umodem.c,v 1.45 2002/09/23 05:51:23 simonb Exp $	*/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
 * Copyright (c) 2005, Takanori Watanabe
 * Copyright (c) 2003, M. Warner Losh <imp@freebsd.org>.
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Comm Class spec:  http://www.usb.org/developers/devclass_docs/usbccs10.pdf
 *                   http://www.usb.org/developers/devclass_docs/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/serial.h>
#include <sys/tty.h>
#include <sys/clist.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/sbuf.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/taskqueue.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#include "usbdevs.h"

typedef struct ufoma_mobile_acm_descriptor{
	uByte bFunctionLength;
	uByte bDescriptorType;
	uByte bDescriptorSubtype;
	uByte bType;
	uByte bMode[1];
}usb_mcpc_acm_descriptor;

#define UISUBCLASS_MCPC 0x88

#define UDESC_VS_INTERFACE 0x44
#define UDESCSUB_MCPC_ACM  0x11

#define UMCPC_ACM_TYPE_AB1 0x1
#define UMCPC_ACM_TYPE_AB2 0x2
#define UMCPC_ACM_TYPE_AB5 0x5
#define UMCPC_ACM_TYPE_AB6 0x6

#define UMCPC_ACM_MODE_DEACTIVATED 0x0
#define UMCPC_ACM_MODE_MODEM 0x1
#define UMCPC_ACM_MODE_ATCOMMAND 0x2
#define UMCPC_ACM_MODE_OBEX 0x60
#define UMCPC_ACM_MODE_VENDOR1 0xc0
#define UMCPC_ACM_MODE_VENDOR2 0xfe
#define UMCPC_ACM_MODE_UNLINKED 0xff

#define UMCPC_CM_MOBILE_ACM 0x0

#define UMCPC_ACTIVATE_MODE 0x60
#define UMCPC_GET_MODETABLE 0x61
#define UMCPC_SET_LINK 0x62
#define UMCPC_CLEAR_LINK 0x63

#define UMCPC_REQUEST_ACKNOLEDGE 0x31

#define UFOMA_MAX_TIMEOUT 15 /*Standard says 10(sec)*/
#define UFOMA_CMD_BUF_SIZE 64

#define UMODEMIBUFSIZE 64
#define UMODEMOBUFSIZE 256
#define DPRINTF(a)

struct ufoma_softc{
	struct ucom_softc sc_ucom;
	int sc_is_ucom;
	int sc_isopen;

	struct mtx sc_mtx;
	int sc_ctl_iface_no;
	usbd_interface_handle sc_ctl_iface;
	usbd_interface_handle	sc_data_iface;
	int sc_data_iface_no;
	int sc_cm_cap;
	int sc_acm_cap;
	usb_cdc_line_state_t	sc_line_state;	/* current line state */
	u_char			sc_dtr;		/* current DTR state */
	u_char			sc_rts;		/* current RTS state */

	usbd_pipe_handle sc_notify_pipe;
	usb_cdc_notification_t sc_notify_buf;
	u_char sc_lsr;
	u_char sc_msr;
	
	struct task sc_task;
	uByte *sc_modetable;
	uByte sc_modetoactivate;
	uByte sc_currentmode;
	char sc_resbuffer[UFOMA_CMD_BUF_SIZE+1];
	int sc_cmdbp;
	int sc_nummsg;
	usbd_xfer_handle sc_msgxf;
};
static usbd_status
ufoma_set_line_coding(struct ufoma_softc *sc, usb_cdc_line_state_t *state);
static int ufoma_match(device_t);
static int ufoma_attach(device_t);
static int ufoma_detach(device_t);
static void *ufoma_get_intconf(usb_config_descriptor_t *cd, usb_interface_descriptor_t *id,int type, int subtype);
static void ufoma_notify(void * ,int count);
static void ufoma_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
static char *ufoma_mode_to_str(int);
static int ufoma_str_to_mode(char *);

/*Pseudo ucom stuff*/
static int ufoma_init_pseudo_ucom(struct ufoma_softc *);
static t_open_t ufomaopen;
static t_close_t ufomaclose;
static t_oproc_t ufomastart;

/*umodem like stuff*/
static int ufoma_init_modem(struct ufoma_softc *, struct usb_attach_arg *);
static void ufoma_get_status(void *, int portno, u_char *lst, u_char *msr);
static void ufoma_set(void *, int portno, int reg, int onoff);
static int ufoma_param(void *, int portno, struct termios *);
static int ufoma_ucom_open(void *, int portno);
static void ufoma_ucom_close(void *, int portno);
static void ufoma_break(struct ufoma_softc *sc, int onoff);
static void ufoma_dtr(struct ufoma_softc *sc, int onoff);
static void ufoma_rts(struct ufoma_softc *sc, int onoff);

/*sysctl stuff*/
static int ufoma_sysctl_support(SYSCTL_HANDLER_ARGS);
static int ufoma_sysctl_current(SYSCTL_HANDLER_ARGS);
static int ufoma_sysctl_open(SYSCTL_HANDLER_ARGS);


static struct ucom_callback ufoma_callback = {
	ufoma_get_status,
	ufoma_set,
	ufoma_param,
	NULL,
	ufoma_ucom_open,
	ufoma_ucom_close,
	NULL,
	NULL,
};


static device_method_t ufoma_methods[] = { 
	/**/
	DEVMETHOD(device_probe, ufoma_match),
	DEVMETHOD(device_attach, ufoma_attach),
	DEVMETHOD(device_detach, ufoma_detach),
	{0, 0}
};
struct umcpc_modetostr_tab{
	int mode;
	char *str;
}umcpc_modetostr_tab[]={
	{UMCPC_ACM_MODE_DEACTIVATED, "deactivated"},
	{UMCPC_ACM_MODE_MODEM, "modem"},
	{UMCPC_ACM_MODE_ATCOMMAND, "handsfree"},
	{UMCPC_ACM_MODE_OBEX, "obex"},
	{UMCPC_ACM_MODE_VENDOR1, "vendor1"},
	{UMCPC_ACM_MODE_VENDOR2, "vendor2"},
	{UMCPC_ACM_MODE_UNLINKED, "unlinked"},
	{0, NULL}
};

static driver_t ufoma_driver = {
	"ucom",
	ufoma_methods,
	sizeof(struct ufoma_softc)
};


DRIVER_MODULE(ufoma, uhub, ufoma_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(ufoma, usb, 1, 1, 1);
MODULE_DEPEND(ufoma, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);

static int
ufoma_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usb_interface_descriptor_t *id;
	usb_config_descriptor_t *cd;
	usb_mcpc_acm_descriptor *mad;
	int ret;

	ret = UMATCH_NONE;

	if(uaa->iface == NULL)
		return(UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	cd = usbd_get_config_descriptor(uaa->device);

	if(id == NULL || cd == NULL)
		return (UMATCH_NONE);
	
	if( id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass == UISUBCLASS_MCPC){
		ret = (UMATCH_IFACECLASS_IFACESUBCLASS);
	}else{
		return UMATCH_NONE;
	}

	mad = ufoma_get_intconf(cd, id , UDESC_VS_INTERFACE, UDESCSUB_MCPC_ACM);
	if(mad == NULL){
		return (UMATCH_NONE);
	}

#if 0
	if(mad->bType != UMCPC_ACM_TYPE_AB5){
		return UMATCH_NONE;
	}
#endif
	return ret;
}
			
static int
ufoma_attach(device_t self)
{
	struct ufoma_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle dev = uaa->device;
	usb_config_descriptor_t *cd;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usb_mcpc_acm_descriptor *mad;
	struct ucom_softc *ucom = &sc->sc_ucom;
	const char *devname,*modename;
	int ctl_notify;
	int i,err;
	int elements;
	uByte *mode;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	
	ucom->sc_dev = self;
	ucom->sc_udev = dev;
	sc->sc_ctl_iface = uaa->iface;
	mtx_init(&sc->sc_mtx, "ufoma", NULL, MTX_DEF);	

	cd = usbd_get_config_descriptor(ucom->sc_udev);
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);
	sc->sc_ctl_iface_no = id->bInterfaceNumber;
	
	devname = device_get_nameunit(self);
	device_printf(self, "iclass %d/%d ifno:%d\n",
	    id->bInterfaceClass, id->bInterfaceSubClass, sc->sc_ctl_iface_no);

	ctl_notify = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctl_iface, i);
		if (ed == NULL)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			ctl_notify = ed->bEndpointAddress;
		}
	}

	if(ctl_notify== -1){
		/*NOTIFY is mandatory.*/
		printf("NOTIFY interface not found\n");
		goto error;
	}

	err = usbd_open_pipe_intr(sc->sc_ctl_iface, ctl_notify, 
	    USBD_SHORT_XFER_OK, &sc->sc_notify_pipe, sc, &sc->sc_notify_buf,
	    sizeof(sc->sc_notify_buf), ufoma_intr, USBD_DEFAULT_INTERVAL);
	if(err){
		printf("PIPE open error %d\n", err);
		goto error;
	}
	mad = ufoma_get_intconf(cd, id , UDESC_VS_INTERFACE, UDESCSUB_MCPC_ACM);
	if(mad ==NULL){
		goto error;
	}

	printf("%s:Supported Mode:", devname);
	for(mode = mad->bMode; 
	    mode < ((uByte *)mad + mad->bFunctionLength); mode++){
		modename = ufoma_mode_to_str(*mode);
		if(modename){
			printf("%s", ufoma_mode_to_str(*mode));
		}else{
			printf("(%x)", *mode);
		}
		if(mode != ((uByte*)mad + mad->bFunctionLength-1)){
			printf(",");
		}
	}
	printf("\n");

	if(mad->bType == UMCPC_ACM_TYPE_AB5){
		sc->sc_is_ucom = 0;
		ufoma_init_pseudo_ucom(sc);
	}else{
		if(ufoma_init_modem(sc, uaa)){
			goto error;
		}
	}
	elements = mad->bFunctionLength - sizeof(*mad)+1;

	sc->sc_msgxf = usbd_alloc_xfer(ucom->sc_udev);
	sc->sc_nummsg = 0;

	/*Initialize Mode vars.*/
	sc->sc_modetable = malloc(elements + 1, M_USBDEV, M_WAITOK);
	sc->sc_modetable[0] = elements + 1;
	bcopy(mad->bMode, &sc->sc_modetable[1], elements);
	sc->sc_currentmode = UMCPC_ACM_MODE_UNLINKED;
	sc->sc_modetoactivate = mad->bMode[0];
	/*Sysctls*/
	sctx = device_get_sysctl_ctx(self);
	soid = device_get_sysctl_tree(self);

	SYSCTL_ADD_PROC(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "supportmode",
			CTLFLAG_RD|CTLTYPE_STRING, sc, 0, ufoma_sysctl_support,
			"A", "Supporting port role");

	SYSCTL_ADD_PROC(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "currentmode",
			CTLFLAG_RD|CTLTYPE_STRING, sc, 0, ufoma_sysctl_current,
			"A", "Current port role");

	SYSCTL_ADD_PROC(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "openmode",
			CTLFLAG_RW|CTLTYPE_STRING, sc, 0, ufoma_sysctl_open,
			"A", "Mode to transit when port is opened");
	return 0;
 error:
	if(sc->sc_modetable)
		free(sc->sc_modetable, M_USBDEV);
	return EIO;
}

static int
ufoma_detach(device_t self)
{
	struct ufoma_softc *sc = device_get_softc(self);
	int rv = 0;

	usbd_free_xfer(sc->sc_msgxf);
	sc->sc_ucom.sc_dying = 1;
	usbd_abort_pipe(sc->sc_notify_pipe);
	usbd_close_pipe(sc->sc_notify_pipe);
	if(sc->sc_is_ucom)
		ucom_detach(&sc->sc_ucom);
	else
		ttyfree(sc->sc_ucom.sc_tty);
	free(sc->sc_modetable, M_USBDEV);
	return rv;
}


static char *ufoma_mode_to_str(int mode)
{
	int i;
	for(i = 0 ;umcpc_modetostr_tab[i].str != NULL; i++){
		if(umcpc_modetostr_tab[i].mode == mode){
			return umcpc_modetostr_tab[i].str;
		}
	}
	return NULL;
}

static int ufoma_str_to_mode(char *str)
{
	int i;
	for(i = 0 ;umcpc_modetostr_tab[i].str != NULL; i++){
		if(strcmp(str, umcpc_modetostr_tab[i].str)==0){
			return umcpc_modetostr_tab[i].mode;
		}
	}
	return -1;
}

static void *ufoma_get_intconf(	usb_config_descriptor_t *cd,
	usb_interface_descriptor_t *id, int type, int subtype)
{
	uByte *p, *end;
	usb_descriptor_t *ud=NULL;
	int flag=0;

					     
	for(p = (uByte *)cd,end = p + UGETW(cd->wTotalLength); p < end;
	    p += ud->bLength){
		ud = (usb_descriptor_t *)p;
		if(flag && ud->bDescriptorType==UDESC_INTERFACE){
			return NULL;
		}
		/*Read through this interface desc.*/
		if(bcmp(p, id, sizeof(*id))==0){
			flag=1;
			continue;
		}
		if(flag==0)
			continue;
		if(ud->bDescriptorType == type 
		    && ud->bDescriptorSubtype == subtype){
			break;
		}
	}
	return ud;
}



static int ufoma_link_state(struct ufoma_softc *sc)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	int err;

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = UMCPC_SET_LINK;
	USETW(req.wValue, UMCPC_CM_MOBILE_ACM);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, sc->sc_modetable[0]);	
	err = usbd_do_request(ucom->sc_udev, &req, sc->sc_modetable);
	if(err){
		printf("SET_LINK:%d\n", err);
		return EIO;
	}
	err = tsleep(&sc->sc_currentmode, PZERO|PCATCH, "fmalnk", hz);
	if(err){
		printf("NO response");
		return EIO;
	}
	if(sc->sc_currentmode != UMCPC_ACM_MODE_DEACTIVATED){
		return EIO;
	}
	return 0;
}

static int ufoma_activate_state(struct ufoma_softc *sc, int state)
{
	usb_device_request_t req;
	int err;
	struct ucom_softc *ucom = &sc->sc_ucom;

	req.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
	req.bRequest = UMCPC_ACTIVATE_MODE;
	USETW(req.wValue, state);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);	
	
	err = usbd_do_request(ucom->sc_udev, &req, NULL);
	if(err){
		printf("%s:ACTIVATE:%s\n", device_get_nameunit(ucom->sc_dev), usbd_errstr(err));
		return EIO;
	}

	err = tsleep(&sc->sc_currentmode, PZERO|PCATCH, "fmaact", UFOMA_MAX_TIMEOUT*hz);
	if(err){
		printf("%s:NO response", device_get_nameunit(ucom->sc_dev));
		return EIO;
	}
	if(sc->sc_currentmode != state){
		return EIO;
	}
	return 0;
}


static inline void ufoma_setup_msg_req(struct ufoma_softc *sc, usb_device_request_t *req)
{
		req->bmRequestType = UT_READ_CLASS_INTERFACE;
		req->bRequest = UCDC_GET_ENCAPSULATED_RESPONSE;
		USETW(req->wIndex, sc->sc_ctl_iface_no);
		USETW(req->wValue, 0);
		USETW(req->wLength, UFOMA_CMD_BUF_SIZE);
}


static void ufoma_msg(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	usb_device_request_t req;
	struct ufoma_softc *sc = priv;
	int actlen,i;
	struct ucom_softc *ucom= &sc->sc_ucom;
	usbd_get_xfer_status(xfer, NULL, NULL, &actlen ,NULL);
	ufoma_setup_msg_req(sc, &req);
	mtx_lock(&sc->sc_mtx);
	for(i = 0;i < actlen; i++){
		if(sc->sc_ucom.sc_tty->t_state & TS_ISOPEN)
			ttyld_rint(sc->sc_ucom.sc_tty, sc->sc_resbuffer[i]);
	}
	sc->sc_nummsg--;
	if(sc->sc_nummsg){
		usbd_setup_default_xfer(sc->sc_msgxf, ucom->sc_udev, 
		    priv, USBD_DEFAULT_TIMEOUT, &req,
		    sc->sc_resbuffer,
		    UFOMA_CMD_BUF_SIZE,
		    0, ufoma_msg);
		usbd_transfer(sc->sc_msgxf);
	}
	mtx_unlock(&sc->sc_mtx);

}

static void ufoma_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ufoma_softc *sc = priv;
	unsigned int a;
	struct ucom_softc *ucom =&sc->sc_ucom;
	usb_device_request_t req;
	ufoma_setup_msg_req(sc, &req);
	u_char mstatus;

	if (sc->sc_ucom.sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		printf("%s: abnormal status: %s\n", device_get_nameunit(ucom->sc_dev),
		       usbd_errstr(status));
		return;
	}
	if((sc->sc_notify_buf.bmRequestType == UT_READ_VENDOR_INTERFACE)&&
	    (sc->sc_notify_buf.bNotification == UMCPC_REQUEST_ACKNOLEDGE)){
		a =  UGETW(sc->sc_notify_buf.wValue);
		sc->sc_currentmode = a>>8;
		if(!(a&0xff)){
			printf("%s:Mode change Failed\n", device_get_nameunit(ucom->sc_dev));
		}
		wakeup(&sc->sc_currentmode);
	}
	if(sc->sc_notify_buf.bmRequestType != UCDC_NOTIFICATION){
		return;
	}
	switch(sc->sc_notify_buf.bNotification){
	case UCDC_N_RESPONSE_AVAILABLE:
		if(sc->sc_is_ucom){
			printf("%s:wrong response request?\n", device_get_nameunit(ucom->sc_dev));
			break;
		}
		mtx_lock(&sc->sc_mtx);
		if(!sc->sc_nummsg){
			usbd_setup_default_xfer(sc->sc_msgxf, ucom->sc_udev, 
			    priv, USBD_DEFAULT_TIMEOUT, &req, sc->sc_resbuffer,
			    UFOMA_CMD_BUF_SIZE,
			    0, ufoma_msg);
			usbd_transfer(sc->sc_msgxf);
		}
		sc->sc_nummsg++;
		mtx_unlock(&sc->sc_mtx);
		break;
	case UCDC_N_SERIAL_STATE:
		if(!sc->sc_is_ucom){
			printf("%s:wrong sereal request?\n",device_get_nameunit(ucom->sc_dev));
			break;
		}

		/*
		 * Set the serial state in ucom driver based on
		 * the bits from the notify message
		 */
		if (UGETW(sc->sc_notify_buf.wLength) != 2) {
			printf("%s: Invalid notification length! (%d)\n",
			       device_get_nameunit(ucom->sc_dev),
			       UGETW(sc->sc_notify_buf.wLength));
			break;
		}
		DPRINTF(("%s: notify bytes = %02x%02x\n",
			 device_get_nameunit(ucom->sc_dev),
			 sc->sc_notify_buf.data[0],
			 sc->sc_notify_buf.data[1]));
		/* Currently, lsr is always zero. */
		sc->sc_lsr = sc->sc_msr = 0;
		mstatus = sc->sc_notify_buf.data[0];

		if (ISSET(mstatus, UCDC_N_SERIAL_RI))
			sc->sc_msr |= SER_RI;
		if (ISSET(mstatus, UCDC_N_SERIAL_DSR))
			sc->sc_msr |= SER_DSR;
		if (ISSET(mstatus, UCDC_N_SERIAL_DCD))
			sc->sc_msr |= SER_DCD;
		/* Deferred notifying to the ucom layer */
		taskqueue_enqueue(taskqueue_swi_giant, &sc->sc_task);
		break;
	default:
		break;
	}
}

static int ufoma_init_pseudo_ucom(struct ufoma_softc *sc)
{
	struct tty *tp;
	struct ucom_softc *ucom = &sc->sc_ucom;
	tp = ucom->sc_tty = ttyalloc();
	tp->t_sc = sc;
	tp->t_oproc = ufomastart;
	tp->t_stop = nottystop;
	tp->t_open = ufomaopen;
	tp->t_close = ufomaclose;
	ttycreate(tp, TS_CALLOUT, "U%d", device_get_unit(ucom->sc_dev));

	return 0;
}


static int ufomaopen(struct tty * tty, struct cdev *cdev)
{

	struct ufoma_softc *sc = tty->t_sc;
	
	if(sc->sc_ucom.sc_dying)
		return (ENXIO);
	
	mtx_lock(&sc->sc_mtx);
	if(sc->sc_isopen){
		mtx_unlock(&sc->sc_mtx);
		return EBUSY;
	}
	mtx_unlock(&sc->sc_mtx);

	return 	ufoma_ucom_open(sc, 0);
}

static void ufomaclose(struct tty *tty)
{
	struct ufoma_softc *sc = tty->t_sc;
	ufoma_ucom_close(sc, 0);
}

static void ufomastart(struct tty *tp)
{
	struct ufoma_softc *sc = tp->t_sc;
	struct ucom_softc *ucom = &sc->sc_ucom;
	usb_device_request_t req;
	int x;
	uByte c;
	x = spltty();
	if(tp->t_state  & (TS_TIMEOUT | TS_TTSTOP)){
		ttwwakeup(tp);
		return;
	}

	tp->t_state |= TS_BUSY;
	while(tp->t_outq.c_cc != 0){
		c = getc(&tp->t_outq);
		req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
		req.bRequest = UCDC_SEND_ENCAPSULATED_COMMAND;
		USETW(req.wIndex, sc->sc_ctl_iface_no);
		USETW(req.wValue, 0);
		USETW(req.wLength, 1);
		usbd_do_request(ucom->sc_udev, &req, &c);
	}
	tp->t_state &= ~TS_BUSY;
	ttwwakeup(tp);
	splx(x);
}

static int ufoma_ucom_open(void *p, int portno)
{
	struct ufoma_softc *sc = p;
	if(sc->sc_currentmode == UMCPC_ACM_MODE_UNLINKED){
		ufoma_link_state(sc);
	}
	sc->sc_cmdbp = 0;
	if(sc->sc_currentmode == UMCPC_ACM_MODE_DEACTIVATED){
		ufoma_activate_state(sc, sc->sc_modetoactivate);
	}

	return 0;
}

static void ufoma_ucom_close(void *p, int portno)
{
	struct ufoma_softc *sc = p;
	ufoma_activate_state(sc, UMCPC_ACM_MODE_DEACTIVATED);
	return ;
}

void
ufoma_break(struct ufoma_softc *sc, int onoff)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	DPRINTF(("ufoma_break: onoff=%d\n", onoff));

	if (!(sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(ucom->sc_udev, &req, 0);
}

void
ufoma_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct ufoma_softc *sc = addr;

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

static void ufoma_set(void * addr, int portno, int reg, int onoff)
{
	struct ufoma_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		ufoma_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		ufoma_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		ufoma_break(sc, onoff);
		break;
	default:
		break;
	}

}

static void
ufoma_set_line_state(struct ufoma_softc *sc)
{
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;
	int ls;

	ls = (sc->sc_dtr ? UCDC_LINE_DTR : 0) |
	     (sc->sc_rts ? UCDC_LINE_RTS : 0);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, ls);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(ucom->sc_udev, &req, 0);

}

void
ufoma_dtr(struct ufoma_softc *sc, int onoff)
{
	DPRINTF(("ufoma_foma: onoff=%d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	ufoma_set_line_state(sc);
}

void
ufoma_rts(struct ufoma_softc *sc, int onoff)
{
	DPRINTF(("ufoma_foma: onoff=%d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	ufoma_set_line_state(sc);
}

usbd_status
ufoma_set_line_coding(struct ufoma_softc *sc, usb_cdc_line_state_t *state)
{

	usbd_status err;
	usb_device_request_t req;
	struct ucom_softc *ucom = &sc->sc_ucom;

	DPRINTF(("ufoma_set_line_coding: rate=%d fmt=%d parity=%d bits=%d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("ufoma_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(ucom->sc_udev, &req, state);
	if (err) {
		DPRINTF(("ufoma_set_line_coding: failed, err=%s\n",
			 usbd_errstr(err)));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

static int
ufoma_param(void *addr, int portno, struct termios *t)
{

	struct ufoma_softc *sc = addr;
	usbd_status err;
	usb_cdc_line_state_t ls;

	DPRINTF(("ufoma_param: sc=%p\n", sc));

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}

	err = ufoma_set_line_coding(sc, &ls);
	if (err) {
		DPRINTF(("ufoma_param: err=%s\n", usbd_errstr(err)));
		return (ENOTTY);
	}

	return (0);
}

static int ufoma_init_modem(struct ufoma_softc *sc,struct usb_attach_arg *uaa)
{
	struct ucom_softc *ucom = &sc->sc_ucom;
	usb_config_descriptor_t *cd;
	usb_cdc_acm_descriptor_t *acm;
	usb_cdc_cm_descriptor_t *cmd;	
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	const char *devname = device_get_nameunit(ucom->sc_dev);
	int i;
	cd = usbd_get_config_descriptor(ucom->sc_udev);
	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);

	sc->sc_is_ucom = 1;
	cmd = ufoma_get_intconf(cd, id, UDESC_CS_INTERFACE, UDESCSUB_CDC_CM);
	sc->sc_cm_cap = cmd->bmCapabilities;
	acm = ufoma_get_intconf(cd, id, UDESC_CS_INTERFACE, UDESCSUB_CDC_ACM);
	sc->sc_acm_cap = acm->bmCapabilities;
	
	if(cmd == NULL)
		return -1;
	sc->sc_data_iface_no = cmd->bDataInterface;
	printf("%s: data interface %d, has %sCM over data, has %sbreak\n",
	    devname, sc->sc_data_iface_no,
	    sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	    sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");
	
	for(i = 0; i < uaa->nifaces; i++){
		if(!uaa->ifaces[i])
			continue;
		id = usbd_get_interface_descriptor(uaa->ifaces[i]);
		if(id != NULL &&
		    id->bInterfaceNumber == sc->sc_data_iface_no){
			sc->sc_data_iface = uaa->ifaces[i];
			//uaa->ifaces[i] = NULL;
		}
	}

	ucom->sc_iface = sc->sc_data_iface;
	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	for(i = 0 ; i < id->bNumEndpoints; i++){
		ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface, i);
		if(ed == NULL){
			printf("%s: endpoint descriptor for %d\n",
			    devname,i);
			return -1;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ucom->sc_bulkout_no = ed->bEndpointAddress;
		}
	}
	
	if (ucom->sc_bulkin_no == -1) {
		printf("%s: Could not find data bulk in\n", devname);
		return -1;
	}
	if (ucom->sc_bulkout_no == -1) {
		printf("%s: Could not find data bulk out\n", devname);
		return -1;
	}
	
	sc->sc_dtr = -1;
	
	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	ucom->sc_ibufsize = UMODEMIBUFSIZE;
	ucom->sc_obufsize = UMODEMOBUFSIZE;
	ucom->sc_ibufsizepad = UMODEMIBUFSIZE;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &ufoma_callback;
	TASK_INIT(&sc->sc_task, 0, ufoma_notify, sc);
	ucom_attach(&sc->sc_ucom);
	
	return 0;
}

static void
ufoma_notify(void *arg, int count)
{
	struct ufoma_softc *sc;

	sc = (struct ufoma_softc *)arg;
	if (sc->sc_ucom.sc_dying)
		return;
	ucom_status_change(&sc->sc_ucom);
}
static int ufoma_sysctl_support(SYSCTL_HANDLER_ARGS)
{
	struct ufoma_softc *sc = (struct ufoma_softc *)oidp->oid_arg1;
	struct sbuf sb;
	int i;
	char *mode;

	sbuf_new(&sb, NULL, 1, SBUF_AUTOEXTEND);
	for(i = 1; i < sc->sc_modetable[0]; i++){
		mode = ufoma_mode_to_str(sc->sc_modetable[i]);
		if(mode !=NULL){
			sbuf_cat(&sb, mode);
		}else{
			sbuf_printf(&sb, "(%02x)", sc->sc_modetable[i]);
		}
		if(i < (sc->sc_modetable[0]-1))
			sbuf_cat(&sb, ",");
	}
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	
	return 0;
}
static int ufoma_sysctl_current(SYSCTL_HANDLER_ARGS)
{
	struct ufoma_softc *sc = (struct ufoma_softc *)oidp->oid_arg1;
	char *mode;
	char subbuf[]="(XXX)";
	mode = ufoma_mode_to_str(sc->sc_currentmode);
	if(!mode){
		mode = subbuf;
		snprintf(subbuf, sizeof(subbuf), "(%02x)", sc->sc_currentmode);
	}
	sysctl_handle_string(oidp, mode, strlen(mode), req);
	
	return 0;
	
}
static int ufoma_sysctl_open(SYSCTL_HANDLER_ARGS)
{
	struct ufoma_softc *sc = (struct ufoma_softc *)oidp->oid_arg1;
	char *mode;
	char subbuf[40];
	int newmode;
	int error;
	int i;

	mode = ufoma_mode_to_str(sc->sc_modetoactivate);
	if(mode){
		strncpy(subbuf, mode, sizeof(subbuf));
	}else{
		snprintf(subbuf, sizeof(subbuf), "(%02x)", sc->sc_modetoactivate);
	}
	error = sysctl_handle_string(oidp, subbuf, sizeof(subbuf), req);
	if(error != 0 || req->newptr == NULL){
		return error;
	}
	
	if((newmode = ufoma_str_to_mode(subbuf)) == -1){
		return EINVAL;
	}
	
	for(i = 1 ; i < sc->sc_modetable[0] ; i++){
		if(sc->sc_modetable[i] == newmode){
			sc->sc_modetoactivate = newmode;
			return 0;
		}
	}
	
	return EINVAL;
}
