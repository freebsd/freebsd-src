# $Id: Makefile,v 1.1 2004/08/13 18:30:24 max Exp $
# $FreeBSD: src/sys/modules/vkbd/Makefile,v 1.2 2004/12/29 08:44:30 imp Exp $

.PATH:	${.CURDIR}/../../dev/vkbd

KMOD=	vkbd
SRCS=	vkbd.c opt_kbd.h

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}

.include <bsd.kmod.mk>
