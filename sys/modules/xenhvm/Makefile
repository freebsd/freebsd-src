# $FreeBSD$

KMOD=	xenhvm

DEVXENDIR=	${.CURDIR}/../../dev/xen/
XENDIR=	${.CURDIR}/../../xen/
.PATH:	${DEVXENDIR}/balloon ${DEVXENDIR}/blkfront ${DEVXENDIR}/blkback \
	${DEVXENDIR}/control ${DEVXENDIR}/netback ${DEVXENDIR}/netfront \
	${DEVXENDIR}/xenpci ${XENDIR}/ ${XENDIR}/evtchn \
	${XENDIR}/xenbus ${XENDIR}/xenstore

SRCS=	xenhvm_mod.c	\
	balloon.c blkfront.c blkback.c control.c netback.c netfront.c	\
	xenpci.c evtchn.c gnttab.c features.c evtchn_dev.c		\
	xenbus.c xenbusb.c xenbusb_front.c xenbusb_back.c		\
	xenbus_if.c xenbus_if.h xenbusb_if.c xenbusb_if.h					\
	xenstore.c xenstore_dev.c
MFILES=	xen/xenbus/xenbus_if.m xen/xenbus/xenbusb_if.m
CFLAGS+=	-DXENHVM -DMODXENHVM

.include <bsd.kmod.mk>
