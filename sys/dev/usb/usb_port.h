/*	$NetBSD: usb_port.h,v 1.5 1999/01/08 11:58:25 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
 * Macro's to cope with the differences between operating systems.
 */

/*
 * NetBSD
 */

#if defined(__NetBSD__)
#include "opt_usbverbose.h"

#define USBDEVNAME(bdev) ((bdev).dv_xname)

typedef struct device bdevice;			/* base device */

#define usb_timeout(f, d, t, h) timeout((f), (d), (t))
#define usb_untimeout(f, d, h) untimeout((f), (d))

#define USB_DECLARE_DRIVER_INIT(dname, _2)  \
int __CONCAT(dname,_match) __P((struct device *, struct cfdata *, void *)); \
void __CONCAT(dname,_attach) __P((struct device *, struct device *, void *)); \
\
extern struct cfdriver __CONCAT(dname,_cd); \
\
struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach) \
}

#define USB_MATCH(dname) \
int \
__CONCAT(dname,_match)(parent, match, aux) \
	struct device *parent; \
	struct cfdata *match; \
	void *aux;

#define USB_MATCH_START(dname, uaa) \
	struct usb_attach_arg *uaa = aux

#define USB_ATTACH(dname) \
void \
__CONCAT(dname,_attach)(parent, self, aux) \
	struct device *parent; \
	struct device *self; \
	void *aux;

#define USB_ATTACH_START(dname, sc, uaa) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self; \
	struct usb_attach_arg *uaa = aux

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return
#define USB_ATTACH_SUCCESS_RETURN	return

#define USB_ATTACH_SETUP printf("\n")

#define USB_GET_SC_OPEN(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc; \
	if (unit >= __CONCAT(dname,_cd).cd_ndevs) \
		return (ENXIO); \
	sc = __CONCAT(dname,_cd).cd_devs[unit]; \
	if (!sc) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc = __CONCAT(dname,_cd).cd_devs[unit]

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	((dev)->softc = config_found_sm(parent, args, print, sub))

#define logprintf	printf



#elif defined(__FreeBSD__)
/*
 * FreeBSD
 */

#include "opt_usb.h"
/*
 * The following is not a type def to avoid error messages
 * because of includes in the wrong order.
 */
#define bdevice device_t
#define USBDEVNAME(bdev) device_get_nameunit(bdev)

/* XXX Change this when FreeBSD has memset
 */
#define memset(d, v, s)	\
		do{			\
		if ((v) == 0)		\
			bzero((d), (s));	\
		else			\
			panic("Non zero filler for memset, cannot handle!"); \
		} while (0)


#define usb_timeout(f, d, t, h) ((h) = timeout((f), (d), (t)))
#define usb_untimeout(f, d, h) untimeout((f), (d), (h))

#define USB_DECLARE_DRIVER_INIT(dname, init...) \
static device_probe_t __CONCAT(dname,_match); \
static device_attach_t __CONCAT(dname,_attach); \
static device_detach_t __CONCAT(dname,_detach); \
\
static devclass_t __CONCAT(dname,_devclass); \
\
static device_method_t __CONCAT(dname,_methods)[] = { \
        DEVMETHOD(device_probe, __CONCAT(dname,_match)), \
        DEVMETHOD(device_attach, __CONCAT(dname,_attach)), \
        DEVMETHOD(device_detach, __CONCAT(dname,_detach)), \
	init, \
        {0,0} \
}; \
\
static driver_t __CONCAT(dname,_driver) = { \
        #dname, \
        __CONCAT(dname,_methods), \
        DRIVER_TYPE_MISC, \
        sizeof(struct __CONCAT(dname,_softc)) \
}

#define USB_MATCH(dname) \
static int \
__CONCAT(dname,_match)(device_t device)

#define USB_MATCH_START(dname, uaa) \
        struct usb_attach_arg *uaa = device_get_ivars(device)

#define USB_ATTACH(dname) \
static int \
__CONCAT(dname,_attach)(device_t self)

#define USB_ATTACH_START(dname, sc, uaa) \
        struct __CONCAT(dname,_softc) *sc = device_get_softc(self); \
        struct usb_attach_arg *uaa = device_get_ivars(self)

/* Returns from attach */
#define USB_ATTACH_ERROR_RETURN	return ENXIO
#define USB_ATTACH_SUCCESS_RETURN	return 0

#define USB_ATTACH_SETUP \
	sc->sc_dev = self; \
	usbd_device_set_desc(self, devinfo)

#define USB_GET_SC_OPEN(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		devclass_get_softc(__CONCAT(dname,_devclass), unit); \
	if (!sc) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		devclass_get_softc(__CONCAT(dname,_devclass), unit)

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub)	\
	(device_probe_and_attach((bdev)) == 0 ?			\
		((dev)->softc = device_get_softc(bdev)) : 0)

/* conversion from one type of queue to the other */
#define SIMPLEQ_REMOVE_HEAD	STAILQ_REMOVE_HEAD_UNTIL
#define SIMPLEQ_INSERT_HEAD	STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL	STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT		STAILQ_NEXT
#define SIMPLEQ_FIRST		STAILQ_FIRST
#define SIMPLEQ_HEAD		STAILQ_HEAD
#define SIMPLEQ_INIT		STAILQ_INIT
#define SIMPLEQ_ENTRY		STAILQ_ENTRY

#include <sys/syslog.h>
#define logprintf(args...)	log(LOG_DEBUG, args);

#endif /* __FreeBSD__ */



#define USB_DECLARE_DRIVER(dname) \
	USB_DECLARE_DRIVER_INIT(dname, {0,0} )
