/*	$NetBSD: usb_port.h,v 1.11 1999/09/11 08:19:27 augustss Exp $	*/
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

#if defined(__NetBSD__)
/*
 * NetBSD
 */

#include "opt_usbverbose.h"

typedef struct device *device_ptr_t;
#define USBBASEDEVICE struct device
#define USBDEV(bdev) (&(bdev))
#define USBDEVNAME(bdev) ((bdev).dv_xname)
#define USBDEVPTRNAME(bdevptr) ((bdevptr)->dv_xname)

#define DECLARE_USB_DMA_T \
	struct usb_dma_block; \
	typedef struct { \
		struct usb_dma_block *block; \
		u_int offs; \
	} usb_dma_t

#define usb_timeout(f, d, t, h) timeout((f), (d), (t))
#define usb_untimeout(f, d, h) untimeout((f), (d))

#define logprintf printf

#define USB_DECLARE_DRIVER_NAME_INIT(_1, dname, _2)  \
int __CONCAT(dname,_match) __P((struct device *, struct cfdata *, void *)); \
void __CONCAT(dname,_attach) __P((struct device *, struct device *, void *)); \
int __CONCAT(dname,_detach) __P((struct device *, int)); \
int __CONCAT(dname,_activate) __P((struct device *, enum devact)); \
\
extern struct cfdriver __CONCAT(dname,_cd); \
\
struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach), \
	__CONCAT(dname,_detach), \
	__CONCAT(dname,_activate), \
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

#define USB_DETACH(dname) \
int \
__CONCAT(dname,_detach)(self, flags) \
	struct device *self; \
	int flags;

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self

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
	(config_found_sm(parent, args, print, sub))

#elif defined(__OpenBSD__)
/*
 * OpenBSD
 */
#define	memcpy(d, s, l)		bcopy((s),(d),(l))
#define	memset(d, v, l)		bzero((d),(l))
#define bswap32(x)		swap32(x)
#define kthread_create1		kthread_create
#define kthread_create		kthread_create_deferred

#define	usbpoll			usbselect
#define	uhidpoll		uhidselect
#define	ugenpoll		ugenselect

typedef struct device device_ptr_t;
#define USBBASEDEVICE struct device
#define USBDEV(bdev) (&(bdev))
#define USBDEVNAME(bdev) ((bdev).dv_xname)
#define USBDEVPTRNAME(bdevptr) ((bdevptr)->dv_xname)

#define DECLARE_USB_DMA_T \
	struct usb_dma_block; \
	typedef struct { \
		struct usb_dma_block *block; \
		u_int offs; \
	} usb_dma_t

#define usb_timeout(f, d, t, h) timeout((f), (d), (t))
#define usb_untimeout(f, d, h) untimeout((f), (d))

#define USB_DECLARE_DRIVER_NAME_INIT(_1, dname, _2)  \
int __CONCAT(dname,_match) __P((struct device *, void *, void *)); \
void __CONCAT(dname,_attach) __P((struct device *, struct device *, void *)); \
int __CONCAT(dname,_detach) __P((struct device *, int)); \
int __CONCAT(dname,_activate) __P((struct device *, enum devact)); \
\
struct cfdriver __CONCAT(dname,_cd) = { \
	NULL, #dname, DV_DULL \
}; \
\
struct cfattach __CONCAT(dname,_ca) = { \
	sizeof(struct __CONCAT(dname,_softc)), \
	__CONCAT(dname,_match), \
	__CONCAT(dname,_attach), \
	__CONCAT(dname,_detach), \
	__CONCAT(dname,_activate), \
}

#define USB_MATCH(dname) \
int \
__CONCAT(dname,_match)(parent, match, aux) \
	struct device *parent; \
	void *match; \
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

#define USB_DETACH(dname) \
int \
__CONCAT(dname,_detach)(self, flags) \
	struct device *self; \
	int flags;

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		(struct __CONCAT(dname,_softc) *)self

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
	(config_found_sm(parent, args, print, sub))

#elif defined(__FreeBSD__)
/*
 * FreeBSD
 */

#include "opt_usb.h"

#define USBVERBOSE

#define device_ptr_t device_t
#define USBBASEDEVICE device_t
#define USBDEV(bdev) (bdev)
#define USBDEVNAME(bdev) device_get_nameunit(bdev)
#define USBDEVPTRNAME(bdev) device_get_nameunit(bdev)

#define DECLARE_USB_DMA_T typedef void * usb_dma_t

/* XXX Change this when FreeBSD has memset
 */
#define	memcpy(d, s, l)		bcopy((s),(d),(l))
#define	memset(d, v, l)		bzero((d),(l))
#define bswap32(x)		swap32(x)		/* XXX not available in FreeBSD */
#define kthread_create1
#define kthread_create


#define usb_timeout(f, d, t, h) ((h) = timeout((f), (d), (t)))
#define usb_untimeout(f, d, h) untimeout((f), (d), (h))

#define USB_DECLARE_DRIVER_NAME_INIT(name, dname, init...) \
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
        name, \
        __CONCAT(dname,_methods), \
        sizeof(struct __CONCAT(dname,_softc)) \
}

#define USB_MATCH(dname) \
static int \
__CONCAT(dname,_match)(device_t self)

#define USB_MATCH_START(dname, uaa) \
        struct usb_attach_arg *uaa = device_get_ivars(self)

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
	device_set_desc_copy(self, devinfo)

#define USB_DETACH(dname) \
static int \
__CONCAT(dname,_detach)(device_t self)

#define USB_DETACH_START(dname, sc) \
	struct __CONCAT(dname,_softc) *sc = device_get_softc(self)

#define USB_GET_SC_OPEN(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		devclass_get_softc(__CONCAT(dname,_devclass), unit); \
	if (!sc) \
		return (ENXIO)

#define USB_GET_SC(dname, unit, sc) \
	struct __CONCAT(dname,_softc) *sc = \
		devclass_get_softc(__CONCAT(dname,_devclass), unit)

#define USB_DO_ATTACH(dev, bdev, parent, args, print, sub) \
	(device_probe_and_attach((bdev)) == 0 ? (bdev) : 0)

/* conversion from one type of queue to the other */
/* XXX In FreeBSD SIMPLEQ_REMOVE_HEAD only removes the head element.
 */
#define SIMPLEQ_REMOVE_HEAD(h, e, f)	do {				\
		if ( (e) != SIMPLEQ_FIRST((h)) )			\
			panic("Removing other than first element");	\
		STAILQ_REMOVE_HEAD(h, f);				\
} while (0)
#define SIMPLEQ_INSERT_HEAD	STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL	STAILQ_INSERT_TAIL
#define SIMPLEQ_NEXT		STAILQ_NEXT
#define SIMPLEQ_FIRST		STAILQ_FIRST
#define SIMPLEQ_HEAD		STAILQ_HEAD
#define SIMPLEQ_INIT		STAILQ_INIT
#define SIMPLEQ_HEAD_INITIALIZER	STAILQ_HEAD_INITIALIZER
#define SIMPLEQ_ENTRY		STAILQ_ENTRY

#include <sys/syslog.h>
/*
#define logprintf(args...)	log(LOG_DEBUG, args)
*/
#define logprintf		printf

#endif /* __FreeBSD__ */

#define NONE {0,0}

#define USB_DECLARE_DRIVER_NAME(name, dname) \
	USB_DECLARE_DRIVER_NAME_INIT(#name, dname, NONE )
#define USB_DECLARE_DRIVER_INIT(dname, init...) \
	USB_DECLARE_DRIVER_NAME_INIT(#dname, dname, init)
#define USB_DECLARE_DRIVER(dname) \
	USB_DECLARE_DRIVER_NAME_INIT(#dname, dname, NONE )

#if defined(__NetBSD__) || defined(__OpenBSD__)
#elif defined(__FreeBSD__)
#endif
